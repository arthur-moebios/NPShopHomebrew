#pragma once

#include <switch.h>
#include <string>
#include <vector>
#include "ui/progress_box.hpp"

namespace npshop {

struct OwoConfig {
    std::string nro_path;
    std::string args{};
    std::string name{};
    std::string author{};
    NacpStruct nacp;
    std::vector<u8> icon;
    std::vector<u8> logo;
    std::vector<u8> gif;

    std::vector<u8> program_nca{};
};

} // namespace npshop
