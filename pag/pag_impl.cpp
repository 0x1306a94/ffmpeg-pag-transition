#include <memory>
#include <pag/file.h>
#include <pag/pag.h>

#include "./pag_impl.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "libavutil/opt.h"
#ifdef __cplusplus
}
#endif

struct PagContext {
    std::shared_ptr<pag::PAGFile> file;
    std::shared_ptr<pag::PAGSurface> surface;
    std::shared_ptr<pag::PAGPlayer> player;
    std::shared_ptr<pag::PAGImage> from;
    std::shared_ptr<pag::PAGImage> to;
};

static bool pag_context_fill_image_internal(std::shared_ptr<pag::PAGImage> &dst, const void *pixels, int width, int height,
                                            pag::ColorType colorType,
                                            pag::AlphaType alphaType) {
    auto image = pag::PAGImage::FromPixels(pixels, width, height, width * 4, colorType, alphaType);
    if (image == nullptr) {
        av_log(nullptr, AV_LOG_ERROR, "pag::PAGImage::FromPixels fail\n");
        return false;
    }
    av_log(nullptr, AV_LOG_DEBUG, "pag::PAGImage::FromPixels success\n");
    dst = image;
    return true;
}

#ifdef __cplusplus
extern "C" {
#endif

void *pag_context_create(const char *path) {
    auto pagFile = pag::PAGFile::Load(path);
    if (pagFile == nullptr) {
        return nullptr;
    }
    auto pagSurface = pag::PAGSurface::MakeOffscreen(pagFile->width(), pagFile->height());
    if (pagSurface == nullptr) {
        return nullptr;
    }

    auto pagPlayer = std::make_shared<pag::PAGPlayer>();
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setComposition(pagFile);

    PagContext *ctx = new PagContext();
    ctx->file = pagFile;
    ctx->surface = pagSurface;
    ctx->player = pagPlayer;
    av_log(nullptr, AV_LOG_DEBUG, "pag file %dx%d\n", pagFile->width(), pagFile->height());
    return static_cast<void *>(ctx);
}

void pag_context_destory(void *ctx) {
    if (ctx == nullptr) {
        return;
    }
    PagContext *pctx = static_cast<PagContext *>(ctx);
    delete pctx;
}

bool pag_context_fill_image(void *ctx, const void *pixels, int width, int height, PAG_Support_Pix_FMT fmt, bool is_from) {
    if (ctx == nullptr) {
        return false;
    }

    if (pixels == nullptr) {
        return false;
    }

    if (width == 0 || height == 0) {
        return false;
    }

    if (fmt < PAG_Support_Pix_FMT_RGBA_8888 || fmt > PAG_Support_Pix_FMT_BGRA_8888) {
        return false;
    }

    av_log(nullptr, AV_LOG_DEBUG, "pag_context_fill_image pixels %p %dx%d\n", pixels, width, height);
    pag::ColorType colorType = (fmt == PAG_Support_Pix_FMT_RGBA_8888) ? pag::ColorType::RGBA_8888 : pag::ColorType::BGRA_8888;
    pag::AlphaType alphaType = pag::AlphaType::Premultiplied;
    PagContext *pctx = static_cast<PagContext *>(ctx);
    bool ret = pag_context_fill_image_internal((is_from ? pctx->from : pctx->to), pixels, width, height, colorType, alphaType);
    if (ret == false) {
        if (is_from) {
            pctx->from = nullptr;
        } else {
            pctx->to = nullptr;
        }
        return false;
    }
    return true;
}

bool pag_context_renderer(void *ctx, bool swap, float progress, PAG_Support_Pix_FMT fmt, uint8_t *out_data) {
    if (ctx == nullptr || out_data == nullptr) {
        return false;
    }
    if (fmt < PAG_Support_Pix_FMT_RGBA_8888 || fmt > PAG_Support_Pix_FMT_BGRA_8888) {
        return false;
    }

    PagContext *pctx = static_cast<PagContext *>(ctx);
    if (pctx->from == nullptr || pctx->to == nullptr) {
        return false;
    }

    if (swap) {
        pctx->file->replaceImage(0, pctx->to);
        pctx->file->replaceImage(1, pctx->from);
    } else {
        pctx->file->replaceImage(0, pctx->from);
        pctx->file->replaceImage(1, pctx->to);
    }

    pctx->player->setProgress(progress);
    if (!pctx->player->flush()) {
        return false;
    }
    pag::ColorType colorType = (fmt == PAG_Support_Pix_FMT_RGBA_8888) ? pag::ColorType::RGBA_8888 : pag::ColorType::BGRA_8888;
    pag::AlphaType alphaType = pag::AlphaType::Premultiplied;
    size_t dstRowBytes = pctx->file->width() * 4;
    if (!pctx->surface->readPixels(colorType, alphaType, out_data, dstRowBytes)) {
        return false;
    }
    return true;
}

#ifdef __cplusplus
}
#endif
