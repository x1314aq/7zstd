// introduce extra experimental APIs
#define ZSTD_STATIC_LINKING_ONLY

#include "method.h"

#include "zstd.h"
#include "zstd_errors.h"

namespace IMethod {

int zstd_decompress(void *dest, size_t destLen,
                    const void *src, size_t srcLen)
{
    size_t err = ZSTD_decompress(dest, destLen, src, srcLen);
    return ZSTD_getErrorCode(err);
}

};
