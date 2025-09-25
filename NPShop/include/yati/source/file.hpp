#pragma once

#include "base.hpp"
#include "fs.hpp"
#include <switch.h>
#include <memory>

namespace npshop::yati::source {

struct File final : Base {
    File(fs::Fs* fs, const fs::FsPath& path);
    Result Read(void* buf, s64 off, s64 size, u64* bytes_read) override;

private:
    fs::Fs* m_fs{};
    fs::File m_file{};
};

} // namespace npshop::yati::source
