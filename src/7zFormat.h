#pragma once

#include "stdc++.h"

// Reference: https://py7zr.readthedocs.io/en/latest/archive_format.html

namespace I7Zip {

class Property {
public:
    static const uint8_t END = 0x00;
    static const uint8_t HEADER = 0x01;
    static const uint8_t ARCHIVE_PROPERTIES = 0x02;
    static const uint8_t ADDITIONAL_STREAMS_INFO = 0x03;
    static const uint8_t MAIN_STREAMS_INFO = 0x04;
    static const uint8_t FILES_INFO = 0x05;
    static const uint8_t PACK_INFO = 0x06;
    static const uint8_t UNPACK_INFO = 0x07;
    static const uint8_t SUBSTREAMS_INFO = 0x08;
    static const uint8_t SIZE = 0x09;
    static const uint8_t CRC = 0x0a;
    static const uint8_t FOLDER = 0x0b;
    static const uint8_t CODERS_UNPACK_SIZE = 0x0c;
    static const uint8_t NUM_UNPACK_STREAM = 0x0d;
    static const uint8_t EMPTY_STREAM = 0x0e;
    static const uint8_t EMPTY_FILE = 0x0f;
    static const uint8_t ANTI = 0x10;
    static const uint8_t NAME = 0x11;
    static const uint8_t CREATION_TIME = 0x12;
    static const uint8_t LAST_ACCESS_TIME = 0x13;
    static const uint8_t LAST_WRITE_TIME = 0x14;
    static const uint8_t ATTRIBUTES = 0x15;
    static const uint8_t COMMENT = 0x16;
    static const uint8_t ENCODED_HEADER = 0x17;
    static const uint8_t START_POS = 0x18;
    static const uint8_t DUMMY = 0x19;
};

class CodecID {
public:
    static const uint64_t COPY = 0x00;
    static const uint64_t DELTA = 0x03;
    static const uint64_t BCJ = 0x04;
    static const uint64_t PPC = 0x05;
    static const uint64_t IA64 = 0x06;
    static const uint64_t ARM = 0x07;
    static const uint64_t ARMT = 0x08;
    static const uint64_t SPARC = 0x09;
    // SWAP = 02..
    static const uint64_t SWAP2 = 0x020302;
    static const uint64_t SWAP4 = 0x020304;
    // 7Z = 03..
    static const uint64_t LZMA = 0x030101;
    static const uint64_t PPMD = 0x030401;
    static const uint64_t P7Z_BCJ = 0x03030103;
    static const uint64_t P7Z_BCJ2 = 0x0303011B;
    static const uint64_t BCJ_PPC = 0x03030205;
    static const uint64_t BCJ_IA64 = 0x03030401;
    static const uint64_t BCJ_ARM = 0x03030501;
    static const uint64_t BCJ_ARMT = 0x03030701;
    static const uint64_t BCJ_SPARC = 0x03030805;
    static const uint64_t LZMA2 = 0x21;
    // MISC : 04..
    static const uint64_t MISC_ZIP = 0x0401;
    static const uint64_t MISC_BZIP2 = 0x040202;
    static const uint64_t MISC_DEFLATE = 0x040108;
    static const uint64_t MISC_DEFLATE64 = 0x040109;
    static const uint64_t MISC_Z = 0x0405;
    static const uint64_t MISC_LZH = 0x0406;
    static const uint64_t NSIS_DEFLATE = 0x040901;
    static const uint64_t NSIS_BZIP2 = 0x040902;
    static const uint64_t MISC_ZSTD = 0x04f71101;
    static const uint64_t MISC_BROTLI = 0x04f71102;
    static const uint64_t MISC_LZ4 = 0x04f71104;
    static const uint64_t MISC_LZS = 0x04f71105;
    static const uint64_t MISC_LIZARD = 0x04f71106;
    // CRYPTO 06..
    static const uint64_t CRYPT_ZIPCRYPT = 0x06f10101;
    static const uint64_t CRYPT_RAR29AES = 0x06f10303;
    static const uint64_t CRYPT_AES256_SHA256 = 0x06f10701;
};

class ByteArray {
public:
    ByteArray(const uint8_t *buffer, size_t size) : _buffer(buffer), _size(size), _pos(0) {}
    uint8_t read_uint8()
    {
        assert(_pos < _size);
        return _buffer[_pos++];
    }

    void read_bytes(uint8_t *dst, size_t size)
    {
        if (size == 0) {
            return;
        }
        assert(size < _size - _pos);
        ::memcpy(dst, _buffer, size);
        _pos += size;
    }

    uint32_t read_uint16()
    {
        assert(_pos + 2 < _size);
        uint16_t v = *(uint16_t *)(_buffer + _pos);
        _pos += 2;
        return v;
    }

    uint32_t read_uint32()
    {
        assert(_pos + 4 < _size);
        uint32_t v = *(uint32_t *)(_buffer + _pos);
        _pos += 4;
        return v;
    }

    uint64_t read_uint64()
    {
        assert(_pos + 8 < _size);
        uint64_t v = *(uint64_t *)(_buffer + _pos);
        _pos += 8;
        return v;

    }

    uint64_t read_number()
    {
        size_t processed;
        uint64_t v = read_number_spec(processed);
        assert(processed);
        _pos += processed;
        return v;
    }

    uint32_t read_num()
    {
        uint64_t v = read_number();
        assert(v < (uint64_t)INT_MAX);
        return (uint32_t)v;
    }

    void skip_data(uint64_t size)
    {
        assert(size < _size - _pos);
        _pos += (size_t)size;
    }

    void skip_data()
    {
        skip_data(read_number());
    }

    const uint8_t *get_current()
    {
        return _buffer + _pos;
    }

private:
    uint64_t read_number_spec(size_t &processed)
    {
        const uint8_t *p = _buffer + _pos;
        size_t size = _size - _pos;

        if (size == 0) {
            processed = 0;
            return 0;
        }

        uint8_t b = *p++;
        size--;

        if ((b & 0x80) == 0) {
            processed = 1;
            return b;
        }

        if (size == 0) {
            processed = 0;
            return 0;
        }

        uint64_t v = (uint64_t)*p;
        p++;
        size--;

        for (int i = 1; i < 8; i++) {
            unsigned mask = (unsigned)0x80 >> i;
            if ((b & mask) == 0) {
                uint64_t high = b & (mask - 1);
                v |= (high << (i * 8));
                processed = i + 1;
                return v;
            }

            if (size == 0) {
                processed = 0;
                return 0;
            }

            v |= ((uint64_t)*p << (i * 8));
            p++;
            size--;
        }

        processed = 9;
        return v;
    }

    const uint8_t *_buffer;
    size_t _size;
    size_t _pos;
};

class Archive {
public:
    constexpr static uint32_t F_READ = 0x0;
    constexpr static uint32_t F_WRITE = 0x1;
    constexpr static uint32_t F_FORCE = 0x2;

    Archive(const std::string &s, uint32_t flag = 0);
    Archive(const Archive &p) = delete;
    Archive(const Archive &&p) = delete;
    Archive & operator=(const Archive &p) = delete;

    ~Archive();

    bool WriteFile(const std::string &f);
    bool WriteAll(const std::string &dir);

    // dump archive with detailed information
    void DumpArchive();

    void DecodeAll();

private:
    bool write_signature();
    bool read_signature();
    bool read_header(ByteArray &obj);
    bool read_encoded_header(ByteArray &obj);
    bool read_pack_info(ByteArray &obj);
    bool read_unpack_info(ByteArray &obj);
    bool read_archive();

    // common member
    std::string _name;
    long _fpos;
    FILE *_fp;

    // signature header
    uint32_t _start_hdr_crc;
    uint64_t _next_hdr_offset;
    uint64_t _next_hdr_size;
    uint32_t _next_hdr_crc;
};

};
