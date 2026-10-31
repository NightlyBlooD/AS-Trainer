#include "injector.hpp"
#include "localization.hpp"

#include <TlHelp32.h>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <limits>

namespace azs {

DWORD injector::find_game() {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD result = 0;
    bool ambiguous = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"AzerothSurvivors.exe") == 0) {
                localization::print(stdout, "Game PID: %lu\n", entry.th32ProcessID);
                if (result) {
                    ambiguous = true;
                }
                result = entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    if (ambiguous) {
        localization::print(stderr, "Multiple games found. Use --pid <PID>.\n");
        return 0;
    }
    if (!result) {
        localization::print(stderr, "Start AzerothSurvivors.exe first, then run this loader.\n");
    }
    return result;
}

std::uintptr_t injector::find_module(DWORD process_id, const wchar_t* module_name) {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::uintptr_t result = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, module_name) == 0) {
                result = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

LPTHREAD_START_ROUTINE injector::find_remote_loader(DWORD process_id) {
    const auto loader = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE owner{};
    if (!loader || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(loader), &owner)) {
        return nullptr;
    }
    wchar_t owner_path[32768]{};
    if (!GetModuleFileNameW(owner, owner_path, static_cast<DWORD>(std::size(owner_path)))) {
        return nullptr;
    }
    const auto owner_name = std::filesystem::path(owner_path).filename().wstring();
    const auto remote_base = find_module(process_id, owner_name.c_str());
    const auto offset = reinterpret_cast<std::uintptr_t>(loader) - reinterpret_cast<std::uintptr_t>(owner);
    return remote_base ? reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_base + offset) : nullptr;
}

std::wstring injector::get_library_path() {
    wchar_t executable_path[32768]{};
    const auto count = GetModuleFileNameW(nullptr, executable_path, static_cast<DWORD>(std::size(executable_path)));
    if (!count || count >= std::size(executable_path)) {
        return {};
    }
    return (std::filesystem::path(executable_path).parent_path() / L"azs_overlay.dll").wstring();
}

bool injector::validate_target(HANDLE process, DWORD process_id) {
    wchar_t executable_path[32768]{};
    DWORD length = static_cast<DWORD>(std::size(executable_path));
    USHORT process_machine{}, native_machine{};
    if (!QueryFullProcessImageNameW(process, 0, executable_path, &length) ||
        _wcsicmp(std::filesystem::path(executable_path).filename().c_str(), L"AzerothSurvivors.exe") != 0 ||
        !IsWow64Process2(process, &process_machine, &native_machine) ||
        process_machine != IMAGE_FILE_MACHINE_UNKNOWN || native_machine != IMAGE_FILE_MACHINE_AMD64 ||
        WaitForSingleObject(process, 0) != WAIT_TIMEOUT) {
        localization::print(stderr, "PID %lu is not a running x64 AzerothSurvivors.exe.\n", process_id);
        return false;
    }
    return true;
}

int injector::run(int argument_count, wchar_t** arguments) {
    localization::initialize();
    DWORD process_id = 0;
    if (argument_count == 3 && std::wcscmp(arguments[1], L"--pid") == 0) {
        wchar_t* end{};
        const auto parsed = std::wcstoull(arguments[2], &end, 10);
        if (end == arguments[2] || *end || !parsed || parsed > std::numeric_limits<DWORD>::max()) {
            localization::print(stderr, "Invalid process ID.\n");
            return 1;
        }
        process_id = static_cast<DWORD>(parsed);
    } else if (argument_count != 1) {
        localization::print(stderr, "Usage: load_overlay.exe [--pid <PID>]\n");
        return 1;
    } else {
        process_id = find_game();
    }
    if (!process_id) {
        return 1;
    }
    const auto library_path = get_library_path();
    if (library_path.empty() || GetFileAttributesW(library_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        localization::print(stderr, "Missing sibling azs_overlay.dll.\n");
        return 2;
    }
    const auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE, FALSE, process_id);
    if (!process) {
        localization::print(stderr, "OpenProcess failed: %lu. Use the same privileges as the game.\n", GetLastError());
        return 3;
    }
    if (!validate_target(process, process_id)) {
        CloseHandle(process);
        return 3;
    }
    if (find_module(process_id, L"azs_overlay.dll")) {
        localization::print(stdout, "Overlay already loaded. Press Insert to toggle. Restart the game to load an updated DLL.\n");
        CloseHandle(process);
        return 0;
    }
    const auto remote_loader = find_remote_loader(process_id);
    const auto bytes = (library_path.size() + 1) * sizeof(wchar_t);
    const auto remote_path = remote_loader ? VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) : nullptr;
    SIZE_T written{};
    if (!remote_path || !WriteProcessMemory(process, remote_path, library_path.c_str(), bytes, &written) || written != bytes) {
        localization::print(stderr, "Cannot prepare DLL path: %lu.\n", GetLastError());
        if (remote_path) {
            VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        }
        CloseHandle(process);
        return 4;
    }
    const auto thread = CreateRemoteThread(process, nullptr, 0, remote_loader, remote_path, 0, nullptr);
    if (!thread) {
        localization::print(stderr, "CreateRemoteThread failed: %lu.\n", GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 5;
    }
    const auto wait_result = WaitForSingleObject(thread, 30000);
    if (wait_result != WAIT_OBJECT_0) {
        localization::print(stderr, "Load pending/indeterminate; do not repeat. Restart the game if it remains stuck.\n");
        CloseHandle(thread);
        CloseHandle(process);
        return 6;
    }
    DWORD loader_result{};
    GetExitCodeThread(thread, &loader_result);
    CloseHandle(thread);
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    const auto module = find_module(process_id, L"azs_overlay.dll");
    CloseHandle(process);
    if (!module) {
        localization::print(stderr, "DLL was not loaded (LoadLibrary result 0x%08lX).\n", loader_result);
        return 7;
    }
    localization::print(stdout, "Overlay loaded at 0x%llX. Press Insert to toggle; End releases all freezes.\n",
                 static_cast<unsigned long long>(module));
    return 0;
}

} // namespace azs

int wmain(int argument_count, wchar_t** arguments) {
    try {
        return azs::injector::run(argument_count, arguments);
    } catch (...) {
        azs::localization::print(stderr, "Unexpected loader failure.\n");
        return 8;
    }
}
