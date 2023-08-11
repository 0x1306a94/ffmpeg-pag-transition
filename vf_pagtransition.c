#include "framesync.h"
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
    FFFrameSync fs;

    // input options
    double duration;
    double offset;
    bool swap;
    char *source;

    PAG_Support_Pix_FMT pix_fmt;

    // timestamp of the first frame in the output, in the timebase units
    int64_t first_pts;
    bool finish;

} PAGTransitionContext;

#define OFFSET(x) offsetof(PAGTransitionContext, x)
#define FLAGS AV_OPT_FLAG_FILTERING_PARAM | AV_OPT_FLAG_VIDEO_PARAM

static const AVOption pagtransition_options[] = {
    {"duration", "transition duration in seconds", OFFSET(duration), AV_OPT_TYPE_DOUBLE, {.dbl = 1.0}, 0, DBL_MAX, FLAGS},
    {"offset", "delay before startingtransition in seconds", OFFSET(offset), AV_OPT_TYPE_DOUBLE, {.dbl = 0.0}, 0, DBL_MAX, FLAGS},
    {"swap", "swap from and to in renderering", OFFSET(swap), AV_OPT_TYPE_BOOL, {.i64 = 0}, INT_MIN, INT_MAX, FLAGS},
    {"source", "path to the pag-transition source file (defaults to basic fade)", OFFSET(source), AV_OPT_TYPE_STRING, {.str = NULL}, CHAR_MIN, CHAR_MAX, FLAGS},

    {NULL},
};

FRAMESYNC_DEFINE_CLASS(pagtransition, PAGTransitionContext, fs);

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

    if (c->pctx != NULL) {
        pag_context_destory(c->pctx);
    }

    if (c->source != NULL) {
        c->pctx = pag_context_create(c->source);
        if (c->pctx == NULL) {
            av_log(c, AV_LOG_ERROR, "invalid pag file\n");
            return -1;
        }
        av_log(c, AV_LOG_INFO, "pag_context_create success\n");
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
    av_log(c, AV_LOG_INFO, "pag pix_fmt %d\n", c->pix_fmt);
    return 0;
}

static AVFrame *apply_transition(FFFrameSync *fs,
                                 AVFilterContext *ctx,
                                 AVFrame *fromFrame,
                                 const AVFrame *toFrame) {
    PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
    AVFilterLink *fromLink = ctx->inputs[FROM];
    AVFilterLink *toLink = ctx->inputs[TO];
    AVFilterLink *outLink = ctx->outputs[0];
    AVFrame *outFrame;

    outFrame = ff_get_video_buffer(outLink, outLink->w, outLink->h);
    if (!outFrame) {
        return NULL;
    }

    av_frame_copy_props(outFrame, fromFrame);

    // if (fs->pts < c->first_pts) {
    //     av_frame_copy(outFrame, fromFrame);
    //     return outFrame;
    // }

    const float ts = ((fs->pts - c->first_pts) / (float)fs->time_base.den) - c->offset;
    const float progress = FFMAX(0.0f, FFMIN(1.0f, ts / c->duration));
    if (progress >= 1.0 && c->finish == true) {
        av_frame_copy_props(outFrame, toFrame);
        av_frame_copy(outFrame, toFrame);
        return outFrame;
    }

    av_log(c, AV_LOG_DEBUG, "fromFrame pix_fmt %s size %dx%d\n", av_get_pix_fmt_name(fromFrame->format), fromFrame->width, fromFrame->height);
    av_log(c, AV_LOG_DEBUG, "toFrame pix_fmt %s size %dx%d\n", av_get_pix_fmt_name(toFrame->format), toFrame->width, toFrame->height);
    av_log(c, AV_LOG_DEBUG, "outFrame pix_fmt %s size %dx%d swap %d\n", av_get_pix_fmt_name(outFrame->format), outFrame->width, outFrame->height, c->swap);

    if (!pag_context_fill_image(c->pctx, (const void *)fromFrame->data[0], fromLink->w, fromLink->h, c->pix_fmt, true)) {
        av_log(c, AV_LOG_ERROR, "fill fromFrame to pag image fail\n");
        av_frame_free(&fromFrame);
        av_frame_free(&outFrame);
        return NULL;
    }

    if (!pag_context_fill_image(c->pctx, (const void *)toFrame->data[0], outLink->w, outLink->h, c->pix_fmt, false)) {
        av_log(c, AV_LOG_ERROR, "fill toFrame to pag image fail\n");
        av_frame_free(&fromFrame);
        av_frame_free(&outFrame);
        return NULL;
    }

    if (!pag_context_renderer(c->pctx, c->swap, progress, c->pix_fmt, (uint8_t *)outFrame->data[0])) {
        av_log(c, AV_LOG_ERROR, "pag renderer fail\n");
        av_frame_free(&fromFrame);
        av_frame_free(&outFrame);
        return NULL;
    }

    if (progress >= 1.0) {
        c->finish = true;
    }
    av_frame_free(&fromFrame);
    return outFrame;
}

static int blend_frame(FFFrameSync *fs) {
    AVFilterContext *ctx = fs->parent;
    PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);

    AVFrame *fromFrame, *toFrame, *outFrame;
    int ret;

    ret = ff_framesync_dualinput_get(fs, &fromFrame, &toFrame);
    if (ret < 0) {
        return ret;
    }

    if (c->first_pts == AV_NOPTS_VALUE && fromFrame && fromFrame->pts != AV_NOPTS_VALUE) {
        c->first_pts = fromFrame->pts;
    }

    if (!toFrame) {
        return ff_filter_frame(ctx->outputs[0], fromFrame);
    }

    outFrame = apply_transition(fs, ctx, fromFrame, toFrame);
    if (!outFrame) {
        return AVERROR(ENOMEM);
    }

    return ff_filter_frame(ctx->outputs[0], outFrame);
}

static av_cold int init(AVFilterContext *ctx) {
    PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
    c->fs.on_event = blend_frame;
    c->first_pts = AV_NOPTS_VALUE;
    av_log(c, AV_LOG_TRACE, "pagtransition init inputs %d\n", ctx->nb_inputs);
    int nb_inputs = ctx->nb_inputs;
    for (int i = 0; i < nb_inputs; i++) {
        AVFilterPad pad = ctx->input_pads[i];
        av_log(c, AV_LOG_TRACE, "pagtransition init input format %s\n", pad.name);
    }

    return 0;
}

static av_cold void uninit(AVFilterContext *ctx) {
    PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
    ff_framesync_uninit(&c->fs);
    av_log(c, AV_LOG_TRACE, "pagtransition uninit\n");
    if (c->pctx != NULL) {
        pag_context_destory(c->pctx);
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
    PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
    return ff_framesync_activate(&c->fs);
}

static int config_output(AVFilterLink *outLink) {
    AVFilterContext *ctx = outLink->src;
    PAGTransitionContext *c = (PAGTransitionContext *)(ctx->priv);
    AVFilterLink *fromLink = ctx->inputs[FROM];
    AVFilterLink *toLink = ctx->inputs[TO];
    int ret;

    av_log(c, AV_LOG_TRACE, "pagtransition config_output nb_inputs %d\n", ctx->nb_inputs);

    if (fromLink->format != toLink->format) {
        av_log(c, AV_LOG_ERROR, "inputs must be of same pixel format\n");
        return AVERROR(EINVAL);
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
    // outLink->time_base = fromLink->time_base;
    outLink->frame_rate = fromLink->frame_rate;

    if ((ret = ff_framesync_init_dualinput(&c->fs, ctx)) < 0) {
        return ret;
    }

    return ff_framesync_configure(&c->fs);
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
    .preinit = pagtransition_framesync_preinit,
    .init = init,
    .uninit = uninit,
    // .inputs = pagtransition_inputs,
    // .nb_inputs = 2,
    // .outputs = pagtransition_outputs,
    // .nb_outputs = 1,
    FILTER_INPUTS(pagtransition_inputs),
    FILTER_OUTPUTS(pagtransition_outputs),
    FILTER_QUERY_FUNC(query_formats),
    .activate = activate,
    .priv_class = &pagtransition_class,
    .flags = AVFILTER_FLAG_SUPPORT_TIMELINE_GENERIC,
};
