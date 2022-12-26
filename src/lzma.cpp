#include "method.h"
#include "LzmaDec.h"

namespace IMethod {
static void *my_alloc(ISzAllocPtr _p, size_t size)
{
    (void)_p;
    return ::malloc(size);
}

static void my_free(ISzAllocPtr _p, void *p)
{
    (void)_p;
    return ::free(p);
}

const ISzAlloc g_alloc = {my_alloc, my_free};

int lzma_decompress(unsigned char *dest, size_t *destLen,
                    const unsigned char *src, size_t *srcLen,
                    const unsigned char *props, size_t propsSize)
{
    ELzmaStatus status;
    return LzmaDecode(dest, destLen, src, srcLen, props, (unsigned)propsSize, LZMA_FINISH_ANY, &status, &g_alloc);
}

};
