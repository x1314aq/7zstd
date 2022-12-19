#include "fmt/core.h"

#include "7zFormat.h"

using namespace std;
using arc7z = I7Zip::Archive;

int main(int argc, char** argv)
{
    fmt::print("__cplusplus {}\n", __cplusplus);

    arc7z f1(argv[1]);
    f1.DumpArchive();

    arc7z f2(argv[2], arc7z::F_WRITE | arc7z::F_FORCE);
    return 0;
}
