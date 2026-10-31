# AS-Trainer 1.0.0

[English](README.md) · [Русский](docs/README.ru.md) · [Deutsch](docs/README.de.md) · [Français](docs/README.fr.md) · [Español](docs/README.es.md) · [Українська](docs/README.uk.md) · [简体中文](docs/README.zh-CN.md)

A C++20 / Dear ImGui trainer for Azeroth Survivors on Windows x64. Edit 25 character fields, prepare next-run settings, save INI profiles, choose rewards automatically, obtain the class legendary set from a final boss, and advance simulation at 1–16× speed.

## Download and launch

Download **`AS-Trainer-1.0.0-windows-x64.zip`** from [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases). Extract the entire archive: keep `azs_overlay.dll`, `load_overlay.exe`, `load_overlay.cmd`, `signature_scan.exe`, `configs`, `signatures`, `fonts`, documentation and license notices together. The release also provides the DLL, loader and `SHA256SUMS.txt` separately.

1. Start the supported game in windowed or borderless mode.
2. Run `load_overlay.cmd` or `load_overlay.exe` with the same privileges as the game.
3. **Insert** shows or hides the panel. Configure values or load a profile under **Configs**. Confirm automation changes with **Apply rules**.
4. **End** cancels queued writes, freezes and Start flags, disables automation and party scope, returns time to 1×, and hides the panel. Existing game values and config files remain in place.

For multiple processes, use `load_overlay.exe --pid 12345`. The DLL stays loaded until the game exits: **restart the game before updating it**. Running the loader again does not replace a loaded DLL.

## Compatibility and language

Supported executable profiles, identified by the exact SHA-256 of `AzerothSurvivors.exe`:

| Game version | SHA-256 |
|---|---|
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

The DLL requires a verified executable hash, object layout and native calling convention (ABI), and checks critical code bytes before installing hooks. Other builds are rejected. It does not modify the executable on disk. The current profile was checked against the executable and offline fixtures; complete live runs covering every feature and real co-op sessions remain unverified.

The interface follows the client's `language.txt`, normally `%APPDATA%\wenilla-rogue\language.txt`. English, Russian, German, French, Spanish, Ukrainian and Simplified Chinese are supported. Unknown values fall back to English. Language changes refresh while the panel is running, without restarting the trainer.

## Values and profiles

**Apply** queues a one-shot absolute value. In a menu it prepares the next run and enables **Start**. **Freeze** reapplies the value after game recalculations and also enables Start. **Start** reapplies a configured value on each new run; turning it off prevents that field's freeze from carrying into the next run. Current values appear after the game provides an active character.

The 25 fields cover damage, running speed, cooldown, pickup radius, area, luck, gold, chest cost, rerolls, skip, current/max health, critical chance/damage, experience/gold gain, damage reduction, regeneration, extra targets, echo, healing on kill, thorns, DoT stacks, buff power and revives.

| Value | Meaning |
|---|---|
| Damage / speed / area | `1` = normal; `2` = twice the multiplier |
| Cooldown | `0.5` = twice as fast; `0.1` = ten times as fast |
| Pickup | Yards |
| Luck | `0.2` = +20%; `100` = +10000% |
| Critical chance / echo / reduction | Fractions: `0.5` = 50% |
| Chest cost / skip | `0` = free chest; skip is a permission flag |

Profiles live in `configs` beside the loaded DLL. Save accepted values, Freeze/Start flags and applied rules; unconfirmed editor text is not saved. Loading replaces the settings. Invalid profiles are rejected as a whole. Save as **`default.ini`** for automatic loading next time the DLL starts; supplied profiles are manual choices.

- [normal.ini](configs/normal.ini): disables automation and returns time to 1×.
- [legendary_16x.ini](configs/legendary_16x.ini): 16× time, automatic choices, legendary-set rewards and selected frozen values.
- [party_boost.ini](configs/party_boost.ini): all 25 fields configured for host-controlled party sharing at 1×.

Numbers are **absolute targets**, not additive bonuses: damage `20` means a 20× damage multiplier.

## Automatic rewards and time

Spell and jewelry auto-choice have separate switches and priorities. Use stable keys from [reference_keys.txt](configs/reference_keys.txt), such as `wrath`, `hurricane`, `chosen` or `legendary:wrath`. Earlier keys win, then higher rarity, then the first card. Unmatched cards are ranked by rarity. Delay is **50–5000 ms of real time**. Only open offers are processed; stored level-ups must first be opened using the game's own button.

The final-boss option awards the **current class's** legendary armor/weapon set from the Ahn'Qiraj/Naxxramas table after the final boss of **any zone**: 9 items for most classes, 10 for Rogue and Shaman. Items enter the equipment collection through the normal loot handler. This is separate from run jewelry and spell rarity.

Time scaling uses native simulation substeps. At 16×, 16 active game minutes take roughly one real minute when performance permits. Pauses and reward selection retain their normal stops; auto-choice delay remains real time. CPU load increases, so exact elapsed time depends on performance.

## Co-op: the room host only

Enable **Apply stats to the whole party (host only)** and apply the rules in your own room. The same configured values and Freeze/Start rules then apply to its characters. Friends retain their classes, inventories, equipment, spells and progression; these are not copied from the host. This switch shares numeric overrides, not auto-choice or inventory contents.

Loading the trainer only as a guest in someone else's room cannot change the host's simulation. The panel displays the host character's current values and party diagnostics. Real co-op sessions with friends have **not yet been verified**; automated party tests use owned fixtures and native-helper stubs.

## Offline signature scan

The ZIP includes `signature_scan.exe` and the `signatures` profiles; they are not separate release assets. From the extracted archive directory, inspect an EXE without launching the game or loading the DLL:

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-0.132.0.ini --output scan.json
```

Profiles use IDA-style hexadecimal bytes with `?` / `??` whole-byte wildcards. Each rule must have exactly one match. Resolution modes report the match itself, a RIP-relative target, the containing function from `.pdata`, or a signed field displacement. UTF-8 JSON includes the hashes, match status, file offset, RVA/VA and any field displacement.

A hash mismatch is reported as `hash_matches: false` and does not prevent offline scanning. A code RVA identifies an instruction or function, not a floating-point stat value; a field displacement is an object offset, not a live address. The scanner can inspect other builds, but matching patterns alone do not establish a compatible layout or ABI and do not make future game versions automatically supported.

## Build and release

Use Visual Studio 2022 with C++ x64 tools, Windows SDK and CMake 3.24+. From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions targets Windows 2022 x64 Release for pushes and pull requests. Version tags `vX.Y.Z` publish release assets after their build and tests succeed. Full live runs covering every feature remain unverified.

## Dependencies

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b) is MIT-licensed; [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4) uses the BSD 2-Clause license. Bundled fonts retain their upstream licenses. Keep included license notices when redistributing. Win32, Direct3D 11 and BCrypt come from the Windows SDK.
