#pragma once

#include <switch.h>

namespace npshop::ncz {

#define NCZ_SECTION_MAGIC 0x4E544345535A434EUL
// todo: byteswap this
#define NCZ_BLOCK_MAGIC std::byteswap(0x4E435A424C4F434BUL)

#define NCZ_SECTION_OFFSET (0x4000 + sizeof(ncz::Header))

struct Header {
    u64 magic; // NCZ_SECTION_MAGIC
    u64 total_sections;
};

struct BlockHeader {
    u64 magic; // NCZ_BLOCK_MAGIC
    u8 version;
    u8 type;
    u8 padding;
    u8 block_size_exponent;
    u32 total_blocks;
    u64 decompressed_size;
};

struct Block {
    u32 size;
};

struct BlockInfo {
    u64 offset; // compressed offset.
    u64 size; // compressed size.

    auto InRange(u64 off) const -> bool {
        return off < offset + size && off >= offset;
    }
};

struct Section {
    u64 offset;
    u64 size;
    u64 crypto_type;
    u64 padding;
    u8 key[0x10];
    u8 counter[0x10];

    auto InRange(u64 off) const -> bool {
        return off < offset + size && off >= offset;
    }
};

} // namespace npshop::ncz
