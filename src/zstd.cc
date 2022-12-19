// introduce extra experimental APIs
#define ZSTD_STATIC_LINKING_ONLY

#include "zstd.h"
#include "zstd_errors.h"
#include "fmt/core.h"

void start_compress()
{
    fmt::print("zstd version: {}\n", ZSTD_versionString());
}
