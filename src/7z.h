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

class ByteArray {
public:
    ByteArray(const void *buffer, size_t size, bool free = false) : _buffer(reinterpret_cast<const uint8_t *>(buffer)), _size(size), _pos(0), _free(free) {}

    ~ByteArray()
    {
        if (_free) {
            delete[] _buffer;
        }
    }

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
        ::memcpy(dst, _buffer + _pos, size);
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

    size_t size()
    {
        return _size - _pos;
    }

    void replace(const void *new_buffer, size_t new_size, bool free)
    {
        if (_free) {
            delete[] _buffer;
        }
        _buffer = reinterpret_cast<const uint8_t *>(new_buffer);
        _size = new_size;
        _pos = 0;
        _free = free;
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
    bool _free;
};

class HashDigest {
public:
    HashDigest():_bitset(nullptr) {}

    bool init(uint8_t all_defined, uint64_t number)
    {
        size_t sz = number / 8;
        if (number % 8) {
            sz++;
        }
        _bitset = new uint8_t[sz];
        if (!_bitset) {
            return false;
        }

        _all_defined = all_defined;
        if (all_defined == 1) {
            ::memset(_bitset, 1, sz);
            size_t u = sz * 8 - number;
            if (u) {
                _bitset[sz - 1] |= ~((1U << u) - 1);
            }
        }

        _crcs.resize(number);
        _number = number;
        _size = sz;
        return true;
    }

    ~HashDigest()
    {
        if (_bitset) {
            delete[] _bitset;
        }
    }

    void set(size_t i)
    {
        _bitset[i / 8] |= (1U << (7 - (i % 8)));
    }

    void clear(size_t i)
    {
        _bitset[i / 8] &= ~(1U << (7 - (i % 8)));
    }

    bool test(size_t i)
    {
        return (_bitset[i / 8] & (1U << (7 - (i % 8)))) != 0;
    }

    void reset()
    {
        if (_bitset) {
            delete[] _bitset;
        }

        _all_defined = 0;
        _number = 0;
        _size = 0;
        _crcs.clear();
    }

    uint8_t _all_defined;
    uint64_t _number;
    size_t _size;
    uint8_t *_bitset;
    std::vector<uint32_t> _crcs;
};

class Coder {
public:
    uint8_t _flag;
    uint8_t _id[16];
    uint64_t _num_in_streams;
    uint64_t _num_out_streams;
    uint64_t _property_size;
    uint8_t *_property;

    Coder():_property(nullptr)
    {
        ::memset(_id, 0, sizeof(_id));
    }

    ~Coder()
    {
        if (_property) {
            delete _property;
        }
    }

    size_t id_size()
    {
        return (size_t)(_flag & 0xF);
    }

    bool is_complex_codec()
    {
        return _flag & 0x10;
    }

    bool has_attributes()
    {
        return _flag & 0x20;
    }

    bool is_not_last_method()
    {
        return _flag & 0x80;
    }

    bool is_lzma()
    {
        const uint8_t lzma_id[] = {0x3, 0x1, 0x1};
        return ::memcmp(_id, lzma_id, id_size()) == 0;
    }

    bool is_lzma2()
    {
        const uint8_t lzma_id[] = {0x21};
        return ::memcmp(_id, lzma_id, id_size()) == 0;
    }

    bool is_zstd()
    {
        const uint8_t zstd_id[] = {0x04, 0xf7, 0x11, 0x01};
        return ::memcmp(_id, zstd_id, id_size()) == 0;
    }
};

class Folder {
public:
    std::vector<Coder> _coders;
    uint32_t _num_in_streams_total;
    uint32_t _num_out_streams_total;
    std::vector<std::pair<uint64_t, uint64_t>> _bind_pairs;
    std::vector<uint64_t> _packed_streams_index;
    std::vector<uint64_t> _unpack_size;
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
    bool read_pack_info(ByteArray &obj);
    bool read_coders_info(ByteArray &obj);
    bool read_hash_digest(ByteArray &obj, uint64_t number, HashDigest &digest);
    bool read_sub_streams_info(ByteArray &obj);
    bool read_streams_info(ByteArray& obj);
    bool read_files_info(ByteArray& obj);
    bool read_archive();

    bool read_encoded_header(ByteArray& obj)
    {
        return read_streams_info(obj);
    }

    bool read_main_streams_info(ByteArray& obj)
    {
        return read_streams_info(obj);
    }

    bool read_additional_streams_info(ByteArray& obj)
    {
        return read_streams_info(obj);
    }

    uint8_t *decompress_header();

    void reset();

    void write_decompressed_header(uint8_t *buf, size_t buf_len);

    // common member
    std::string _name;
    FILE *_fp;

    // signature header
    uint32_t _start_hdr_crc;
    uint64_t _next_hdr_offset;
    uint64_t _next_hdr_size;
    uint32_t _next_hdr_crc;

    // pack info
    uint64_t _pack_pos;
    std::vector<uint64_t> _pack_size;
    HashDigest _pack_digest;

    // unpack info
    std::vector<Folder> _folders;
    HashDigest _unpack_digest;
};

};
