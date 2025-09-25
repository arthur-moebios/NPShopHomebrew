#include "ui/sidebar.hpp"
#include "ui/menus/file_picker.hpp"
#include "app.hpp"
#include "ui/popup_list.hpp"
#include "ui/nvg_util.hpp"
#include "i18n.hpp"
#include "swkbd.hpp"
#include <algorithm>

namespace npshop::ui {
namespace {

auto DistanceBetweenY(Vec4 va, Vec4 vb) -> Vec4 {
    return Vec4{
        va.x, va.y,
        va.w, vb.y - va.y
    };
}

} // namespace

SidebarEntryBase::SidebarEntryBase(const std::string& title, const std::string& info)
: m_title{title}, m_info{info} {

}

void SidebarEntryBase::Draw(NVGcontext* vg, Theme* theme, const Vec4& root_pos, bool left) {
    // draw spacers or highlight box if in focus (selected)
    if (HasFocus()) {
        gfx::drawRectOutline(vg, theme, 4.f, m_pos);

        const auto& info = IsEnabled() ? m_info : m_depends_info;

        if (!info.empty()) {
            // reset clip here as the box will draw oob.
            nvgSave(vg);
            nvgScissor(vg, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            ON_SCOPE_EXIT(nvgRestore(vg));

            Vec4 info_box{};
            info_box.y = 86;
            info_box.w = 400;

            if (left) {
                info_box.x = root_pos.x + root_pos.w + 10;
            } else {
                info_box.x = root_pos.x - info_box.w - 10;
            }

            const float info_pad = 30;
            const float title_font_size = 18;
            const float info_font_size = 18;
            const float pad_after_title = title_font_size + info_pad;
            const float x = info_box.x + info_pad;
            const auto end_w = info_box.w - info_pad * 2;

            float bounds[4];
            nvgFontSize(vg, info_font_size);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            nvgTextLineHeight(vg, 1.7);
            nvgTextBoxBounds(vg, 0, 0, end_w, info.c_str(), nullptr, bounds);
            info_box.h = pad_after_title + info_pad * 2 + bounds[3] - bounds[1];

            gfx::drawRect(vg, info_box, theme->GetColour(ThemeEntryID_SIDEBAR), 5);

            float y = info_box.y + info_pad;
            m_scolling_title.Draw(vg, true, x, y, end_w, title_font_size, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, theme->GetColour(ThemeEntryID_TEXT), m_title.c_str());

            y += pad_after_title;
            gfx::drawTextBox(vg, x, y, info_font_size, end_w, theme->GetColour(ThemeEntryID_TEXT), info.c_str());
        }
    }
}

auto SidebarEntryBase::OnFocusGained() noexcept -> void {
    Widget::OnFocusGained();
}

auto SidebarEntryBase::OnFocusLost() noexcept -> void {
    Widget::OnFocusLost();
    m_scolling_value.Reset();
}

void SidebarEntryBase::DrawEntry(NVGcontext* vg, Theme* theme, const std::string& left, const std::string& right, bool use_selected) {
    const auto colour_id = IsEnabled() ? ThemeEntryID_TEXT : ThemeEntryID_TEXT_INFO;

    // scrolling text
    float bounds[4];
    nvgFontSize(vg, 20);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgTextBounds(vg, 0, 0, left.c_str(), nullptr, bounds);
    const float start_x = bounds[2] + 50;
    const float max_off = m_pos.w - start_x - 15.f;

    nvgTextBounds(vg, 0, 0, right.c_str(), nullptr, bounds);

    const Vec2 key_text_pos{m_pos.x + 15.f, m_pos.y + (m_pos.h / 2.f)};
    gfx::drawText(vg, key_text_pos, 20.f, theme->GetColour(colour_id), left.c_str(), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    const auto value_id = use_selected ? ThemeEntryID_TEXT_SELECTED : ThemeEntryID_TEXT;
    const float xpos = m_pos.x + m_pos.w - 15.f - std::min(max_off, bounds[2]);
    const float ypos = m_pos.y + (m_pos.h / 2.f);

    m_scolling_value.Draw(vg, HasFocus(), xpos, ypos, max_off, 20.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, theme->GetColour(value_id), right);
}

SidebarEntryBool::SidebarEntryBool(const std::string& title, bool option, Callback cb, const std::string& info, const std::string& true_str, const std::string& false_str)
: SidebarEntryBase{title, info}
, m_option{option}
, m_callback{cb}
, m_true_str{true_str}
, m_false_str{false_str} {

    if (m_true_str == "On") {
        m_true_str = i18n::get(m_true_str);
    }
    if (m_false_str == "Off") {
        m_false_str = i18n::get(m_false_str);
    }

    SetAction(Button::A, Action{"OK"_i18n, [this](){
        if (!IsEnabled()) {
            DependsClick();
        } else {
            m_option ^= 1;
            m_callback(m_option);
        } }
    });
}

SidebarEntryBool::SidebarEntryBool(const std::string& title, bool& option, const std::string& info, const std::string& true_str, const std::string& false_str)
: SidebarEntryBool{title, option, Callback{}, info, true_str, false_str} {
    m_callback = [&option](bool&){
        option ^= 1;
    };
}

SidebarEntryBool::SidebarEntryBool(const std::string& title, option::OptionBool& option, const Callback& cb, const std::string& info, const std::string& true_str, const std::string& false_str)
: SidebarEntryBool{title, option.Get(), Callback{}, info, true_str, false_str} {
    m_callback = [&option, cb](bool& v_out){
        if (cb) {
            cb(v_out);
        }
        option.Set(v_out);
    };
}

SidebarEntryBool::SidebarEntryBool(const std::string& title, option::OptionBool& option, const std::string& info, const std::string& true_str, const std::string& false_str)
: SidebarEntryBool{title, option, Callback{}, info, true_str, false_str} {
}

void SidebarEntryBool::Draw(NVGcontext* vg, Theme* theme, const Vec4& root_pos, bool left) {
    SidebarEntryBase::Draw(vg, theme, root_pos, left);
    SidebarEntryBase::DrawEntry(vg, theme, m_title, m_option ? m_true_str : m_false_str, m_option);
}

SidebarEntryCallback::SidebarEntryCallback(const std::string& title, Callback cb, bool pop_on_click, const std::string& info)
: SidebarEntryBase{title, info}
, m_callback{cb}
, m_pop_on_click{pop_on_click} {
    SetAction(Button::A, Action{"OK"_i18n, [this](){
        if (!IsEnabled()) {
            DependsClick();
        } else {
            m_callback();
            if (m_pop_on_click) {
                SetPop();
            }
        }}
    });
}

SidebarEntryCallback::SidebarEntryCallback(const std::string& title, Callback cb, const std::string& info)
: SidebarEntryCallback{title, cb, false, info} {

}

void SidebarEntryCallback::Draw(NVGcontext* vg, Theme* theme, const Vec4& root_pos, bool left) {
    SidebarEntryBase::Draw(vg, theme, root_pos, left);

    const auto colour_id = IsEnabled() ? ThemeEntryID_TEXT : ThemeEntryID_TEXT_INFO;
    gfx::drawText(vg, Vec2{m_pos.x + 15.f, m_pos.y + (m_pos.h / 2.f)}, 20.f, theme->GetColour(colour_id), m_title.c_str(), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
}

SidebarEntryArray::SidebarEntryArray(const std::string& title, const Items& items, std::string& index, const std::string& info)
: SidebarEntryArray{title, items, Callback{}, 0, info} {

    const auto it = std::find(m_items.cbegin(), m_items.cend(), index);
    if (it != m_items.cend()) {
        m_index = std::distance(m_items.cbegin(), it);
    }

    m_list_callback = [&index, this]() {
        App::Push<PopupList>(
            m_title, m_items, index, m_index
        );
    };
}

SidebarEntryArray::SidebarEntryArray(const std::string& title, const Items& items, Callback cb, const std::string& index, const std::string& info)
: SidebarEntryArray{title, items, cb, 0, info} {

    const auto it = std::find(m_items.cbegin(), m_items.cend(), index);
    if (it != m_items.cend()) {
        m_index = std::distance(m_items.cbegin(), it);
    }
}

SidebarEntryArray::SidebarEntryArray(const std::string& title, const Items& items, Callback cb, s64 index, const std::string& info)
: SidebarEntryBase{title, info}
, m_items{items}
, m_callback{cb}
, m_index{index} {

    m_list_callback = [this]() {
        App::Push<PopupList>(
            m_title, m_items, [this](auto op_idx){
                if (op_idx) {
                    m_index = *op_idx;
                    m_callback(m_index);
                }
            }, m_index
        );
    };

    SetAction(Button::A, Action{"OK"_i18n, [this](){
        if (!IsEnabled()) {
            DependsClick();
        } else {
            // m_callback(m_index);
            m_list_callback();
        }}
    });
}

void SidebarEntryArray::Draw(NVGcontext* vg, Theme* theme, const Vec4& root_pos, bool left) {
    SidebarEntryBase::Draw(vg, theme, root_pos, left);
    SidebarEntryBase::DrawEntry(vg, theme, m_title, m_items[m_index], true);
}

SidebarEntryTextBase::SidebarEntryTextBase(const std::string& title, const std::string& value, const Callback& cb, const std::string& info)
: SidebarEntryBase{title, info}
, m_value{value}
, m_callback{cb} {
    SetAction(Button::A, Action{"OK"_i18n, [this](){
        if (m_callback) {
            m_callback();
        }
    }});
}

void SidebarEntryTextBase::Draw(NVGcontext* vg, Theme* theme, const Vec4& root_pos, bool left) {
    SidebarEntryBase::Draw(vg, theme, root_pos, left);
    SidebarEntryBase::DrawEntry(vg, theme, m_title, m_value, true);
}

SidebarEntryTextInput::SidebarEntryTextInput(const std::string& title, const std::string& value, const std::string& guide, s64 len_min, s64 len_max, const std::string& info)
: SidebarEntryTextBase{title, value, {}, info}
, m_guide{guide}
, m_len_min{len_min}
, m_len_max{len_max} {

    SetCallback([this](){
        std::string out;
        if (R_SUCCEEDED(swkbd::ShowText(out, m_guide.c_str(), GetValue().c_str(), m_len_min, m_len_max))) {
            SetValue(out);
        }
    });
}

SidebarEntryFilePicker::SidebarEntryFilePicker(const std::string& title, const std::string& value, const std::vector<std::string>& filter, const std::string& info)
: SidebarEntryTextBase{title, value, {}, info}, m_filter{filter} {

    SetCallback([this](){
        App::Push<menu::filepicker::Menu>(
            [this](const fs::FsPath& path) {
                SetValue(path);
                return true;
            },
            m_filter
        );
    });
}

Sidebar::Sidebar(const std::string& title, Side side, Items&& items)
: Sidebar{title, "", side, std::forward<decltype(items)>(items)} {
}

Sidebar::Sidebar(const std::string& title, Side side)
: Sidebar{title, "", side, {}} {
}

Sidebar::Sidebar(const std::string& title, const std::string& sub, Side side, Items&& items)
: m_title{title}
, m_sub{sub}
, m_side{side}
, m_items{std::forward<decltype(items)>(items)} {
    switch (m_side) {
        case Side::LEFT:
            SetPos(Vec4{0.f, 0.f, 450.f, SCREEN_HEIGHT});
            break;

        case Side::RIGHT:
            SetPos(Vec4{SCREEN_WIDTH - 450.f, 0.f, 450.f, SCREEN_HEIGHT});
            break;
    }

    // setup top and bottom bar
    m_top_bar = Vec4{m_pos.x + 15.f, 86.f, m_pos.w - 30.f, 1.f};
    m_bottom_bar = Vec4{m_pos.x + 15.f, 646.f, m_pos.w - 30.f, 1.f};
    m_title_pos = Vec2{m_pos.x + 30.f, m_pos.y + 40.f};
    m_base_pos = Vec4{GetX() + 30.f, GetY() + 170.f, m_pos.w - (30.f * 2.f), 70.f};

    // set button positions
    SetUiButtonPos({m_pos.x + m_pos.w - 60.f, 675});

    const Vec4 pos = DistanceBetweenY(m_top_bar, m_bottom_bar);
    m_list = std::make_unique<List>(1, 6, pos, m_base_pos);
    m_list->SetScrollBarPos(GetX() + GetW() - 20, m_base_pos.y - 10, pos.h - m_base_pos.y + 48);
}

Sidebar::Sidebar(const std::string& title, const std::string& sub, Side side)
: Sidebar{title, sub, side, {}} {
}


auto Sidebar::Update(Controller* controller, TouchInfo* touch) -> void {
    Widget::Update(controller, touch);

    // if touched out of bounds, pop the sidebar and all widgets below it.
    if (touch->is_clicked && !touch->in_range(GetPos())) {
        App::PopToMenu();
    } else {
        m_list->OnUpdate(controller, touch, m_index, m_items.size(), [this](bool touch, auto i) {
            SetIndex(i);
            if (touch) {
                FireAction(Button::A);
            }
        });
    }

    if (m_items[m_index]->ShouldPop()) {
        SetPop();
    }
}

auto Sidebar::Draw(NVGcontext* vg, Theme* theme) -> void {
    gfx::drawRect(vg, m_pos, theme->GetColour(ThemeEntryID_SIDEBAR));
    gfx::drawText(vg, m_title_pos, m_title_size, theme->GetColour(ThemeEntryID_TEXT), m_title.c_str());
    if (!m_sub.empty()) {
        gfx::drawTextArgs(vg, m_pos.x + m_pos.w - 30.f, m_title_pos.y + 10.f, 16, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT, theme->GetColour(ThemeEntryID_TEXT_INFO), m_sub.c_str());
    }
    gfx::drawRect(vg, m_top_bar, theme->GetColour(ThemeEntryID_LINE));
    gfx::drawRect(vg, m_bottom_bar, theme->GetColour(ThemeEntryID_LINE));

    Widget::Draw(vg, theme);

    m_list->Draw(vg, theme, m_items.size(), [this](auto* vg, auto* theme, auto v, auto i) {
        const auto& [x, y, w, h] = v;

        if (i != m_items.size() - 1) {
            gfx::drawRect(vg, x, y + h, w, 1.f, theme->GetColour(ThemeEntryID_LINE_SEPARATOR));
        }

        m_items[i]->SetY(y);
        m_items[i]->Draw(vg, theme, m_pos, m_side == Side::LEFT);
    });
}

auto Sidebar::OnFocusGained() noexcept -> void {
    Widget::OnFocusGained();
    SetHidden(false);
}

auto Sidebar::OnFocusLost() noexcept -> void {
    Widget::OnFocusLost();
    SetHidden(true);
}

auto Sidebar::Add(std::unique_ptr<SidebarEntryBase>&& _entry) -> SidebarEntryBase* {
    auto& entry = m_items.emplace_back(std::forward<decltype(_entry)>(_entry));
    entry->SetPos(m_base_pos);

    // give focus to first entry.
    if (m_items.size() == 1) {
        entry->OnFocusGained();
        SetupButtons();
    }

    return entry.get();
}

void Sidebar::SetIndex(s64 index) {
    // if we moved
    if (m_index != index) {
        m_items[m_index]->OnFocusLost();
        m_index = index;
        m_items[m_index]->OnFocusGained();
        SetupButtons();
    }
}

void Sidebar::SetupButtons() {
    RemoveActions();

    // add entry actions
    for (const auto& [button, action] : m_items[m_index]->GetActions()) {
        SetAction(button, action);
    }

    // add default actions, overriding if needed.
    this->SetActions(
        // each item has it's own Action, but we take over B
        std::make_pair(Button::B, Action{"Back"_i18n, [this](){
            SetPop();
        }})
    );
}

} // namespace npshop::ui
