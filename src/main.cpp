#include "fmt/core.h"

#include "7z.h"

using namespace std;
using arc7z = I7Zip::Archive;

int main(int argc, char** argv)
{
    if (argc < 3) {
        fmt::print("Usage 7zstd [-tlex] archive.7z\n"
                   "  -t            Test archive integrity\n"
                   "  -l            List archive contents\n"
                   "  -e <xxx.x>    Extract specific files\n"
                   "  -x            eXtract files with full paths\n");
        return -1;
    }

    arc7z arc(argv[argc - 1]);
    if (!arc.read_archive()) {
        fmt::print("read_archive() failed\n");
        return -1;
    }

    if (strcmp(argv[1], "-t") == 0) {
        arc.TestArchive();
    } else if (strcmp(argv[1], "-l") == 0) {
        arc.ListFiles();
    } else if (strcmp(argv[1], "-x") == 0) {
        arc.ExtractAll();
    } else if (strcmp(argv[1], "-e") == 0) {
        fmt::print("Unsupport now\n");
        return -1;
        // arc.ExtractFile(0);
    } else {
        fmt::print("Unknown command {}\n", argv[1]);
        return -1;
    }

    return 0;
}
