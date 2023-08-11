#ifndef _pag_impl_h
#define _pag_impl_h

#ifdef __cplusplus
extern "C" {
#endif

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum {
    PAG_Support_Pix_FMT_RGBA_8888 = 1,
    PAG_Support_Pix_FMT_BGRA_8888 = 2,
} PAG_Support_Pix_FMT;

extern void *pag_context_create(const char *path);
extern void pag_context_destory(void *ctx);
extern bool pag_context_fill_image(void *ctx, const void *pixels, int width, int height, PAG_Support_Pix_FMT fmt, bool is_from);
extern bool pag_context_renderer(void *ctx, float progress, PAG_Support_Pix_FMT fmt, uint8_t *out_data);

#ifdef __cplusplus
}
#endif

#endif
