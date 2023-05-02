#include "method.h"
#include "LzmaDec.h"
#include "Lzma2Dec.h"
#include "Bcj2.h"
#include "Bra.h"

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

int lzma2_decompress(unsigned char *dest, size_t *destLen,
                     const unsigned char *src, size_t *srcLen,
                     unsigned char prop)
{
    ELzmaStatus status;
    return Lzma2Decode(dest, destLen, src, srcLen, prop, LZMA_FINISH_ANY, &status, &g_alloc);
}

size_t bcj_decode(unsigned char *data, size_t size)
{
    uint32_t state;
    x86_Convert_Init(state);
    return x86_Convert(data, size, 0, &state, 0);
}

};
