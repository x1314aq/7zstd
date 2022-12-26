#include "fmt/core.h"

#include "7z.h"

using namespace std;
using arc7z = I7Zip::Archive;

int main(int argc, char** argv)
{
    fmt::print("__cplusplus {}\n", __cplusplus);

    arc7z f1(argv[1]);
    f1.DumpArchive();

    return 0;
}
