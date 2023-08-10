#ifndef _pag_impl_h
#define _pag_impl_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>

extern void *pag_context_create(const char *path);
extern void pag_context_destory(void *ctx);
extern bool pag_context_fill_from(void *ctx);
extern bool pag_context_fill_to(void *ctx);

#ifdef __cplusplus
}
#endif

#endif
