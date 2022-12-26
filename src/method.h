#pragma once

#include "stdc++.h"

namespace IMethod {
    // ZSTD
    int zstd_decompress(void *dest, size_t destLen,
                        const void *src, size_t srcLen);

    // LZMA
    int lzma_decompress(unsigned char *dest, size_t *destLen,
                        const unsigned char *src, size_t *srcLen,
                        const unsigned char *props, size_t propsSize);
};
