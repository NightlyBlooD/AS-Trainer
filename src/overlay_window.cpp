#include "overlay_window.hpp"
#include "config_store.hpp"
#include "trainer_state.hpp"
#include "localization.hpp"
#include "ui_strings.hpp"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <string_view>
#include <utility>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM word_parameter, LPARAM long_parameter);

namespace azs {

ImFontGlyphRangesBuilder* overlay_window::font_builder_ = nullptr;

DWORD WINAPI overlay_window::thread_main(void* context) noexcept {
    (void)context;
    localization::refresh();
    overlay_window overlay;
    DWORD exit_code = 0;
    try {
        if (!overlay.initialize()) {
            trainer_state::instance().set_message(ui_strings::get(ui_text_id::overlay_create_failed));
            overlay.cleanup();
            return 1;
        }

        bool finished = false;
        while (!finished) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    finished = true;
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (finished) {
                break;
            }
            overlay.handle_hotkeys();
            if (!overlay.sync_target_window()) {
                Sleep(16);
                continue;
            }
            if (!overlay.render_frame()) {
                trainer_state::instance().set_message(ui_strings::get(ui_text_id::graphics_failed));
                exit_code = 1;
                break;
            }
        }
    } catch (...) {
        trainer_state::instance().set_message(ui_strings::get(ui_text_id::overlay_stopped));
        exit_code = 1;
    }
    trainer_state::instance().reset_all();
    overlay.cleanup();
    return exit_code;
}

bool overlay_window::initialize() noexcept {
    instance_ = GetModuleHandleW(nullptr);
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_CLASSDC;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance_;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.lpszClassName = L"AzsTrainerOverlayWindow";
    if (!RegisterClassExW(&window_class)) {
        return false;
    }
    registered_class_ = true;
    window_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        window_class.lpszClassName, L"Azeroth Survivors Trainer", WS_POPUP,
        0, 0, 1280, 720, nullptr, nullptr, instance_, this);
    if (!window_ || !SetLayeredWindowAttributes(window_, RGB(0, 0, 0), 255, LWA_COLORKEY)) {
        return false;
    }
    if (!create_device()) {
        return false;
    }

    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        return false;
    }
    imgui_context_ready_ = true;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    if (!load_fonts()) {
        return false;
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 5.0f;
    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.CellPadding = ImVec2(7.0f, 5.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.065f, 0.075f, 0.095f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.13f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.20f, 0.29f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.15f, 0.20f, 1.0f);

    win32_backend_ready_ = ImGui_ImplWin32_Init(window_);
    if (!win32_backend_ready_) {
        return false;
    }
    dx11_backend_ready_ = ImGui_ImplDX11_Init(device_, device_context_);
    previous_insert_ = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    previous_end_ = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    return dx11_backend_ready_;
}

bool overlay_window::load_fonts() noexcept {
    std::array<wchar_t, MAX_PATH> windows_directory{};
    std::array<wchar_t, 1024> font_path{};
    const UINT directory_length = GetWindowsDirectoryW(windows_directory.data(),
                                                        static_cast<UINT>(windows_directory.size()));
    bool base_loaded = false;
    if (directory_length && directory_length < windows_directory.size()) {
        for (const wchar_t* filename : {L"segoeui.ttf", L"arial.ttf"}) {
            if (std::swprintf(font_path.data(), font_path.size(), L"%ls\\Fonts\\%ls",
                              windows_directory.data(), filename) > 0
                && add_font_file(font_path.data(), false)) {
                base_loaded = true;
                break;
            }
        }
    }
    if (!base_loaded) {
        base_loaded = ImGui::GetIO().Fonts->AddFontDefault() != nullptr;
    }

    HMODULE current_module{};
    std::array<wchar_t, 1024> module_path{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&thread_main), &current_module)) {
        const DWORD path_length = GetModuleFileNameW(current_module, module_path.data(),
                                                    static_cast<DWORD>(module_path.size()));
        if (path_length && path_length < module_path.size()) {
            if (wchar_t* separator = std::wcsrchr(module_path.data(), L'\\')) {
                separator[1] = L'\0';
                for (const wchar_t* filename : {L"NotoSansCJKsc-Regular.otf",
                                               L"NotoSansSC-Regular.ttf",
                                               L"NotoSansSC-VariableFont_wght.ttf"}) {
                    if (std::swprintf(font_path.data(), font_path.size(), L"%lsfonts\\%ls",
                                      module_path.data(), filename) > 0
                        && add_font_file(font_path.data(), true)) {
                        return base_loaded;
                    }
                }
            }
        }
    }
    if (directory_length && directory_length < windows_directory.size()) {
        for (const wchar_t* filename : {L"msyh.ttc", L"simsun.ttc"}) {
            if (std::swprintf(font_path.data(), font_path.size(), L"%ls\\Fonts\\%ls",
                              windows_directory.data(), filename) > 0
                && add_font_file(font_path.data(), true)) {
                return base_loaded;
            }
        }
    }
    return false;
}

bool overlay_window::add_font_file(const wchar_t* path, bool merge) noexcept {
    const DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }
    std::array<char, 4096> utf8_path{};
    if (!WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_path.data(),
                             static_cast<int>(utf8_path.size()), nullptr, nullptr)) {
        return false;
    }
    static constexpr ImWchar base_ranges[] = {
        0x0020, 0x024f, 0x0400, 0x052f, 0x2000, 0x206f, 0
    };
    static ImVector<ImWchar> merged_ranges;
    if (merge && merged_ranges.empty()) {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesChineseSimplifiedCommon());
        for (std::size_t index = 0; index < static_cast<std::size_t>(ui_text_id::count); ++index) {
            for (std::uint32_t language = 0; language < 7; ++language) {
                builder.AddText(ui_strings::get(static_cast<ui_text_id>(index),
                                                static_cast<client_language>(language)));
            }
        }
        font_builder_ = &builder;
        localization::visit_translations(collect_font_text);
        font_builder_ = nullptr;
        builder.BuildRanges(&merged_ranges);
    }
    ImFontConfig configuration;
    configuration.MergeMode = merge;
    configuration.PixelSnapH = true;
    configuration.OversampleH = 1;
    configuration.OversampleV = 1;
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(utf8_path.data(), 17.0f, &configuration,
                                                  merge ? merged_ranges.Data : base_ranges) != nullptr;
}

void overlay_window::collect_font_text(const char* text) noexcept {
    if (font_builder_ && text) {
        font_builder_->AddText(text);
    }
}

void overlay_window::update_window_title() noexcept {
    const auto language = static_cast<std::uint32_t>(localization::current_language());
    if (language == title_language_ || !window_) {
        return;
    }
    const char* title = ui_strings::get(ui_text_id::title);
    const char* id_suffix = std::strstr(title, "###");
    const int title_length = static_cast<int>(id_suffix ? id_suffix - title : std::strlen(title));
    std::array<wchar_t, 256> caption{};
    const int converted = MultiByteToWideChar(CP_UTF8, 0, title, title_length,
                                              caption.data(), static_cast<int>(caption.size() - 1));
    if (converted > 0) {
        caption[converted] = L'\0';
        SetWindowTextW(window_, caption.data());
        title_language_ = language;
    }
}

bool overlay_window::create_device() noexcept {
    DXGI_SWAP_CHAIN_DESC swap_description{};
    swap_description.BufferCount = 2;
    swap_description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_description.OutputWindow = window_;
    swap_description.SampleDesc.Count = 1;
    swap_description.Windowed = TRUE;
    swap_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL supported_levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL created_level{};
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        supported_levels, ARRAYSIZE(supported_levels), D3D11_SDK_VERSION,
        &swap_description, &swap_chain_, &device_, &created_level, &device_context_);
    if (FAILED(result)) {
        if (swap_chain_) {
            swap_chain_->Release();
            swap_chain_ = nullptr;
        }
        if (device_context_) {
            device_context_->Release();
            device_context_ = nullptr;
        }
        if (device_) {
            device_->Release();
            device_ = nullptr;
        }
        result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            supported_levels, ARRAYSIZE(supported_levels), D3D11_SDK_VERSION,
            &swap_description, &swap_chain_, &device_, &created_level, &device_context_);
    }
    return SUCCEEDED(result) && create_render_target();
}

bool overlay_window::create_render_target() noexcept {
    ID3D11Texture2D* back_buffer = nullptr;
    const HRESULT buffer_result = swap_chain_->GetBuffer(
        0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer));
    if (FAILED(buffer_result)) {
        return false;
    }
    const HRESULT target_result = device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_);
    back_buffer->Release();
    return SUCCEEDED(target_result);
}

void overlay_window::release_render_target() noexcept {
    if (render_target_) {
        render_target_->Release();
        render_target_ = nullptr;
    }
}

void overlay_window::cleanup() noexcept {
    if (dx11_backend_ready_) {
        ImGui_ImplDX11_Shutdown();
        dx11_backend_ready_ = false;
    }
    if (win32_backend_ready_) {
        ImGui_ImplWin32_Shutdown();
        win32_backend_ready_ = false;
    }
    if (imgui_context_ready_) {
        ImGui::DestroyContext();
        imgui_context_ready_ = false;
    }
    if (device_context_) {
        device_context_->ClearState();
    }
    release_render_target();
    if (swap_chain_) {
        swap_chain_->Release();
        swap_chain_ = nullptr;
    }
    if (device_context_) {
        device_context_->Release();
        device_context_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
    if (window_) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    if (registered_class_) {
        UnregisterClassW(L"AzsTrainerOverlayWindow", instance_);
        registered_class_ = false;
    }
}

BOOL CALLBACK overlay_window::enum_windows_callback(HWND window, LPARAM context) noexcept {
    auto& overlay = *reinterpret_cast<overlay_window*>(context);
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != GetCurrentProcessId() || window == overlay.window_
        || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)
        || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
        return TRUE;
    }
    RECT client_rect{};
    if (!GetClientRect(window, &client_rect)) {
        return TRUE;
    }
    const LONG width = client_rect.right - client_rect.left;
    const LONG height = client_rect.bottom - client_rect.top;
    if (width < 200 || height < 150) {
        return TRUE;
    }
    const auto area = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (area > overlay.candidate_area_) {
        overlay.candidate_area_ = area;
        overlay.candidate_window_ = window;
    }
    return TRUE;
}

LRESULT CALLBACK overlay_window::window_proc(HWND window, UINT message,
                                             WPARAM word_parameter, LPARAM long_parameter) noexcept {
    auto* overlay = reinterpret_cast<overlay_window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(long_parameter);
        overlay = static_cast<overlay_window*>(creation->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
    }
    if (overlay && overlay->win32_backend_ready_ && overlay->input_enabled_
        && ImGui_ImplWin32_WndProcHandler(window, message, word_parameter, long_parameter)) {
        return 1;
    }
    switch (message) {
    case WM_SIZE:
        if (overlay && word_parameter != SIZE_MINIMIZED) {
            overlay->pending_width_ = LOWORD(long_parameter);
            overlay->pending_height_ = HIWORD(long_parameter);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SYSCOMMAND:
        if ((word_parameter & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_CLOSE:
        if (overlay) {
            overlay->set_visible(false);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, word_parameter, long_parameter);
}

bool overlay_window::sync_target_window() noexcept {
    const std::uint64_t now_ms = GetTickCount64();
    if (now_ms >= next_window_scan_ms_ || !IsWindow(target_window_)) {
        candidate_window_ = nullptr;
        candidate_area_ = 0;
        EnumWindows(enum_windows_callback, reinterpret_cast<LPARAM>(this));
        target_window_ = candidate_window_;
        next_window_scan_ms_ = now_ms + 1000;
    }

    const HWND foreground_window = GetForegroundWindow();
    bool eligible = requested_visible_ && target_window_ && IsWindowVisible(target_window_)
        && !IsIconic(target_window_)
        && (foreground_window == target_window_ || foreground_window == window_);
    RECT client_rect{};
    POINT origin{};
    if (eligible) {
        eligible = GetClientRect(target_window_, &client_rect)
            && ClientToScreen(target_window_, &origin)
            && client_rect.right > client_rect.left && client_rect.bottom > client_rect.top;
    }
    if (!eligible) {
        if (shown_) {
            ShowWindow(window_, SW_HIDE);
            shown_ = false;
        }
        set_input_enabled(false);
        return false;
    }

    const RECT screen_rect{origin.x, origin.y,
        origin.x + client_rect.right - client_rect.left,
        origin.y + client_rect.bottom - client_rect.top};
    set_input_enabled(true);
    if (!shown_ || !EqualRect(&target_rect_, &screen_rect)) {
        target_rect_ = screen_rect;
        SetWindowPos(window_, HWND_TOPMOST, screen_rect.left, screen_rect.top,
                     screen_rect.right - screen_rect.left, screen_rect.bottom - screen_rect.top,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
        shown_ = true;
    }
    return true;
}

void overlay_window::handle_hotkeys() noexcept {
    const bool insert_pressed = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    const bool end_pressed = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    const HWND foreground_window = GetForegroundWindow();
    const bool relevant_foreground = foreground_window == window_
        || (target_window_ && foreground_window == target_window_);
    if (relevant_foreground && end_pressed && !previous_end_) {
        trainer_state::instance().reset_all();
        refresh_automation_editor();
        set_visible(false);
    } else if (relevant_foreground && insert_pressed && !previous_insert_) {
        set_visible(!requested_visible_);
    }
    previous_insert_ = insert_pressed;
    previous_end_ = end_pressed;
}

void overlay_window::set_visible(bool visible) noexcept {
    requested_visible_ = visible;
    if (!visible && window_) {
        const bool had_focus = GetForegroundWindow() == window_;
        ShowWindow(window_, SW_HIDE);
        shown_ = false;
        set_input_enabled(false);
        if (had_focus && IsWindow(target_window_) && !IsIconic(target_window_)) {
            SetForegroundWindow(target_window_);
        }
    }
}

void overlay_window::set_input_enabled(bool enabled) noexcept {
    if (input_enabled_ == enabled || !window_) {
        return;
    }
    LONG_PTR extended_style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    if (enabled) {
        extended_style &= ~(static_cast<LONG_PTR>(WS_EX_TRANSPARENT) | WS_EX_NOACTIVATE);
    } else {
        extended_style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    }
    SetWindowLongPtrW(window_, GWL_EXSTYLE, extended_style);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    input_enabled_ = enabled;
}

bool overlay_window::render_frame() {
    localization::refresh();
    update_window_title();
    if (pending_width_ && pending_height_) {
        device_context_->OMSetRenderTargets(0, nullptr, nullptr);
        release_render_target();
        const HRESULT resize_result = swap_chain_->ResizeBuffers(
            0, pending_width_, pending_height_, DXGI_FORMAT_UNKNOWN, 0);
        pending_width_ = 0;
        pending_height_ = 0;
        if (FAILED(resize_result) || !create_render_target()) {
            return false;
        }
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw_interface();
    ImGui::Render();

    const float transparent_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
    device_context_->OMSetRenderTargets(1, &render_target_, nullptr);
    device_context_->ClearRenderTargetView(render_target_, transparent_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const HRESULT present_result = swap_chain_->Present(1, 0);
    if (present_result == DXGI_STATUS_OCCLUDED) {
        Sleep(16);
    }
    return SUCCEEDED(present_result);
}

void overlay_window::draw_interface() {
    trainer_state& state = trainer_state::instance();
    const runtime_snapshot runtime = state.get_runtime();
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const float available_width = std::max(320.0f, display_size.x - 40.0f);
    const float available_height = std::max(240.0f, display_size.y - 40.0f);
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(std::min(850.0f, available_width),
                                   std::min(740.0f, available_height)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(560.0f, available_width),
                                             std::min(320.0f, available_height)),
                                        ImVec2(available_width, available_height));
    bool open = true;
    if (ImGui::Begin(ui_strings::get(ui_text_id::title), &open, ImGuiWindowFlags_NoCollapse)) {
        if (runtime.hooks_ready && runtime.run_active) {
            ImGui::TextColored(ImVec4(0.45f, 0.88f, 0.62f, 1.0f),
                               ui_strings::get(ui_text_id::run_active_format), runtime.level);
        } else if (runtime.hooks_ready) {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.38f, 1.0f),
                               ui_strings::get(ui_text_id::next_run));
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.58f, 0.48f, 1.0f),
                               ui_strings::get(ui_text_id::not_ready));
        }
        ImGui::TextWrapped("%s", ui_strings::get(ui_text_id::hotkeys));
        if (ImGui::Button(ui_strings::get(ui_text_id::reset))) {
            state.reset_all();
            refresh_automation_editor();
            state.set_message(ui_strings::get(ui_text_id::reset_done));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ui_strings::get(ui_text_id::reset_hint));
        }
        ImGui::SameLine();
        if (ImGui::Button(ui_strings::get(ui_text_id::hide))) {
            open = false;
        }
        const std::string status_message = state.get_message();
        if (!status_message.empty()) {
            ImGui::TextWrapped("%s", ui_strings::translate_message(status_message.c_str()));
        }
        draw_config_section();
        draw_automation_section(runtime.hooks_ready);
        ImGui::Separator();
        draw_fields(runtime.hooks_ready, runtime.run_active);
        ImGui::TextWrapped(ui_strings::get(ui_text_id::hold_start_hint));
    }
    ImGui::End();
    if (!open) {
        set_visible(false);
    }
}

void overlay_window::draw_config_section() {
    if (!ImGui::CollapsingHeader(ui_strings::get(ui_text_id::configs))) {
        return;
    }
    if (!config_list_ready_) {
        refresh_config_list();
    }
    ImGui::SetNextItemWidth(-110.0f);
    if (ImGui::BeginCombo("##config_list", config_filename_)) {
        for (const std::string& filename : config_files_) {
            const bool fits = filename.size() < sizeof(config_filename_);
            ImGui::BeginDisabled(!fits);
            if (ImGui::Selectable(filename.c_str(), filename == config_filename_)) {
                std::snprintf(config_filename_, sizeof(config_filename_), "%s", filename.c_str());
            }
            ImGui::EndDisabled();
            if (!fits && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(ui_strings::get(ui_text_id::filename_too_long));
            }
        }
        if (config_files_.empty()) {
            ImGui::TextDisabled(ui_strings::get(ui_text_id::no_configs));
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_strings::get(ui_text_id::refresh))) {
        refresh_config_list();
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##config_filename", config_filename_, sizeof(config_filename_));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(ui_strings::get(ui_text_id::filename_hint));
    }
    if (ImGui::Button(ui_strings::get(ui_text_id::load))) {
        if (config_store::load(config_filename_)) {
            reset_field_editors();
            refresh_automation_editor();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ui_strings::get(ui_text_id::save))) {
        if (config_store::save(config_filename_)) {
            refresh_config_list();
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled(ui_strings::get(ui_text_id::save_hint));
    const std::string config_message = config_store::get_message();
    if (!config_message.empty()) {
        ImGui::TextWrapped("%s", ui_strings::translate_message(config_message.c_str()));
    }
}

void overlay_window::draw_automation_section(bool controls_ready) {
    if (!ImGui::CollapsingHeader(ui_strings::get(ui_text_id::automation))) {
        return;
    }
    if (!automation_editor_ready_) {
        refresh_automation_editor();
    }
    ImGui::BeginDisabled(!controls_ready);
    ImGui::Checkbox("##apply_to_party", &apply_to_party_);
    ImGui::SameLine();
    ImGui::TextWrapped(ui_strings::get(ui_text_id::party_toggle));
    if (apply_to_party_) {
        const runtime_snapshot runtime = trainer_state::instance().get_runtime();
        if (runtime.party_host) {
            ImGui::Text(ui_strings::get(ui_text_id::party_status_format),
                        runtime.party_members, static_cast<unsigned long long>(runtime.party_writes));
        } else {
            ImGui::TextDisabled(ui_strings::get(ui_text_id::party_wait));
        }
    }
    ImGui::TextWrapped("%s", ui_strings::get(ui_text_id::party_help));
    ImGui::Checkbox(ui_strings::get(ui_text_id::auto_spells), &auto_choose_spells_);
    ImGui::Checkbox(ui_strings::get(ui_text_id::auto_jewelry), &auto_choose_jewelry_);
    ImGui::Checkbox("##legendary_final_boss", &legendary_final_boss_);
    ImGui::SameLine();
    ImGui::TextWrapped(ui_strings::get(ui_text_id::legendary_toggle));
    ImGui::TextWrapped(ui_strings::get(ui_text_id::legendary_help));

    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputInt(ui_strings::get(ui_text_id::choice_delay), &choice_delay_ms_, 50, 250);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        choice_delay_ms_ = std::clamp(choice_delay_ms_, 50, 5000);
    }
    const double minimum_time_scale = 1.0;
    const double maximum_time_scale = 16.0;
    ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - 200.0f));
    ImGui::SliderScalar(ui_strings::get(ui_text_id::game_time), ImGuiDataType_Double, &time_scale_,
                        &minimum_time_scale, &maximum_time_scale, "%.2fx",
                        ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    if (ImGui::Button(ui_strings::get(ui_text_id::speed_16x))) {
        time_scale_ = 16.0;
    }
    ImGui::TextWrapped(ui_strings::get(ui_text_id::time_help));

    ImGui::TextWrapped("%s", ui_strings::get(ui_text_id::priority_help));
    ImGui::TextUnformatted(ui_strings::get(ui_text_id::spell_priority_title));
    ImGui::InputTextMultiline("##spell_priorities", spell_priority_text_.data(),
                             spell_priority_text_.size(), ImVec2(-1.0f, 80.0f));
    ImGui::TextUnformatted(ui_strings::get(ui_text_id::jewelry_priority_title));
    ImGui::InputTextMultiline("##jewelry_priorities", jewelry_priority_text_.data(),
                             jewelry_priority_text_.size(), ImVec2(-1.0f, 80.0f));
    ImGui::TextDisabled(ui_strings::get(ui_text_id::priority_limits));
    if (ImGui::Button(ui_strings::get(ui_text_id::apply_rules))) {
        automation_settings settings{};
        settings.apply_to_party = apply_to_party_;
        settings.auto_choose_spells = auto_choose_spells_;
        settings.auto_choose_jewelry = auto_choose_jewelry_;
        settings.legendary_final_boss = legendary_final_boss_;
        settings.choice_delay_ms = static_cast<std::uint32_t>(std::clamp(choice_delay_ms_, 50, 5000));
        settings.time_scale = time_scale_;
        std::string error;
        if (!parse_priorities(spell_priority_text_.data(), settings.spell_priorities, error)
            || !parse_priorities(jewelry_priority_text_.data(), settings.jewelry_priorities, error)) {
            trainer_state::instance().set_message(error.c_str());
        } else if (trainer_state::instance().set_automation(settings)) {
            refresh_automation_editor();
            trainer_state::instance().set_message(ui_strings::get(ui_text_id::rules_applied));
        } else {
            trainer_state::instance().set_message(ui_strings::get(ui_text_id::rules_rejected));
        }
    }
    ImGui::EndDisabled();
}

void overlay_window::refresh_config_list() {
    config_files_ = config_store::list_configs();
    config_list_ready_ = true;
}

void overlay_window::refresh_automation_editor() noexcept {
    const auto settings = trainer_state::instance().get_automation();
    if (!settings) {
        return;
    }
    apply_to_party_ = settings->apply_to_party;
    auto_choose_spells_ = settings->auto_choose_spells;
    auto_choose_jewelry_ = settings->auto_choose_jewelry;
    legendary_final_boss_ = settings->legendary_final_boss;
    choice_delay_ms_ = static_cast<int>(settings->choice_delay_ms);
    time_scale_ = settings->time_scale;
    format_priorities(settings->spell_priorities, spell_priority_text_.data(), spell_priority_text_.size());
    format_priorities(settings->jewelry_priorities, jewelry_priority_text_.data(), jewelry_priority_text_.size());
    automation_editor_ready_ = true;
}

void overlay_window::reset_field_editors() noexcept {
    initialized_values_.fill(false);
    dirty_values_.fill(false);
}

bool overlay_window::parse_priorities(const char* text, std::vector<std::string>& priorities,
                                      std::string& error) {
    std::vector<std::string> parsed;
    const std::string_view input = text ? text : "";
    std::size_t token_start = 0;
    for (std::size_t position = 0; position <= input.size(); ++position) {
        if (position != input.size() && input[position] != ',' && input[position] != '\n') {
            continue;
        }
        const std::string_view token = input.substr(token_start, position - token_start);
        const std::size_t first = token.find_first_not_of(" \t\r\v\f");
        if (first != std::string_view::npos) {
            const std::size_t last = token.find_last_not_of(" \t\r\v\f");
            const std::string_view key = token.substr(first, last - first + 1);
            if (key.size() > 128) {
                error = ui_strings::get(ui_text_id::priority_key_long);
                return false;
            }
            if (parsed.size() == 64) {
                error = ui_strings::get(ui_text_id::priority_too_many);
                return false;
            }
            parsed.emplace_back(key);
        }
        token_start = position + 1;
    }
    priorities = std::move(parsed);
    return true;
}

void overlay_window::format_priorities(const std::vector<std::string>& priorities,
                                       char* destination, std::size_t capacity) noexcept {
    if (!capacity) {
        return;
    }
    std::size_t written = 0;
    for (const std::string& key : priorities) {
        if (written && written < capacity - 1) {
            destination[written++] = '\n';
        }
        const std::size_t copied = std::min(key.size(), capacity - 1 - written);
        std::memcpy(destination + written, key.data(), copied);
        written += copied;
        if (written == capacity - 1) {
            break;
        }
    }
    destination[written] = '\0';
}

void overlay_window::draw_fields(bool controls_ready, bool run_active) {
    trainer_state& state = trainer_state::instance();
    const auto count = std::min(trainer_state::field_count(), maximum_fields);
    const float table_height = std::max(100.0f, ImGui::GetContentRegionAvail().y - 34.0f);
    const ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("fields", 6, table_flags, ImVec2(0.0f, table_height))) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(ui_strings::get(ui_text_id::col_parameter), ImGuiTableColumnFlags_WidthStretch, 2.4f);
    ImGui::TableSetupColumn(ui_strings::get(ui_text_id::col_current), ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn(ui_strings::get(ui_text_id::col_desired), ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn(ui_strings::get(ui_text_id::col_freeze), ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableSetupColumn(ui_strings::get(ui_text_id::col_start), ImGuiTableColumnFlags_WidthFixed, 48.0f);
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < count; ++index) {
        const field_descriptor& descriptor = trainer_state::field_at(index);
        const field_snapshot snapshot = state.get_field(index);
        if (!initialized_values_[index]) {
            edited_values_[index] = snapshot.configured ? snapshot.desired_value
                : (run_active && snapshot.available ? snapshot.current_value
                                                    : std::clamp(0.0, descriptor.minimum, descriptor.maximum));
            initialized_values_[index] = true;
        } else if (!dirty_values_[index]) {
            if (snapshot.configured) {
                edited_values_[index] = snapshot.desired_value;
            } else if (run_active && snapshot.available) {
                edited_values_[index] = snapshot.current_value;
            }
        }

        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextWrapped("%s", ui_strings::field_name(descriptor.key));
        const char* unit = ui_strings::field_unit(descriptor.key);
        if (unit[0]) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", unit);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(ui_strings::get(ui_text_id::range_format),
                              descriptor.key, descriptor.minimum, descriptor.maximum);
        }
        ImGui::TableSetColumnIndex(1);
        if (!run_active || !snapshot.available) {
            ImGui::TextDisabled(ui_strings::get(ui_text_id::unavailable));
        } else if (descriptor.kind == value_kind::boolean) {
            ImGui::TextUnformatted(snapshot.current_value != 0.0 ? ui_strings::get(ui_text_id::yes) : ui_strings::get(ui_text_id::no));
        } else {
            ImGui::Text(descriptor.kind == value_kind::unsigned_integer ? "%.0f" : "%.3f",
                        snapshot.current_value);
        }

        ImGui::BeginDisabled(!controls_ready);
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-1.0f);
        if (descriptor.kind == value_kind::boolean) {
            bool desired = edited_values_[index] != 0.0;
            if (ImGui::Checkbox("##desired", &desired)) {
                edited_values_[index] = desired ? 1.0 : 0.0;
                dirty_values_[index] = true;
            }
        } else if (ImGui::InputDouble("##desired", &edited_values_[index], descriptor.step,
                                      descriptor.step * 10.0,
                                      descriptor.kind == value_kind::unsigned_integer ? "%.0f" : "%.3f")) {
            dirty_values_[index] = true;
        }
        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button(snapshot.pending ? ui_strings::get(ui_text_id::pending) : ui_strings::get(ui_text_id::apply), ImVec2(-1.0f, 0.0f))) {
            if (state.apply(index, edited_values_[index])) {
                dirty_values_[index] = false;
                state.set_message(run_active ? ui_strings::get(ui_text_id::value_sent)
                                             : ui_strings::get(ui_text_id::value_prepared));
            } else {
                state.set_message(ui_strings::get(ui_text_id::value_rejected));
            }
        }
        ImGui::TableSetColumnIndex(4);
        bool frozen = snapshot.frozen;
        if (ImGui::Checkbox("##freeze", &frozen)) {
            const double freeze_value = frozen || !snapshot.configured
                ? edited_values_[index] : snapshot.desired_value;
            if (state.freeze(index, frozen, freeze_value)) {
                if (frozen) {
                    dirty_values_[index] = false;
                }
            } else {
                state.set_message(ui_strings::get(ui_text_id::freeze_rejected));
            }
        }
        ImGui::TableSetColumnIndex(5);
        bool apply_on_start = snapshot.apply_on_start;
        if (ImGui::Checkbox("##start", &apply_on_start)) {
            bool accepted = true;
            if (apply_on_start && (!snapshot.configured || dirty_values_[index])) {
                accepted = state.apply(index, edited_values_[index]);
            }
            if (accepted && state.set_apply_on_start(index, apply_on_start)) {
                if (apply_on_start) {
                    dirty_values_[index] = false;
                }
            } else {
                state.set_message(ui_strings::get(ui_text_id::start_rejected));
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(ui_strings::get(ui_text_id::start_hint));
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndTable();
}

} // namespace azs
