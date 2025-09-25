#include "device_auth.hpp"

#include <cstring>
#include <strings.h>   // strcasecmp
#include <vector>
#include <string>
#include <algorithm>

#include <yyjson.h>            // JSON
#include <switch.h>            // svcSleepThread, ticks
#include "defines.hpp"         // Result_* codes, R_SUCCEED etc
#include "app.hpp"             // npshop::App
#include "download.hpp"        // npshop::curl::Api
#include "pulsar.h"            // (mantido; usamos shim local com nomes iguais e static)
#include "owo.hpp"             // (i18n e utilidades comuns da base)
#include "ui/option_box.hpp"   // npshop::ui::OptionBox
#include "ui/progress_box.hpp" // npshop::ui::ProgressBox
#include "log.hpp"
#include "i18n.hpp"            // para "_i18n" se necessário

// --- INI shim: ini_gets / ini_puts -----------------------------------------
// Formato simples: linhas "key=value" dentro de [section]. Ignora espaços.
// - ini_gets: retorna número de bytes copiados em 'out'. Se não achar, copia 'def'.
// - ini_puts: cria/atualiza key no section; cria seção se não existir. Retorna 1 ok, 0 erro.

namespace {

    // helpers
    static inline std::string ltrim(std::string s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {return !std::isspace(c); })); return s;
    }
    static inline std::string rtrim(std::string s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {return !std::isspace(c); }).base(), s.end()); return s;
    }
    static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }
    static inline bool iequals(const std::string& a, const std::string& b) { return ::strcasecmp(a.c_str(), b.c_str()) == 0; }

    // carrega arquivo inteiro para string
    static bool read_all_text(const char* path, std::string& out) {
        fs::FsNativeSd fs;
        fs::File f{};
        if (R_FAILED(fs.OpenFile(path, FsOpenMode_Read, &f))) {
            out.clear(); // não existe ainda
            return true;
        }
        s64 size = 0;
        if (R_FAILED(f.GetSize(&size)) || size < 0) { f.Close(); return false; }
        out.resize((size_t)size);
        u64 br = 0;
        Result rc = f.Read(0, out.data(), (size_t)size, FsReadOption_None, &br);
        f.Close();
        if (R_FAILED(rc)) return false;
        if (br < out.size()) out.resize((size_t)br);
        return true;
    }

    // grava texto (sobrescreve). Cria pasta se preciso.
    static bool write_all_text(const char* path, const std::string& content) {
        fs::FsNativeSd fs;
        fs.CreateDirectoryRecursivelyWithPath(path);
        fs.DeleteFile(path); // ok se não existir
        if (R_FAILED(fs.CreateFile(path, 0, 0))) {
            // pode ser que já exista; vamos tentar abrir direto
        }
        fs::File f{};
        if (R_FAILED(fs.OpenFile(path, FsOpenMode_Write | FsOpenMode_Append, &f))) {
            return false;
        }
        Result rc = f.Write(0, content.data(), content.size(), FsWriteOption_None);
        f.Close();
        return R_SUCCEEDED(rc);
    }

    // procura seção e key; retorna índice da linha da key (ou -1). Se seção não existir, retorna -1 e
    // em 'section_start' devolve o índice onde deveríamos inserir a nova seção no fim do arquivo.
    static int find_key_line(const std::vector<std::string>& lines,
        const std::string& section,
        const std::string& key,
        int& section_start,
        int& section_end)
    {
        section_start = -1; section_end = (int)lines.size();
        bool in = false;
        for (int i = 0; i < (int)lines.size(); ++i) {
            auto ln = trim(lines[i]);
            if (ln.empty() || ln[0] == ';' || ln[0] == '#') continue;
            if (ln.front() == '[' && ln.back() == ']') {
                std::string sec = trim(ln.substr(1, ln.size() - 2));
                if (iequals(sec, section)) {
                    in = true; section_start = i; section_end = (int)lines.size();
                } else {
                    if (in) { section_end = i; break; }
                    in = false;
                }
                continue;
            }
            if (in) {
                auto pos = ln.find('=');
                if (pos != std::string::npos) {
                    auto k = rtrim(ln.substr(0, pos));
                    if (iequals(k, key)) return i;
                }
            }
        }
        return -1;
    }

} // anon ns

// Assinaturas iguais às usadas no projeto original:
static int ini_gets(const char* section, const char* key, const char* defval,
    char* out, size_t outsz, const char* filepath)
{
    std::string content;
    if (!read_all_text(filepath, content)) {
        // erro de leitura => devolve def
        std::snprintf(out, outsz, "%s", defval ? defval : "");
        return (int)std::strlen(out);
    }

    // quebra em linhas
    std::vector<std::string> lines;
    {
        size_t start = 0;
        while (start <= content.size()) {
            size_t end = content.find('\n', start);
            if (end == std::string::npos) end = content.size();
            std::string line = content.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
            start = end + 1;
        }
    }

    int sec_begin = -1, sec_end = -1;
    int key_line = find_key_line(lines, section ? section : "", key ? key : "", sec_begin, sec_end);

    std::string value;
    if (key_line >= 0) {
        auto ln = lines[key_line];
        auto pos = ln.find('=');
        value = (pos == std::string::npos) ? "" : trim(ln.substr(pos + 1));
    } else {
        value = defval ? defval : "";
    }

    std::snprintf(out, outsz, "%s", value.c_str());
    return (int)std::strlen(out);
}

static int ini_puts(const char* section, const char* key, const char* value, const char* filepath)
{
    if (!section || !*section || !key || !*key || !value) return 0;

    std::string content;
    if (!read_all_text(filepath, content)) return 0;

    // quebra em linhas
    std::vector<std::string> lines;
    {
        size_t start = 0;
        while (start <= content.size()) {
            size_t end = content.find('\n', start);
            if (end == std::string::npos) end = content.size();
            std::string line = content.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
            start = end + 1;
        }
        if (lines.empty()) lines.emplace_back("");
    }

    int sec_begin = -1, sec_end = -1;
    int key_line = find_key_line(lines, section, key, sec_begin, sec_end);

    const std::string newline = std::string(key) + "=" + value;

    if (key_line >= 0) {
        // atualiza valor
        lines[key_line] = newline;
    } else {
        // inserir nova key
        if (sec_begin >= 0) {
            // dentro da seção existente (antes do próximo [sec] ou fim)
            int insert_at = (sec_end >= 0) ? sec_end : (int)lines.size();
            lines.insert(lines.begin() + insert_at, newline);
        } else {
            // cria seção no fim do arquivo
            if (!lines.empty() && !trim(lines.back()).empty()) lines.emplace_back("");
            lines.emplace_back(std::string("[") + section + "]");
            lines.emplace_back(newline);
        }
    }

    // re-monta conteúdo
    std::string out;
    out.reserve(content.size() + newline.size() + 32);
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 != lines.size()) out += "\n";
    }

    return write_all_text(filepath, out) ? 1 : 0;
}
// ---------------------------------------------------------------------------

namespace npshop::deviceauth {

    // ====== INI helpers ======
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

    static std::string GetOrCreateDeviceId() {
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

    static bool IniGetBool(const char* sec, const char* key, bool def = false) {
        char buf[8]{};
        ini_gets(sec, key, def ? "1" : "0", buf, sizeof(buf), ::npshop::App::CONFIG_PATH);
        return std::strcmp(buf, "1") == 0 || strcasecmp(buf, "true") == 0;
    }
    static void IniPutBool(const char* sec, const char* key, bool v) {
        ini_puts(sec, key, v ? "1" : "0", ::npshop::App::CONFIG_PATH);
    }

    static ApiConfig g_cfg{}; // defaults

    bool IsActivated() {
        return IniGetBool(INI_SEC_AUTH, INI_KEY_ACTIV, false);
    }

    void SetActivated(const char* token) {
        IniPutBool(INI_SEC_AUTH, INI_KEY_ACTIV, true);
        if (token && *token) ini_puts(INI_SEC_AUTH, INI_KEY_TOKEN, token, ::npshop::App::CONFIG_PATH);
        ::npshop::App::Notify("Device activated");
    }

    void SetApiConfig(const ApiConfig& cfg) {
        g_cfg = cfg;
    }

    // ====== HTTP helper (via teu wrapper curl) ======
    static bool PostJson(const std::string& url,
                         const std::string& body,
                         std::string& out,
                         long& code)
    {
        using namespace npshop::curl;
        Header hdr{
            { "Content-Type", "application/json" },
            { "Accept",       "application/json" }
        };

        auto res = Api().ToMemory(
            Url{ url },
            Fields{ body },
            std::move(hdr),
            Flags{ Flag_AllowErrorBody } // <<<<<< permite corpo em 4xx
        );

        code = res.code;
        out.clear();
        if (!res.data.empty()) {
            out.assign(reinterpret_cast<const char*>(res.data.data()), res.data.size());
        }

        log_write("[auth] POST %s -> HTTP %ld, body-len=%zu\n", url.c_str(), code, out.size());
        if (code >= 400) {
            log_write("[auth] RESP (err): %.*s\n", (int)std::min<size_t>(out.size(), 512), out.c_str());
        }

        return res.success && code >= 200 && code < 300;
    }

    // ====== Fluxo Device Code ======
    struct DeviceCodeResp {
        std::string device_code;
        std::string user_code;
        std::string verification_uri;
        std::string qrcode_url; // opcional
        int interval{ 5 };
        int expires_in{ 600 };
    };

    // Popup gráfico com QR e instruções
    struct QrCodeBox final : ::npshop::ui::Widget {
        struct Callbacks {
            std::function<void()> on_confirm; // A
            std::function<void()> on_cancel;  // B
        };

        QrCodeBox(const fs::FsPath& imgPath,
                  std::string title,
                  std::string subtitle,
                  Callbacks cbs)
        : m_imgPath{ imgPath }
        , m_title(std::move(title))
        , m_sub(std::move(subtitle))
        , m_cbs(std::move(cbs))
        {
            this->SetActions(
                std::make_pair(Button::B, ::npshop::Action{"Back"_i18n, [this](){
                    this->SetPop();
                    if (m_cbs.on_cancel) m_cbs.on_cancel();
                }}),
                std::make_pair(Button::A, ::npshop::Action{"Continue"_i18n, [this](){
                    if (m_cbs.on_confirm) m_cbs.on_confirm();
                    // this->SetPop(); // se quiser fechar antes de abrir o ProgressBox
                }})
            );
        }

        ~QrCodeBox() override {
            if (m_img > 0) {
                nvgDeleteImage(::npshop::App::GetVg(), m_img);
                m_img = 0;
            }
        }

        void OnFocusGained() override {}

        // Assinatura correta (Controller/TouchInfo em ::npshop)
        void Update(::npshop::Controller* controller, ::npshop::TouchInfo* touch) override {
            ::npshop::ui::Widget::Update(controller, touch);
            // Nada aqui: B/A são tratados por SetActions acima.
        }

        // Assinatura correta (Theme em ::npshop)
        void Draw(NVGcontext* vg, ::npshop::Theme* /*theme*/) override {
            constexpr float W = 1280.0f, H = 720.0f;
            // fundo escuro
            nvgBeginPath(vg); nvgRect(vg, 0, 0, W, H);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 160)); nvgFill(vg);

            // painel central
            const float bw = 900.0f, bh = 640.0f;
            const float x = (W - bw) * 0.5f, y = (H - bh) * 0.5f;
            nvgBeginPath(vg); nvgRoundedRect(vg, x, y, bw, bh, 18.0f);
            nvgFillColor(vg, nvgRGBA(30, 30, 33, 230)); nvgFill(vg);

            // título
            nvgFontSize(vg, 32.0f);
            nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
            nvgText(vg, x + bw * 0.5f, y + 44.0f, m_title.c_str(), nullptr);

            // carrega imagem uma única vez
            if (!m_img) {
                m_img = nvgCreateImage(vg, m_imgPath.s, 0);
                if (m_img <= 0) {
                    log_write("[auth] failed to create NVG image for QR: %s\n", m_imgPath.s);
                }
            }

            // área do QR
            const float imgBox = 440.0f;
            const float ix = x + (bw - imgBox) * 0.5f;
            const float iy = y + 88.0f;

            if (m_img > 0) {
                // moldura branca
                nvgBeginPath(vg); nvgRect(vg, ix - 8, iy - 8, imgBox + 16, imgBox + 16);
                nvgFillColor(vg, nvgRGBA(255, 255, 255, 255)); nvgFill(vg);

                nvgBeginPath(vg); nvgRect(vg, ix, iy, imgBox, imgBox);
                NVGpaint p = nvgImagePattern(vg, ix, iy, imgBox, imgBox, 0.0f, m_img, 1.0f);
                nvgFillPaint(vg, p); nvgFill(vg);
            } else {
                // fallback sem imagem
                nvgFontSize(vg, 22.0f);
                nvgFillColor(vg, nvgRGBA(255, 220, 120, 255));
                nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_CENTER);
                nvgTextBox(vg, x + 40, iy, bw - 80, "QR image not available", nullptr);
            }

            // subtítulo (URI + código)
            nvgFontSize(vg, 22.0f);
            nvgFillColor(vg, nvgRGBA(220, 220, 220, 255));
            nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_CENTER);
            nvgTextBox(vg, x + 40, iy + imgBox + 24.0f, bw - 80, m_sub.c_str(), nullptr);

            // rodapé: dicas de botões
            nvgFontSize(vg, 20.0f);
            nvgFillColor(vg, nvgRGBA(200, 200, 200, 200));
            nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
            nvgText(vg, x + 24, y + bh - 22, "(A) Continue", nullptr);
            nvgTextAlign(vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_RIGHT);
            nvgText(vg, x + bw - 24, y + bh - 22, "(B) Cancel", nullptr);
        }

    private:
        fs::FsPath m_imgPath{};
        std::string m_title;
        std::string m_sub;
        Callbacks m_cbs{};
        int m_img{ 0 }; // handle NVG da imagem
    };

    static bool StartDeviceCode(DeviceCodeResp& out)
    {
        const std::string deviceId = GetOrCreateDeviceId();

        // seu backend exige: { device_id, serial? }
        std::string body = "{\"device_id\":\"" + deviceId + "\"}";
        const std::string url = std::string(g_cfg.base_url) + g_cfg.device_code_endpoint;

        log_write("[auth] start URL: %s\n", url.c_str());
        log_write("[auth] start BODY: %s\n", body.c_str());

        std::string resp; long code = 0;
        bool ok = PostJson(url, body, resp, code);
        log_write("[auth] start HTTP: %ld ok=%d\n", code, ok);
        if (!ok) {
            if (!resp.empty())
                log_write("[auth] start RESP: %.*s\n", (int)std::min<size_t>(resp.size(), 512), resp.c_str());
            return false;
        }

        yyjson_doc* doc = yyjson_read(resp.c_str(), resp.size(), 0);
        if (!doc) return false;
        yyjson_val* root = yyjson_doc_get_root(doc);
        if (!root) { yyjson_doc_free(doc); return false; }

        auto getstr = [&](const char* k)->const char* {
            yyjson_val* v = yyjson_obj_get(root, k);
            return v ? yyjson_get_str(v) : nullptr;
        };
        auto getint = [&](const char* k, int d)->int {
            yyjson_val* v = yyjson_obj_get(root, k);
            return v ? (int)yyjson_get_int(v) : d;
        };

        if (const char* s = getstr("device_code"))       out.device_code = s;
        if (const char* s = getstr("user_code"))         out.user_code = s;
        if (const char* s = getstr("verification_uri"))  out.verification_uri = s;
        if (const char* s = getstr("qr_url"))            out.qrcode_url = s;   // <- nome certo
        out.interval = getint("interval", 5);
        out.expires_in = 600; // opcional; seu backend não manda, manter um teto interno

        yyjson_doc_free(doc);
        const bool valid = !out.device_code.empty() && !out.verification_uri.empty();
        if (!valid) log_write("[auth] start parse invalid (missing fields)\n");
        return valid;
    }

    static Result PollUntilActivated(::npshop::ui::ProgressBox* pbox, const DeviceCodeResp& dc)
    {
        const std::string deviceId = GetOrCreateDeviceId();
        const u64 tick_hz = armGetSystemTickFreq();
        const u64 startTick = armGetSystemTick();

        auto makeBody = [&](const std::string& code) {
            // backend exige device_code + device_id
            std::string b = "{\"device_code\":\"";
            b += code;
            b += "\",\"device_id\":\"";
            b += deviceId;
            b += "\"}";
            return b;
        };

        const std::string url = std::string(g_cfg.base_url) + g_cfg.poll_endpoint;
        int interval = std::max(1, dc.interval);

        while (!pbox->ShouldExit()) {
            pbox->NewTransfer("Waiting for authorization...");

            std::string resp; long code = 0;
            bool ok = PostJson(url, makeBody(dc.device_code), resp, code);
            log_write("[auth] poll HTTP: %ld ok=%d\n", code, ok);

            if (code == 200) {
                yyjson_doc* doc = yyjson_read(resp.c_str(), resp.size(), 0);
                if (doc) {
                    yyjson_val* root = yyjson_doc_get_root(doc);
                    if (root) {
                        // approved?
                        if (yyjson_is_true(yyjson_obj_get(root, "approved"))) {
                            SetActivated(""); // se tiver token depois, salve aqui
                            yyjson_doc_free(doc);
                            R_SUCCEED();
                        }
                        // authorization_pending? atualiza intervalo, se enviado
                        yyjson_val* iv = yyjson_obj_get(root, "interval");
                        if (iv) {
                            interval = std::max<int>(1, (int)yyjson_get_int(iv));
                        }
                    }
                    yyjson_doc_free(doc);
                }
            } else if (code == 400) {
                // erros: invalid_device_code, invalid_device
                log_write("[auth] poll 400: %.*s\n", (int)std::min<size_t>(resp.size(), 256), resp.c_str());
                return Result_MainFailedToDownloadUpdate;
            } else if (!ok) {
                // outros erros transitórios (rede, 5xx, etc.) — opcional: continue tentando
                log_write("[auth] poll transient err, keep waiting\n");
            }

            // espera 'interval' segundos (ou cancelamento)
            for (int i = 0; i < interval && !pbox->ShouldExit(); ++i) {
                svcSleepThread(1000 * 1000 * 1000LL);
            }

            const u64 elapsed = (armGetSystemTick() - startTick) / tick_hz;
            if ((int)elapsed >= dc.expires_in) {
                return Result_MainFailedToDownloadUpdate;
            }
        }

        return Result_UsbCancelled;
    }

    void StartAuthorizationUI()
    {
        if (IsActivated()) {
            ::npshop::App::Notify("Already activated");
            return;
        }

        DeviceCodeResp dc{};
        if (!StartDeviceCode(dc)) {
            ::npshop::App::PushErrorBox(Result_MainFailedToDownloadUpdate, "Failed to start device authorization");
            return;
        }

        // Monta subtítulo (URI + user_code)
        std::string subtitle = std::string("Visit: ") + dc.verification_uri;
        if (!dc.user_code.empty()) {
            subtitle += "\nCode: ";
            subtitle += dc.user_code;
        }

        // Inicia polling (usado pelo botão A)
        auto start_poll = [dc]() {
            ::npshop::App::Push<::npshop::ui::ProgressBox>(
                0,
                "Authorize",
                "Waiting for confirmation on your phone",
                [dc](auto pbox)->Result {
                    return PollUntilActivated(pbox, dc);
                },
                [](Result rc) {
                    if (R_SUCCEEDED(rc)) {
                        ::npshop::App::Notify("Device authorized!");
                    } else if (rc != Result_UsbCancelled) {
                        ::npshop::App::PushErrorBox(rc, "Authorization failed");
                    }
                }
            );
        };

        // Tenta baixar o QR (usar o endpoint PNG do backend)
        bool haveQr = false;
        fs::FsPath qrPath{ "/switch/npshop/cache/device_qr.png" };

        {
            fs::FsNativeSd fs;
            fs.CreateDirectoryRecursivelyWithPath("/switch/npshop/cache/"); // garante pasta
        }

        std::string qr_png_url;
        if (!dc.user_code.empty()) {
            // usa o endpoint dedicado que retorna PNG:
            //   /api/auth/device/qrcode/{user_code}
            // (se quiser URL-encode: npshop::curl::EscapeString(dc.user_code))
            qr_png_url = std::string(g_cfg.base_url) + "/api/auth/device/qrcode/" + dc.user_code;
        } else if (!dc.qrcode_url.empty()) {
            // fallback: se vier só a página HTML no start, tenta mesmo assim
            qr_png_url = dc.qrcode_url;
        }

        if (!qr_png_url.empty()) {
            auto resDl = ::npshop::curl::Api().ToFile(
                ::npshop::curl::Url{ qr_png_url },
                ::npshop::curl::Path{ qrPath }
            );
            haveQr = resDl.success && resDl.code >= 200 && resDl.code < 300;
            log_write("[auth] QR download: success=%d code=%ld url=%s path=%s\n",
                      (int)haveQr, resDl.code, qr_png_url.c_str(), qrPath.s);
        }

        if (haveQr) {
            QrCodeBox::Callbacks cbs{};
            cbs.on_confirm = start_poll;
            cbs.on_cancel  = [](){};
            ::npshop::App::Push<QrCodeBox>(qrPath, "Authorize this Switch", subtitle, cbs);
        } else {
            ::npshop::App::Push<::npshop::ui::OptionBox>(
                "Authorize this Switch",
                "Cancel", "Continue", 1,
                [start_poll](auto op_index) {
                    if (!op_index || !*op_index) return;
                    start_poll();
                }
            );
            ::npshop::App::Notify(subtitle.c_str());
        }
    }
} // namespace npshop::deviceauth
