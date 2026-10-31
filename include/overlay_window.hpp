#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ImFontGlyphRangesBuilder;

namespace azs {

class overlay_window final {
public:
    /** @brief Own the overlay message loop and its independent graphics resources.
     * @param context Reserved DLL thread argument; ignored by the overlay.
     * @return Zero after an ordinary shutdown, one after initialization/render failure.
     * No game renderer is hooked and the loaded DLL is never unloaded here.
     */
    static DWORD WINAPI thread_main(void* context) noexcept;

private:
    /** @brief Initialize one private window, D3D11 device and ImGui context.
     * @return True when every resource required to render the overlay is ready.
     * Partial initialization remains owned by cleanup() on failure.
     */
    bool initialize() noexcept;

    /** @brief Load shared Latin/Cyrillic glyphs and merge bounded Chinese catalog glyphs.
     * @return True when the base atlas and a Chinese-capable fallback font are loaded.
     * Font candidates are resolved beside this DLL and in the Windows Fonts directory.
     */
    bool load_fonts() noexcept;

    /** @brief Add one verified existing UTF-8 font path to the current ImGui atlas.
     * @param path Null-terminated Windows path to a TTF/OTF/TTC font file.
     * @param merge True to merge exact catalog glyph ranges into the base font.
     * @return True when the file was loaded and accepted by ImGui.
     */
    static bool add_font_file(const wchar_t* path, bool merge) noexcept;

    /** @brief Add one immutable translated diagnostic to the active atlas range builder.
     * @param text Null-terminated UTF-8 catalog text provided by localization.
     * @return None; used synchronously only during load_fonts().
     */
    static void collect_font_text(const char* text) noexcept;

    /** @brief Refresh the native window caption when the client language changes.
     * @return None; visible ImGui IDs remain stable across language switches.
     */
    void update_window_title() noexcept;

    /** @brief Create a hardware D3D11 device and swap chain, with a WARP fallback.
     * @return True when the device, immediate context and initial target exist.
     */
    bool create_device() noexcept;

    /** @brief Create the render-target view for the swap chain's current back buffer.
     * @return True if the back buffer and render-target view were obtained.
     */
    bool create_render_target() noexcept;

    /** @brief Release the render target before resizing or destroying the swap chain.
     * @return None; repeated calls are harmless.
     */
    void release_render_target() noexcept;

    /** @brief Shut down initialized backends and release all resources on their owner thread.
     * @return None; no game window or game memory is modified.
     */
    void cleanup() noexcept;

    /** @brief Enumerate this process's visible top-level windows and keep the largest client.
     * @param window Candidate HWND supplied by EnumWindows.
     * @param context Pointer to the overlay_window instance performing enumeration.
     * @return TRUE to continue enumeration; this callback never closes a window.
     */
    static BOOL CALLBACK enum_windows_callback(HWND window, LPARAM context) noexcept;

    /** @brief Dispatch private-window messages to ImGui and defer graphics resizing.
     * @param window Overlay HWND; @param message Win32 message identifier.
     * @param word_parameter Message-specific integer argument.
     * @param long_parameter Message-specific integer or pointer argument.
     * @return Message result required by Win32 or DefWindowProcW.
     */
    static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                       WPARAM word_parameter, LPARAM long_parameter) noexcept;

    /** @brief Locate the game client, track its rectangle and honor foreground/minimize state.
     * @return True only when the overlay is currently eligible to be displayed.
     * Window discovery uses HWND metadata and never dereferences game memory.
     */
    bool sync_target_window() noexcept;

    /** @brief Apply Insert/End key transitions while the game or overlay has foreground focus.
     * @return None; End stops overrides, automation and acceleration before hiding.
     */
    void handle_hotkeys() noexcept;

    /** @brief Change requested interface visibility and restore game focus after hiding.
     * @param visible True to permit rendering/input while the game is foreground.
     * @return None; hiding does not stop the game or unload the DLL.
     */
    void set_visible(bool visible) noexcept;

    /** @brief Switch between editable overlay input and a nonactivating hidden window.
     * @param enabled True to permit normal mouse/keyboard activation.
     * @return None; the underlying game retains input when the interface is hidden.
     */
    void set_input_enabled(bool enabled) noexcept;

    /** @brief Resize buffers when needed, build the interface and present one frame.
     * @return True after a rendered frame; false after an unrecoverable graphics failure.
     */
    bool render_frame();

    /** @brief Draw status, controls and all editable descriptors using synchronized snapshots.
     * @return None; all writes are submitted through trainer_state commands.
     */
    void draw_interface();

    /** @brief Draw preset selection, local filename editing and atomic load/save actions.
     * @return None; loading refreshes field and automation editors from accepted settings.
     */
    void draw_config_section();

    /** @brief Draw unpublished party scope, automation, priority and time settings.
     * @param controls_ready True when the supported hook bridge or preview is ready.
     * @return None; settings change only after the user applies the complete rule draft.
     */
    void draw_automation_section(bool controls_ready);

    /** @brief Replace the cached preset filename list with the config store's current inventory.
     * @return None; only the configured local preset directory is enumerated.
     */
    void refresh_config_list();

    /** @brief Copy the immutable automation snapshot into editable local controls.
     * @return None; reads trainer state only and never accesses live game data.
     */
    void refresh_automation_editor() noexcept;

    /** @brief Discard local field drafts so the next draw uses the newly loaded preset.
     * @return None; configured values remain in trainer_state until refreshed by drawing.
     */
    void reset_field_editors() noexcept;

    /** @brief Parse ordered stable keys separated by commas or newlines, trimming whitespace.
     * @param text Null-terminated UTF-8 editor contents.
     * @param priorities Output keys, replaced only after the entire list is valid.
     * @param error Output explanation when a key exceeds 128 bytes or the list exceeds 64 keys.
     * @return True for a valid bounded list, including an empty list; false on invalid bounds.
     */
    static bool parse_priorities(const char* text, std::vector<std::string>& priorities,
                                 std::string& error);

    /** @brief Join accepted priority keys into a bounded multiline editor buffer.
     * @param priorities Validated ordered keys from an immutable settings snapshot.
     * @param destination Writable output buffer; @param capacity Buffer bytes including terminator.
     * @return None; output always ends with a null terminator when capacity is nonzero.
     */
    static void format_priorities(const std::vector<std::string>& priorities,
                                  char* destination, std::size_t capacity) noexcept;

    /** @brief Draw current/desired values and Apply/Freeze/Start controls for every field.
     * @param controls_ready True when validated hooks or the offline preview permit configuration.
     * @param run_active True only while current game snapshots are fresh.
     * @return None; absent live values display a dash while desired values remain editable.
     */
    void draw_fields(bool controls_ready, bool run_active);

    static constexpr std::size_t maximum_fields = 64;
    static constexpr std::size_t priority_buffer_size = 64 * 129 + 1;
    static ImFontGlyphRangesBuilder* font_builder_;
    HWND window_{};
    HWND target_window_{};
    HWND candidate_window_{};
    HINSTANCE instance_{};
    ID3D11Device* device_{};
    ID3D11DeviceContext* device_context_{};
    IDXGISwapChain* swap_chain_{};
    ID3D11RenderTargetView* render_target_{};
    std::array<double, maximum_fields> edited_values_{};
    std::array<bool, maximum_fields> initialized_values_{};
    std::array<bool, maximum_fields> dirty_values_{};
    std::array<char, priority_buffer_size> spell_priority_text_{};
    std::array<char, priority_buffer_size> jewelry_priority_text_{};
    std::vector<std::string> config_files_;
    char config_filename_[128] = "default.ini";
    int choice_delay_ms_{500};
    double time_scale_{1.0};
    std::uint32_t title_language_{~std::uint32_t{0}};
    std::uint64_t next_window_scan_ms_{};
    std::uint64_t candidate_area_{};
    RECT target_rect_{};
    UINT pending_width_{};
    UINT pending_height_{};
    bool registered_class_{};
    bool imgui_context_ready_{};
    bool win32_backend_ready_{};
    bool dx11_backend_ready_{};
    bool requested_visible_{true};
    bool shown_{};
    bool input_enabled_{true};
    bool previous_insert_{};
    bool previous_end_{};
    bool config_list_ready_{};
    bool automation_editor_ready_{};
    bool apply_to_party_{};
    bool auto_choose_spells_{};
    bool auto_choose_jewelry_{};
    bool legendary_final_boss_{};
};

} // namespace azs
