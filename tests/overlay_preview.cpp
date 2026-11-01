#include "overlay_preview.hpp"
#include "overlay_window.hpp"
#include "trainer_state.hpp"
#include "config_store.hpp"
#include "game_layout.hpp"

#include <algorithm>
#include <cstring>

namespace azs {

void preview_host::initialize_data() {
    std::memset(data_, 0, 0x2000);
    for (std::size_t index = 0; index < trainer_state::field_count(); ++index) {
        const auto& descriptor = trainer_state::field_at(index);
        if (descriptor.access == field_access::scalar && descriptor.kind == value_kind::real) {
            const auto value = static_cast<float>(std::max(1.0, descriptor.minimum));
            std::memcpy(data_ + descriptor.offset, &value, sizeof(value));
        }
    }
    *reinterpret_cast<std::uint64_t*>(data_ + game_layout::run_offset(0x4b0)) = 101;
    *reinterpret_cast<std::uint8_t*>(data_ + game_layout::run_offset(0x354)) = 11;
    *reinterpret_cast<std::uint32_t*>(data_ + game_layout::run_offset(0x530)) = 30;
    *reinterpret_cast<std::uint8_t*>(data_ + game_layout::run_offset(0xda3)) = 1;
    *reinterpret_cast<float*>(data_ + game_layout::run_offset(0x4c8)) = 250;
    *reinterpret_cast<float*>(data_ + game_layout::run_offset(0x538)) = 250;
    *reinterpret_cast<float*>(data_ + game_layout::run_offset(0x4cc)) = 1.5f;
    *reinterpret_cast<float*>(data_ + game_layout::run_offset(0x4e0)) = 15;
    *reinterpret_cast<float*>(data_ + game_layout::run_offset(0x4f4)) = 0.2f;
    *reinterpret_cast<std::uint32_t*>(data_ + game_layout::run_offset(0x544)) = 12345;
    *reinterpret_cast<std::uint32_t*>(data_ + game_layout::run_offset(0xd40)) = 25;
    *reinterpret_cast<std::uint32_t*>(data_ + game_layout::run_offset(0x59c)) = 3;
    trainer_state::instance().set_hooks_ready(true);
    trainer_state::instance().set_message("Тестовое окно: изменяется только синтетический буфер, игра не подключена.");
    trainer_state::instance().process_run(data_, true);
    update_data();
}

void preview_host::update_data() {
    trainer_state& state = trainer_state::instance();
    state.process_run(data_, false);
    float lifesteal = 0.25f;
    state.try_lifesteal_override(data_, lifesteal);
    state.observe_lifesteal(data_, lifesteal);
    state.process_run(data_, true);
}

LRESULT CALLBACK preview_host::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_TIMER) {
        update_data();
        if (GetTickCount64() - started_ms_ > 180000) {
            DestroyWindow(window);
        }
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        const auto context = BeginPaint(window, &paint);
        SetBkMode(context, TRANSPARENT);
        SetTextColor(context, RGB(210, 220, 235));
        constexpr auto title = L"Azeroth Survivors overlay — isolated preview";
        constexpr auto detail = L"No game process is attached. Insert toggles the menu; End releases all freezes.";
        TextOutW(context, 24, 24, title, static_cast<int>(wcslen(title)));
        TextOutW(context, 24, 55, detail, static_cast<int>(wcslen(detail)));
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int preview_host::run(HINSTANCE instance) {
    SetProcessDPIAware();
    game_layout::select(layout_id::current);
    data_ = static_cast<std::byte*>(VirtualAlloc(nullptr, 0x2000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!data_) {
        return 1;
    }
    initialize_data();
    config_store::initialize(instance);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = window_proc;
    window_class.lpszClassName = L"azs_overlay_preview_host";
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(RGB(25, 37, 52));
    if (!RegisterClassExW(&window_class)) {
        return 2;
    }
    const auto window = CreateWindowExW(0, window_class.lpszClassName, L"Azeroth overlay preview",
        WS_OVERLAPPEDWINDOW, 60, 60, 1280, 900, nullptr, nullptr, instance, nullptr);
    if (!window) {
        return 3;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    started_ms_ = GetTickCount64();
    SetTimer(window, 1, 16, nullptr);
    const auto overlay_thread = CreateThread(nullptr, 0, overlay_window::thread_main, instance, 0, nullptr);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    trainer_state::instance().set_hooks_ready(false);
    if (overlay_thread) {
        CloseHandle(overlay_thread);
    }
    DeleteObject(window_class.hbrBackground);
    UnregisterClassW(window_class.lpszClassName, instance);
    VirtualFree(data_, 0, MEM_RELEASE);
    return 0;
}

} // namespace azs

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    return azs::preview_host::run(instance);
}
