#pragma once
#include <string>

namespace npshop::deviceauth {

    struct ApiConfig {
        const char* base_url = "https://npshop.org";
        const char* device_code_endpoint = "/api/auth/device/start";
        const char* poll_endpoint = "/api/auth/device/poll";
    };

    // Lê/salva do INI
    bool IsActivated();
    void SetActivated(const char* token); // opcionalmente grava token

    // Configura os endpoints (opcional; se não chamar, usa defaults acima)
    void SetApiConfig(const ApiConfig& cfg);

    // Mostra popup (QR/URL/código) e inicia ProgressBox com polling
    void StartAuthorizationUI();

} // namespace npshop::deviceauth
