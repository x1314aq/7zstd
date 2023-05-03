#include "fmt/core.h"

#include "7z.h"

using namespace std;
using arc7z = I7Zip::Archive;

std::vector<std::string> split(std::string &&s, std::string &&delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fmt::print("Usage 7zstd [-tlex] archive.7z\n"
                   "  -t            Test archive integrity\n"
                   "  -l            List archive contents\n"
                   "  -g <xxx.x>    Extract files with glob match. Multiple comma-separated globs are supported.\n"
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
    } else if (strcmp(argv[1], "-g") == 0) {
        arc.ExtractFile(split(argv[2], ","));
    } else {
        fmt::print("Unknown command {}\n", argv[1]);
        return -1;
    }

    return 0;
}
