#include <memory>
#include <pag/file.h>
#include <pag/pag.h>

#include "./pag_impl.h"

struct PagContext {
	std::shared_ptr<pag::PAGFile> file;
	std::shared_ptr<pag::PAGImage> from;
	std::shared_ptr<pag::PAGImage> to;
};

#ifdef __cplusplus
extern "C" {
#endif

void *pag_context_create(const char *path) {
	PagContext *ctx = new PagContext();
	return static_cast<void *>(ctx);
}

void pag_context_destory(void *ctx) {
	if (ctx == nullptr) {
		return;
	}
	PagContext *pctx = static_cast<PagContext *>(ctx);
	delete pctx;
}

bool pag_context_fill_from(void *ctx) {
	if (ctx == nullptr) {
		return false;
	}
	PagContext *pctx = static_cast<PagContext *>(ctx);
	return true;
}

bool pag_context_fill_to(void *ctx) {
	if (ctx == nullptr) {
		return false;
	}
	PagContext *pctx = static_cast<PagContext *>(ctx);
	return true;
}

#ifdef __cplusplus
}
#endif
