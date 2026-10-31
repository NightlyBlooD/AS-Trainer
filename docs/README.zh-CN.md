# AS-Trainer 1.0.0

[English](../README.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md) · [Українська](README.uk.md) · [简体中文](README.zh-CN.md)

适用于 Windows x64 版 Azeroth Survivors 的 C++20 / Dear ImGui 修改器。支持修改 25 项角色属性、预设下一局参数、保存 INI 配置、自动选择奖励、获取职业传说套装，以及 1–16× 模拟速度。

## 下载与启动

从 [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases) 下载 **`AS-Trainer-1.0.0-windows-x64.zip`**。请完整解压，保留同目录下的 `azs_overlay.dll`、`load_overlay.exe`、`load_overlay.cmd`、`signature_scan.exe`、`configs`、`signatures`、`fonts`、文档和许可证。发布页面也单独提供 DLL、加载器和 `SHA256SUMS.txt`。

1. 以窗口或无边框窗口模式启动受支持的游戏版本。
2. 以与游戏相同的权限运行 `load_overlay.cmd` 或 `load_overlay.exe`。
3. 按 **Insert** 显示或隐藏面板。手动设置数值，或在**配置**中加载预设。修改自动化选项后，点击**应用规则**。
4. 按 **End** 取消待执行写入、锁定和“开局”选项，关闭自动化与全队模式，将时间恢复为 1×，并隐藏面板。已经修改的游戏数值和配置文件会保留。

运行多个游戏进程时可使用 `load_overlay.exe --pid 12345`。DLL 会一直驻留到游戏退出，因此**更新前必须重启游戏**。再次运行加载器不会替换已加载的 DLL。

## 兼容版本与语言

支持的可执行文件配置通过 `AzerothSurvivors.exe` 的完整 SHA-256 识别：

| 游戏版本 | SHA-256 |
|---|---|
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

DLL 要求已验证的 EXE 哈希、对象字段布局和原生调用约定（ABI），并在安装钩子前检查关键代码字节。其他构建版本会被拒绝，不会修改磁盘上的 EXE。当前配置已根据可执行文件和离线测试对象核验；覆盖全部功能的完整真实对局和真实联机仍待验证。

界面语言跟随客户端的 `language.txt`，通常位于 `%APPDATA%\wenilla-rogue\language.txt`。支持英语、俄语、德语、法语、西班牙语、乌克兰语和简体中文。未知值回退到英语。语言变更会在面板运行期间刷新，无需重启修改器。

## 数值与配置

**应用**提交一次绝对目标值。在菜单中操作时，会为下一局准备该数值并启用**开局**。**锁定**会在游戏重新计算属性后再次应用数值，同时启用“开局”。**开局**会在每局开始时应用已配置的数值；关闭它后，该属性的锁定不会延续到下一局。游戏提供有效角色后才会显示当前值。

25 项参数包括：伤害、跑速、冷却、拾取半径、作用范围、幸运、金币、宝箱费用、重选次数、跳过许可、当前/最大生命值、暴击率/暴击伤害、经验/金币获取、减伤、生命恢复、额外目标、回响、击杀治疗、反伤、持续伤害层数、增益强度和复活次数。

| 参数 | 单位与含义 |
|---|---|
| 伤害 / 跑速 / 范围 | `1` 为正常；`2` 为两倍倍率 |
| 冷却 | `0.5` 为两倍速度；`0.1` 为十倍速度 |
| 拾取半径 | 码 |
| 幸运 | `0.2` 表示 +20%；`100` 表示 +10000% |
| 暴击率 / 回响 / 减伤 | 小数比例：`0.5` 表示 50% |
| 宝箱费用 / 跳过 | `0` 为免费宝箱；跳过是许可开关 |

配置保存在已加载 DLL 旁的 `configs` 文件夹中。保存内容包括已确认数值、锁定/开局选项和已应用规则，不包含编辑框中尚未确认的输入。加载会替换修改器设置；无效配置会被整份拒绝。保存为 **`default.ini`** 后，下次加载 DLL 时会自动读取。附带预设需手动加载：

- [normal.ini](../configs/normal.ini)：关闭自动化，时间恢复 1×。
- [legendary_16x.ini](../configs/legendary_16x.ini)：16× 时间、自动选择、传说套装奖励及部分锁定数值。
- [party_boost.ini](../configs/party_boost.ini)：为房主的全队配置全部 25 项参数，时间为 1×。

数值是**绝对目标值**，不是额外加成。例如伤害 `20` 表示 20× 伤害倍率。

## 奖励与时间

法术和饰品拥有独立的自动选择开关及优先级。请使用 [reference_keys.txt](../configs/reference_keys.txt) 中的内部键名，例如 `wrath`、`hurricane`、`chosen` 或 `legendary:wrath`。先比较键名在列表中的顺序，再比较稀有度，仍相同则选择第一张卡。未匹配优先级时按稀有度选择。延迟为 **50–5000 毫秒实际时间**。仅处理已经打开的奖励界面；储存的升级选择需先通过游戏原生按钮打开。

最终首领选项会在**任意区域**的最终首领掉落处理中，发放 Ahn'Qiraj/Naxxramas 装备表中属于**当前职业**的传说护甲与武器套装。大多数职业为 9 件，盗贼和萨满祭司为 10 件。物品通过原生掉落处理进入装备收藏。这与局内饰品及法术稀有度是不同机制。

加速通过原生模拟子步实现。性能足够时，16× 下的 16 分钟有效游戏时间约等于 1 分钟实际时间。暂停和奖励选择保留原有的停顿；自动选择延迟仍使用实际时间。CPU 负载会增加，实际用时取决于性能。

## 联机：仅限房主

在自己创建的房间中，启用**将属性应用到全队（仅房主）**并应用规则。同一组数值及锁定/开局设置将用于房间中的角色。好友仍保留自己的职业、背包、装备、法术和进度；不会复制房主的内容。该选项共享数值修改，不共享自动选择或背包内容。

仅在别人房间的访客进程中加载修改器，无法改变房主的模拟。面板显示房主角色的当前属性及队伍诊断。**尚未验证与好友的真实联机对局**；自动化队伍测试使用独立测试对象和原生函数替身。

## 离线签名扫描

ZIP 内含 `signature_scan.exe` 和 `signatures` 配置，不会将其作为单独的发布下载文件。在解压目录中运行以下命令即可检查 EXE，无需启动游戏或加载 DLL：

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-0.132.0.ini --output scan.json
```

配置使用 IDA 风格的十六进制字节，`?` / `??` 表示任意一个完整字节。每条规则必须恰好匹配一次。解析模式支持匹配位置本身、RIP 相对目标、由 `.pdata` 确定的函数起点，以及有符号字段偏移。UTF-8 JSON 报告包含哈希、匹配状态、文件偏移、RVA/VA 和适用的字段偏移。

哈希不一致会记录为 `hash_matches: false`，但不会阻止离线扫描。代码 RVA 指向指令或函数，并非浮点属性值；字段偏移是对象内的位置，不是运行中的内存地址。扫描器可以分析其他构建，但签名匹配本身不能确认字段布局或 ABI 兼容，也不会自动支持未来的游戏版本。

## 构建与发布

需要 Visual Studio 2022 的 C++ x64 工具、Windows SDK 和 CMake 3.24+。在仓库根目录运行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions 针对 push 和 pull request 使用 Windows 2022 x64 Release 构建。`vX.Y.Z` 标签会在构建及测试成功后发布产物。包含全部功能的完整真实对局仍待验证。

## 依赖与许可证

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b) 使用 MIT 许可证；[MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4) 使用 BSD 2-Clause 许可证。附带字体保留原作者许可证。再分发时请保留许可证声明。Win32、Direct3D 11 和 BCrypt 由 Windows SDK 提供。
