#pragma once

#include "base.hpp"
#include <vector>
#include <memory>
#include <switch.h>

namespace npshop::yati::container {

struct Xci final : Base {
    using Base::Base;
    Result GetCollections(Collections& out) override;
};

} // namespace npshop::yati::container
