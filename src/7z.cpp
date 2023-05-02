#include "7z.h"
#include "method.h"

#include "fmt/core.h"
#include "fmt/os.h"
#include "fmt/format.h"
#include "fmt/xchar.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace I7Zip {

constexpr static uint8_t MAGIC_AND_VERSION[8] = {'7', 'z', 0xbc, 0xaf, 0x27, 0x1c, 0x0, 0x4};
constexpr static uint32_t SIGNATURE_HEADER_SIZE = 32;

static bool is_valid(uint8_t *buf)
{
    return ::memcmp(buf, MAGIC_AND_VERSION, 8) == 0;
}

static std::string utf16_to_utf8(wchar_t *in)
{
    int size = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, NULL, NULL);
    std::string out(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, in, -1, &out[0], size, NULL, NULL);
    return out;
}

static bool process_empty_stream(const FileInfo &info)
{
    wchar_t *name = (wchar_t *)info._name.data();

    if (info.is_directory()) {
        if (CreateDirectoryW(name, nullptr) == FALSE) {
            fmt::print("CreateDirectoryW() failed {}\n", GetLastError());
            return false;
        }
    } else {
        HANDLE h = CreateFileW(name, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, info._attribute, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            fmt::print("CreateFileW() failed {}\n", GetLastError());
            return false;
        }
        CloseHandle(h);
    }

    return true;
}

static bool write_file(const FileInfo &info, const uint8_t *buffer)
{
    uint32_t crc = crc32(buffer, info._size);
    assert(info._crc == crc);

    wchar_t *name = (wchar_t *)info._name.data();
    fmt::print(L"- {}\n", name);
    HANDLE h = CreateFileW(name, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, info._attribute, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fmt::print("CreateFileW() failed {}\n", GetLastError());
        return false;
    }

    DWORD written = 0;
    DWORD size = info._size;
    while (written < size) {
        DWORD curr = 0;
        if (WriteFile(h, buffer + written, size - written, &curr, nullptr) == FALSE) {
            fmt::print("WirteFile() failed {}\n", GetLastError());
            CloseHandle(h);
            return false;
        }
        written += curr;
    }
    CloseHandle(h);

    return true;
}

void Archive::ExtractAll()
{
    auto it = _files_info.cbegin();

    while (it->is_empty_stream()) {
        if (!process_empty_stream(*it)) {
            fmt::print("process_empty_stream() failed\n");
        }
        ++it;
    }

    for (auto f = _folders.begin(); f != _folders.end(); ++f) {
        size_t unpack_size = f->get_unpack_size();
        size_t in_size = _pack_size[f->_start_packed_stream_index];
        auto in = std::make_unique<uint8_t[]>(in_size);
        off_t offset = SIGNATURE_HEADER_SIZE + _pack_pos;
        for (uint32_t i = 0; i < _pack_size.size(); i++) {
            if (i == f->_start_packed_stream_index) {
                break;
            }
            offset += _pack_size[i];
        }

        fseek(_fp, offset, SEEK_SET);
        fread(in.get(), in_size, 1, _fp);

        auto out = std::make_unique<uint8_t[]>(unpack_size);
        if (!f->decompress(in.get(), in_size, out.get(), unpack_size)) {
            fmt::print("decompress folder failed\n");
            return;
        }

        size_t used = 0;
        while (used < unpack_size) {
            const uint8_t *p = out.get() + used;
            if (!write_file(*it, p)) {
                fmt::print("write_file() failed\n");
                return;
            }
            used += it->_size;
            ++it;
        }
    }

    assert(it == _files_info.cend());
}

bool Archive::ExtractFile(uint32_t index)
{
    return false;
}

void Archive::ListFiles()
{
    fmt::print("{:<20} {:<10} {:<15} {:<10} File Name\n", "Last Write Time", "Attributes", "File Size", "CRC");
    auto s = fmt::memory_buffer();
    for (auto it = _files_info.cbegin(); it != _files_info.cend(); ++it) {
        if (it->has_mtime()) {
            ULARGE_INTEGER ui;
            ui.QuadPart = it->_mtime;
            FILETIME ft{ui.u.LowPart, ui.u.HighPart}, local_ft;
            SYSTEMTIME st;
            if (FileTimeToLocalFileTime(&ft, &local_ft) == FALSE) {
                fmt::print("Failed to convert FILETIME to local FILETIME\n");
                return;
            }
            if (FileTimeToSystemTime(&local_ft, &st) == FALSE) {
                fmt::print("Failed to convert FILETIME {} to SYSTEMTIME\n", it->_mtime);
                return;
            }
            fmt::format_to(std::back_inserter(s), "{}-{:02}-{:02} {:02}:{:02}:{:02}  ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        }
        if (it->has_attribute()) {
            char a[6];
            a[0] = (char)(it->is_directory() ? 'D' : '.');
            a[1] = (char)(it->is_readonly() ? 'R' : '.');
            a[2] = (char)(it->is_hidden() ? 'H' : '.');
            a[3] = (char)(it->is_system() ? 'S' : '.');
            a[4] = (char)(it->is_archive() ? 'A' : '.');
            a[5] = 0;
            fmt::format_to(std::back_inserter(s), "{:<10} ", a);
        }
        fmt::format_to(std::back_inserter(s), "{:<15} {:0<#10x} {}", it->_size, it->_crc, utf16_to_utf8((wchar_t *)it->_name.data()));
        if (it->is_directory()) {
            s.push_back('/');
        }
        s.push_back('\n');
    }
    fmt::print("{}\n", fmt::to_string(s));
}

void Archive::TestArchive()
{

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

bool Archive::read_bitmap_digest(ByteArray &arr, uint32_t number, BitmapDigest &digest)
{
    uint8_t all_defined = arr.read_uint8();
    if (!digest.init(all_defined, number)) {
        fmt::print("init digest failed\n");
        return false;
    }

    if (all_defined == 0) {
        arr.read_bytes(digest._bitset, digest._size);
    }

    for (uint32_t i = 0; i < number; i++) {
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

    uint8_t t;
    while (true) {
        t = arr.read_uint8();
        if (t == Property::SIZE) {
            break;
        }
        arr.skip_data();
    }

    for (uint64_t i = 0; i < num_pack_streams; i++) {
        _pack_size[i] = arr.read_number();
    }

    t = arr.read_uint8();
    if (t == Property::CRC) {
        read_bitmap_digest(arr, num_pack_streams, _pack_digest);
        t = arr.read_uint8();
    }

    assert(t == Property::END);
    return true;
}

bool Archive::read_coders_info(ByteArray &arr)
{
    uint8_t t = arr.read_uint8();
    if (t != Property::FOLDER) {
        return false;
    }

    uint32_t num_folder = arr.read_num_u32();
    _folders.resize(num_folder);

    uint8_t external = arr.read_uint8();
    if (external != 0) {
        fmt::print("Unsupported feature\n");
        return false;
    }

    uint32_t packed_stream_index = 0;
    for (uint32_t i = 0; i < num_folder; i++) {
        uint8_t num_coders = arr.read_num_u8();
        if (num_coders > MAX_NUM_CODERS) {
            fmt::print("Too many coders ({}) in folder\n", num_coders);
            return false;
        }
        auto &f = _folders[i];
        f._coders.resize(num_coders);
        for (uint64_t j = 0; j < num_coders; j++) {
            auto &c = f._coders[j];
            c._flag = arr.read_uint8();
            arr.read_bytes(c._id, c.id_size());
            if (c.is_complex_codec()) {
                c._num_in_streams = arr.read_num_u8();
                c._num_out_streams = arr.read_num_u8();
                if (c._num_out_streams > 1) {
                    fmt::print("Too many output streams ({}) in coder\n", c._num_out_streams);
                    return false;
                }
            } else {
                c._num_in_streams = c._num_out_streams = 1;
            }
            c._start_in_index = f._num_in_streams_total;
            c._start_out_index = f._num_out_streams_total;
            f._num_in_streams_total += (uint16_t)c._num_in_streams;
            f._num_out_streams_total += (uint16_t)c._num_out_streams;

            if (c.has_attributes()) {
                c._property_size = arr.read_num_u32();
                c._property = new uint8_t[c._property_size];
                // TODO implement memory allocator for small chunk (<32 bytes)
                if (!c._property) {
                    return false;
                }
                arr.read_bytes(c._property, c._property_size);
            }
        }
        uint16_t num_bind_pairs = f._num_out_streams_total - 1;
        if (f._num_in_streams_total < num_bind_pairs) {
            fmt::print("Incorrect folder\n");
            return false;
        }
        if (f._num_in_streams_total > MAX_NUM_STREAMS_FOLDER) {
            fmt::print("Too many input streams ({}) in folder\n", f._num_in_streams_total);
            return false;
        }
        if (num_bind_pairs != 0) {
            f._bind_pairs.resize(num_bind_pairs);
            for (uint16_t j = 0; j < num_bind_pairs; j++) {
                f._bind_pairs[j].first = arr.read_num_u8();
                f._bind_pairs[j].second = arr.read_num_u8();
            }
        }
        uint16_t num_packed_streams = f._num_in_streams_total - num_bind_pairs;
        if (num_packed_streams != 1) {
            f._packed_streams_index.resize(num_packed_streams);
            for (uint16_t j = 0; j < num_packed_streams; j++) {
                f._packed_streams_index[j] = arr.read_num_u32();
            }
        }
        f._start_packed_stream_index = packed_stream_index;
        if (num_packed_streams > _pack_size.size() - packed_stream_index) {
            fmt::print("Too many packed streams in folder {}\n", i);
            return false;
        }
        packed_stream_index += num_packed_streams;
    }

    t = arr.read_uint8();
    if (t == Property::CODERS_UNPACK_SIZE) {
        for (uint32_t i = 0; i < num_folder; i++) {
            auto &f = _folders[i];
            for (auto &c : f._coders) {
                c._unpack_size = arr.read_number();
            }
        }
        t = arr.read_uint8();
    }

    if (t == Property::CRC) {
        read_bitmap_digest(arr, num_folder, _unpack_digest);
        t = arr.read_uint8();
    }

    assert(t == Property::END);
    return true;
}

bool Archive::read_sub_streams_info(ByteArray &arr)
{
    uint8_t t = arr.read_uint8();
    size_t num_folders = _folders.size();
    uint32_t num_digests = 0;
    std::vector<uint32_t> num_unpack_streams(num_folders);

    _substream_sizes.resize(num_folders);

    if (t == Property::NUM_UNPACK_STREAM) {
        for (size_t i = 0; i < num_folders; i++) {
            num_unpack_streams[i] = arr.read_num_u32();
            num_digests += num_unpack_streams[i];
        }
        t = arr.read_uint8();
    }

    if (t == Property::SIZE) {
        for (size_t i = 0; i < num_folders; i++) {
            size_t num = num_unpack_streams[i];
            uint32_t total_size = 0;
            auto& v = _substream_sizes[i];

            v.resize(num);
            for (size_t j = 0; j < num - 1; j++) {
                v[j] = arr.read_number();
                total_size += v[j];
            }

            assert(_folders[i].get_unpack_size() > total_size);
            v[num - 1] = _folders[i].get_unpack_size() - total_size;
        }
        t = arr.read_uint8();
    }

    if (t == Property::CRC) {
        if (!read_bitmap_digest(arr, num_digests, _substreams_digest)) {
            fmt::print("read_hash_digest() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    assert(t == Property::END);
    return true;
}

bool Archive::read_files_info(ByteArray& arr)
{
    uint32_t num_files = arr.read_num_u32();
    _files_info.resize(num_files);
    uint32_t num_empty_streams = 0;
    std::map<uint32_t, uint32_t> m;

    while (true) {
        uint8_t t = arr.read_uint8();
        if (t == Property::END) {
            break;
        }

        uint64_t size = arr.read_number();
        Bitmap bm;
        uint8_t external;

        switch (t) {
        case Property::DUMMY:
            arr.skip_data(size);
            break;
        case Property::EMPTY_STREAM:
            if (!bm.init(num_files)) {
                fmt::print("Bitmap.init({}) failed\n", num_files);
                return false;
            }
            arr.read_bytes(bm._bitset, size);
            for (uint32_t i = 0; i < num_files; i++) {
                if (bm.test(i)) {
                    _files_info[i].set_empty_stream();
                    m[num_empty_streams] = i;
                    num_empty_streams++;
                }
            }
            break;
        case Property::EMPTY_FILE:
            if (!bm.init(num_empty_streams)) {
                fmt::print("Bitmap.init({}) failed\n", num_empty_streams);
                return false;
            }
            arr.read_bytes(bm._bitset, size);
            for (uint32_t i = 0; i < num_empty_streams; i++) {
                if (bm.test(i)) {
                    uint32_t j = m[i];
                    _files_info[j].set_empty_file();
                    _files_info[j]._size = 0;
                }
            }
            break;
        case Property::ANTI:
            fmt::print("Unsupported PROPERTY::ANTI\n");
            break;
        case Property::NAME:
            external = arr.read_uint8();
            if (external) {
                uint32_t index = arr.read_num_u32();
                fmt::print("Unsupported external flag in FileName (dataindex {})\n", index);
                return false;
            }
            --size;
            if ((size & 1) != 0) {
                fmt::print("Incorrect size in FileName which is {}\n", size);
                return false;
            }
            for (uint32_t i = 0; i < num_files; i++) {
                while (true) {
                    uint16_t v = arr.read_uint16();
                    _files_info[i]._name.push_back(v);
                    if (v == 0) {
                        break;
                    }
                }
            }
            break;
        case Property::CREATION_TIME:
            if (!read_times(arr, num_files, Property::CREATION_TIME)) {
                fmt::print("read CREATION_TIME failed\n");
                return false;
            }
            break;
        case Property::LAST_ACCESS_TIME:
            if (!read_times(arr, num_files, Property::LAST_ACCESS_TIME)) {
                fmt::print("read LAST_ACCESS_TIME failed\n");
                return false;
            }
            break;
        case Property::LAST_WRITE_TIME:
            if (!read_times(arr, num_files, Property::LAST_WRITE_TIME)) {
                fmt::print("read LAST_WRITE_TIME failed\n");
                return false;
            }
            break;
        case Property::ATTRIBUTES:
            if (!read_attrs(arr, num_files)) {
                fmt::print("read ATTRIBUTES failed\n");
                return false;
            }
            break;
        default:
            fmt::print("unknown property {}\n", t);
            break;
        }
    }
    return true;
}

bool Archive::read_streams_info(ByteArray &arr)
{
    uint8_t t = arr.read_uint8();

    if (t == Property::PACK_INFO) {
        if (!read_pack_info(arr)) {
            fmt::print("read_pack_info() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    if (t == Property::UNPACK_INFO) {
        if (!read_coders_info(arr)) {
            fmt::print("read_unpack_info() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    if (t == Property::SUBSTREAMS_INFO) {
        if (!read_sub_streams_info(arr)) {
            fmt::print("read_substreams_info() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    assert(t == Property::END);
    return true;
}

bool Archive::read_header(ByteArray &arr)
{
    uint8_t t = arr.read_uint8();

    // mostly unused now
    if (t == Property::ARCHIVE_PROPERTIES) {
        while (true) {
            uint8_t type = arr.read_uint8();
            if (type == Property::END) {
                break;
            }
            arr.skip_data();
        }
        t = arr.read_uint8();
    }

    if (t == Property::ADDITIONAL_STREAMS_INFO) {
        if (!read_additional_streams_info(arr)) {
            fmt::print("read_additional_streams_info() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    if (t == Property::MAIN_STREAMS_INFO) {
        if (!read_main_streams_info(arr)) {
            fmt::print("read_main_streams_info() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    if (t == Property::FILES_INFO) {
        if (!read_files_info(arr)) {
            fmt::print("read_files_info() failed\n");
            return false;
        }
        t = arr.read_uint8();
    }

    assert(t == Property::END);

    if (!update_files_info()) {
        fmt::print("update_files_info() failed\n");
        return false;
    }

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
    dummy.crc = crc32(buf, buf_len);
    uint32_t start_hdr_crc = crc32(&dummy, 20);

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
    size_t dest_len = _folders[0].get_unpack_size();

    uint8_t *dest = new uint8_t[dest_len];
    if (!dest) {
        fmt::print("malloc() failed\n");
        return nullptr;
    }

    uint8_t *src = new uint8_t[src_len];
    if (!src) {
        delete[] dest;
        fmt::print("malloc() failed\n");
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

void Archive::reset()
{
    _pack_pos = 0;
    _pack_size.clear();
    _pack_digest.reset();

    _folders.clear();
    _unpack_digest.reset();
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
    uint8_t t = arr.read_uint8();

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

        size_t new_size = _folders[0].get_unpack_size();
        arr.replace(new_header, new_size, true);

        if (_dump) {
            write_decompressed_header(new_header, new_size);
        }

        if (arr.read_number() != Property::HEADER) {
            fmt::print("unknown Property\n");
            return false;
        }

        reset();
    }

    return read_header(arr);
}

bool Archive::read_times(ByteArray &arr, uint32_t num_files, uint8_t t)
{
    Bitmap bm(num_files);
    uint8_t all_defined = arr.read_uint8();
    if (!all_defined) {
        arr.read_bytes(bm._bitset, bm._size);
    } else {
        bm.set_all();
    }

    uint8_t external = arr.read_uint8();
    if (external) {
        uint32_t index = arr.read_num_u32();
        fmt::print("Unsupported external flag in type {} (DataIndex {})\n", t, index);
        return false;
    }

    for (uint64_t i = 0; i < num_files; i++) {
        if (bm.test(i)) {
            uint64_t v = arr.read_uint64();
            switch (t) {
            case Property::CREATION_TIME:
                _files_info[i]._ctime = v;
                _files_info[i].set_ctime();
                break;
            case Property::LAST_ACCESS_TIME:
                _files_info[i]._atime = v;
                _files_info[i].set_atime();
                break;
            case Property::LAST_WRITE_TIME:
                _files_info[i]._mtime = v;
                _files_info[i].set_mtime();
                break;
            default:
                break;
            }
        }
    }
    return true;
}

bool Archive::read_attrs(ByteArray& arr, uint32_t num_files)
{
    Bitmap bm(num_files);
    uint8_t all_defined = arr.read_uint8();
    if (!all_defined) {
        arr.read_bytes(bm._bitset, bm._size);
    } else {
        bm.set_all();
    }

    uint8_t external = arr.read_uint8();
    if (external) {
        uint32_t index = arr.read_num_u32();
        fmt::print("Unsupported external flag in ATTIBUTES (DataIndex {})\n", index);
        return false;
    }

    for (uint64_t i = 0; i < num_files; i++) {
        if (bm.test(i)) {
            uint32_t v = arr.read_uint32();
            _files_info[i]._attribute = v;
            _files_info[i].set_attribute();
        }
    }
    return true;
}

bool Archive::update_files_info()
{
    auto it = _files_info.begin();
    auto end = _files_info.end();
    uint32_t num_folders = _folders.size();

    for (uint32_t i = 0; i < num_folders; i++) {
        auto &u = _substream_sizes[i];
        uint32_t num_substreams = u.size();
        for (uint32_t j = 0; j < num_substreams; j++) {
            while (it->is_empty_stream()) {
                ++it;
            }
            it->_folder = i;
            it->_size = u[j];
            ++it;
        }
    }
    assert(it == end);

    auto crc_it = _substreams_digest._crcs.cbegin();
    for (it = _files_info.begin(); it != end; ++it) {
        if (it->is_empty_stream()) {
            continue;
        }
        it->_crc = *crc_it;
        ++crc_it;
    }
    assert(crc_it == _substreams_digest._crcs.cend());

    return true;
}

Archive::Archive(const std::string &s, uint32_t flags) : _name(s), _fp(nullptr), _dump(false)
{
    std::string mode;

    if (flags & A_F_WRITE) {
        mode = "wb";
        if (flags & A_F_FORCE) {
            mode.push_back('+');
        }
        _start_hdr_crc = -1;
        _next_hdr_size = -1;
        _next_hdr_offset = -1;
        _next_hdr_crc = -1;
    } else {
        mode = "rb";
    }

    if (flags & A_F_DUMP) {
        _dump = true;
    }

    _fp = fopen(s.c_str(), mode.c_str());
    if (!_fp) {
        throw fmt::system_error(errno, "{} ", s.c_str());
    }

    if (flags & A_F_WRITE) {
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

bool Folder::decompress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size)
{
    uint32_t num_coders = _coders.size();
    const uint8_t *curr_in = in;
    size_t curr_in_size = in_size;
    uint8_t *curr_out = nullptr;
    std::vector<uint8_t *> v;
    bool err = true;

    for (uint32_t i = 0; i < num_coders; i++) {
        auto &c = _coders[i];
        size_t curr_out_size;
        if (i == num_coders - 1) {
            curr_out_size = out_size;
            curr_out = out;
        } else {
            curr_out_size = c._unpack_size;
            curr_out = new uint8_t[curr_out_size];
            if (!curr_out) {
                fmt::print("alloc failed with {}\n", curr_out_size);
                err = false;
                break;
            }
            v.push_back(curr_out);
        }

        if (c.is_lzma()) {
            int err = IMethod::lzma_decompress(curr_out, &curr_out_size, curr_in, &curr_in_size, c._property, c._property_size);
            if (err) {
                fmt::print("lzma_decompress() failed\n");
                err = false;
                break;
            }
        } else if (c.is_lzma2()) {
            int err = IMethod::lzma2_decompress(curr_out, &curr_out_size, curr_in, &curr_in_size, c._property[0]);
            if (err) {
                fmt::print("lzma2_decompress() failed\n");
                err = false;
                break;
            }
        } else if (c.is_zstd()) {
            int err = IMethod::zstd_decompress(curr_out, curr_out_size, curr_in, curr_in_size);
            if (err) {
                fmt::print("zstd_decompress() failed\n");
                err = false;
                break;
            }
        } else if (c.is_bcj()) {
            assert(curr_in_size == curr_out_size);
            ::memcpy(curr_out, curr_in, curr_in_size);
            IMethod::bcj_decode(curr_out, curr_out_size);
        } else {
            fmt::print("Unsupported coder\n");
            err = false;
            break;
        }

        curr_in_size = curr_out_size;
        curr_in = curr_out;
    }

    for (auto p : v) {
        delete[] p;
    }
    return err;
}

}
