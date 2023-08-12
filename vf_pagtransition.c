#include "filters.h"
#include "internal.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"

#include "./pag/pag_impl.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>

#define FROM (0)
#define TO (1)

typedef struct {
	const AVClass *class;
	void *pctx;

	// input options
	int64_t duration;
	int64_t offset;

	char *from;
	char *to;
	char *source;

	PAG_Support_Pix_FMT pix_fmt;

	int64_t duration_pts;
	int64_t offset_pts;
	int64_t first_pts;
	int64_t last_pts;
	int64_t pts;
	int is_over;
	int need_second;
	int eof[2];
	AVFrame *xf[2];

} PAGTransitionContext;

#define OFFSET(x) offsetof(PAGTransitionContext, x)
#define FLAGS AV_OPT_FLAG_FILTERING_PARAM | AV_OPT_FLAG_VIDEO_PARAM

static const AVOption pagtransition_options[] = {
    {"duration", "set cross fade duration", OFFSET(duration), AV_OPT_TYPE_DURATION, {.i64 = AV_TIME_BASE}, 0, (60 * AV_TIME_BASE), FLAGS},
    {"offset", "set cross fade start relative to first input stream", OFFSET(offset), AV_OPT_TYPE_DURATION, {.i64 = 0}, INT64_MIN, INT64_MAX, FLAGS},
    {"from", "pag imageLayer name", OFFSET(from), AV_OPT_TYPE_STRING, {.str = "from"}, CHAR_MIN, CHAR_MAX, FLAGS},
    {"to", "pag imageLayer name", OFFSET(to), AV_OPT_TYPE_STRING, {.str = "to"}, CHAR_MIN, CHAR_MAX, FLAGS},
    {"source", "path to the pag-transition source file", OFFSET(source), AV_OPT_TYPE_STRING, {.str = NULL}, CHAR_MIN, CHAR_MAX, FLAGS},
    {NULL},
};

AVFILTER_DEFINE_CLASS(pagtransition);

static const enum AVPixelFormat alpha_pix_fmts[] = {
    AV_PIX_FMT_RGBA,
    AV_PIX_FMT_BGRA,
    AV_PIX_FMT_NONE,
};

static const enum AVPixelFormat support_pix_fmts[] = {
    AV_PIX_FMT_RGBA,
    AV_PIX_FMT_BGRA,
    AV_PIX_FMT_NONE,
};

static int setup_pag(AVFilterLink *inLink) {
	AVFilterContext *ctx = inLink->dst;
	PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);

	av_log(c, AV_LOG_TRACE, "pagtransition setup_pag\n");

	if (c->source == NULL || strlen(c->source) == 0) {
		av_log(c, AV_LOG_ERROR, "invalid pag file with source empty\n");
		return -1;
	}

	if (c->pctx != NULL) {
		pag_context_destory(c, c->pctx);
	}

	if (c->source != NULL) {
		c->pctx = pag_context_create(c, c->source);
		if (c->pctx == NULL) {
			av_log(c, AV_LOG_ERROR, "invalid pag file %s\n", c->source);
			return -1;
		}
		av_log(c, AV_LOG_INFO, "pag_context_create success\n");
		if (!pag_context_check_layer_exist(c, c->pctx, c->from)) {
			av_log(c, AV_LOG_ERROR, "pag from imagelayer not exists, name: %s\n", c->from);
			return -1;
		}

		if (!pag_context_check_layer_exist(c, c->pctx, c->to)) {
			av_log(c, AV_LOG_ERROR, "pag to imagelayer not exists, name: %s\n", c->to);
			return -1;
		}
	}

	bool alpha = ff_fmt_is_in(inLink->format, alpha_pix_fmts);
	av_log(c, AV_LOG_INFO, "alpha: %d, inLink->format: %s\n", alpha, av_get_pix_fmt_name(inLink->format));
	if (!alpha) {
		av_log(c, AV_LOG_ERROR, "only RGBA BGRA is supported\n");
		return -1;
	}
	if (inLink->format == AV_PIX_FMT_RGBA) {
		c->pix_fmt = PAG_Support_Pix_FMT_RGBA_8888;
	} else {
		c->pix_fmt = PAG_Support_Pix_FMT_BGRA_8888;
	}
	av_log(c, AV_LOG_INFO, "pag pix_fmt %s\n", av_get_pix_fmt_name(inLink->format));
	av_log(c, AV_LOG_INFO, "pag from_layer_name %s to_layer_name %s\n", c->from, c->to);
	return 0;
}

static int apply_transition(AVFilterContext *ctx,
                            AVFrame *a,
                            AVFrame *b) {
	PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
	AVFilterLink *fromLink = ctx->inputs[FROM];
	// AVFilterLink *toLink    = ctx->inputs[TO];
	AVFilterLink *outLink = ctx->outputs[0];
	float progress = av_clipf(((float)(c->pts - c->first_pts - c->offset_pts) / c->duration_pts), 0.f, 1.f);

	av_log(c, AV_LOG_INFO, "apply_transition progress %.1f\n", progress);

	AVFrame *out;

	out = ff_get_video_buffer(outLink, outLink->w, outLink->h);
	if (!out) {
		return AVERROR(ENOMEM);
	}

	av_frame_copy_props(out, a);

	av_log(c, AV_LOG_DEBUG, "fromFrame pix_fmt %s size %dx%d\n", av_get_pix_fmt_name(a->format), a->width, a->height);
	av_log(c, AV_LOG_DEBUG, "toFrame pix_fmt %s size %dx%d\n", av_get_pix_fmt_name(b->format), b->width, b->height);
	av_log(c, AV_LOG_DEBUG, "outFrame pix_fmt %s size %dx%d\n", av_get_pix_fmt_name(out->format), out->width, out->height);

	if (!pag_context_fill_image(c, c->pctx, (const void *)a->data[0], fromLink->w, fromLink->h, c->pix_fmt, true)) {
		av_log(c, AV_LOG_ERROR, "fill fromFrame to pag image fail\n");
		return AVERROR_BUG;
	}

	if (!pag_context_fill_image(c, c->pctx, (const void *)b->data[0], outLink->w, outLink->h, c->pix_fmt, false)) {
		av_log(c, AV_LOG_ERROR, "fill toFrame to pag image fail\n");
		return AVERROR_BUG;
	}

	if (!pag_context_renderer(c, c->pctx, c->from, c->to, progress, c->pix_fmt, (uint8_t *)out->data[0])) {
		av_log(c, AV_LOG_ERROR, "pag renderer fail\n");
		return AVERROR_BUG;
	}

	out->pts = c->pts;

	return ff_filter_frame(outLink, out);
}

static av_cold void uninit(AVFilterContext *ctx) {
	PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
	av_log(c, AV_LOG_TRACE, "pagtransition uninit\n");
	if (c->pctx != NULL) {
		pag_context_destory(c, c->pctx);
		c->pctx = NULL;
	}
}

static int query_formats(AVFilterContext *ctx) {
	PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
	av_log(c, AV_LOG_TRACE, "pagtransition query_formats\n");
	AVFilterFormats *fmts_list;

	fmts_list = ff_make_format_list((const int *)support_pix_fmts);
	if (!fmts_list) {
		return AVERROR(ENOMEM);
	}
	return ff_set_common_formats(ctx, fmts_list);
}

static int activate(AVFilterContext *ctx) {
	PAGTransitionContext *s = (PAGTransitionContext *)(ctx->priv);
	AVFilterLink *outlink = ctx->outputs[0];
	AVFrame *in = NULL;
	int ret = 0, status;
	int64_t pts;

	FF_FILTER_FORWARD_STATUS_BACK_ALL(outlink, ctx);

	if (s->is_over) {
		if (!s->eof[0]) {
			ret = ff_inlink_consume_frame(ctx->inputs[0], &in);
			if (ret > 0) {
				av_frame_free(&in);
			}
		}
		ret = ff_inlink_consume_frame(ctx->inputs[1], &in);
		if (ret < 0) {
			return ret;
		} else if (ret > 0) {
			in->pts = (in->pts - s->last_pts) + s->pts;
			return ff_filter_frame(outlink, in);
		} else if (ff_inlink_acknowledge_status(ctx->inputs[1], &status, &pts)) {
			ff_outlink_set_status(outlink, status, s->pts);
			return 0;
		} else if (!ret) {
			if (ff_outlink_frame_wanted(outlink)) {
				ff_inlink_request_frame(ctx->inputs[1]);
			}
			return 0;
		}
	}

	if (ff_inlink_queued_frames(ctx->inputs[0]) > 0) {
		s->xf[0] = ff_inlink_peek_frame(ctx->inputs[0], 0);
		if (s->xf[0]) {
			if (s->first_pts == AV_NOPTS_VALUE) {
				s->first_pts = s->xf[0]->pts;
			}
			s->pts = s->xf[0]->pts;
			if (s->first_pts + s->offset_pts > s->xf[0]->pts) {
				s->xf[0] = NULL;
				s->need_second = 0;
				ff_inlink_consume_frame(ctx->inputs[0], &in);
				return ff_filter_frame(outlink, in);
			}

			s->need_second = 1;
		}
	}

	if (s->xf[0] && ff_inlink_queued_frames(ctx->inputs[1]) > 0) {
		ff_inlink_consume_frame(ctx->inputs[0], &s->xf[0]);
		ff_inlink_consume_frame(ctx->inputs[1], &s->xf[1]);

		s->last_pts = s->xf[1]->pts;
		s->pts = s->xf[0]->pts;
		if (s->xf[0]->pts - (s->first_pts + s->offset_pts) > s->duration_pts) {
			s->is_over = 1;
		}
		ret = apply_transition(ctx, s->xf[0], s->xf[1]);
		av_frame_free(&s->xf[0]);
		av_frame_free(&s->xf[1]);
		return ret;
	}

	if (ff_inlink_queued_frames(ctx->inputs[0]) > 0 &&
	    ff_inlink_queued_frames(ctx->inputs[1]) > 0) {
		ff_filter_set_ready(ctx, 100);
		return 0;
	}

	if (ff_outlink_frame_wanted(outlink)) {
		if (!s->eof[0] && ff_outlink_get_status(ctx->inputs[0])) {
			s->eof[0] = 1;
			s->is_over = 1;
		}
		if (!s->eof[1] && ff_outlink_get_status(ctx->inputs[1])) {
			s->eof[1] = 1;
		}
		if (!s->eof[0] && !s->xf[0] && ff_inlink_queued_frames(ctx->inputs[0]) == 0)
			ff_inlink_request_frame(ctx->inputs[0]);
		if (!s->eof[1] && (s->need_second || s->eof[0]) && ff_inlink_queued_frames(ctx->inputs[1]) == 0)
			ff_inlink_request_frame(ctx->inputs[1]);
		if (s->eof[0] && s->eof[1] && (ff_inlink_queued_frames(ctx->inputs[0]) <= 0 && ff_inlink_queued_frames(ctx->inputs[1]) <= 0)) {
			ff_outlink_set_status(outlink, AVERROR_EOF, AV_NOPTS_VALUE);
		} else if (s->is_over) {
			ff_filter_set_ready(ctx, 100);
		}
		return 0;
	}

	return FFERROR_NOT_READY;
}

static int config_output(AVFilterLink *outLink) {
	AVFilterContext *ctx = outLink->src;
	PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
	AVFilterLink *fromLink = ctx->inputs[FROM];
	AVFilterLink *toLink = ctx->inputs[TO];
	// int ret = 0;

	av_log(c, AV_LOG_TRACE, "pagtransition config_output nb_inputs %d\n", ctx->nb_inputs);

	if (fromLink->format != toLink->format) {
		av_log(c, AV_LOG_ERROR, "inputs must be of same pixel format\n");
		return AVERROR(AVERROR_BUG);
	}

	if (fromLink->w != toLink->w || fromLink->h != toLink->h) {
		av_log(c, AV_LOG_ERROR, "First input link %s parameters "
		                        "(size %dx%d) do not match the corresponding "
		                        "second input link %s parameters (size %dx%d)\n",
		       ctx->input_pads[FROM].name, fromLink->w, fromLink->h,
		       ctx->input_pads[TO].name, toLink->w, toLink->h);
		return AVERROR(EINVAL);
	}

	outLink->w = fromLink->w;
	outLink->h = fromLink->h;
	outLink->time_base = fromLink->time_base;
	outLink->sample_aspect_ratio = fromLink->sample_aspect_ratio;
	outLink->frame_rate = fromLink->frame_rate;

	c->first_pts = c->last_pts = c->pts = AV_NOPTS_VALUE;

	if (c->duration) {
		c->duration_pts = av_rescale_q(c->duration, AV_TIME_BASE_Q, outLink->time_base);
	}

	if (c->offset) {
		c->offset_pts = av_rescale_q(c->offset, AV_TIME_BASE_Q, outLink->time_base);
	}

	int64_t duration_ts = c->duration_pts * av_q2d(outLink->time_base);
	int64_t offset_ts = c->offset_pts * av_q2d(outLink->time_base);
	av_log(c, AV_LOG_INFO, "transition duration %llds offset %llds\n", duration_ts, offset_ts);

	return 0;
}

static const AVFilterPad pagtransition_inputs[] = {
    {
        .name = "from",
        .type = AVMEDIA_TYPE_VIDEO,
        .config_props = setup_pag,
    },
    {
        .name = "to",
        .type = AVMEDIA_TYPE_VIDEO,
    },
};

static const AVFilterPad pagtransition_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
        .config_props = config_output,
    },
};

AVFilter ff_vf_pagtransition = {
    .name = "pagtransition",
    .description = NULL_IF_CONFIG_SMALL("libpag blend transitions"),
    .priv_size = sizeof(PAGTransitionContext),
    .uninit = uninit,
    FILTER_INPUTS(pagtransition_inputs),
    FILTER_OUTPUTS(pagtransition_outputs),
    FILTER_QUERY_FUNC(query_formats),
    .activate = activate,
    .priv_class = &pagtransition_class,
    .flags = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC,
};
