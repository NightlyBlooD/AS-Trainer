#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>
#include <string>

namespace azs {

class injector final {
public:
    /** @brief Load the sibling overlay DLL into one explicitly identified local game.
     * @param argument_count Number of command-line arguments including the executable.
     * @param arguments Optional --pid <decimal_pid>; otherwise exactly one game is required.
     * @return Zero on confirmed module loading, nonzero with a printed diagnostic.
     */
    static int run(int argument_count, wchar_t** arguments);

private:
    /** @brief Find the only AzerothSurvivors.exe process without selecting another app.
     * @return Its PID, or zero when absent/ambiguous; diagnostics are printed to stderr.
     */
    static DWORD find_game();

    /** @brief Resolve LoadLibraryW by module-relative offset in the target process.
     * @param process_id Target game's PID with the same x64 architecture as this loader.
     * @return Remote LoadLibraryW entry point, or null if its owner module cannot be found.
     */
    static LPTHREAD_START_ROUTINE find_remote_loader(DWORD process_id);

    /** @brief Resolve a module path in a target process through Toolhelp enumeration.
     * @param process_id Target process; @param module_name Module filename to locate.
     * @return Loaded base address, or zero if enumeration/matching fails.
     */
    static std::uintptr_t find_module(DWORD process_id, const wchar_t* module_name);

    /** @brief Derive the DLL path from this loader's own executable directory.
     * @return Absolute path to sibling azs_overlay.dll; empty on path API failure.
     */
    static std::wstring get_library_path();

    /** @brief Confirm target identity and matching x64 architecture before any allocation.
     * @param process Open process handle; @param process_id Target PID for reporting.
     * @return True only for a running x64 AzerothSurvivors.exe process.
     */
    static bool validate_target(HANDLE process, DWORD process_id);
};

} // namespace azs
