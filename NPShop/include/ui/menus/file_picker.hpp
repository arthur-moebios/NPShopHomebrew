#pragma once

#include "ui/menus/filebrowser.hpp"
#include "ui/menus/menu_base.hpp"
#include "ui/scrolling_text.hpp"
#include "ui/list.hpp"
#include "fs.hpp"
#include "option.hpp"
#include <span>

namespace npshop::ui::menu::filepicker {

enum FsEntryFlag {
    FsEntryFlag_None,
    // write protected.
    FsEntryFlag_ReadOnly = 1 << 0,
    // supports file assoc.
    FsEntryFlag_Assoc = 1 << 1,
};

enum SortType {
    SortType_Size,
    SortType_Alphabetical,
};

enum OrderType {
    OrderType_Descending,
    OrderType_Ascending,
};

using FsType = filebrowser::FsType;
using FsEntry = filebrowser::FsEntry;
using FileEntry = filebrowser::FileEntry;
using LastFile = filebrowser::LastFile;

using Callback = std::function<bool(const fs::FsPath& path)>;

struct Menu final : MenuBase {
    explicit Menu(const Callback& cb, const std::vector<std::string>& filter = {}, const fs::FsPath& path = {});
    ~Menu();

    auto GetShortTitle() const -> const char* override { return "Picker"; };
    void Update(Controller* controller, TouchInfo* touch) override;
    void Draw(NVGcontext* vg, Theme* theme) override;
    void OnFocusGained() override;

    static auto GetNewPath(const fs::FsPath& root_path, const fs::FsPath& file_path) -> fs::FsPath {
        return fs::AppendPath(root_path, file_path);
    }

private:
    auto GetFs() {
        return m_fs.get();
    }

    auto& GetFsEntry() const {
        return m_fs_entry;
    }

    void SetIndex(s64 index);

    auto Scan(const fs::FsPath& new_path, bool is_walk_up = false) -> Result;

    auto GetNewPath(const FileEntry& entry) const -> fs::FsPath {
        return GetNewPath(m_path, entry.name);
    }

    auto GetNewPath(s64 index) const -> fs::FsPath {
        return GetNewPath(m_path, GetEntry(index).name);
    }

    auto GetNewPathCurrent() const -> fs::FsPath {
        return GetNewPath(m_index);
    }

    auto GetEntry(u32 index) -> FileEntry& {
        return m_entries[m_entries_current[index]];
    }

    auto GetEntry(u32 index) const -> const FileEntry& {
        return m_entries[m_entries_current[index]];
    }

    auto GetEntry() -> FileEntry& {
        return GetEntry(m_index);
    }

    auto GetEntry() const -> const FileEntry& {
        return GetEntry(m_index);
    }

    auto IsSd() const -> bool {
        return m_fs_entry.type == FsType::Sd;
    }

    void Sort();
    void SortAndFindLastFile(bool scan = false);
    void SetIndexFromLastFile(const LastFile& last_file);

    void SetFs(const fs::FsPath& new_path, const FsEntry& new_entry);

    auto GetNative() -> fs::FsNative* {
        return (fs::FsNative*)m_fs.get();
    }

    void DisplayOptions();

    void UpdateSubheading();
    void PromptIfShouldExit();

private:
    static constexpr inline const char* INI_SECTION = "filepicker";

    Callback m_callback;
    std::vector<std::string> m_filter;

    std::unique_ptr<fs::Fs> m_fs{};
    FsEntry m_fs_entry{};
    fs::FsPath m_path{};
    std::vector<FileEntry> m_entries{};
    std::vector<u32> m_entries_index{}; // files not including hidden
    std::vector<u32> m_entries_index_hidden{}; // includes hidden files
    std::span<u32> m_entries_current{};

    std::unique_ptr<List> m_list{};

    // this keeps track of the highlighted file before opening a folder
    // if the user presses B to go back to the previous dir
    // this vector is popped, then, that entry is checked if it still exists
    // if it does, the index becomes that file.
    std::vector<LastFile> m_previous_highlighted_file{};
    s64 m_index{};
    ScrollingText m_scroll_name{};

    option::OptionLong m_sort{INI_SECTION, "sort", SortType::SortType_Alphabetical, false};
    option::OptionLong m_order{INI_SECTION, "order", OrderType::OrderType_Descending, false};
    option::OptionBool m_show_hidden{INI_SECTION, "show_hidden", false, false};
    option::OptionBool m_folders_first{INI_SECTION, "folders_first", true, false};
    option::OptionBool m_hidden_last{INI_SECTION, "hidden_last", false, false};
    option::OptionBool m_ignore_read_only{INI_SECTION, "ignore_read_only", false, false};
};

} // namespace npshop::ui::menu::filepicker
