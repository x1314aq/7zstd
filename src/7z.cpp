#include "7zFormat.h"

#include "fmt/core.h"
#include "fmt/os.h"
#include "fmt/format.h"

namespace I7Zip {

constexpr static uint8_t MAGIC_AND_VERSION[8] = {'7', 'z', 0xbc, 0xaf, 0x27, 0x1c, 0x0, 0x4};

static bool calc_crc(uint8_t *buf, size_t size)
{
    return true;
}

static bool is_valid(uint8_t *buf)
{
    return ::memcmp(buf, MAGIC_AND_VERSION, 8) == 0;
}

bool Archive::write_signature()
{
    size_t sz = 0;

    sz += fwrite(MAGIC_AND_VERSION, 1, 8, _fp);
    sz += fwrite(&_start_hdr_crc, 1, sizeof(uint32_t), _fp);
    sz += fwrite(&_next_hdr_offset, 1, sizeof(uint64_t), _fp);
    sz += fwrite(&_next_hdr_size, 1, sizeof(uint64_t), _fp);
    sz += fwrite(&_next_hdr_crc, 1, sizeof(uint32_t), _fp);

    return sz == 32;
}

bool Archive::read_signature()
{
    size_t sz = 0;

    sz += fread(&_start_hdr_crc, 1, sizeof(uint32_t), _fp);
    sz += fread(&_next_hdr_offset, 1, sizeof(uint64_t), _fp);
    sz += fread(&_next_hdr_size, 1, sizeof(uint64_t), _fp);
    sz += fread(&_next_hdr_crc, 1, sizeof(uint32_t), _fp);

    return sz == 32;
}

void Archive::DumpArchive()
{
    fmt::print("StartHeaderCRC: {:#x}\n", _start_hdr_crc);
    fmt::print("NextHeaderOffset: {:#x}\n", _next_hdr_offset);
    fmt::print("NextHeaderSize: {:#x}\n", _next_hdr_size);
    fmt::print("NextHeaderCRC: {:#x}\n", _next_hdr_crc);
}

bool Archive::read_encoded_header(ByteArray &arr)
{
    return true;
}

bool Archive::read_header(ByteArray &arr)
{
    return true;
}

bool Archive::read_archive()
{
    bool success;
    uint8_t *buf = nullptr;

    success = read_signature();
    if (!success) {
        fmt::print("read_signature() failed\n");
        return success;
    }

    fseek(_fp, _next_hdr_offset, SEEK_CUR);
    buf = new uint8_t[_next_hdr_size];
    if (!buf) {
        fmt::print("malloc failed\n");
        return false;
    }

    ByteArray arr(buf, _next_hdr_size, true);
    uint8_t t = arr.read_uint8();

    if (t == Property::ENCODED_HEADER) {
        success = read_encoded_header(arr);
        if (!success) {
            fmt::print("read_encoded_header() failed\n");
            return success;
        }

        // do something

        if (arr.read_uint8() != Property::HEADER) {
            fmt::print("unknown Property\n");
            return false;
        }
    }

    return read_header(arr);
}

Archive::Archive(const std::string &s, uint32_t flags) : _name(s)
{
    std::string mode;

    if (flags & F_WRITE) {
        mode = "wb";
        if (flags & F_FORCE) {
            mode.push_back('+');
        }
        _start_hdr_crc = -1;
        _next_hdr_size = -1;
        _next_hdr_offset = -1;
        _next_hdr_crc = -1;
    } else {
        mode = "rb";
    }
    _fp = fopen(s.c_str(), mode.c_str());
    if (!_fp) {
        throw fmt::system_error(errno, "{} ", s.c_str());
    }

    if (flags & F_WRITE) {
        fwrite(MAGIC_AND_VERSION, 1, sizeof(MAGIC_AND_VERSION), _fp);
    } else {
        uint8_t buf[8];
        fread(buf, 1, sizeof(buf), _fp);
        if (!is_valid(buf)) {
            throw fmt::system_error(errno, "invalid Signature");
        }
        read_signature();
    }
    _fpos = ftell(_fp);
}

Archive::~Archive()
{
    fclose(_fp);
}

}
