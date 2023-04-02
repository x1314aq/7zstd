#include "7z.h"
#include "method.h"

#include "fmt/core.h"
#include "fmt/os.h"
#include "fmt/format.h"

namespace I7Zip {

constexpr static uint8_t MAGIC_AND_VERSION[8] = {'7', 'z', 0xbc, 0xaf, 0x27, 0x1c, 0x0, 0x4};
constexpr static uint32_t SIGNATURE_HEADER_SIZE = 32;

static uint32_t calc_crc(void *buf, size_t size)
{
    return 0;
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

    return sz == 24;
}

void Archive::DumpArchive()
{
    if (!read_archive()) {
        fmt::print("read_archive() failed");
        return;
    }

    fmt::print("StartHeaderCRC: {:#x}\n", _start_hdr_crc);
    fmt::print("NextHeaderOffset: {:#x}\n", _next_hdr_offset);
    fmt::print("NextHeaderSize: {:#x}\n", _next_hdr_size);
    fmt::print("NextHeaderCRC: {:#x}\n", _next_hdr_crc);
}

bool Archive::read_hash_digest(ByteArray &arr, uint64_t number, HashDigest &digest)
{
    uint8_t all_defined = arr.read_uint8();
    if (!digest.init(all_defined, number)) {
        fmt::print("init digest failed\n");
        return false;
    }

    if (all_defined == 0) {
        arr.read_bytes(digest._bitset, digest._size);
    }

    for (uint64_t i = 0; i < number; i++) {
        if (digest.test(i)) {
            uint32_t crc = arr.read_uint32();
            digest._crcs[i] = crc;
        }
    }
    return true;
}

bool Archive::read_pack_info(ByteArray &arr)
{
    _pack_pos = arr.read_number();
    uint64_t num_pack_streams = arr.read_number();
    _pack_size.resize(num_pack_streams);

    uint64_t t;
    while (true) {
        t = arr.read_number();
        if (t == Property::SIZE) {
            break;
        }
        arr.skip_data();
    }

    for (uint64_t i = 0; i < num_pack_streams; i++) {
        _pack_size[i] = arr.read_number();
    }

    t = arr.read_number();
    if (t == Property::CRC) {
        read_hash_digest(arr, num_pack_streams, _pack_digest);
        t = arr.read_number();
    }

    assert(t == Property::END);

    return true;
}

bool Archive::read_unpack_info(ByteArray &arr)
{
    uint64_t t = arr.read_number();
    if (t != Property::FOLDER) {
        return false;
    }

    uint64_t num_folder = arr.read_number();
    _folders.resize(num_folder);

    uint8_t external = arr.read_uint8();
    if (external != 0) {
        fmt::print("not supported feature\n");
        return false;
    }

    for (uint64_t i = 0; i < num_folder; i++) {
        uint64_t num_coders = arr.read_number();
        auto &f = _folders[i];
        f._coders.resize(num_coders);
        for (uint64_t j = 0; j < num_coders; j++) {
            auto &c = f._coders[j];
            c._flag = arr.read_uint8();
            arr.read_bytes(c._id, c.id_size());
            if (c.is_complex_codec()) {
                c._num_in_streams = arr.read_number();
                c._num_out_streams = arr.read_number();
            } else {
                c._num_in_streams = c._num_out_streams = 1;
            }
            f._num_in_streams_total += c._num_in_streams;
            f._num_out_streams_total += c._num_out_streams;

            if (c.has_attributes()) {
                c._property_size = arr.read_number();
                c._property = new uint8_t[c._property_size];
                if (!c._property) {
                    return false;
                }
                arr.read_bytes(c._property, c._property_size);
            }
        }
        f._bind_pairs.resize(f._num_out_streams_total - 1);
        for (auto it = f._bind_pairs.begin(); it != f._bind_pairs.end(); ++it) {
            it->first = arr.read_number();
            it->second = arr.read_number();
        }
        if (f._num_in_streams_total > f._num_out_streams_total) {
            f._packed_streams_index.resize(f._num_in_streams_total - f._num_out_streams_total + 1);
            for (auto it = f._packed_streams_index.begin(); it != f._packed_streams_index.end(); ++it) {
                *it = arr.read_number();
            }
        }
        f._unpack_size.resize(f._num_out_streams_total);
    }

    t = arr.read_number();
    if (t == Property::CODERS_UNPACK_SIZE) {
        for (size_t i = 0; i < num_folder; i++) {
            auto &f = _folders[i];
            for (uint64_t j = 0; j < f._num_out_streams_total; j++) {
                f._unpack_size[j] = arr.read_number();
            }
        }
        t = arr.read_number();
    }

    if (t == Property::CRC) {
        read_hash_digest(arr, num_folder, _unpack_digest);
        t = arr.read_number();
    }

    assert(t == Property::END);
    return true;
}

bool Archive::read_substreams_info(ByteArray &arr)
{
    return true;
}

bool Archive::read_encoded_header(ByteArray &arr)
{
    uint64_t t = arr.read_number();

    if (t == Property::PACK_INFO) {
        if (!read_pack_info(arr)) {
            fmt::print("read_pack_info() failed\n");
            return false;
        }
        t = arr.read_number();
    }

    if (t == Property::UNPACK_INFO) {
        if (!read_unpack_info(arr)) {
            fmt::print("read_unpack_info() failed\n");
            return false;
        }
        t = arr.read_number();
    }

    if (t == Property::SUBSTREAMS_INFO) {
        if (!read_substreams_info(arr)) {
            fmt::print("read_substreams_info() failed\n");
        }
        t = arr.read_number();
    } else {

    }

    assert(t == Property::END);
    return true;
}

bool Archive::read_header(ByteArray &arr)
{
    return true;
}

void Archive::write_decompressed_header(uint8_t *buf, size_t buf_len)
{
    auto const pos = _name.find_last_of("/\\") + 1;
    std::string basename = _name.substr(pos);
    std::string new_name = std::string("decompress-") + basename;
    FILE *new_fp = fopen(new_name.c_str(), "wb+");
    if (!new_fp) {
        fmt::print("fopen({}, 'wb+') failed errno {}\n", new_name, errno);
        return;
    }

    // write signature header
    struct {
        uint64_t offset;
        uint64_t size;
        uint32_t crc;
    } dummy;

    dummy.offset = _pack_pos;
    dummy.size = buf_len;
    dummy.crc = calc_crc(buf, buf_len);
    uint32_t start_hdr_crc = calc_crc(&dummy, 20);

    if (fwrite(MAGIC_AND_VERSION, 1, 8, new_fp) != 8) {
        fmt::print("fwrite() failed 1\n");
        fclose(new_fp);
        return;
    }
    if (fwrite(&start_hdr_crc, 1, sizeof(uint32_t), new_fp) != sizeof(uint32_t)) {
        fmt::print("fwrite() failed 2\n");
        fclose(new_fp);
        return;
    }
    if (fwrite(&dummy, 1, 20, new_fp) != 20) {
        fmt::print("fwrite() failed 3\n");
        fclose(new_fp);
        return;
    }

    uint64_t copied = 0;
    uint8_t tmp[1024];
    size_t sz;
    fseek(_fp, SIGNATURE_HEADER_SIZE, SEEK_SET);
    while (copied < _pack_pos) {
        if (copied + 1024 <= _pack_pos) {
            sz = 1024;
        } else {
            sz = _pack_pos - copied;
        }
        if (fread(tmp, 1, sz, _fp) != sz) {
            fmt::print("fread() failed {}/{}", copied, _pack_pos);
            fclose(new_fp);
            return;
        }

        if (fwrite(tmp, 1, sz, new_fp) != sz) {
            fmt::print("fwrite() failed {}/{}", copied, _pack_pos);
            fclose(new_fp);
            return;
        }

        copied += sz;
    }

    if (fwrite(buf, 1, buf_len, new_fp) != buf_len) {
        fmt::print("fwrite() failed 4\n");
        fclose(new_fp);
        return;
    }

    fclose(new_fp);
    return;
}

uint8_t * Archive::decompress_header()
{
    bool success = true;
    size_t src_len = _pack_size[0];
    size_t dest_len = _folders[0]._unpack_size[0];

    uint8_t *dest = new uint8_t[dest_len];
    if (!dest) {
        fmt::print("malloc() failed\n");
        return nullptr;
    }

    uint8_t *src = new uint8_t[src_len];
    if (!src) {
        delete[] dest;
        return nullptr;
    }

    fseek(_fp, SIGNATURE_HEADER_SIZE + _pack_pos , SEEK_SET);
    fread(src, 1, src_len, _fp);

    auto &c = _folders[0]._coders[0];
    if (c.is_lzma()) {
        int err = IMethod::lzma_decompress(dest, &dest_len, src, &src_len, c._property, c._property_size);
        if (err) {
            fmt::print("lzma_decompress() error {}\n", err);
            success = false;
        }
    } else if (c.is_zstd()) {
        int err = IMethod::zstd_decompress(dest, dest_len, src, src_len);
        if (err) {
            fmt::print("zstd_decompress() error {}\n", err);
            success = false;
        }
    } else {
        fmt::print("unknown coder id\n");
        success = false;
    }

    if (!success) {
        delete[] dest;
        dest = nullptr;
    }

    delete[] src;
    return dest;
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

    fread(buf, 1, _next_hdr_size, _fp);
    ByteArray arr(buf, _next_hdr_size, true);
    uint64_t t = arr.read_number();

    assert(t == Property::ENCODED_HEADER || t == Property::HEADER);

    if (t == Property::ENCODED_HEADER) {
        success = read_encoded_header(arr);
        if (!success) {
            fmt::print("read_encoded_header() failed\n");
            return success;
        }

        uint8_t *new_header = decompress_header();
        if (!new_header) {
            fmt::print("decompress_header() failed\n");
            return false;
        }

        size_t new_size = _folders[0]._unpack_size[0];
        arr.replace(new_header, new_size, true);

        write_decompressed_header(new_header, new_size);

        if (arr.read_number() != Property::HEADER) {
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
            fmt::print("invalid Signature\n");
            return;
        }
    }
}

Archive::~Archive()
{
    fclose(_fp);
}

}
