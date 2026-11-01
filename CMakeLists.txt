cmake_minimum_required(VERSION 3.24)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" AS_TRAINER_VERSION LIMIT_COUNT 1)
project(azeroth_survivors_overlay VERSION ${AS_TRAINER_VERSION} LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build with MSVC x64 on Windows; SEH is required for guarded game access.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
configure_file(include/build_version.hpp.in generated/build_version.hpp @ONLY)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/signatures/azeroth-survivors-0.132.0.ini" AS_SIGNATURE_PROFILE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/signatures/azeroth-survivors-adaptive.ini" AS_ADAPTIVE_SIGNATURE_PROFILE)
string(REPLACE "\n" "\n)as_profile\"\nR\"as_profile(" AS_ADAPTIVE_SIGNATURE_PROFILE "${AS_ADAPTIVE_SIGNATURE_PROFILE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/signatures/azeroth-survivors-0.132.0.ini"
    "${CMAKE_CURRENT_SOURCE_DIR}/signatures/azeroth-survivors-adaptive.ini")
configure_file(include/embedded_signatures.hpp.in generated/embedded_signatures.hpp @ONLY)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp" OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/src/hook.c")
    message(FATAL_ERROR "Bundled ImGui and MinHook sources are missing. See README.md.")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/minhook EXCLUDE_FROM_ALL)
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32)

add_library(trainer_bridge STATIC src/trainer_state.cpp src/config_store.cpp src/simulation_clock.cpp src/localization.cpp src/game_layout.cpp)
target_include_directories(trainer_bridge PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(trainer_bridge PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(trainer_bridge PRIVATE /W4 /permissive- /utf-8 /EHsc)

add_library(trainer_scanning STATIC src/pattern_scanner.cpp src/signature_profile.cpp src/native_addresses.cpp)
target_include_directories(trainer_scanning PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(trainer_scanning PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(trainer_scanning PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_link_libraries(trainer_scanning PUBLIC trainer_bridge bcrypt)

add_library(trainer_actions STATIC src/auto_choice.cpp src/legendary_loot.cpp)
target_include_directories(trainer_actions PUBLIC include)
target_compile_definitions(trainer_actions PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(trainer_actions PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_link_libraries(trainer_actions PUBLIC minhook trainer_bridge trainer_scanning)

add_library(azs_overlay SHARED src/dll_main.cpp src/game_runtime.cpp src/party_runtime.cpp src/overlay_window.cpp src/ui_strings.cpp)
target_include_directories(azs_overlay PRIVATE include)
target_link_libraries(azs_overlay PRIVATE trainer_bridge trainer_actions imgui minhook bcrypt user32 gdi32)
target_compile_options(azs_overlay PRIVATE /W4 /permissive- /utf-8 /EHsc)
set_target_properties(azs_overlay PROPERTIES OUTPUT_NAME azs_overlay)

add_executable(load_overlay src/injector.cpp)
target_include_directories(load_overlay PRIVATE include)
target_compile_definitions(load_overlay PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(load_overlay PRIVATE /W4 /permissive- /utf-8 /EHsc)
target_link_libraries(load_overlay PRIVATE trainer_bridge user32)

add_executable(signature_scan src/signature_tool.cpp)
target_link_libraries(signature_scan PRIVATE trainer_scanning)
target_compile_options(signature_scan PRIVATE /W4 /permissive- /utf-8 /EHsc)

foreach(component IN ITEMS azs_overlay load_overlay)
    set(AS_COMPONENT_NAME "${component}")
    if(component STREQUAL "azs_overlay")
        set(AS_COMPONENT_TYPE VFT_DLL)
        set(AS_COMPONENT_FILENAME azs_overlay.dll)
        set(AS_COMPONENT_DESCRIPTION "AS Trainer overlay")
    else()
        set(AS_COMPONENT_TYPE VFT_APP)
        set(AS_COMPONENT_FILENAME load_overlay.exe)
        set(AS_COMPONENT_DESCRIPTION "AS Trainer loader")
    endif()
    configure_file(resources/version.rc.in "generated/${component}_version.rc" @ONLY)
    target_sources(${component} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/${component}_version.rc")
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_executable(trainer_tests tests/trainer_tests.cpp)
    target_link_libraries(trainer_tests PRIVATE trainer_bridge)
    target_compile_options(trainer_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME verified_memory_bridge COMMAND trainer_tests)

    add_executable(speed_sync_tests tests/speed_sync_tests.cpp src/game_runtime.cpp src/party_runtime.cpp)
    target_include_directories(speed_sync_tests PRIVATE include tests)
    target_link_libraries(speed_sync_tests PRIVATE trainer_bridge trainer_actions minhook bcrypt)
    target_compile_options(speed_sync_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME isolated_speed_sync COMMAND speed_sync_tests)

    add_executable(legendary_loot_tests tests/legendary_loot_tests.cpp)
    target_include_directories(legendary_loot_tests PRIVATE tests)
    target_link_libraries(legendary_loot_tests PRIVATE trainer_actions)
    target_compile_options(legendary_loot_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME final_boss_legendary_set COMMAND legendary_loot_tests)

    add_executable(auto_choice_tests tests/auto_choice_tests.cpp)
    target_include_directories(auto_choice_tests PRIVATE tests)
    target_link_libraries(auto_choice_tests PRIVATE trainer_actions trainer_bridge)
    target_compile_options(auto_choice_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME priority_auto_choice COMMAND auto_choice_tests)

    add_executable(config_store_tests tests/config_store_tests.cpp)
    target_include_directories(config_store_tests PRIVATE tests)
    target_link_libraries(config_store_tests PRIVATE trainer_bridge)
    target_compile_options(config_store_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME atomic_ini_configs COMMAND config_store_tests)

    add_executable(party_runtime_tests tests/party_runtime_tests.cpp src/party_runtime.cpp)
    target_include_directories(party_runtime_tests PRIVATE tests)
    target_link_libraries(party_runtime_tests PRIVATE trainer_bridge)
    target_compile_options(party_runtime_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME hosted_party_overrides COMMAND party_runtime_tests)

    add_executable(localization_tests tests/localization_tests.cpp)
    target_include_directories(localization_tests PRIVATE tests)
    target_link_libraries(localization_tests PRIVATE trainer_bridge)
    target_compile_options(localization_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME client_language_localization COMMAND localization_tests)

    add_executable(pattern_scanner_tests tests/pattern_scanner_tests.cpp)
    target_include_directories(pattern_scanner_tests PRIVATE tests)
    target_link_libraries(pattern_scanner_tests PRIVATE trainer_scanning)
    target_compile_options(pattern_scanner_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME bounded_pe_pattern_scanner COMMAND pattern_scanner_tests)

    add_executable(signature_profile_tests tests/signature_profile_tests.cpp)
    target_include_directories(signature_profile_tests PRIVATE tests)
    target_link_libraries(signature_profile_tests PRIVATE trainer_scanning)
    target_compile_options(signature_profile_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME strict_signature_profiles COMMAND signature_profile_tests)

    add_executable(native_addresses_tests tests/native_addresses_tests.cpp)
    target_include_directories(native_addresses_tests PRIVATE tests)
    target_link_libraries(native_addresses_tests PRIVATE trainer_scanning)
    target_compile_options(native_addresses_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME adaptive_native_bindings COMMAND native_addresses_tests)

    add_executable(game_layout_tests tests/game_layout_tests.cpp src/party_runtime.cpp)
    target_include_directories(game_layout_tests PRIVATE tests)
    target_link_libraries(game_layout_tests PRIVATE trainer_bridge)
    target_compile_options(game_layout_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME verified_build_layouts COMMAND game_layout_tests)

    add_executable(ui_strings_tests tests/ui_strings_tests.cpp src/ui_strings.cpp)
    target_include_directories(ui_strings_tests PRIVATE tests)
    target_link_libraries(ui_strings_tests PRIVATE trainer_bridge imgui)
    target_compile_options(ui_strings_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME localized_ui_and_fonts COMMAND ui_strings_tests)
    set_tests_properties(localized_ui_and_fonts PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(overlay_preview WIN32 tests/overlay_preview.cpp src/overlay_window.cpp src/ui_strings.cpp)
    target_link_libraries(overlay_preview PRIVATE trainer_bridge imgui user32 gdi32)
    target_compile_options(overlay_preview PRIVATE /W4 /permissive- /utf-8 /EHsc)

    add_executable(overlay_automation_tests tests/overlay_automation_tests.cpp src/overlay_window.cpp src/ui_strings.cpp)
    target_include_directories(overlay_automation_tests PRIVATE tests)
    target_link_libraries(overlay_automation_tests PRIVATE trainer_bridge imgui user32 gdi32)
    target_compile_options(overlay_automation_tests PRIVATE /W4 /permissive- /utf-8 /EHsc)
    add_test(NAME immediate_overlay_automation COMMAND overlay_automation_tests)
endif()

install(TARGETS azs_overlay load_overlay signature_scan RUNTIME DESTINATION .)
install(FILES README.md load_overlay.cmd third_party/imgui/LICENSE.txt DESTINATION .)
install(FILES third_party/minhook/LICENSE.txt DESTINATION . RENAME MINHOOK_LICENSE.txt)
install(FILES third_party/imgui/LICENSE.txt DESTINATION third_party/imgui)
install(FILES third_party/minhook/LICENSE.txt DESTINATION third_party/minhook)
install(DIRECTORY configs DESTINATION .)
install(DIRECTORY docs fonts DESTINATION .)
install(DIRECTORY signatures DESTINATION .)
install(FILES THIRD_PARTY.md VERSION DESTINATION .)
