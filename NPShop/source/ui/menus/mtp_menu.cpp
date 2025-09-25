#if ENABLE_NETWORK_INSTALL

#include "ui/menus/mtp_menu.hpp"
#include "usb/usbds.hpp"
#include "app.hpp"
#include "defines.hpp"
#include "log.hpp"
#include "ui/nvg_util.hpp"
#include "i18n.hpp"
#include "haze_helper.hpp"

namespace npshop::ui::menu::mtp {

Menu::Menu(u32 flags) : stream::Menu{"MTP Install"_i18n, flags} {
    m_was_mtp_enabled = App::GetMtpEnable();
    if (!m_was_mtp_enabled) {
        log_write("[MTP] wasn't enabled, forcefully enabling\n");
        App::SetMtpEnable(true);
    }

    haze::InitInstallMode(
        [this](const char* path){ return OnInstallStart(path); },
        [this](const void *buf, size_t size){ return OnInstallWrite(buf, size); },
        [this](){ return OnInstallClose(); }
    );
}

Menu::~Menu() {
    // signal for thread to exit and wait.
    haze::DisableInstallMode();

    if (!m_was_mtp_enabled) {
        log_write("[MTP] disabling on exit\n");
        App::SetMtpEnable(false);
    }
}

void Menu::Update(Controller* controller, TouchInfo* touch) {
    stream::Menu::Update(controller, touch);

    static TimeStamp poll_ts;
    if (poll_ts.GetSeconds() >= 1) {
        poll_ts.Update();

        UsbState state{UsbState_Detached};
        usbDsGetState(&state);

        UsbDeviceSpeed speed{(UsbDeviceSpeed)UsbDeviceSpeed_None};
        usbDsGetSpeed(&speed);

        char buf[128];
        std::snprintf(buf, sizeof(buf), "State: %s | Speed: %s", i18n::get(GetUsbDsStateStr(state)).c_str(), i18n::get(GetUsbDsSpeedStr(speed)).c_str());
        SetSubHeading(buf);
    }
}

void Menu::OnDisableInstallMode() {
    haze::DisableInstallMode();
}

} // namespace npshop::ui::menu::mtp

#endif
