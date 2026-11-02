# AS-Trainer 1.1.2

[English](README.md) · [Русский](docs/README.ru.md) · [Deutsch](docs/README.de.md) · [Français](docs/README.fr.md) · [Español](docs/README.es.md) · [Українська](docs/README.uk.md) · [简体中文](docs/README.zh-CN.md)

A C++20 / Dear ImGui trainer for Azeroth Survivors on Windows x64. Edit 26 character fields in v0.132.0/v0.135.1 builds or 25 in v0.131.0, prepare next-run settings, save INI profiles, choose rewards automatically, obtain the class legendary set from a final boss, and advance simulation at 1–16× speed.

## Download and launch

Download **`AS-Trainer-1.1.2-windows-x64.zip`** from [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases). Extract the entire archive: keep `azs_overlay.dll`, `load_overlay.exe`, `load_overlay.cmd`, `signature_scan.exe`, `configs`, `signatures`, `fonts`, documentation and license notices together. The release also provides the DLL, loader and `SHA256SUMS.txt` separately.

1. Start the supported game in windowed or borderless mode.
2. Run `load_overlay.cmd` or `load_overlay.exe` with the same privileges as the game.
3. **Insert** shows or hides the panel. Use the **Parameters**, **Auto-choice** and **Configs** tabs. Automation switches, delay and speed apply immediately; **Apply rules** confirms only spell and jewelry priorities.
4. **End** cancels queued writes, freezes and Start flags, disables automation, God mode and party scope, returns time to 1×, and hides the panel. Existing game values and config files remain in place.

For multiple processes, use `load_overlay.exe --pid 12345`. The DLL stays loaded until the game exits: **restart the game before updating it**. Running the loader again does not replace a loaded DLL.

## Compatibility and language

Supported executables and their SHA-256 identities:

| Game version | SHA-256 |
|---|---|
| `v0.139.0 (f79c5e2b4)` | `0e8e314df4d78ab211b1bef5c48d78d713ed64260894af342f998d298d74d274` |
| `v0.136.0` | `eb744751b4e17ec1397b0cdc85a22af63b6af63c1da1d9c2861762c3c4d8947c` |
| `v0.135.1 (90898d5e3)` | `71e5f1767279db4bd428d1cd9c5d50d37759b72295386a69f9e3f6e70bc59249` |
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

Known builds retain their profiles. Other hashes use a one-time adaptive scan: all required function signatures, individual run/member offsets, participant stride and calling conventions must pass one complete embedded family certificate. The v0.139.0 and v0.135/0.136 compiler families are checked independently; partial matches are never combined. The DLL then checks the loaded image and its exact code bytes before installing hooks. Missing or ambiguous evidence refuses initialization. Compatible address/layout changes can be discovered automatically; arbitrary compiler or game-logic changes may still require a new catalog. The EXE on disk is unchanged.

The interface and diagnostics follow the client's `language.txt`, normally `%APPDATA%\wenilla-rogue\language.txt`. All 14 client languages are supported: English (`en`), Russian (`ru`), German (`de`), French (`fr`), Spanish (`es`), Ukrainian (`uk`), Simplified Chinese (`zh`), Polish (`pl`), Czech (`cs`), Brazilian Portuguese (`pt`), Romanian (`ro`), Turkish (`tr`), Japanese (`ja`) and Korean (`ko`). Unknown values fall back to English. Language changes refresh within one second while the panel is running, without restarting the trainer. The bundled font covers the translated interface and diagnostics, including Japanese and Korean.

## Values and profiles

The dark overlay opens at 500 × 460 pixels and can be resized. Tabs keep the parameter table, automation and profiles separate. Hover over labels and controls for units, ranges and explanations.

On the **Parameters** tab, finish a numeric edit with **Enter** or move focus away to queue a one-shot absolute value; boolean changes apply immediately. In a menu this prepares the next run and enables **Start**. Enabling **Hold** or **Start** immediately accepts the edited value. **Hold** also reapplies it after game recalculations and enables Start. **Start** reapplies a configured value on each new run; turning it off prevents that field's hold from carrying into the next run. Their icons show the available action: a green check enables an inactive setting; a red prohibition symbol cancels an active setting. Current values appear after the game provides an active character.

The 25 shared fields cover damage, running speed, cooldown, pickup radius, area, luck, gold, chest cost, rerolls, skip, current/max health, critical chance/damage, experience/gold gain, damage reduction, regeneration, extra targets, echo, healing on kill, buff power, DoT stacks, DoT tick speed and revives. The v0.132.0/v0.135.1 bindings add **Lifesteal** as a 26th field; legacy v0.131.0 retains 25 fields.

| Value | Meaning |
|---|---|
| Damage / speed / area | `1` = normal; `2` = twice the multiplier |
| Cooldown | `0.5` = twice as fast; `0.1` = ten times as fast |
| Pickup | Yards |
| Luck | `0.2` = +20%; `100` = +10000% |
| Critical chance / echo / reduction | Fractions: `0.5` = 50% |
| Buff power | Bonus fraction: `0` = normal; `0.2` = +20% stronger buffs |
| DoT tick speed | Bonus fraction from `0` to `1`: `0` = normal; `1` = +100% tick rate, the native cap |
| Lifesteal, modern bindings | Fraction of damage converted to healing: `0.25` = 25%; editor range `0`–`10`, not a native game cap |
| Chest cost / skip | `0` = free chest; skip is a permission flag |

Lifesteal is a computed game statistic. An accepted value overrides its native getter and is used by the game's healing path. A one-shot edit lasts until the next native stat recalculation; **Hold** preserves the override across recalculations, and **Start** reapplies it on each new run. Without an active override, the original getter runs normally. This option is available for v0.132.0/v0.135.1.

Profiles live in `configs` beside the loaded DLL. Save accepted values, Freeze/Start flags and applied rules; unconfirmed editor text is not saved. Loading replaces the settings. Invalid profiles are rejected as a whole. Save as **`default.ini`** for automatic loading next time the DLL starts; supplied profiles are manual choices.

**Buff power** and **DoT tick speed** control separate native statistics. Damage reflection is computed from the game's effects and has no separate scalar editor here.

New profiles use `[trainer] version=4`. Version 3 profiles are migrated in memory on load: `thorns` becomes `buff_power`, and the old `buff_power` becomes `dot_haste`, preserving their Hold/Start flags. Loading leaves the source file unchanged; saving writes version 4. If a migrated DoT tick speed is outside `0`–`1`, the whole profile is rejected without clamping any values.

- [normal.ini](configs/normal.ini): disables automation and returns time to 1×.
- [legendary_16x.ini](configs/legendary_16x.ini): 16× time, automatic choices, legendary-set rewards and selected frozen values.
- [party_boost.ini](configs/party_boost.ini): a version 4 profile with the 25 shared field keys for host-controlled party sharing at 1×; current-build lifesteal can be configured separately.

Numbers are **absolute targets**, not additive bonuses: damage `20` means a 20× damage multiplier.

## Automatic rewards and time

Spell and jewelry auto-choice have separate switches and priorities. Use stable keys from [reference_keys.txt](configs/reference_keys.txt), such as `wrath`, `hurricane`, `chosen` or `legendary:wrath`. Earlier keys win, then higher rarity, then the first card. Unmatched cards are ranked by rarity. Delay is **50–5000 ms of real time**. Only open offers are processed; stored level-ups must first be opened using the game's own button.

Party scope, spell/jewelry auto-choice, God mode, legendary loot, delay and game speed apply as soon as their controls change. **Apply rules** saves only the two priority lists. Unapplied priority text stays in the editor while other controls change.

**God mode** deals lethal damage equal to each enemy's remaining health when it appears, at any distance, without casting spells. Damage is credited to your character. The mode works only in single player and is blocked in network games, including locally hosted rooms. It can be enabled before starting a run and is saved in presets as `god_mode`.

God mode discovers its native damage and spell-event functions once at startup using certified signatures. It accepts new executable hashes and relocated addresses when the complete normalized functions, linked helpers, constants and core layout still match the verified contracts. The current contracts cover v0.132.0, v0.135.1, v0.136.0 and v0.139.0. Changes to enemy layout, calling conventions or damage logic require a new compatibility proof; an incomplete proof disables only God mode.

The final-boss option awards the **current class's** legendary armor/weapon set from the Ahn'Qiraj/Naxxramas table after the final boss of **any zone**: 9 items for most classes, 10 for Rogue and Shaman. Items enter the equipment collection through the normal loot handler. This is separate from run jewelry and spell rarity.

Time scaling uses native simulation substeps. At 16×, 16 active game minutes take roughly one real minute when performance permits. Pauses and reward selection retain their normal stops; auto-choice delay remains real time. CPU load increases, so exact elapsed time depends on performance.

Hover over game speed for the mean owner-call time, bridge time and substeps per intercepted frame over the latest one-second sample. Nested hooks are included in owner time; native helpers called by the trainer are included in bridge time. These are elapsed times across call boundaries, not a full CPU/GPU profiler.

## Co-op: the room host only

Enable **Apply stats to the whole party (host only)** in your own room; the switch takes effect immediately. The same configured values and Hold/Start rules then apply to its characters. Friends retain their classes, inventories, equipment, spells and progression; these are not copied from the host. This switch shares numeric overrides, not auto-choice or inventory contents.

Loading the trainer only as a guest in someone else's room cannot change the host's simulation. The panel displays the host character's current values and party diagnostics.

## Offline signature scan

The ZIP includes `signature_scan.exe` and the `signatures` profiles; they are not separate release assets. From the extracted archive directory, inspect an EXE without launching the game or loading the DLL:

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-0.132.0.ini --output scan.json
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --auto --output compatibility.json
```

Profiles use IDA-style hexadecimal bytes with `?` / `??` whole-byte wildcards. Each rule must have exactly one match. Resolution modes report the match itself, a RIP-relative target, the containing function from `.pdata`, or a signed field displacement. UTF-8 JSON includes the hashes, match status, file offset, RVA/VA and any field displacement.

With `--profile`, a hash mismatch is reported as `hash_matches: false` and does not prevent diagnostic scanning; matching those rules alone grants no runtime admission. `--auto` uses the embedded catalogs and reports the complete offline layout/ABI decision as `offline_compatible`; successful live-image verification is still required before hooks. A code RVA identifies an instruction or function, not a floating-point stat value; a field displacement is an object offset, not a live address.

## Build and release

Use Visual Studio 2022 with C++ x64 tools, Windows SDK and CMake 3.24+. From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions targets Windows 2022 x64 Release for pushes and pull requests. Version tags `vX.Y.Z` publish release assets after their build and tests succeed.

## Dependencies

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b) is MIT-licensed; [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4) uses the BSD 2-Clause license. Bundled fonts retain their upstream licenses. Keep included license notices when redistributing. Win32, Direct3D 11 and BCrypt come from the Windows SDK.
