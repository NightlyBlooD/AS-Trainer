#include "localization.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdarg>
#include <cstring>
#include <string>

namespace azs {

void localization::initialize() noexcept {
    refresh();
}

client_language localization::current_language() noexcept {
    return language_.load(std::memory_order_acquire);
}

void localization::refresh() noexcept {
    refresh_at(GetTickCount64());
}

void localization::refresh_at(std::uint64_t now_ms) noexcept {
    auto deadline = next_poll_ms_.load(std::memory_order_relaxed);
    if (now_ms < deadline || !next_poll_ms_.compare_exchange_strong(deadline, now_ms + 1000,
            std::memory_order_acq_rel)) return;
    std::filesystem::path path;
    language_.store(language_path(path) ? read_language(path) : client_language::english,
        std::memory_order_release);
}

client_language localization::parse_language(std::string_view code) noexcept {
    while (!code.empty() && (code.front() == ' ' || code.front() == '\t' ||
            code.front() == '\r' || code.front() == '\n')) code.remove_prefix(1);
    while (!code.empty() && (code.back() == ' ' || code.back() == '\t' ||
            code.back() == '\r' || code.back() == '\n')) code.remove_suffix(1);
    if (code == "ru") return client_language::russian;
    if (code == "de") return client_language::german;
    if (code == "fr") return client_language::french;
    if (code == "es") return client_language::spanish;
    if (code == "uk") return client_language::ukrainian;
    if (code == "zh") return client_language::chinese_simplified;
    return client_language::english;
}

bool localization::language_path(std::filesystem::path& path) noexcept {
    try {
        std::array<wchar_t, 32768> buffer{};
        auto length = GetEnvironmentVariableW(L"WOW_ROGUE_SAVE", buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length >= buffer.size()) return false;
        if (length != 0) {
            path = std::filesystem::path(std::wstring_view(buffer.data(), length)).parent_path() / L"language.txt";
            return true;
        }
        length = GetEnvironmentVariableW(L"APPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) return false;
        path = std::filesystem::path(std::wstring_view(buffer.data(), length)) / L"wenilla-rogue" / L"language.txt";
        return true;
    } catch (...) {
        return false;
    }
}

client_language localization::read_language(const std::filesystem::path& path) noexcept {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return client_language::english;
    std::array<char, 128> buffer{};
    LARGE_INTEGER size{};
    DWORD read{};
    const bool complete = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
        size.QuadPart <= static_cast<LONGLONG>(buffer.size()) &&
        ReadFile(file, buffer.data(), static_cast<DWORD>(size.QuadPart), &read, nullptr) &&
        read == static_cast<DWORD>(size.QuadPart);
    CloseHandle(file);
    return complete ? parse_language(std::string_view(buffer.data(), read)) : client_language::english;
}

const char* localization::translate(const char* english) noexcept {
    if (english == nullptr) return "";
    const auto language = current_language();
    for (const auto& row : catalog()) {
        for (const auto* variant : row) {
            if (english == variant || std::strcmp(english, variant) == 0)
                return row[static_cast<std::size_t>(language)];
        }
    }
    return english;
}

void localization::visit_translations(void(*visitor)(const char*)) noexcept {
    if (visitor == nullptr) return;
    for (const auto& row : catalog()) {
        for (const auto* text : row) visitor(text);
    }
}

void localization::print(std::FILE* output, const char* english, ...) noexcept {
    if (output == nullptr || english == nullptr) return;
    std::array<char, 4096> text{};
    va_list arguments;
    va_start(arguments, english);
    std::vsnprintf(text.data(), text.size(), translate(english), arguments);
    va_end(arguments);
    const auto size = std::strlen(text.data());
    const HANDLE handle = GetStdHandle(output == stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    DWORD mode{};
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr && GetConsoleMode(handle, &mode)) {
        std::array<wchar_t, 4096> wide{};
        const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(size),
            wide.data(), static_cast<int>(wide.size()));
        DWORD written{};
        if (count > 0) WriteConsoleW(handle, wide.data(), static_cast<DWORD>(count), &written, nullptr);
    } else {
        std::fwrite(text.data(), 1, size, output);
    }
}

std::span<const localization::catalog_row> localization::catalog() noexcept {
    static constexpr catalog_row messages[] = {
        {"Compatibility check failed.", "Проверка совместимости не пройдена.", "Kompatibilitätsprüfung fehlgeschlagen.", "Échec de la vérification de compatibilité.", "Falló la comprobación de compatibilidad.", "Перевірку сумісності не пройдено.", "兼容性检查失败。"},
        {"Trainer initialization failed.", "Не удалось инициализировать тренер.", "Trainerinitialisierung fehlgeschlagen.", "Échec de l’initialisation du trainer.", "Error al inicializar el entrenador.", "Не вдалося ініціалізувати тренер.", "训练器初始化失败。"},
        {"Waiting for game verification...", "Ожидание проверки игры...", "Spielprüfung wird erwartet...", "En attente de la vérification du jeu...", "Esperando la verificación del juego...", "Очікування перевірки гри...", "正在等待游戏验证……"},
        {"Too many configured fields.", "Слишком много настроенных параметров.", "Zu viele konfigurierte Werte.", "Trop de paramètres configurés.", "Demasiados parámetros configurados.", "Забагато налаштованих параметрів.", "配置的参数过多。"},
        {"Unknown or duplicate field key: ", "Неизвестный или повторяющийся ключ параметра: ", "Unbekannter oder doppelter Parameterschlüssel: ", "Clé de paramètre inconnue ou dupliquée : ", "Clave de parámetro desconocida o duplicada: ", "Невідомий або повторний ключ параметра: ", "未知或重复的参数键："},
        {"Invalid value for field: ", "Недопустимое значение параметра: ", "Ungültiger Parameterwert: ", "Valeur de paramètre invalide : ", "Valor de parámetro no válido: ", "Неприпустиме значення параметра: ", "参数值无效："},
        {"Automation delay/time multiplier is outside its supported range.", "Задержка автоматизации или множитель времени вне допустимого диапазона.", "Automatisierungsverzögerung oder Zeitfaktor liegt außerhalb des gültigen Bereichs.", "Le délai automatique ou le multiplicateur de temps dépasse la plage autorisée.", "El retraso automático o el multiplicador de tiempo está fuera del intervalo permitido.", "Затримка автоматизації або множник часу поза допустимим діапазоном.", "自动操作延迟或时间倍率超出支持范围。"},
        {"A priority list exceeds 64 entries.", "В списке приоритетов больше 64 записей.", "Eine Prioritätsliste enthält mehr als 64 Einträge.", "Une liste de priorités dépasse 64 entrées.", "Una lista de prioridades supera las 64 entradas.", "У списку пріоритетів понад 64 записи.", "优先列表超过64项。"},
        {"Invalid stable key in a priority list.", "Недопустимый ключ в списке приоритетов.", "Ungültiger fester Schlüssel in einer Prioritätsliste.", "Clé permanente invalide dans une liste de priorités.", "Clave estable no válida en una lista de prioridades.", "Неприпустимий ключ у списку пріоритетів.", "优先列表中的固定键无效。"},
        {"Cannot format preset number.", "Не удалось отформатировать число профиля.", "Profilzahl kann nicht formatiert werden.", "Impossible de formater un nombre du profil.", "No se puede dar formato a un número del perfil.", "Не вдалося відформатувати число профілю.", "无法格式化预设数值。"},
        {"Preset is oversized or contains a NUL byte.", "Профиль слишком большой или содержит байт NUL.", "Das Profil ist zu groß oder enthält ein NUL-Byte.", "Le profil est trop volumineux ou contient un octet NUL.", "El perfil es demasiado grande o contiene un byte NUL.", "Профіль завеликий або містить байт NUL.", "预设过大或包含NUL字节。"},
        {"Malformed section at line ", "Некорректный раздел в строке ", "Fehlerhafter Abschnitt in Zeile ", "Section incorrecte à la ligne ", "Sección incorrecta en la línea ", "Некоректний розділ у рядку ", "节格式错误，行号："},
        {"Duplicate section at line ", "Повторяющийся раздел в строке ", "Doppelter Abschnitt in Zeile ", "Section dupliquée à la ligne ", "Sección duplicada en la línea ", "Повторний розділ у рядку ", "节重复，行号："},
        {"Unknown field section at line ", "Неизвестный раздел параметра в строке ", "Unbekannter Parameterabschnitt in Zeile ", "Section de paramètre inconnue à la ligne ", "Sección de parámetro desconocida en la línea ", "Невідомий розділ параметра в рядку ", "未知参数节，行号："},
        {"Unknown section at line ", "Неизвестный раздел в строке ", "Unbekannter Abschnitt in Zeile ", "Section inconnue à la ligne ", "Sección desconocida en la línea ", "Невідомий розділ у рядку ", "未知节，行号："},
        {"Expected a section/key assignment at line ", "Ожидалось присваивание ключа в разделе, строка ", "Abschnitt oder Schlüsselzuweisung erwartet, Zeile ", "Section ou affectation de clé attendue à la ligne ", "Se esperaba una sección o asignación de clave en la línea ", "Очікувалося присвоєння ключа в розділі, рядок ", "需要节或键赋值，行号："},
        {"Unknown, duplicate or invalid key/value at line ", "Неизвестный, повторный или недопустимый ключ/значение в строке ", "Unbekannter, doppelter oder ungültiger Schlüssel/Wert in Zeile ", "Clé ou valeur inconnue, dupliquée ou invalide à la ligne ", "Clave o valor desconocido, duplicado o no válido en la línea ", "Невідомий, повторний або неприпустимий ключ/значення в рядку ", "键或值未知、重复或无效，行号："},
        {"Missing [trainer] version=3.", "Отсутствует [trainer] version=3.", "[trainer] version=3 fehlt.", "[trainer] version=3 est absent.", "Falta [trainer] version=3.", "Відсутнє [trainer] version=3.", "缺少[trainer] version=3。"},
        {"Every field needs value, frozen and apply_on_start.", "Каждому параметру нужны value, frozen и apply_on_start.", "Jeder Parameter benötigt value, frozen und apply_on_start.", "Chaque paramètre nécessite value, frozen et apply_on_start.", "Cada parámetro necesita value, frozen y apply_on_start.", "Кожному параметру потрібні value, frozen та apply_on_start.", "每个参数都需要value、frozen和apply_on_start。"},
        {"Cannot allocate or parse preset storage.", "Не удалось выделить память или разобрать профиль.", "Profilspeicher kann nicht angelegt oder gelesen werden.", "Impossible d'allouer la mémoire ou d'analyser le profil.", "No se puede reservar memoria o analizar el perfil.", "Не вдалося виділити пам'ять або розібрати профіль.", "无法分配内存或解析预设。"},
        {"Cannot resolve the loaded module's config directory.", "Не удалось определить папку конфигов загруженного модуля.", "Der Konfigurationsordner des geladenen Moduls kann nicht ermittelt werden.", "Impossible de trouver le dossier de configuration du module chargé.", "No se puede localizar la carpeta de configuración del módulo cargado.", "Не вдалося визначити теку конфігурацій завантаженого модуля.", "无法确定已加载模块的配置目录。"},
        {"Cannot create or access the local configs directory.", "Не удалось создать или открыть локальную папку configs.", "Der lokale Ordner configs kann nicht erstellt oder geöffnet werden.", "Impossible de créer ou d'ouvrir le dossier local configs.", "No se puede crear o abrir la carpeta local configs.", "Не вдалося створити або відкрити локальну теку configs.", "无法创建或访问本地configs目录。"},
        {"Config directory ready; no preset was loaded automatically.", "Папка конфигов готова; профиль автоматически не загружался.", "Konfigurationsordner bereit; kein Profil wurde automatisch geladen.", "Dossier de configuration prêt ; aucun profil chargé automatiquement.", "Carpeta de configuración lista; no se cargó ningún perfil automáticamente.", "Тека конфігурацій готова; профіль автоматично не завантажувався.", "配置目录已就绪；未自动加载预设。"},
        {"Cannot initialize preset storage.", "Не удалось подготовить хранилище профилей.", "Profilspeicher kann nicht initialisiert werden.", "Impossible d'initialiser le stockage des profils.", "No se puede inicializar el almacenamiento de perfiles.", "Не вдалося підготувати сховище профілів.", "无法初始化预设存储。"},
        {"Cannot enumerate all local config presets.", "Не удалось получить полный список локальных профилей.", "Nicht alle lokalen Profile können aufgelistet werden.", "Impossible de répertorier tous les profils locaux.", "No se pueden enumerar todos los perfiles locales.", "Не вдалося отримати повний список локальних профілів.", "无法列出所有本地配置预设。"},
        {"Cannot open preset (Win32 ", "Не удалось открыть профиль (Win32 ", "Profil kann nicht geöffnet werden (Win32 ", "Impossible d'ouvrir le profil (Win32 ", "No se puede abrir el perfil (Win32 ", "Не вдалося відкрити профіль (Win32 ", "无法打开预设（Win32 "},
        {"Preset file exceeds one MiB or has an unreadable size.", "Файл профиля больше одного МиБ или его размер не читается.", "Die Profildatei ist größer als ein MiB oder ihre Größe ist nicht lesbar.", "Le profil dépasse un Mio ou sa taille est illisible.", "El archivo del perfil supera un MiB o su tamaño no se puede leer.", "Файл профілю більший за один МіБ або його розмір не читається.", "预设文件超过1 MiB或无法读取其大小。"},
        {"Cannot read the complete preset file.", "Не удалось прочитать файл профиля целиком.", "Die Profildatei kann nicht vollständig gelesen werden.", "Impossible de lire le fichier du profil en entier.", "No se puede leer el archivo del perfil completo.", "Не вдалося прочитати файл профілю повністю.", "无法完整读取预设文件。"},
        {"Cannot create a local temporary preset file.", "Не удалось создать временный локальный файл профиля.", "Eine lokale temporäre Profildatei kann nicht erstellt werden.", "Impossible de créer un fichier de profil temporaire local.", "No se puede crear un archivo temporal local del perfil.", "Не вдалося створити тимчасовий локальний файл профілю.", "无法创建本地临时预设文件。"},
        {"Cannot write and flush the complete preset; previous file preserved.", "Не удалось полностью записать профиль; прежний файл сохранён.", "Das Profil kann nicht vollständig geschrieben werden; die bisherige Datei bleibt erhalten.", "Impossible d'écrire le profil entièrement ; le fichier précédent est conservé.", "No se puede escribir el perfil completo; se conserva el archivo anterior.", "Не вдалося повністю записати профіль; попередній файл збережено.", "无法完整写入并刷新预设；原文件已保留。"},
        {"Cannot replace preset atomically (Win32 ", "Не удалось атомарно заменить профиль (Win32 ", "Profil kann nicht atomar ersetzt werden (Win32 ", "Impossible de remplacer le profil atomiquement (Win32 ", "No se puede sustituir el perfil de forma atómica (Win32 ", "Не вдалося атомарно замінити профіль (Win32 ", "无法原子替换预设（Win32 "},
        {"Choose a plain ASCII .ini filename inside configs.", "Укажите простое ASCII-имя файла .ini в папке configs.", "Wählen Sie einen einfachen ASCII-Dateinamen mit .ini im Ordner configs.", "Choisissez un nom de fichier ASCII simple avec .ini dans configs.", "Elija un nombre ASCII sencillo con .ini dentro de configs.", "Укажіть просте ASCII-ім'я файлу .ini в теці configs.", "请在configs目录中使用简单的ASCII .ini文件名。"},
        {"Saved ", "Сохранён профиль: ", "Gespeichert: ", "Enregistré : ", "Guardado: ", "Збережено профіль: ", "已保存："},
        {"Cannot serialize or save preset; previous file preserved.", "Не удалось сформировать или сохранить профиль; прежний файл сохранён.", "Profil kann nicht serialisiert oder gespeichert werden; die bisherige Datei bleibt erhalten.", "Impossible de préparer ou d'enregistrer le profil ; le fichier précédent est conservé.", "No se puede preparar o guardar el perfil; se conserva el archivo anterior.", "Не вдалося сформувати або зберегти профіль; попередній файл збережено.", "无法序列化或保存预设；原文件已保留。"},
        {"Preset rejected; current trainer settings preserved.", "Профиль отклонён; текущие настройки трейнера сохранены.", "Profil abgelehnt; die aktuellen Trainer-Einstellungen bleiben erhalten.", "Profil refusé ; les réglages actuels du trainer sont conservés.", "Perfil rechazado; se conservan los ajustes actuales del trainer.", "Профіль відхилено; поточні налаштування трейнера збережено.", "预设已拒绝；当前修改器设置已保留。"},
        {"Loaded ", "Загружен профиль: ", "Geladen: ", "Chargé : ", "Cargado: ", "Завантажено профіль: ", "已加载："},
        {"Cannot load preset; current trainer settings preserved.", "Не удалось загрузить профиль; текущие настройки трейнера сохранены.", "Profil kann nicht geladen werden; die aktuellen Trainer-Einstellungen bleiben erhalten.", "Impossible de charger le profil ; les réglages actuels du trainer sont conservés.", "No se puede cargar el perfil; se conservan los ajustes actuales del trainer.", "Не вдалося завантажити профіль; поточні налаштування трейнера збережено.", "无法加载预设；当前修改器设置已保留。"},
        {"Cannot open the game executable for SHA-256 verification.", "Не удалось открыть исполняемый файл игры для проверки SHA-256.", "Die Spieldatei kann zur SHA-256-Prüfung nicht geöffnet werden.", "Impossible d'ouvrir l'exécutable du jeu pour vérifier SHA-256.", "No se puede abrir el ejecutable del juego para verificar SHA-256.", "Не вдалося відкрити виконуваний файл гри для перевірки SHA-256.", "无法打开游戏可执行文件以验证SHA-256。"},
        {"Windows SHA-256 provider is unavailable.", "Служба SHA-256 Windows недоступна.", "Der SHA-256-Anbieter von Windows ist nicht verfügbar.", "Le fournisseur SHA-256 de Windows est indisponible.", "El proveedor SHA-256 de Windows no está disponible.", "Служба SHA-256 Windows недоступна.", "Windows SHA-256提供程序不可用。"},
        {"Windows SHA-256 context has an unsupported size.", "Неподдерживаемый размер контекста SHA-256 Windows.", "Der Windows-SHA-256-Kontext hat eine nicht unterstützte Größe.", "La taille du contexte SHA-256 de Windows n'est pas prise en charge.", "El contexto SHA-256 de Windows tiene un tamaño no compatible.", "Непідтримуваний розмір контексту SHA-256 Windows.", "Windows SHA-256上下文大小不受支持。"},
        {"Cannot initialize the executable SHA-256 calculation.", "Не удалось начать вычисление SHA-256 исполняемого файла.", "Die SHA-256-Berechnung der Spieldatei kann nicht initialisiert werden.", "Impossible d'initialiser le calcul SHA-256 de l'exécutable.", "No se puede iniciar el cálculo SHA-256 del ejecutable.", "Не вдалося почати обчислення SHA-256 виконуваного файлу.", "无法初始化可执行文件的SHA-256计算。"},
        {"Cannot read the complete game executable.", "Не удалось прочитать исполняемый файл игры целиком.", "Die Spieldatei kann nicht vollständig gelesen werden.", "Impossible de lire l'exécutable du jeu en entier.", "No se puede leer el ejecutable del juego completo.", "Не вдалося прочитати виконуваний файл гри повністю.", "无法完整读取游戏可执行文件。"},
        {"Windows SHA-256 rejected an executable data block.", "Служба SHA-256 Windows отклонила блок данных исполняемого файла.", "Windows SHA-256 hat einen Datenblock der Spieldatei abgelehnt.", "Windows SHA-256 a refusé un bloc de données de l'exécutable.", "Windows SHA-256 rechazó un bloque de datos del ejecutable.", "Служба SHA-256 Windows відхилила блок даних виконуваного файлу.", "Windows SHA-256拒绝了可执行文件的数据块。"},
        {"Cannot finalize the executable SHA-256 calculation.", "Не удалось завершить вычисление SHA-256 исполняемого файла.", "Die SHA-256-Berechnung der Spieldatei kann nicht abgeschlossen werden.", "Impossible de terminer le calcul SHA-256 de l'exécutable.", "No se puede completar el cálculo SHA-256 del ejecutable.", "Не вдалося завершити обчислення SHA-256 виконуваного файлу.", "无法完成可执行文件的SHA-256计算。"},
        {"Speed change was cancelled; other settings remain active.", "Изменение скорости отменено; остальные параметры работают.", "Geschwindigkeitsänderung abgebrochen; andere Einstellungen bleiben aktiv.", "Modification de vitesse annulée ; les autres réglages restent actifs.", "Cambio de velocidad cancelado; los demás ajustes siguen activos.", "Зміну швидкості скасовано; решта параметрів працює.", "速度修改已取消；其他设置仍然有效。"},
        {"Auto-choice stopped for this reward: the native handler returned an error.", "Автовыбор остановлен для этой награды: штатный обработчик вернул ошибку.", "Automatische Auswahl für diese Belohnung gestoppt: Der native Handler meldete einen Fehler.", "Choix automatique arrêté pour cette récompense : le traitement natif a signalé une erreur.", "Elección automática detenida para esta recompensa: el controlador nativo devolvió un error.", "Автовибір зупинено для цієї нагороди: штатний обробник повернув помилку.", "此奖励的自动选择已停止：原生处理程序返回错误。"},
        {"Hook initialization failed (%s): %s. Restart the game before retrying.", "Не удалось подключить перехват (%s): %s. Перезапустите игру перед повторной попыткой.", "Hook-Initialisierung fehlgeschlagen (%s): %s. Starten Sie das Spiel vor einem neuen Versuch neu.", "Échec de l'initialisation du hook (%s) : %s. Redémarrez le jeu avant de réessayer.", "Error al iniciar el hook (%s): %s. Reinicie el juego antes de volver a intentarlo.", "Не вдалося підключити перехоплення (%s): %s. Перезапустіть гру перед повторною спробою.", "钩子初始化失败（%s）：%s。请重启游戏后重试。"},
        {"Cannot resolve the main executable path.", "Не удалось определить путь исполняемого файла игры.", "Der Pfad der Hauptprogrammdatei kann nicht ermittelt werden.", "Impossible de trouver le chemin de l'exécutable principal.", "No se puede localizar la ruta del ejecutable principal.", "Не вдалося визначити шлях виконуваного файлу гри.", "无法确定主可执行文件的路径。"},
        {"Unsupported process: expected AzerothSurvivors.exe.", "Неподдерживаемый процесс: требуется AzerothSurvivors.exe.", "Nicht unterstützter Prozess: AzerothSurvivors.exe erwartet.", "Processus non pris en charge : AzerothSurvivors.exe attendu.", "Proceso no compatible: se esperaba AzerothSurvivors.exe.", "Непідтримуваний процес: потрібен AzerothSurvivors.exe.", "不支持的进程：需要AzerothSurvivors.exe。"},
        {"Unsupported game build: AzerothSurvivors.exe SHA-256 does not match version 0.2.0.", "Неподдерживаемая сборка: SHA-256 AzerothSurvivors.exe не соответствует версии 0.2.0.", "Nicht unterstützte Spielversion: SHA-256 von AzerothSurvivors.exe entspricht nicht Version 0.2.0.", "Version du jeu non prise en charge : le SHA-256 d'AzerothSurvivors.exe ne correspond pas à la version 0.2.0.", "Versión del juego no compatible: el SHA-256 de AzerothSurvivors.exe no coincide con la versión 0.2.0.", "Непідтримувана збірка: SHA-256 AzerothSurvivors.exe не відповідає версії 0.2.0.", "不支持的游戏版本：AzerothSurvivors.exe的SHA-256与0.2.0版本不匹配。"},
        {"Game code bytes differ from the supported build. Disable other code patches and restart the game.", "Код игры отличается от поддерживаемой сборки. Отключите другие изменения кода и перезапустите игру.", "Der Spielcode weicht von der unterstützten Version ab. Deaktivieren Sie andere Code-Patches und starten Sie das Spiel neu.", "Le code du jeu diffère de la version prise en charge. Désactivez les autres correctifs de code et redémarrez le jeu.", "El código del juego difiere de la versión compatible. Desactive otros parches de código y reinicie el juego.", "Код гри відрізняється від підтримуваної збірки. Вимкніть інші зміни коду та перезапустіть гру.", "游戏代码与支持版本不同。请禁用其他代码补丁并重启游戏。"},
        {"Reward selection handler could not be verified for this build.", "Не удалось проверить обработчик выбора награды для этой сборки.", "Der Handler zur Belohnungsauswahl konnte für diese Version nicht geprüft werden.", "Le traitement du choix des récompenses n'a pas pu être vérifié pour cette version.", "No se pudo verificar el controlador de selección de recompensas para esta versión.", "Не вдалося перевірити обробник вибору нагороди для цієї збірки.", "无法验证此版本的奖励选择处理程序。"},
        {"Legendary set support could not be initialized. Verify the game build and restart.", "Не удалось подключить выдачу легендарного комплекта. Проверьте сборку и перезапустите игру.", "Die Vergabe des legendären Sets konnte nicht initialisiert werden. Prüfen Sie die Spielversion und starten Sie neu.", "Impossible d'initialiser l'attribution de l'ensemble légendaire. Vérifiez la version du jeu et redémarrez.", "No se pudo iniciar la entrega del conjunto legendario. Verifique la versión del juego y reinícielo.", "Не вдалося підключити видачу легендарного комплекту. Перевірте збірку та перезапустіть гру.", "无法初始化传说套装功能。请检查游戏版本并重启。"},
        {"Game build verified. Party settings are enabled separately by the host.", "Сборка игры проверена. Групповые параметры отдельно включает хозяин комнаты.", "Spielversion geprüft. Gruppeneinstellungen werden separat vom Gastgeber aktiviert.", "Version du jeu vérifiée. L'hôte active les réglages du groupe séparément.", "Versión del juego verificada. El anfitrión activa los ajustes del grupo por separado.", "Збірку гри перевірено. Групові параметри окремо вмикає господар кімнати.", "游戏版本已验证。队伍设置由房主单独启用。"},
        {"Could not read the preset list.", "Не удалось прочитать список профилей.", "Die Profilliste konnte nicht gelesen werden.", "Impossible de lire la liste des profils.", "No se pudo leer la lista de perfiles.", "Не вдалося прочитати список профілів.", "无法读取预设列表。"},
        {"Legendary set was not added: loot queue validation failed. Other features remain active.", "Легендарный комплект не добавлен: очередь добычи не прошла проверку. Остальные функции работают.", "Legendäres Set nicht hinzugefügt: Prüfung der Beutewarteschlange fehlgeschlagen. Andere Funktionen bleiben aktiv.", "Ensemble légendaire non ajouté : validation de la file de butin échouée. Les autres fonctions restent actives.", "No se añadió el conjunto legendario: falló la validación de la cola de botín. Las demás funciones siguen activas.", "Легендарний комплект не додано: черга здобичі не пройшла перевірку. Решта функцій працює.", "未添加传说套装：战利品队列验证失败。其他功能仍然有效。"},
        {"One player's speed change was cancelled because native queue validation failed. Other settings remain active.", "Скорость одного участника отменена: штатная очередь не прошла проверку. Остальные параметры работают.", "Die Geschwindigkeitsänderung eines Spielers wurde wegen einer ungültigen nativen Warteschlange abgebrochen. Andere Einstellungen bleiben aktiv.", "La vitesse d'un joueur n'a pas été modifiée : validation de la file native échouée. Les autres réglages restent actifs.", "Se canceló el cambio de velocidad de un jugador porque falló la validación de la cola nativa. Los demás ajustes siguen activos.", "Зміну швидкості одного учасника скасовано: штатна черга не пройшла перевірку. Решта параметрів працює.", "一名玩家的速度修改已取消，因为原生队列验证失败。其他设置仍然有效。"},
        {"Party changes stopped: could not synchronize a player's health fields.", "Групповые изменения остановлены: не удалось синхронизировать здоровье участника.", "Gruppenänderungen gestoppt: Gesundheitswerte eines Spielers konnten nicht synchronisiert werden.", "Modifications du groupe arrêtées : impossible de synchroniser la santé d'un joueur.", "Cambios del grupo detenidos: no se pudo sincronizar la salud de un jugador.", "Групові зміни зупинено: не вдалося синхронізувати здоров'я учасника.", "队伍修改已停止：无法同步玩家生命值参数。"},
        {"Party traversal stopped after a native helper error; the host context was restored.", "Групповой обход остановлен после ошибки штатного обработчика; контекст хозяина восстановлен.", "Gruppendurchlauf nach einem Fehler im nativen Helfer gestoppt; der Gastgeberkontext wurde wiederhergestellt.", "Parcours du groupe arrêté après une erreur native ; le contexte de l'hôte a été restauré.", "Recorrido del grupo detenido tras un error nativo; se restauró el contexto del anfitrión.", "Груповий обхід зупинено після помилки штатного обробника; контекст господаря відновлено.", "原生辅助程序出错，队伍遍历已停止；房主上下文已恢复。"},
        {"Party traversal stopped: host context restoration could not be confirmed.", "Групповой обход остановлен: не удалось подтвердить восстановление контекста хозяина.", "Gruppendurchlauf gestoppt: Wiederherstellung des Gastgeberkontexts konnte nicht bestätigt werden.", "Parcours du groupe arrêté : restauration du contexte de l'hôte non confirmée.", "Recorrido del grupo detenido: no se pudo confirmar la restauración del contexto del anfitrión.", "Груповий обхід зупинено: не вдалося підтвердити відновлення контексту господаря.", "队伍遍历已停止：无法确认房主上下文是否已恢复。"},
        {"Party changes stopped: could not synchronize the host's health.", "Групповые изменения остановлены: не удалось синхронизировать здоровье хозяина.", "Gruppenänderungen gestoppt: Gesundheit des Gastgebers konnte nicht synchronisiert werden.", "Modifications du groupe arrêtées : impossible de synchroniser la santé de l'hôte.", "Cambios del grupo detenidos: no se pudo sincronizar la salud del anfitrión.", "Групові зміни зупинено: не вдалося синхронізувати здоров'я господаря.", "队伍修改已停止：无法同步房主生命值。"},
        {"Party traversal stopped: the host's native queue is unavailable.", "Групповой обход остановлен: штатная очередь хозяина недоступна.", "Gruppendurchlauf gestoppt: Die native Warteschlange des Gastgebers ist nicht verfügbar.", "Parcours du groupe arrêté : la file native de l'hôte est indisponible.", "Recorrido del grupo detenido: la cola nativa del anfitrión no está disponible.", "Груповий обхід зупинено: штатна черга господаря недоступна.", "队伍遍历已停止：房主的原生队列不可用。"},
        {"Game PID: %lu\n", "PID игры: %lu\n", "Spiel-PID: %lu\n", "PID du jeu : %lu\n", "PID del juego: %lu\n", "PID гри: %lu\n", "游戏PID：%lu\n"},
        {"Multiple games found. Use --pid <PID>.\n", "Найдено несколько игр. Укажите --pid <PID>.\n", "Mehrere Spiele gefunden. Verwenden Sie --pid <PID>.\n", "Plusieurs jeux trouvés. Utilisez --pid <PID>.\n", "Se encontraron varios juegos. Use --pid <PID>.\n", "Знайдено кілька ігор. Укажіть --pid <PID>.\n", "检测到多个游戏进程。请使用--pid <PID>。\n"},
        {"Start AzerothSurvivors.exe first, then run this loader.\n", "Сначала запустите AzerothSurvivors.exe, затем этот загрузчик.\n", "Starten Sie zuerst AzerothSurvivors.exe und danach diesen Loader.\n", "Lancez d'abord AzerothSurvivors.exe, puis ce chargeur.\n", "Inicie primero AzerothSurvivors.exe y luego este cargador.\n", "Спочатку запустіть AzerothSurvivors.exe, потім цей завантажувач.\n", "请先启动AzerothSurvivors.exe，再运行此加载器。\n"},
        {"PID %lu is not a running x64 AzerothSurvivors.exe.\n", "PID %lu не соответствует запущенному AzerothSurvivors.exe x64.\n", "PID %lu ist kein laufendes AzerothSurvivors.exe für x64.\n", "Le PID %lu ne correspond pas à AzerothSurvivors.exe x64 en cours d'exécution.\n", "El PID %lu no corresponde a AzerothSurvivors.exe x64 en ejecución.\n", "PID %lu не відповідає запущеному AzerothSurvivors.exe x64.\n", "PID %lu不是正在运行的x64版AzerothSurvivors.exe。\n"},
        {"Invalid process ID.\n", "Недопустимый идентификатор процесса.\n", "Ungültige Prozess-ID.\n", "Identifiant de processus invalide.\n", "Identificador de proceso no válido.\n", "Неприпустимий ідентифікатор процесу.\n", "进程ID无效。\n"},
        {"Usage: load_overlay.exe [--pid <PID>]\n", "Использование: load_overlay.exe [--pid <PID>]\n", "Aufruf: load_overlay.exe [--pid <PID>]\n", "Utilisation : load_overlay.exe [--pid <PID>]\n", "Uso: load_overlay.exe [--pid <PID>]\n", "Використання: load_overlay.exe [--pid <PID>]\n", "用法：load_overlay.exe [--pid <PID>]\n"},
        {"Missing sibling azs_overlay.dll.\n", "Рядом с загрузчиком отсутствует azs_overlay.dll.\n", "azs_overlay.dll fehlt neben dem Loader.\n", "azs_overlay.dll est absent du dossier du chargeur.\n", "Falta azs_overlay.dll junto al cargador.\n", "Поруч із завантажувачем відсутній azs_overlay.dll.\n", "加载器所在目录中缺少azs_overlay.dll。\n"},
        {"OpenProcess failed: %lu. Use the same privileges as the game.\n", "Ошибка OpenProcess: %lu. Запустите с теми же правами, что и игру.\n", "OpenProcess fehlgeschlagen: %lu. Verwenden Sie dieselben Rechte wie das Spiel.\n", "Échec d'OpenProcess : %lu. Utilisez les mêmes droits que le jeu.\n", "Error de OpenProcess: %lu. Use los mismos privilegios que el juego.\n", "Помилка OpenProcess: %lu. Запустіть із тими самими правами, що й гру.\n", "OpenProcess失败：%lu。请使用与游戏相同的权限。\n"},
        {"Overlay already loaded. Press Insert to toggle. Restart the game to load an updated DLL.\n", "Оверлей уже загружен. Insert — показать/скрыть. Для обновления DLL перезапустите игру.\n", "Overlay bereits geladen. Mit Insert ein-/ausblenden. Starten Sie das Spiel für eine aktualisierte DLL neu.\n", "Overlay déjà chargé. Insert pour afficher/masquer. Redémarrez le jeu pour charger une DLL mise à jour.\n", "La superposición ya está cargada. Insert para mostrar u ocultar. Reinicie el juego para cargar una DLL actualizada.\n", "Оверлей уже завантажено. Insert — показати/сховати. Для оновлення DLL перезапустіть гру.\n", "悬浮界面已加载。按Insert显示或隐藏。请重启游戏以加载更新后的DLL。\n"},
        {"Cannot prepare DLL path: %lu.\n", "Не удалось подготовить путь DLL: %lu.\n", "DLL-Pfad kann nicht vorbereitet werden: %lu.\n", "Impossible de préparer le chemin de la DLL : %lu.\n", "No se puede preparar la ruta de la DLL: %lu.\n", "Не вдалося підготувати шлях DLL: %lu.\n", "无法准备DLL路径：%lu。\n"},
        {"CreateRemoteThread failed: %lu.\n", "Ошибка CreateRemoteThread: %lu.\n", "CreateRemoteThread fehlgeschlagen: %lu.\n", "Échec de CreateRemoteThread : %lu.\n", "Error de CreateRemoteThread: %lu.\n", "Помилка CreateRemoteThread: %lu.\n", "CreateRemoteThread失败：%lu。\n"},
        {"Load pending/indeterminate; do not repeat. Restart the game if it remains stuck.\n", "Результат загрузки ещё не определён; не повторяйте её. При зависании перезапустите игру.\n", "Ladevorgang läuft oder ist unbestimmt; nicht wiederholen. Starten Sie das Spiel neu, falls er hängen bleibt.\n", "Chargement en attente ou indéterminé ; ne le relancez pas. Redémarrez le jeu s'il reste bloqué.\n", "Carga pendiente o indeterminada; no la repita. Reinicie el juego si sigue bloqueada.\n", "Результат завантаження ще не визначено; не повторюйте його. У разі зависання перезапустіть гру.\n", "加载仍在进行或结果不明；请勿重复操作。如果持续卡住，请重启游戏。\n"},
        {"DLL was not loaded (LoadLibrary result 0x%08lX).\n", "DLL не загружена (результат LoadLibrary 0x%08lX).\n", "DLL wurde nicht geladen (LoadLibrary-Ergebnis 0x%08lX).\n", "La DLL n'a pas été chargée (résultat LoadLibrary 0x%08lX).\n", "La DLL no se cargó (resultado de LoadLibrary 0x%08lX).\n", "DLL не завантажено (результат LoadLibrary 0x%08lX).\n", "DLL未加载（LoadLibrary结果0x%08lX）。\n"},
        {"Overlay loaded at 0x%llX. Press Insert to toggle; End releases all freezes.\n", "Оверлей загружен по адресу 0x%llX. Insert — показать/скрыть; End — снять все заморозки.\n", "Overlay unter 0x%llX geladen. Insert zum Ein-/Ausblenden; End hebt alle Fixierungen auf.\n", "Overlay chargé à l'adresse 0x%llX. Insert pour afficher/masquer ; End désactive tous les verrouillages.\n", "Superposición cargada en 0x%llX. Insert para mostrar u ocultar; End desactiva todos los bloqueos.\n", "Оверлей завантажено за адресою 0x%llX. Insert — показати/сховати; End — зняти всі заморожування.\n", "悬浮界面已加载至0x%llX。按Insert显示或隐藏；按End解除所有数值锁定。\n"},
        {"Unexpected loader failure.\n", "Непредвиденная ошибка загрузчика.\n", "Unerwarteter Loader-Fehler.\n", "Erreur inattendue du chargeur.\n", "Error inesperado del cargador.\n", "Непередбачена помилка завантажувача.\n", "加载器发生意外错误。\n"},
    };
    return messages;
}

} // namespace azs
