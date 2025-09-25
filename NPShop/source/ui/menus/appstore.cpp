#include "ui/menus/appstore.hpp"
#include "ui/menus/homebrew.hpp"
#include "ui/sidebar.hpp"
#include "ui/popup_list.hpp"
#include "ui/progress_box.hpp"
#include "ui/option_box.hpp"

#include "download.hpp"
#include "defines.hpp"
#include "log.hpp"
#include "app.hpp"
#include "ui/nvg_util.hpp"
#include "fs.hpp"
#include "yyjson_helper.hpp"
#include "swkbd.hpp"
#include "i18n.hpp"
#include "hasher.hpp"
#include "threaded_file_transfer.hpp"
#include "web.hpp"
#include "minizip_helper.hpp"
#include "yati/yati.hpp"
#include "yati/source/file.hpp"

#include <minIni.h>
#include <string>
#include <cstring>
#include <yyjson.h>
#include <stb_image.h>
#include <minizip/unzip.h>
#include <algorithm>
#include <ranges>
#include <utility>
#include <cctype>
#include <unordered_map>

namespace npshop::ui::menu::appstore {
namespace {

constexpr fs::FsPath REPO_PATH{"/switch/npshop/cache/appstore/repo.json"};
constexpr fs::FsPath CACHE_PATH{"/switch/npshop/cache/appstore"};
constexpr auto URL_BASE = "https://npshop.org";
constexpr auto URL_JSON = "https://npshop.org/api/hb/games";
constexpr auto URL_POST_FEEDBACK = "http://switchbru.com/appstore/feedback";
constexpr auto URL_GET_FEEDACK = "http://switchbru.com/appstore/feedback";
static std::unordered_map<std::string, std::string> g_icon_url_by_id;
static std::unordered_map<std::string, std::string> g_banner_url_by_id;

constexpr const u8 UPDATE_IMAGE_DATA[]{
    #embed <icons/UPDATE.png>
};

constexpr const u8 GET_IMAGE_DATA[]{
    #embed <icons/GET.png>
};

constexpr const u8 LOCAL_IMAGE_DATA[]{
    #embed <icons/LOCAL.png>
};

constexpr const u8 INSTALLED_IMAGE_DATA[]{
    #embed <icons/INSTALLED.png>
};

constexpr const char* FILTER_STR[] = {
    "All",
};

constexpr const char* SORT_STR[] = {
    "Alphabetical",
};

constexpr const char* ORDER_STR[] = {
    "Desc",
    "Asc",
};
    static constexpr const char* INI_SEC_AUTH = "auth";
    static constexpr const char* INI_KEY_ACTIV = "activated";
    static constexpr const char* INI_KEY_TOKEN = "token";
    static constexpr const char* INI_KEY_DEV_ID = "device_id";

    static std::string BytesToHexUpper(const uint8_t* p, size_t n) {
        static const char* k = "0123456789ABCDEF";
        std::string s; s.resize(n * 2);
        for (size_t i = 0; i < n; i++) { s[2 * i] = k[(p[i] >> 4) & 0xF]; s[2 * i + 1] = k[p[i] & 0xF]; }
        return s;
    }

static std::string g_device_id() {
        // tenta ler do INI
        char buf[80]{}; // 32*2 + margem
        ini_gets(INI_SEC_AUTH, INI_KEY_DEV_ID, "", buf, sizeof(buf), ::npshop::App::CONFIG_PATH);
        if (buf[0]) {
            return std::string(buf);
        }
        // gera 16 bytes aleatórios => 32 HEX UPPER
        uint8_t rnd[16];
        csrngGetRandomBytes(rnd, sizeof(rnd));
        std::string id = BytesToHexUpper(rnd, sizeof(rnd));
        ini_puts(INI_SEC_AUTH, INI_KEY_DEV_ID, id.c_str(), ::npshop::App::CONFIG_PATH);
        return id;
}
    static fs::FsPath DownloadsDir() { return "/switch/npshop/downloads"; }
    static fs::FsPath LocalDownloadPath(const std::string& filename) {
        return DownloadsDir() + "/" + filename;
    }

    static void InstallFromSdWithDeletePrompt(const fs::FsPath& file_path,
                                              const std::string& display_name) {
        // opcional: respeitar a flag global de install
        if (!::npshop::App::GetInstallEnable()) {
            ::npshop::App::ShowEnableInstallPrompt();
            return;
        }

        ::npshop::App::Push<::npshop::ui::ProgressBox>(
            0, "Installing "_i18n, display_name,
            [file_path](auto pbox) -> Result {
                fs::FsNativeSd fs;
                R_TRY(fs.GetFsOpenResult());
                // Instalador já usado no File Browser:
                // yati::InstallFromFile(pbox, &fs, file_path);
                R_TRY(yati::InstallFromFile(pbox, &fs, file_path));  // :contentReference[oaicite:1]{index=1}
                R_SUCCEED();
            },
            [file_path, display_name](Result rc) {
                ::npshop::App::PushErrorBox(rc, "File install failed!"_i18n);
                if (R_SUCCEEDED(rc)) {
                    ::npshop::App::Notify("Installed "_i18n + display_name);
                    // Pergunta se quer deletar o arquivo para liberar espaço
                    ::npshop::App::Push<::npshop::ui::OptionBox>(
                        "Delete downloaded file to free space?"_i18n,
                        "No"_i18n, "Yes"_i18n, 1,
                        [file_path](auto op_index) {
                            if (op_index && *op_index) {
                                fs::FsNativeSd fs;
                                if (R_SUCCEEDED(fs.DeleteFile(file_path))) {
                                    ::npshop::App::Notify("File deleted"_i18n);
                                } else {
                                    ::npshop::App::Notify("Failed to delete file"_i18n);
                                }
                            }
                        }
                    );
                }
            }
        );
    }

    auto BuildIconUrl(const Entry& e) -> std::string {
        if (auto it = g_icon_url_by_id.find(e.name); it != g_icon_url_by_id.end() && !it->second.empty()) {
            return it->second;
        }
        char out[0x200];
        std::snprintf(out, sizeof(out), "%s/packages/%s/icon.png", URL_BASE, e.name.c_str());
        return out;
    }

    auto BuildBannerUrl(const Entry& e) -> std::string {
        if (auto it = g_banner_url_by_id.find(e.name); it != g_banner_url_by_id.end() && !it->second.empty()) {
            return it->second;
        }
        char out[0x200];
        std::snprintf(out, sizeof(out), "%s/packages/%s/screen.png", URL_BASE, e.name.c_str());
        return out;
    }


auto BuildBannerUrl(const Entry& e) -> std::string {
    char out[0x100];
    std::snprintf(out, sizeof(out), "%s/packages/%s/screen.png", URL_BASE, e.name.c_str());
    return out;
}

auto BuildManifestUrl(const Entry& e) -> std::string {
    char out[0x100];
    std::snprintf(out, sizeof(out), "%s/packages/%s/manifest.install", URL_BASE, e.name.c_str());
    return out;
}

auto BuildIconCachePath(const Entry& e) -> fs::FsPath {
    fs::FsPath out;
    std::snprintf(out, sizeof(out), "%s/icons/%s.png", CACHE_PATH.s, e.name.c_str());
    return out;
}

auto BuildBannerCachePath(const Entry& e) -> fs::FsPath {
    fs::FsPath out;
    std::snprintf(out, sizeof(out), "%s/banners/%s.png", CACHE_PATH.s, e.name.c_str());
    return out;
}

#if 0
auto BuildScreensCachePath(const Entry& e, u8 num) -> fs::FsPath {
    fs::FsPath out;
    std::snprintf(out, sizeof(out), "%s/screens/%s%u.png", CACHE_PATH, e.name.c_str(), num+1);
    return out;
}
#endif

// use appstore path in order to maintain compat with appstore
auto BuildPackageCachePath(const Entry& e) -> fs::FsPath {
    return "/switch/appstore/.get/packages/" + e.name;
}

auto BuildInfoCachePath(const Entry& e) -> fs::FsPath {
    return BuildPackageCachePath(e) + "/info.json";
}

auto BuildManifestCachePath(const Entry& e) -> fs::FsPath {
    return BuildPackageCachePath(e) + "/manifest.install";
}

auto BuildFeedbackCachePath(const Entry& e) -> fs::FsPath {
    return BuildPackageCachePath(e) + "/feedback.json";
}

static void debug_dump_repo_file_head() {
    std::vector<u8> head;
    if (R_SUCCEEDED(fs::read_entire_file(REPO_PATH, head))) {
        const size_t n = std::min<size_t>(head.size(), 2048);
        std::string s(reinterpret_cast<const char*>(head.data()), n);
        log_write("[AppStore] repo.json head (%zu bytes):\n%.*s\n", head.size(), (int)n, s.c_str());
    } else {
        log_write("[AppStore] repo.json not found to dump head\n");
    }
}

// Parser para o JSON do hbindex()
static void from_json_hb(const fs::FsPath& path, std::vector<Entry>& out) {
    yyjson_read_err err;
    yyjson_doc* doc = yyjson_read_file(path, YYJSON_READ_NOFLAG, nullptr, &err);
    if (!doc) return;
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!yyjson_is_arr(root)) { yyjson_doc_free(doc); return; }

    size_t idx, max;
    yyjson_val* it;
    yyjson_arr_foreach(root, idx, max, it) {
        Entry e{};
        auto v_title = yyjson_obj_get(it, "title");
        auto v_titleid = yyjson_obj_get(it, "titleid");
        auto v_base = yyjson_obj_get(it, "base");
        auto v_update = yyjson_obj_get(it, "update");
        auto v_img = yyjson_obj_get(it, "image");
        auto v_ban = yyjson_obj_get(it, "banner");

        if (v_img && yyjson_is_str(v_img)) {
            g_icon_url_by_id[e.name] = yyjson_get_str(v_img);
        }
        if (v_ban && yyjson_is_str(v_ban)) {
            g_banner_url_by_id[e.name] = yyjson_get_str(v_ban);
        }

        e.title = v_title ? yyjson_get_str(v_title) : "";
        e.name = v_titleid ? yyjson_get_str(v_titleid) : e.title;
        e.hb_titleid = e.name;

        e.category = "game";
        e.author = "";
        e.description.clear();
        e.binary = "none";
        e.url.clear();
        e.md5.clear();

        if (v_base && yyjson_is_obj(v_base)) {
            auto bfn = yyjson_obj_get(v_base, "filename");
            auto burl = yyjson_obj_get(v_base, "url");
            auto bsz = yyjson_obj_get(v_base, "size");
            e.hb_base_filename = bfn ? yyjson_get_str(bfn) : "";
            e.hb_base_url = burl ? yyjson_get_str(burl) : "";
            e.hb_base_size = bsz ? (u64)yyjson_get_uint(bsz) : 0ULL;
            e.extracted = (u32)(e.hb_base_size / 1024); // KiB
        }

        if (v_update && yyjson_is_obj(v_update)) {
            auto ufn = yyjson_obj_get(v_update, "filename");
            auto uurl = yyjson_obj_get(v_update, "url");
            auto uver = yyjson_obj_get(v_update, "version");
            auto uvi = yyjson_obj_get(v_update, "version_int");
            e.hb_upd_filename = ufn ? yyjson_get_str(ufn) : "";
            e.hb_upd_url = uurl ? yyjson_get_str(uurl) : "";
            e.hb_upd_version = uver ? yyjson_get_str(uver) : "";
            e.hb_upd_version_int = uvi ? (int)yyjson_get_sint(uvi) : -1;
            e.version = !e.hb_upd_version.empty() ? e.hb_upd_version : "v0";
        } else {
            e.version = "v0";
        }

        e.updated = "—";
        e.updated_num = 0;
        e.status = !e.hb_upd_url.empty() ? EntryStatus::Update : EntryStatus::Get;

        char det[512];
        std::snprintf(det, sizeof(det),
            "TitleID: %s\nBase: %s (%.2f MiB)\nUpdate: %s %s\n",
            e.hb_titleid.c_str(),
            e.hb_base_filename.c_str(), (double)e.hb_base_size / (1024.0 * 1024.0),
            e.hb_upd_filename.empty() ? "(nenhum)" : e.hb_upd_filename.c_str(),
            e.hb_upd_version.empty() ? "" : e.hb_upd_version.c_str());
        e.details = det;

        out.emplace_back(std::move(e));
    }
    yyjson_doc_free(doc);
}


auto ParseManifest(std::span<const char> view) -> ManifestEntries {
    ManifestEntries entries;
    for (const auto line : std::views::split(view, '\n')) {
        if (line.size() <= 3) {
            continue;
        }
        ManifestEntry entry{};
        entry.command = line[0];
        std::strncpy(entry.path, line.data() + 3, line.size() - 3);
        entries.emplace_back(entry);
    }
    return entries;
}

auto EntryLoadImageData(std::span<const u8> image_buf, LazyImage& image) -> bool {
    // already have the image
    if (image.image) {
        return true;
    }
    auto vg = App::GetVg();

    int channels_in_file;
    auto buf = stbi_load_from_memory(image_buf.data(), image_buf.size(), &image.w, &image.h, &channels_in_file, 4);
    if (buf) {
        ON_SCOPE_EXIT(stbi_image_free(buf));
        std::memcpy(image.first_pixel, buf, sizeof(image.first_pixel));
        image.image = nvgCreateImageRGBA(vg, image.w, image.h, 0, buf);
    }

    return image.image;
}

auto EntryLoadImageFile(fs::Fs& fs, const fs::FsPath& path, LazyImage& image) -> bool {
    // already have the image
    if (image.image) {
        return true;
    }

    std::vector<u8> image_buf;
    if (R_FAILED(fs.read_entire_file(path, image_buf))) {
        log_write("failed to load image from file: %s\n", path.s);
    } else {
        EntryLoadImageData(image_buf, image);
    }

    if (!image.image) {
        log_write("failed to load image from file: %s\n", path.s);
        return false;
    } else {
        return true;
    }
}

auto EntryLoadImageFile(const fs::FsPath& path, LazyImage& image) -> bool {
    if (!strncasecmp("romfs:/", path, 7)) {
        fs::FsStdio fs;
        return EntryLoadImageFile(fs, path, image);
    } else {
        fs::FsNativeSd fs;
        return EntryLoadImageFile(fs, path, image);
    }
}
// --- PLACEHOLDER COVER/BANNER (derivado do titleid) -------------------------
static inline uint32_t fnv1a32(std::string_view s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}
static NVGcolor mk_col(uint32_t v, int bias, int span) {
    auto ch = [&](uint32_t x, int sh){ int c = bias + int((x >> sh) & 0xFF) % span; return std::clamp(c, 0, 255); };
    return nvgRGB(ch(v,0), ch(v,8), ch(v,16));
}

static void DrawCoverPlaceholder(NVGcontext* vg, const Vec4& r,
                                 std::string_view title, std::string_view titleid,
                                 bool rounded = true)
{
    // Cores determinísticas a partir do titleid
    const uint32_t h = fnv1a32(titleid.empty() ? title : titleid);
    const NVGcolor c1 = mk_col(h * 9176u + 0x45ab23u, 40, 160);   // fundo
    const NVGcolor c2 = mk_col(h * 13331u + 0x89f1d1u, 120, 135); // topo

    nvgSave(vg);
    if (rounded) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, r.x, r.y, r.w, r.h, 8.f);
        NVGpaint g = nvgLinearGradient(vg, r.x, r.y, r.x + r.w, r.y + r.h, c2, c1);
        nvgFillPaint(vg, g);
        nvgFill(vg);
    } else {
        nvgBeginPath(vg);
        nvgRect(vg, r.x, r.y, r.w, r.h);
        NVGpaint g = nvgLinearGradient(vg, r.x, r.y, r.x + r.w, r.y + r.h, c2, c1);
        nvgFillPaint(vg, g);
        nvgFill(vg);
    }

    // Texto (nome ou titleid)
    nvgScissor(vg, r.x, r.y, r.w, r.h);
    nvgFillColor(vg, nvgRGBA(255,255,255,220));
    nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);

    const float fh_title = std::max(14.f, std::min(r.h * 0.16f, 26.f));
    const float fh_id    = std::max(10.f, std::min(r.h * 0.11f, 18.f));

    // Título (ou parte dele)
    if (!title.empty()) {
        nvgFontSize(vg, fh_title);
        nvgTextBox(vg, r.x + 12, r.y + r.h * 0.52f, r.w - 24, std::string(title).c_str(), nullptr);
    } else if (!titleid.empty()) {
        nvgFontSize(vg, fh_title);
        nvgText(vg, r.x + r.w * 0.5f, r.y + r.h * 0.52f, std::string(titleid).c_str(), nullptr);
    }

    // TitleID pequeno no rodapé
    if (!titleid.empty()) {
        nvgFillColor(vg, nvgRGBA(255,255,255,190));
        nvgFontSize(vg, fh_id);
        nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_RIGHT);
        nvgText(vg, r.x + r.w - 8, r.y + r.h - 6, std::string(titleid).c_str(), nullptr);
    }

    nvgResetScissor(vg);
    nvgRestore(vg);
}

void DrawIcon(NVGcontext* vg, const LazyImage& l, const LazyImage& d, float x, float y, float w, float h, bool rounded = true, float scale = 1.0) {
    const auto& i = l.image ? l : d;

    const float iw = (float)i.w / scale;
    const float ih = (float)i.h / scale;
    float ix = x;
    float iy = y;
    bool rounded_image = rounded;

    if (w > iw) {
        ix = x + abs((w - iw) / 2);
    } else if (w < iw) {
        ix = x - abs((w - iw) / 2);
    }
    if (h > ih) {
        iy = y + abs((h - ih) / 2);
    } else if (h < ih) {
        iy = y - abs((h - ih) / 2);
    }

    bool crop = false;
    if (iw < w || ih < h) {
        rounded_image = false;
        gfx::drawRect(vg, x, y, w, h, nvgRGB(i.first_pixel[0], i.first_pixel[1], i.first_pixel[2]), rounded ? 5 : 0);
    }
    if (iw > w || ih > h) {
        crop = true;
        nvgSave(vg);
        nvgIntersectScissor(vg, x, y, w, h);
    }

    gfx::drawImage(vg, ix, iy, iw, ih, i.image, rounded_image ? 5 : 0);
    if (crop) {
        nvgRestore(vg);
    }
}

void DrawIcon(NVGcontext* vg, const LazyImage& l, const LazyImage& d, Vec4 vec, bool rounded = true, float scale = 1.0) {
    DrawIcon(vg, l, d, vec.x, vec.y, vec.w, vec.h, rounded, scale);
}

auto AppDlToStr(u32 value) -> std::string {
    auto str = std::to_string(value);
    u32 inc = 3;
    for (u32 i = inc; i < str.size(); i += inc) {
        str.insert(str.cend() - i , ',');
        inc++;
    }
    return str;
}

void ReadFromInfoJson(Entry& e) {
    const auto info_path = BuildInfoCachePath(e);

    yyjson_read_err err;
    auto doc = yyjson_read_file(info_path, YYJSON_READ_NOFLAG, nullptr, &err);
    if (doc) {
        const auto root = yyjson_doc_get_root(doc);
        const auto version = yyjson_obj_get(root, "version");
        if (version) {
            if (!std::strcmp(yyjson_get_str(version), e.version.c_str())) {
                e.status = EntryStatus::Installed;
            } else {
                e.status = EntryStatus::Update;
                log_write("info.json said %s needs update: %s vs %s\n", e.name.c_str(), yyjson_get_str(version), e.version.c_str());
            }
        }
        yyjson_doc_free(doc);
    }
}

// case-insensitive version of str.find()
auto FindCaseInsensitive(std::string_view base, std::string_view term) -> bool {
    const auto it = std::search(base.cbegin(), base.cend(), term.cbegin(), term.cend(), [](char a, char b){
        return std::toupper((unsigned char)a) == std::toupper((unsigned char)b);
    });
    return it != base.cend();
}

} // namespace (anon)

// ------------------------ EntryMenu ------------------------

EntryMenu::EntryMenu(Entry& entry, const LazyImage& default_icon, Menu& menu)
: MenuBase{entry.title, MenuFlag_None}
, m_entry{entry}
, m_default_icon{default_icon}
, m_menu{menu} {
    this->SetActions(
        std::make_pair(Button::DPAD_DOWN | Button::RS_DOWN, Action{[this](){
            if (m_index < (m_options.size() - 1)) {
                SetIndex(m_index + 1);
            }
        }}),
        std::make_pair(Button::DPAD_UP | Button::RS_UP, Action{[this](){
            if (m_index != 0) {
                SetIndex(m_index - 1);
            }
        }}),
        std::make_pair(Button::X, Action{"Options"_i18n, [this](){
            auto options = std::make_unique<Sidebar>("Options"_i18n, Sidebar::Side::RIGHT);
            ON_SCOPE_EXIT(App::Push(std::move(options)));

            options->Add<SidebarEntryCallback>("More by Author"_i18n, [this](){
                m_menu.SetAuthor();
                SetPop();
            }, true);

            options->Add<SidebarEntryCallback>("Leave Feedback"_i18n, [this](){
                std::string out;
                if (R_SUCCEEDED(swkbd::ShowText(out)) && !out.empty()) {
                    const auto post = "name=" "switch_user" "&package=" + m_entry.name + "&message=" + out;
                    const auto file = BuildFeedbackCachePath(m_entry);

                    curl::Api().ToAsync(
                        curl::Url{URL_POST_FEEDBACK},
                        curl::Path{file},
                        curl::Fields{post},
                        curl::StopToken{this->GetToken()},
                        curl::OnComplete{[](auto& result){
                            if (result.success) {
                                log_write("got feedback!\n");
                            } else {
                                log_write("failed to send feedback :(");
                            }
                        }}
                    );
                }
            }, true);

            if (App::IsApplication() && !m_entry.url.empty()) {
                options->Add<SidebarEntryCallback>("Visit Website"_i18n, [this](){
                    WebShow(m_entry.url);
                });
            }
        }}),
        std::make_pair(Button::B, Action{"Back"_i18n, [this](){
            SetPop();
        }}),
        std::make_pair(Button::L2, Action{"Files"_i18n, [this](){
            m_show_file_list ^= 1;

            if (m_show_file_list && !m_manifest_list && m_file_list_state == ImageDownloadState::None) {
                m_file_list_state = ImageDownloadState::Progress;
                const auto path = BuildManifestCachePath(m_entry);
                std::vector<u8> data;

                if (R_SUCCEEDED(fs::read_entire_file(path, data))) {
                    m_file_list_state = ImageDownloadState::Done;
                    data.push_back('\\0');
                    m_manifest_list = std::make_unique<ScrollableText>((const char*)data.data(), 0, 374, 250, 768, 18);
                } else {
                    curl::Api().ToMemoryAsync(
                        curl::Url{BuildManifestUrl(m_entry)},
                        curl::StopToken{this->GetToken()},
                        curl::OnComplete{[this](auto& result){
                            if (result.success) {
                                m_file_list_state = ImageDownloadState::Done;
                                result.data.push_back('\\0');
                                m_manifest_list = std::make_unique<ScrollableText>((const char*)result.data.data(), 0, 374, 250, 768, 18);
                            } else {
                                m_file_list_state = ImageDownloadState::Failed;
                            }
                        }}
                    );
                }
            }
        }})
    );

    SetTitleSubHeading("by " + m_entry.author);

    m_details = std::make_unique<ScrollableText>(m_entry.details, 0, 374, 250, 768, 18);
    m_changelog = std::make_unique<ScrollableText>(m_entry.changelog, 0, 374, 250, 768, 18);

    m_show_changlog ^= 1;
    ShowChangelogAction();

    const auto path = BuildBannerCachePath(m_entry);
    const auto url = BuildBannerUrl(m_entry);
    m_banner.cached = EntryLoadImageFile(path, m_banner);

    // race condition if we pop the widget before the download completes
    curl::Api().ToFileAsync(
        curl::Url{url},
        curl::Path{path},
        curl::Flags{curl::Flag_Cache},
        curl::StopToken{this->GetToken()},
        curl::OnComplete{[this, path](auto& result){
            if (result.success) {
                if (result.code == 304) {
                    m_banner.cached = false;
                } else {
                    EntryLoadImageFile(path, m_banner);
                }
            }
        }}
    );

    SetSubHeading(m_entry.binary);
    SetSubHeading(m_entry.description);
    UpdateOptions();
}

EntryMenu::~EntryMenu() {}

void EntryMenu::Update(Controller* controller, TouchInfo* touch) {
    MenuBase::Update(controller, touch);

    if (m_show_file_list) {
        if (m_manifest_list) {
            m_manifest_list->Update(controller, touch);
        }
    } else {
        m_detail_changelog->Update(controller, touch);
    }
}

void EntryMenu::Draw(NVGcontext* vg, Theme* theme) {
    MenuBase::Draw(vg, theme);

    constexpr Vec4 line_vec(30, 86, 1220, 646);
    constexpr Vec4 banner_vec(70, line_vec.y + 20, 848.f, 208.f);
    constexpr Vec4 icon_vec(968, line_vec.y + 30, 256, 150);
    constexpr Vec4 grid_vec(icon_vec.x - 50, line_vec.y + 1, line_vec.w, line_vec.h - line_vec.y - 1);

    gfx::drawRect(vg, grid_vec, theme->GetColour(ThemeEntryID_GRID));
    // Banner grande
    if (m_banner.image) {
        DrawIcon(vg, m_banner, m_default_icon, banner_vec, false);
    } else {
        DrawCoverPlaceholder(vg, banner_vec, m_entry.title, m_entry.hb_titleid, false);
    }

    // Ícone / capa quadrada
    if (m_entry.image.image) {
        DrawIcon(vg, m_entry.image, m_default_icon, icon_vec);
    } else {
        DrawCoverPlaceholder(vg, icon_vec, m_entry.title, m_entry.hb_titleid, true);
    }

    constexpr float text_start_x = icon_vec.x;
    float text_start_y = 218 + line_vec.y;
    const float text_inc_y = 32;
    const float font_size = 20;

    gfx::drawTextArgs(vg, text_start_x, text_start_y, font_size, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, theme->GetColour(ThemeEntryID_TEXT), "version: %s"_i18n.c_str(), m_entry.version.c_str());
    text_start_y += text_inc_y;
    gfx::drawTextArgs(vg, text_start_x, text_start_y, font_size, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, theme->GetColour(ThemeEntryID_TEXT), "updated: %s"_i18n.c_str(), m_entry.updated.c_str());
    text_start_y += text_inc_y;
    gfx::drawTextArgs(vg, text_start_x, text_start_y, font_size, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, theme->GetColour(ThemeEntryID_TEXT), "category: %s"_i18n.c_str(), m_entry.category.c_str());
    text_start_y += text_inc_y;
    gfx::drawTextArgs(
        vg, text_start_x, text_start_y, font_size, NVG_ALIGN_LEFT | NVG_ALIGN_TOP,
        theme->GetColour(ThemeEntryID_TEXT), "base size: %.2f MiB"_i18n.c_str(), (double)m_entry.hb_base_size / (1024.0 * 1024.0));
    text_start_y += text_inc_y;
    gfx::drawTextArgs(vg, text_start_x, text_start_y, font_size, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, theme->GetColour(ThemeEntryID_TEXT), "app_dls: %s"_i18n.c_str(), AppDlToStr(m_entry.app_dls).c_str());
    text_start_y += text_inc_y;

    constexpr float mm = 0;
    constexpr Vec4 block{968.f + mm, 110.f, 256.f - mm*2, 60.f};
    const float x = block.x;
    float y = 1.f + text_start_y + (text_inc_y * 3) ;
    const float h = block.h;
    const float w = block.w;

    for (s32 i = m_options.size() - 1; i >= 0; i--) {
        const auto& option = m_options[i];
        auto text_id = ThemeEntryID_TEXT;
        if (m_index == i) {
            text_id = ThemeEntryID_TEXT_SELECTED;
            gfx::drawRectOutline(vg, theme, 4.f, Vec4{x, y, w, h});
        }

        gfx::drawTextArgs(vg, x + w / 2, y + h / 2, 22, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER, theme->GetColour(text_id), option.display_text.c_str());
        y -= block.h + 18;
    }

    if (m_show_file_list) {
        if (m_manifest_list) {
            m_manifest_list->Draw(vg, theme);
        } else if (m_file_list_state == ImageDownloadState::Progress) {
            gfx::drawText(vg, 110, 374, 18, theme->GetColour(ThemeEntryID_TEXT), "Loading..."_i18n.c_str());
        } else if (m_file_list_state == ImageDownloadState::Failed) {
            gfx::drawText(vg, 110, 374, 18, theme->GetColour(ThemeEntryID_TEXT), "Failed to download manifest"_i18n.c_str());
        }
    } else {
        m_detail_changelog->Draw(vg, theme);
    }
}

void EntryMenu::ShowChangelogAction() {
    std::function<void()> func = std::bind(&EntryMenu::ShowChangelogAction, this);
    m_show_changlog ^= 1;
    m_show_file_list = false;

    if (m_show_changlog) {
        SetAction(Button::L, Action{"Details"_i18n, func});
        m_detail_changelog = m_changelog.get();
    } else {
        SetAction(Button::L, Action{"Changelog"_i18n, func});
        m_detail_changelog = m_details.get();
    }
}

void EntryMenu::UpdateOptions() {
    // ---- download direto para o SD ----
    auto download_to_sd = [this](const std::string& url, const std::string& filename) -> void {
        if (url.empty() || filename.empty()) {
            App::Notify("URL ou filename vazio"_i18n);
            return;
        }
        fs::FsNativeSd fs;
        fs.CreateDirectoryRecursively("/switch/npshop/downloads");
        const fs::FsPath out = std::string("/switch/npshop/downloads/") + filename;

        App::Push<ProgressBox>(
            m_entry.image.image, "Baixando "_i18n, filename,
            [url, out](auto pbox) -> Result {
                curl::Api api{
                    curl::Url{url},
                    curl::Path{out},
                    curl::OnProgress{pbox->OnDownloadProgressCallback()},
                };
                auto r = curl::ToFile(api);
                if (r.success) { R_SUCCEED(); }
                return Result_AppstoreFailedZipDownload;
            },
            [this, filename](Result rc) {
                App::PushErrorBox(rc, "Falha ao baixar"_i18n);
                if (R_SUCCEEDED(rc)) App::Notify("Baixado: "_i18n + filename);
            }
        );
    };

    m_options.clear();

    const bool is_installed = (m_entry.status == EntryStatus::Installed || m_entry.status == EntryStatus::Local);

    if (!is_installed) {
        // NÃO instalado: só baixar a base
        if (!m_entry.hb_base_url.empty()) {
            m_options.emplace_back(Option{
                "Download base"_i18n,
                [this, download_to_sd]{
                    download_to_sd(m_entry.hb_base_url, m_entry.hb_base_filename);
                }
            });
        }
    } else {
        // Instalado: oferecer Updates e DLCs sob demanda

        // Se o JSON já trouxe o último update, atalho direto:
        if (!m_entry.hb_upd_url.empty()) {
            m_options.emplace_back(Option{
                "Baixar último update"_i18n,
                [this, download_to_sd]{
                    download_to_sd(m_entry.hb_upd_url, m_entry.hb_upd_filename);
                }
            });
        }

        // Lista completa de updates (endpoint dedicado sugerido)
        m_options.emplace_back(Option{
            "Ver atualizações"_i18n,
            [this, download_to_sd]{
                const std::string url = std::string("https://npshop.org/api/hb/updates?titleid=") + m_entry.hb_titleid;
                curl::Api().ToMemoryAsync(
                    curl::Url{url},
                    curl::Header{{ {"X-Device-ID", g_device_id()} }},
                    curl::StopToken{this->GetToken()},
                    curl::OnComplete{[this, download_to_sd](auto& res){
                        if (!res.success) { App::Notify("Falha ao listar updates"_i18n); return; }
                        yyjson_doc* doc = yyjson_read((const char*)res.data.data(), res.data.size(), 0);
                        if (!doc) { App::Notify("JSON inválido"_i18n); return; }
                        yyjson_val* root = yyjson_doc_get_root(doc);
                        if (!yyjson_is_arr(root)) { yyjson_doc_free(doc); App::Notify("JSON inválido"_i18n); return; }

                        std::vector<std::pair<std::string,std::string>> items; // (label, url)
                        yyjson_val* it; size_t idx,max;
                        yyjson_arr_foreach(root, idx, max, it) {
                            auto vfn = yyjson_obj_get(it, "filename");
                            auto vurl= yyjson_obj_get(it, "url");
                            auto vver= yyjson_obj_get(it, "version");
                            std::string fn = vfn ? yyjson_get_str(vfn) : "";
                            std::string u  = vurl? yyjson_get_str(vurl): "";
                            std::string ver= vver ? yyjson_get_str(vver): "";
                            if (!fn.empty() && !u.empty()) {
                                std::string label = ver.empty() ? fn : (fn + " (" + ver + ")");
                                items.emplace_back(label, u);
                            }
                        }
                        yyjson_doc_free(doc);
                        if (items.empty()) { App::Notify("Sem updates disponíveis"_i18n); return; }

                        auto pop = std::make_unique<PopupList>("Atualizações"_i18n);
                        for (auto& p : items) {
                            pop->Add(p.first, [this, download_to_sd, url=p.second, label=p.first](){
                                std::string filename = label;
                                auto pos = filename.find(" (");
                                if (pos != std::string::npos) filename.erase(pos);
                                download_to_sd(url, filename);
                            });
                        }
                        App::Push(std::move(pop));
                    }}
                );
            }
        });

        // DLCs (endpoint dedicado sugerido)
        m_options.emplace_back(Option{
            "Ver DLCs"_i18n,
            [this, download_to_sd]{
                const std::string url = std::string("https://npshop.org/api/hb/dlcs?titleid=") + m_entry.hb_titleid;
                curl::Api().ToMemoryAsync(
                    curl::Url{url},
                    curl::Header{{ {"X-Device-ID", g_device_id()} }},
                    curl::StopToken{this->GetToken()},
                    curl::OnComplete{[this, download_to_sd](auto& res){
                        if (!res.success) { App::Notify("Falha ao listar DLCs"_i18n); return; }
                        yyjson_doc* doc = yyjson_read((const char*)res.data.data(), res.data.size(), 0);
                        if (!doc) { App::Notify("JSON inválido"_i18n); return; }
                        yyjson_val* root = yyjson_doc_get_root(doc);
                        if (!yyjson_is_arr(root)) { yyjson_doc_free(doc); App::Notify("JSON inválido"_i18n); return; }

                        struct Item { std::string name; std::string url; };
                        std::vector<Item> items;
                        yyjson_val* it; size_t idx,max;
                        yyjson_arr_foreach(root, idx, max, it) {
                            auto vfn = yyjson_obj_get(it, "filename");
                            auto vurl= yyjson_obj_get(it, "url");
                            std::string fn = vfn ? yyjson_get_str(vfn) : "";
                            std::string u  = vurl? yyjson_get_str(vurl): "";
                            if (!fn.empty() && !u.empty()) items.push_back({fn,u});
                        }
                        yyjson_doc_free(doc);
                        if (items.empty()) { App::Notify("Sem DLCs disponíveis"_i18n); return; }

                        auto pop = std::make_unique<PopupList>("DLCs"_i18n);
                        for (auto& it : items) {
                            pop->Add(it.name, [this, download_to_sd, it](){
                                download_to_sd(it.url, it.name);
                            });
                        }
                        App::Push(std::move(pop));
                    }}
                );
            }
        });
    }

    // Mantém Launch se houver .nro
    if (!m_entry.binary.empty() && m_entry.binary != "none") {
        m_options.emplace_back(Option{ "Launch"_i18n, [this] { nro_launch(m_entry.binary); } });
    }

    SetIndex(0);
}


void EntryMenu::SetIndex(s64 index) {
    m_index = index;
    const auto option = m_options[m_index];
    if (option.confirm_text.empty()) {
        SetAction(Button::A, Action{option.display_text, option.func});
    } else {
        SetAction(Button::A, Action{option.display_text, [this, option](){
            App::Push<OptionBox>(option.confirm_text, "No"_i18n, "Yes"_i18n, 1, [this, option](auto op_index){
                if (op_index && *op_index) {
                    option.func();
                }
            });
        }});
    }
}

// ------------------------ Menu ------------------------

Menu::Menu(u32 flags) : grid::Menu{"AppStore"_i18n, flags} {
    fs::FsNativeSd fs;
    fs.CreateDirectoryRecursively("/switch/npshop/cache/appstore/icons");
    fs.CreateDirectoryRecursively("/switch/npshop/cache/appstore/banners");
    fs.CreateDirectoryRecursively("/switch/npshop/cache/appstore/screens");

    this->SetActions(
        std::make_pair(Button::B, Action{"Back"_i18n, [this](){
            if (m_is_author) {
                m_is_author = false;
                if (m_is_search) {
                    SetSearch(m_search_term);
                } else {
                    SetFilter();
                }

                SetIndex(m_entry_author_jump_back);
                if (m_entry_author_jump_back >= 9) {
                    m_list->SetYoff((((m_entry_author_jump_back - 9) + 3) / 3) * m_list->GetMaxY());
                } else {
                    m_list->SetYoff(0);
                }
            } else if (m_is_search) {
                m_is_search = false;
                SetFilter();
                SetIndex(m_entry_search_jump_back);
                if (m_entry_search_jump_back >= 9) {
                    m_list->SetYoff(0);
                    m_list->SetYoff((((m_entry_search_jump_back - 9) + 3) / 3) * m_list->GetMaxY());
                } else {
                    m_list->SetYoff(0);
                }
            } else {
                SetPop();
            }
        }}),
        std::make_pair(Button::A, Action{"Info"_i18n, [this](){
            if (m_entries_current.empty()) {
                return;
            }
            App::Push<EntryMenu>(m_entries[m_entries_current[m_index]], m_default_image, *this);
        }}),
        std::make_pair(Button::X, Action{"Options"_i18n, [this](){
            auto options = std::make_unique<Sidebar>("AppStore Options"_i18n, Sidebar::Side::RIGHT);
            ON_SCOPE_EXIT(App::Push(std::move(options)));

            SidebarEntryArray::Items filter_items;
            filter_items.push_back("All"_i18n);
            filter_items.push_back("Games"_i18n);
            filter_items.push_back("Emulators"_i18n);
            filter_items.push_back("Tools"_i18n);
            filter_items.push_back("Advanced"_i18n);
            filter_items.push_back("Themes"_i18n);
            filter_items.push_back("Legacy"_i18n);
            filter_items.push_back("Misc"_i18n);

            SidebarEntryArray::Items sort_items;
            sort_items.push_back("Updated"_i18n);
            sort_items.push_back("Downloads"_i18n);
            sort_items.push_back("Size"_i18n);
            sort_items.push_back("Alphabetical"_i18n);

            SidebarEntryArray::Items order_items;
            order_items.push_back("Descending"_i18n);
            order_items.push_back("Ascending"_i18n);

            SidebarEntryArray::Items layout_items;
            layout_items.push_back("List"_i18n);
            layout_items.push_back("Icon"_i18n);
            layout_items.push_back("Grid"_i18n);

            options->Add<SidebarEntryArray>("Filter"_i18n, filter_items, [this](s64& index_out){
                m_filter.Set(index_out);
                SetFilter();
            }, m_filter.Get());

            options->Add<SidebarEntryArray>("Sort"_i18n, sort_items, [this](s64& index_out){
                m_sort.Set(index_out);
                SortAndFindLastFile();
            }, m_sort.Get());

            options->Add<SidebarEntryArray>("Order"_i18n, order_items, [this](s64& index_out){
                m_order.Set(index_out);
                SortAndFindLastFile();
            }, m_order.Get());

            options->Add<SidebarEntryArray>("Layout"_i18n, layout_items, [this](s64& index_out){
                m_layout.Set(index_out);
                OnLayoutChange();
            }, m_layout.Get());

            options->Add<SidebarEntryCallback>("Search"_i18n, [this](){
                std::string out;
                if (R_SUCCEEDED(swkbd::ShowText(out)) && !out.empty()) {
                    SetSearch(out);
                    log_write("got %s\n", out.c_str());
                }
            });
        }})
    );
    m_repo_download_state = ImageDownloadState::Progress;

    curl::Api().ToFileAsync(
        curl::Url{ URL_JSON },
        curl::Path{ REPO_PATH },
        curl::Flags{ curl::Flag_Cache },
        // header opcional com device id:
        curl::Header{ { {"X-Device-ID", g_device_id()} } },
        curl::StopToken{ this->GetToken() },
        curl::OnComplete{ [this](auto& result) {
            if (result.success) {
                m_repo_download_state = ImageDownloadState::Done;
                debug_dump_repo_file_head();
                if (HasFocus()) ScanHomebrew();
            } else {
                m_repo_download_state = ImageDownloadState::Failed;
                log_write("[AppStore] repo download failed (code=%d)\n", result.code);
            }
        } }
    );

    OnLayoutChange();
}

Menu::~Menu() {}

void Menu::Update(Controller* controller, TouchInfo* touch) {
    MenuBase::Update(controller, touch);
    m_list->OnUpdate(controller, touch, m_index, m_entries_current.size(), [this](bool touch, auto i) {
        if (touch && m_index == i) {
            FireAction(Button::A);
        } else {
            SetIndex(i);
        }
    });
}

void Menu::Draw(NVGcontext* vg, Theme* theme) {
    MenuBase::Draw(vg, theme);

    if (m_repo_download_state == ImageDownloadState::Failed) {
        gfx::drawTextArgs(vg, SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f, 28.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, theme->GetColour(ThemeEntryID_TEXT_INFO), "Failed to load repository"_i18n.c_str());
        return;
    }

    if (m_entries.empty()) {
        gfx::drawTextArgs(vg, SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f, 36.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, theme->GetColour(ThemeEntryID_TEXT_INFO), "Loading..."_i18n.c_str());
        return;
    }

    if (m_entries_current.empty()) {
        gfx::drawTextArgs(vg, SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f, 36.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, theme->GetColour(ThemeEntryID_TEXT_INFO), "Empty!"_i18n.c_str());
        return;
    }

    // max images per frame, in order to not hit io / gpu too hard.
    const int image_load_max = 2;
    int image_load_count = 0;

    m_list->Draw(vg, theme, m_entries_current.size(), [this, &image_load_count](auto* vg, auto* theme, auto v, auto pos) {
        const auto& [x, y, w, h] = v;
        const auto index = m_entries_current[pos];
        auto& e = m_entries[index];
        auto& image = e.image;

        // try and load cached image.
        if (image_load_count < image_load_max && !image.image && !image.tried_cache) {
            image.tried_cache = true;
            image.cached = EntryLoadImageFile(BuildIconCachePath(e), image);
            if (image.cached) {
                image_load_count++;
            }
        }

        // lazy load image
        if (!image.image || image.cached) {
            switch (image.state) {
                case ImageDownloadState::None: {
                    const auto path = BuildIconCachePath(e);
                    const auto url = BuildIconUrl(e);
                    image.state = ImageDownloadState::Progress;
                    curl::Api().ToFileAsync(
                        curl::Url{url},
                        curl::Path{path},
                        curl::Flags{curl::Flag_Cache},
                        curl::StopToken{this->GetToken()},
                        curl::OnComplete{[this, &image](auto& result) {
                            if (result.success) {
                                image.state = ImageDownloadState::Done;
                                // data hasn't changed
                                if (result.code == 304) {
                                    image.cached = false;
                                }
                            } else {
                                image.state = ImageDownloadState::Failed;
                                log_write("failed to download image\n");
                            }
                        }}
                    );
                }   break;
                case ImageDownloadState::Progress: {
                }   break;
                case ImageDownloadState::Done: {
                    if (image_load_count < image_load_max) {
                        image.cached = false;
                        if (!EntryLoadImageFile(BuildIconCachePath(e), e.image)) {
                            image.state = ImageDownloadState::Failed;
                        } else {
                            image_load_count++;
                        }
                    }
                }   break;
                case ImageDownloadState::Failed: {
                }   break;
            }
        }

        const auto selected = pos == m_index;
        const auto image_vec = DrawEntryNoImage(vg, theme, m_layout.Get(), v, selected, e.title.c_str(), e.author.c_str(), e.version.c_str());

        if (e.image.image) {
            const auto image_scale = 256.0 / image_vec.w;
            DrawIcon(vg, e.image, m_default_image, image_vec.x, image_vec.y, image_vec.w, image_vec.h, true, image_scale);
        } else {
            DrawCoverPlaceholder(vg, image_vec, e.title, e.hb_titleid, true);
        }

        float i_size = 22;
        switch (e.status) {
            case EntryStatus::Get:
                gfx::drawImage(vg, x + w - 30.f, y + 110, i_size, i_size, m_get.image, 20);
                break;
            case EntryStatus::Installed:
                gfx::drawImage(vg, x + w - 30.f, y + 110, i_size, i_size, m_installed.image, 20);
                break;
            case EntryStatus::Local:
                gfx::drawImage(vg, x + w - 30.f, y + 110, i_size, i_size, m_local.image, 20);
                break;
            case EntryStatus::Update:
                gfx::drawImage(vg, x + w - 30.f, y + 110, i_size, i_size, m_update.image, 20);
                break;
        }
    });
}

void Menu::OnFocusGained() {
    MenuBase::OnFocusGained();

    if (!m_default_image.image) {
        EntryLoadImageData(App::GetDefaultImageData(), m_default_image);
        EntryLoadImageData(UPDATE_IMAGE_DATA, m_update);
        EntryLoadImageData(GET_IMAGE_DATA, m_get);
        EntryLoadImageData(LOCAL_IMAGE_DATA, m_local);
        EntryLoadImageData(INSTALLED_IMAGE_DATA, m_installed);
    }

    if (m_entries.empty()) {
        if (m_repo_download_state == ImageDownloadState::Done) {
            ScanHomebrew();
        }
    } else {
        if (m_dirty) {
            m_dirty = false;
            const auto& current_entry = m_entries[m_entries_current[m_index]];
            Sort();

            for (u32 i = 0; i < m_entries_current.size(); i++) {
                if (current_entry.name == m_entries[m_entries_current[i]].name) {
                    const auto index = i;
                    const auto row = m_list->GetRow();
                    const auto page = m_list->GetPage();
                    if (index >= page) {
                        m_list->SetYoff((((index - page) + row) / row) * m_list->GetMaxY());
                    } else {
                        m_list->SetYoff(0);
                    }
                    SetIndex(i);
                    break;
                }
            }
        }
    }
}

void Menu::SetIndex(s64 index) {
    m_index = index;
    if (!m_index) {
        m_list->SetYoff(0);
    }

    this->SetSubHeading(std::to_string(m_index + 1) + " / " + std::to_string(m_entries_current.size()));
}

void Menu::ScanHomebrew() {
    App::SetBoostMode(true);
    ON_SCOPE_EXIT(App::SetBoostMode(false));

    m_entries.clear();
    from_json_hb(REPO_PATH, m_entries);

    fs::FsNativeSd fs;
    if (R_FAILED(fs.GetFsOpenResult())) {
        log_write("failed to open sd card in appstore scan\n");
        return;
    }

    for (auto& index : m_entries_index) {
        index.reserve(m_entries.size());
    }

    for (u32 i = 0; i < m_entries.size(); i++) {
        auto& e = m_entries[i];

        m_entries_index[Filter_All].push_back(i);

        if (e.category == std::string_view{"game"}) {
            m_entries_index[Filter_Games].push_back(i);
        } else if (e.category == std::string_view{"emu"}) {
            m_entries_index[Filter_Emulators].push_back(i);
        } else if (e.category == std::string_view{"tool"}) {
            m_entries_index[Filter_Tools].push_back(i);
        } else if (e.category == std::string_view{"advanced"}) {
            m_entries_index[Filter_Advanced].push_back(i);
        } else if (e.category == std::string_view{"theme"}) {
            m_entries_index[Filter_Themes].push_back(i);
        } else if (e.category == std::string_view{"legacy"}) {
            m_entries_index[Filter_Legacy].push_back(i);
        } else {
            m_entries_index[Filter_Misc].push_back(i);
        }

        // fwiw, this is how N stores update info
        e.updated_num = std::atoi(e.updated.c_str()); // day
        e.updated_num += std::atoi(e.updated.c_str() + 3) * 100; // month
        e.updated_num += std::atoi(e.updated.c_str() + 6) * 100 * 100; // year

        e.status = EntryStatus::Get;
        if (e.binary.empty() || e.binary == "none") {
            ReadFromInfoJson(e);
        } else {
            if (fs.FileExists(e.binary)) {
                ReadFromInfoJson(e);
                if (e.status == EntryStatus::Get) {
                    bool filtered{};
                    if (e.name == "hbmenu") {
                        NacpStruct nacp;
                        if (R_SUCCEEDED(nro_get_nacp(e.binary, nacp))) {
                            filtered = std::strcmp(nacp.lang[0].name, "nx-hbmenu");
                        }
                    }
                    else if (e.name == "snes9x_2010") {
                        filtered = true;
                    }
                    if (!filtered) {
                        e.status = EntryStatus::Local;
                    } else {
                        log_write("filtered: %s path: %s\n", e.name.c_str(), e.binary.c_str());
                    }
                }
            }
        }

        e.image.state = ImageDownloadState::None;
        e.image.image = 0; // images are lazy loaded
    }

    for (auto& index : m_entries_index) {
        index.shrink_to_fit();
    }

    SetFilter();
    SetIndex(0);
    Sort();
}

void Menu::Sort() {
    const auto sort = m_sort.Get();
    const auto order = m_order.Get();
    const auto filter = m_filter.Get();

    const auto sorter = [this, sort, order](EntryMini _lhs, EntryMini _rhs) -> bool {
        const auto& lhs = m_entries[_lhs];
        const auto& rhs = m_entries[_rhs];

        if (lhs.status == EntryStatus::Update && !(rhs.status == EntryStatus::Update)) {
            return true;
        } else if (!(lhs.status == EntryStatus::Update) && rhs.status == EntryStatus::Update) {
            return false;
        } else if (lhs.status == EntryStatus::Installed && !(rhs.status == EntryStatus::Installed)) {
            return true;
        } else if (!(lhs.status == EntryStatus::Installed) && rhs.status == EntryStatus::Installed) {
            return false;
        } else if (lhs.status == EntryStatus::Local && !(rhs.status == EntryStatus::Local)) {
            return true;
        } else if (!(lhs.status == EntryStatus::Local) && rhs.status == EntryStatus::Local) {
            return false;
        } else {
            switch (sort) {
                case SortType_Updated: {
                    if (lhs.updated_num == rhs.updated_num) {
                        return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
                    } else if (order == OrderType_Descending) {
                        return lhs.updated_num > rhs.updated_num;
                    } else {
                        return lhs.updated_num < rhs.updated_num;
                    }
                } break;
                case SortType_Downloads: {
                    if (lhs.app_dls == rhs.app_dls) {
                        return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
                    } else if (order == OrderType_Descending) {
                        return lhs.app_dls > rhs.app_dls;
                    } else {
                        return lhs.app_dls < rhs.app_dls;
                    }
                } break;
                case SortType_Size: {
                    if (lhs.extracted == rhs.extracted) {
                        return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
                    } else if (order == OrderType_Descending) {
                        return lhs.extracted > rhs.extracted;
                    } else {
                        return lhs.extracted < rhs.extracted;
                    }
                } break;
                case SortType_Alphabetical: {
                    if (order == OrderType_Descending) {
                        return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
                    } else {
                        return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) > 0;
                    }
                } break;
            }
        }
        return false;
    };

    char subheader[128]{};
    std::snprintf(subheader, sizeof(subheader), "Filter: %s | Sort: %s | Order: %s"_i18n.c_str(), i18n::get(FILTER_STR[filter]).c_str(), i18n::get(SORT_STR[sort]).c_str(), i18n::get(ORDER_STR[order]).c_str());
    SetTitleSubHeading(subheader);

    std::sort(m_entries_current.begin(), m_entries_current.end(), sorter);
}

void Menu::SortAndFindLastFile() {
    const auto name = GetEntry().name;
    Sort();
    SetIndex(0);

    s64 index = -1;
    for (u64 i = 0; i < m_entries_current.size(); i++) {
        if (name == GetEntry(i).name) {
            index = i;
            break;
        }
    }

    if (index >= 0) {
        const auto row = m_list->GetRow();
        const auto page = m_list->GetPage();
        if (index >= page) {
            m_list->SetYoff((((index - page) + row) / row) * m_list->GetMaxY());
        } else {
            m_list->SetYoff(0);
        }
        SetIndex(index);
    }
}

void Menu::SetFilter() {
    m_is_search = false;
    m_is_author = false;

    m_entries_current = m_entries_index[m_filter.Get()];
    SetIndex(0);
    Sort();
}

void Menu::SetSearch(const std::string& term) {
    if (!m_is_search) {
        m_entry_search_jump_back = m_index;
    }

    m_search_term = term;
    m_entries_index_search.clear();
    const auto query = m_search_term;

    for (u64 i = 0; i < m_entries.size(); i++) {
        const auto& e = m_entries[i];
        if (FindCaseInsensitive(e.title, query) || FindCaseInsensitive(e.author, query) || FindCaseInsensitive(e.description, query)) {
            m_entries_index_search.emplace_back(i);
        }
    }

    m_is_search = true;
    m_entries_current = m_entries_index_search;
    SetIndex(0);
    Sort();
}

void Menu::SetAuthor() {
    if (!m_is_author) {
        m_entry_author_jump_back = m_index;
    }

    m_author_term = m_entries[m_entries_current[m_index]].author;
    m_entries_index_author.clear();
    const auto query = m_author_term;

    for (u64 i = 0; i < m_entries.size(); i++) {
        const auto& e = m_entries[i];
        if (FindCaseInsensitive(e.author, query)) {
            m_entries_index_author.emplace_back(i);
        }
    }

    m_is_author = true;
    m_entries_current = m_entries_index_author;
    SetIndex(0);
    Sort();
}

void Menu::OnLayoutChange() {
    m_index = 0;
    grid::Menu::OnLayoutChange(m_list, m_layout.Get());
}

LazyImage::~LazyImage() {
    if (image) {
        nvgDeleteImage(App::GetVg(), image);
    }
}

} // namespace npshop::ui::menu::appstore
