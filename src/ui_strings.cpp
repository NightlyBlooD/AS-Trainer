#include "ui_strings.hpp"
#include "build_version.hpp"

#include <cstring>
#include <iterator>

namespace azs {
namespace {

constexpr const char* text_catalog[][7] = {
    {"Could not create the trainer window.", "Не удалось создать окно тренера.", "Das Trainerfenster konnte nicht erstellt werden.", "Impossible de créer la fenêtre du trainer.", "No se pudo crear la ventana del entrenador.", "Не вдалося створити вікно тренера.", "无法创建修改器窗口。"}, // overlay_create_failed
    {"Trainer graphics device error.", "Ошибка графического устройства окна тренера.", "Grafikgerätefehler des Trainers.", "Erreur du périphérique graphique du trainer.", "Error del dispositivo gráfico del entrenador.", "Помилка графічного пристрою тренера.", "修改器图形设备出错。"}, // graphics_failed
    {"The trainer window stopped after an internal error.", "Работа окна тренера остановлена после внутренней ошибки.", "Das Trainerfenster wurde nach einem internen Fehler beendet.", "La fenêtre du trainer s’est arrêtée après une erreur interne.", "La ventana del entrenador se detuvo tras un error interno.", "Вікно тренера зупинено через внутрішню помилку.", "修改器窗口因内部错误而停止。"}, // overlay_stopped
    {"Azeroth Survivors — Trainer " AS_TRAINER_VERSION "###azs_trainer", "Azeroth Survivors — Тренер " AS_TRAINER_VERSION "###azs_trainer", "Azeroth Survivors — Trainer " AS_TRAINER_VERSION "###azs_trainer", "Azeroth Survivors — Trainer " AS_TRAINER_VERSION "###azs_trainer", "Azeroth Survivors — Entrenador " AS_TRAINER_VERSION "###azs_trainer", "Azeroth Survivors — Тренер " AS_TRAINER_VERSION "###azs_trainer", "Azeroth Survivors — 修改器 " AS_TRAINER_VERSION "###azs_trainer"}, // title
    {"Run active · Level %u", "Забег активен · Уровень %u", "Durchlauf aktiv · Stufe %u", "Partie en cours · Niveau %u", "Partida activa · Nivel %u", "Забіг активний · Рівень %u", "挑战进行中 · 等级 %u"}, // run_active_format
    {"Configure the next run", "Настройка следующего забега", "Nächsten Durchlauf vorbereiten", "Configurer la prochaine partie", "Configurar la siguiente partida", "Налаштування наступного забігу", "配置下一次挑战"}, // next_run
    {"The trainer is not ready to change values", "Тренер пока не готов к изменению значений", "Der Trainer ist noch nicht bereit, Werte zu ändern", "Le trainer n’est pas encore prêt à modifier les valeurs", "El entrenador aún no está listo para cambiar valores", "Тренер ще не готовий змінювати значення", "修改器尚未准备好更改数值"}, // not_ready
    {"Insert — show/hide · End — stop and hide: holds, party, auto-choice, loot and speed", "Insert — показать / скрыть · End — стоп и скрыть: фиксации, группа, автовыбор, добыча и ускорение", "Insert — anzeigen/ausblenden · End — stoppen und ausblenden: Fixierungen, Gruppe, Auswahl, Beute und Tempo", "Insert — afficher/masquer · End — arrêter et masquer : maintien, groupe, choix, butin et vitesse", "Insert — mostrar/ocultar · End — detener y ocultar: fijaciones, grupo, selección, botín y velocidad", "Insert — показати / сховати · End — зупинити й сховати: фіксації, групу, автовибір, здобич і прискорення", "Insert — 显示/隐藏 · End — 停止并隐藏：锁定、队伍、自动选择、掉落及加速"}, // hotkeys
    {"Reset rules and holds###reset_all", "Сбросить правила и фиксации###reset_all", "Regeln und Fixierungen zurücksetzen###reset_all", "Réinitialiser règles et maintien###reset_all", "Restablecer reglas y fijaciones###reset_all", "Скинути правила та фіксації###reset_all", "重置规则与锁定###reset_all"}, // reset
    {"Rules, speed, holds and pending changes have been reset.", "Правила, ускорение, фиксации и отложенные изменения сброшены.", "Regeln, Tempo, Fixierungen und ausstehende Änderungen wurden zurückgesetzt.", "Règles, vitesse, maintien et modifications en attente réinitialisés.", "Se restablecieron las reglas, la velocidad, las fijaciones y los cambios pendientes.", "Правила, прискорення, фіксації та відкладені зміни скинуто.", "已重置规则、加速、锁定及待处理更改。"}, // reset_done
    {"Also disables Start flags, party mode, auto-choice and time acceleration.", "Также отключает отметки Старт, режим группы, автовыбор и ускорение времени.", "Deaktiviert auch Start-Markierungen, Gruppenmodus, automatische Auswahl und Zeitbeschleunigung.", "Désactive aussi les options Départ, le mode groupe, le choix automatique et l’accélération.", "También desactiva las marcas Inicio, el modo de grupo, la selección automática y la aceleración.", "Також вимикає позначки Старт, режим групи, автовибір та прискорення часу.", "同时禁用开局应用、队伍模式、自动选择和时间加速。"}, // reset_hint
    {"Hide###hide_overlay", "Скрыть###hide_overlay", "Ausblenden###hide_overlay", "Masquer###hide_overlay", "Ocultar###hide_overlay", "Сховати###hide_overlay", "隐藏###hide_overlay"}, // hide
    {"Hold keeps a value applied; Start applies it at the beginning of every run.", "Фикс. — удерживать значение; Старт — применять при каждом новом забеге.", "Fixieren hält einen Wert; Start wendet ihn zu Beginn jedes Durchlaufs an.", "Maintien conserve une valeur ; Départ l’applique au début de chaque partie.", "Fijar mantiene un valor; Inicio lo aplica al empezar cada partida.", "Фікс. — утримувати значення; Старт — застосовувати на початку кожного забігу.", "锁定：持续应用数值；开局：每次挑战开始时应用。"}, // hold_start_hint
    {"Configs###configs_section", "Конфиги###configs_section", "Konfigurationen###configs_section", "Configurations###configs_section", "Configuraciones###configs_section", "Конфіги###configs_section", "配置文件###configs_section"}, // configs
    {"The filename must be shorter than 128 bytes.", "Имя должно содержать меньше 128 байт.", "Der Dateiname muss kürzer als 128 Bytes sein.", "Le nom du fichier doit faire moins de 128 octets.", "El nombre debe tener menos de 128 bytes.", "Ім’я має містити менше ніж 128 байтів.", "文件名必须少于 128 字节。"}, // filename_too_long
    {"No saved configs", "Сохранённых конфигов нет", "Keine gespeicherten Konfigurationen", "Aucune configuration enregistrée", "No hay configuraciones guardadas", "Збережених конфігів немає", "没有已保存的配置"}, // no_configs
    {"Refresh###refresh_configs", "Обновить###refresh_configs", "Aktualisieren###refresh_configs", "Actualiser###refresh_configs", "Actualizar###refresh_configs", "Оновити###refresh_configs", "刷新###refresh_configs"}, // refresh
    {"An .ini filename in the configs folder beside the trainer.", "Имя файла .ini в папке configs рядом с тренером.", "Name einer .ini-Datei im Ordner configs neben dem Trainer.", "Nom du fichier .ini dans le dossier configs à côté du trainer.", "Nombre del archivo .ini en la carpeta configs junto al entrenador.", "Ім’я файлу .ini у папці configs поруч із тренером.", "修改器旁 configs 文件夹中的 .ini 文件名。"}, // filename_hint
    {"Load###load_config", "Загрузить###load_config", "Laden###load_config", "Charger###load_config", "Cargar###load_config", "Завантажити###load_config", "加载###load_config"}, // load
    {"Save###save_config", "Сохранить###save_config", "Speichern###save_config", "Enregistrer###save_config", "Guardar###save_config", "Зберегти###save_config", "保存###save_config"}, // save
    {"Saves applied values and rules.", "Сохраняются применённые значения и правила.", "Speichert angewendete Werte und Regeln.", "Enregistre les valeurs et règles appliquées.", "Guarda los valores y las reglas aplicados.", "Зберігаються застосовані значення та правила.", "保存已应用的数值与规则。"}, // save_hint
    {"Auto-choice and loot###automation_section", "Автовыбор и добыча###automation_section", "Automatische Auswahl und Beute###automation_section", "Choix automatique et butin###automation_section", "Selección automática y botín###automation_section", "Автовибір і здобич###automation_section", "自动选择与掉落###automation_section"}, // automation
    {"Apply stats to the whole party (host only)", "Применять характеристики ко всей группе (только хост)", "Werte auf die ganze Gruppe anwenden (nur Host)", "Appliquer les caractéristiques à tout le groupe (hôte uniquement)", "Aplicar atributos a todo el grupo (solo anfitrión)", "Застосовувати характеристики до всієї групи (лише хост)", "为整个队伍应用属性（仅房主）"}, // party_toggle
    {"Host: %u players; party applications: %llu", "Хост: %u игроков; применений к участникам: %llu", "Host: %u Spieler; Gruppenanwendungen: %llu", "Hôte : %u joueurs ; applications au groupe : %llu", "Anfitrión: %u jugadores; aplicaciones al grupo: %llu", "Хост: %u гравців; застосувань до учасників: %llu", "房主：%u 名玩家；队伍应用次数：%llu"}, // party_status_format
    {"Waiting for a local host run", "Ожидание локального забега хоста", "Warten auf einen lokalen Host-Durchlauf", "En attente d’une partie locale de l’hôte", "Esperando una partida local del anfitrión", "Очікування локального забігу хоста", "等待房主的本地挑战"}, // party_wait
    {"All room members use the same field values and Hold/Start flags. Each hero keeps their own class, gear and inventory. This rule has no effect for a guest.", "Для всей комнаты используются общие значения полей и отметки Фикс./Старт. Каждый герой сохраняет свой класс, экипировку и инвентарь. У гостя комнаты это правило не действует.", "Alle im Raum verwenden dieselben Werte und Fixieren/Start-Markierungen. Jeder Held behält Klasse, Ausrüstung und Inventar. Bei Gästen hat diese Regel keine Wirkung.", "Tous les membres utilisent les mêmes valeurs et options Maintien/Départ. Chaque héros conserve sa classe, son équipement et son inventaire. Cette règle est sans effet chez un invité.", "Todos usan los mismos valores y marcas Fijar/Inicio. Cada héroe conserva su clase, equipo e inventario. Esta regla no funciona para un invitado.", "Для всієї кімнати діють спільні значення й позначки Фікс./Старт. Кожен герой зберігає свій клас, спорядження та інвентар. У гостя кімнати це правило не діє.", "房间成员共用数值及锁定/开局设置。每位英雄保留自己的职业、装备和背包。访客启用此规则无效。"}, // party_help
    {"Choose spells automatically###auto_spells", "Автоматически выбирать заклинания###auto_spells", "Zauber automatisch wählen###auto_spells", "Choisir les sorts automatiquement###auto_spells", "Elegir hechizos automáticamente###auto_spells", "Автоматично вибирати заклинання###auto_spells", "自动选择法术###auto_spells"}, // auto_spells
    {"Choose jewelry automatically###auto_jewelry", "Автоматически выбирать украшения###auto_jewelry", "Schmuck automatisch wählen###auto_jewelry", "Choisir les bijoux automatiquement###auto_jewelry", "Elegir joyas automáticamente###auto_jewelry", "Автоматично вибирати прикраси###auto_jewelry", "自动选择饰品###auto_jewelry"}, // auto_jewelry
    {"Full legendary set from the final boss (any zone)", "Весь легендарный комплект с финального босса (любая зона)", "Vollständiges legendäres Set vom Endboss (jede Zone)", "Ensemble légendaire complet du boss final (toute zone)", "Conjunto legendario completo del jefe final (cualquier zona)", "Повний легендарний комплект із фінального боса (будь-яка зона)", "最终首领掉落整套传说装备（任意区域）"}, // legendary_toggle
    {"The legendary set is added to the permanent gear collection.", "Легендарный комплект добавляется в постоянную коллекцию экипировки.", "Das legendäre Set wird der dauerhaften Ausrüstungssammlung hinzugefügt.", "L’ensemble légendaire est ajouté à la collection permanente d’équipement.", "El conjunto legendario se añade a la colección permanente de equipo.", "Легендарний комплект додається до постійної колекції спорядження.", "传说套装将加入永久装备收藏。"}, // legendary_help
    {"Choice delay, ms (50–5000)###choice_delay", "Задержка выбора, мс (50–5000)###choice_delay", "Auswahlverzögerung, ms (50–5000)###choice_delay", "Délai de choix, ms (50–5000)###choice_delay", "Retraso de elección, ms (50–5000)###choice_delay", "Затримка вибору, мс (50–5000)###choice_delay", "选择延迟，毫秒（50–5000）###choice_delay"}, // choice_delay
    {"Game time###game_time", "Игровое время###game_time", "Spielzeit###game_time", "Temps de jeu###game_time", "Tiempo de juego###game_time", "Ігровий час###game_time", "游戏时间###game_time"}, // game_time
    {"16x###speed_16x", "16x###speed_16x", "16x###speed_16x", "16x###speed_16x", "16x###speed_16x", "16x###speed_16x", "16x###speed_16x"}, // speed_16x
    {"16 game minutes per real minute (excluding pauses/choices)", "16 игровых минут за 1 реальную (без пауз/выбора)", "16 Spielminuten pro realer Minute (ohne Pausen/Auswahl)", "16 minutes de jeu par minute réelle (hors pauses/choix)", "16 minutos de juego por minuto real (sin pausas/elecciones)", "16 ігрових хвилин за 1 реальну (без пауз/вибору)", "每 1 分钟现实时间推进 16 分钟游戏时间（不含暂停/选择）"}, // time_help
    {"Priority: the first matching key in the list, then rarity. Enter one key per line or separate keys with commas.", "Приоритет: первый совпавший ключ в списке, затем редкость. Ключи указываются по одному на строку или через запятую.", "Priorität: zuerst der erste passende Schlüssel in der Liste, dann Seltenheit. Ein Schlüssel pro Zeile oder durch Kommas getrennt.", "Priorité : première clé correspondante de la liste, puis rareté. Une clé par ligne ou séparées par des virgules.", "Prioridad: primera clave coincidente de la lista y después rareza. Una clave por línea o separadas por comas.", "Пріоритет: перший відповідний ключ у списку, потім рідкість. По одному ключу на рядок або через кому.", "优先使用列表中最靠前的匹配键，其次比较稀有度。每行一个键，或用逗号分隔。"}, // priority_help
    {"Spell priorities", "Приоритеты заклинаний", "Zauberprioritäten", "Priorités des sorts", "Prioridades de hechizos", "Пріоритети заклинань", "法术优先级"}, // spell_priority_title
    {"Jewelry priorities", "Приоритеты украшений", "Schmuckprioritäten", "Priorités des bijoux", "Prioridades de joyas", "Пріоритети прикрас", "饰品优先级"}, // jewelry_priority_title
    {"Up to 64 keys per list, 128 bytes per key. An empty list uses rarity.", "Не более 64 ключей в списке, до 128 байт на ключ. Пустой список — по редкости.", "Bis zu 64 Schlüssel pro Liste, 128 Bytes je Schlüssel. Eine leere Liste verwendet die Seltenheit.", "64 clés maximum par liste, 128 octets par clé. Une liste vide utilise la rareté.", "Hasta 64 claves por lista y 128 bytes por clave. Una lista vacía usa la rareza.", "До 64 ключів у списку, до 128 байтів на ключ. Порожній список — за рідкістю.", "每个列表最多 64 个键，每键最多 128 字节。列表为空时按稀有度选择。"}, // priority_limits
    {"Apply rules###apply_rules", "Применить правила###apply_rules", "Regeln anwenden###apply_rules", "Appliquer les règles###apply_rules", "Aplicar reglas###apply_rules", "Застосувати правила###apply_rules", "应用规则###apply_rules"}, // apply_rules
    {"Party, auto-choice, loot and game-time rules applied.", "Правила группы, автовыбора, добычи и игрового времени применены.", "Regeln für Gruppe, Auswahl, Beute und Spielzeit angewendet.", "Règles de groupe, choix, butin et temps de jeu appliquées.", "Se aplicaron las reglas de grupo, selección, botín y tiempo de juego.", "Правила групи, автовибору, здобичі та ігрового часу застосовано.", "已应用队伍、自动选择、掉落与游戏时间规则。"}, // rules_applied
    {"Rules rejected: check the keys and game-time range 1–16.", "Правила отклонены: проверьте ключи и диапазон игрового времени 1–16.", "Regeln abgelehnt: Schlüssel und Spielzeitbereich 1–16 prüfen.", "Règles refusées : vérifiez les clés et le temps de jeu entre 1 et 16.", "Reglas rechazadas: comprueba las claves y el intervalo de tiempo 1–16.", "Правила відхилено: перевірте ключі та діапазон ігрового часу 1–16.", "规则被拒绝：请检查键及游戏时间范围 1–16。"}, // rules_rejected
    {"A priority key exceeds 128 bytes.", "Ключ приоритета длиннее 128 байт.", "Ein Prioritätsschlüssel überschreitet 128 Bytes.", "Une clé de priorité dépasse 128 octets.", "Una clave de prioridad supera los 128 bytes.", "Ключ пріоритету довший за 128 байтів.", "优先级键超过 128 字节。"}, // priority_key_long
    {"A priority list may contain at most 64 keys.", "В одном списке допускается не более 64 ключей приоритета.", "Eine Prioritätsliste darf höchstens 64 Schlüssel enthalten.", "Une liste de priorité peut contenir 64 clés maximum.", "Una lista de prioridad puede contener hasta 64 claves.", "Один список може містити не більше ніж 64 ключі пріоритету.", "每个优先级列表最多包含 64 个键。"}, // priority_too_many
    {"Parameter###col_parameter", "Параметр###col_parameter", "Parameter###col_parameter", "Paramètre###col_parameter", "Parámetro###col_parameter", "Параметр###col_parameter", "参数###col_parameter"}, // col_parameter
    {"Current###col_current", "Сейчас###col_current", "Aktuell###col_current", "Actuel###col_current", "Actual###col_current", "Зараз###col_current", "当前###col_current"}, // col_current
    {"Set to###col_desired", "Задать###col_desired", "Zielwert###col_desired", "Définir###col_desired", "Establecer###col_desired", "Задати###col_desired", "设为###col_desired"}, // col_desired
    {"Hold###col_freeze", "Фикс.###col_freeze", "Fix.###col_freeze", "Maintien###col_freeze", "Fijar###col_freeze", "Фікс.###col_freeze", "锁定###col_freeze"}, // col_freeze
    {"Start###col_start", "Старт###col_start", "Start###col_start", "Départ###col_start", "Inicio###col_start", "Старт###col_start", "开局###col_start"}, // col_start
    {"Key: %s\nAllowed range: %.3f … %.3f", "Ключ: %s\nДопустимые значения: %.3f … %.3f", "Schlüssel: %s\nWertebereich: %.3f … %.3f", "Clé : %s\nPlage autorisée : %.3f … %.3f", "Clave: %s\nIntervalo permitido: %.3f … %.3f", "Ключ: %s\nДопустимі значення: %.3f … %.3f", "键：%s\n允许范围：%.3f … %.3f"}, // range_format
    {"—", "—", "—", "—", "—", "—", "—"}, // unavailable
    {"Yes", "Да", "Ja", "Oui", "Sí", "Так", "是"}, // yes
    {"No", "Нет", "Nein", "Non", "No", "Ні", "否"}, // no
    {"Pending###apply_value", "Ожидает###apply_value", "Wartet###apply_value", "En attente###apply_value", "Pendiente###apply_value", "Очікує###apply_value", "待处理###apply_value"}, // pending
    {"Apply###apply_value", "Применить###apply_value", "Anwenden###apply_value", "Appliquer###apply_value", "Aplicar###apply_value", "Застосувати###apply_value", "应用###apply_value"}, // apply
    {"Value queued for the active run.", "Значение отправлено в активный забег.", "Wert für den aktiven Durchlauf vorgemerkt.", "Valeur envoyée à la partie en cours.", "Valor enviado a la partida activa.", "Значення надіслано до активного забігу.", "数值已排队应用到当前挑战。"}, // value_sent
    {"Value prepared for the next run.", "Значение подготовлено для следующего забега.", "Wert für den nächsten Durchlauf vorbereitet.", "Valeur préparée pour la prochaine partie.", "Valor preparado para la siguiente partida.", "Значення підготовлено для наступного забігу.", "数值已为下一次挑战准备就绪。"}, // value_prepared
    {"Change rejected: check the allowed range.", "Изменение отклонено: проверьте допустимый диапазон.", "Änderung abgelehnt: Wertebereich prüfen.", "Modification refusée : vérifiez la plage autorisée.", "Cambio rechazado: comprueba el intervalo permitido.", "Зміну відхилено: перевірте допустимий діапазон.", "更改被拒绝：请检查允许范围。"}, // value_rejected
    {"Hold rejected: check the allowed range.", "Фиксация отклонена: проверьте допустимый диапазон.", "Fixierung abgelehnt: Wertebereich prüfen.", "Maintien refusé : vérifiez la plage autorisée.", "Fijación rechazada: comprueba el intervalo permitido.", "Фіксацію відхилено: перевірте допустимий діапазон.", "锁定被拒绝：请检查允许范围。"}, // freeze_rejected
    {"Start setting rejected: enter an allowed value first.", "Настройка старта отклонена: сначала задайте допустимое значение.", "Start-Einstellung abgelehnt: zuerst einen gültigen Wert eingeben.", "Option Départ refusée : saisissez d’abord une valeur autorisée.", "Ajuste de Inicio rechazado: introduce primero un valor permitido.", "Налаштування старту відхилено: спочатку задайте допустиме значення.", "开局设置被拒绝：请先输入允许的数值。"}, // start_rejected
    {"Save this value and apply it at the beginning of every run.", "Сохранить заданное значение и применять при каждом новом забеге.", "Diesen Wert speichern und zu Beginn jedes Durchlaufs anwenden.", "Enregistrer cette valeur et l’appliquer au début de chaque partie.", "Guardar este valor y aplicarlo al empezar cada partida.", "Зберегти значення й застосовувати на початку кожного забігу.", "保存此数值，并在每次挑战开始时应用。"}, // start_hint
    {"multiplier, 1 = normal", "множитель, 1 = обычный", "Multiplikator, 1 = normal", "multiplicateur, 1 = normal", "multiplicador, 1 = normal", "множник, 1 = звичайний", "倍率，1 = 正常"}, // unit_multiplier_one
    {"time: 0.5 = twice as fast", "время: 0.5 = вдвое быстрее", "Zeit: 0.5 = doppelt so schnell", "temps : 0.5 = deux fois plus rapide", "tiempo: 0.5 = el doble de rápido", "час: 0.5 = удвічі швидше", "时间：0.5 = 快一倍"}, // unit_cooldown
    {"yards", "ярды", "Yards", "yards", "yardas", "ярди", "码"}, // unit_yards
    {"fraction: 1 = +100%", "доля: 1 = +100%", "Anteil: 1 = +100%", "fraction : 1 = +100%", "fracción: 1 = +100%", "частка: 1 = +100%", "小数：1 = +100%"}, // unit_bonus_fraction
    {"coins", "монеты", "Münzen", "pièces", "monedas", "монети", "金币"}, // unit_coins
    {"coins; 0 = free", "монеты; 0 = бесплатно", "Münzen; 0 = kostenlos", "pièces ; 0 = gratuit", "monedas; 0 = gratis", "монети; 0 = безкоштовно", "金币；0 = 免费"}, // unit_chest_coins
    {"remaining count", "оставшееся количество", "verbleibende Anzahl", "nombre restant", "cantidad restante", "залишок", "剩余次数"}, // unit_remaining
    {"skip permission, no consumable counter", "разрешение скипа, без счётчика расходов", "Überspringen erlaubt, ohne Verbrauchszähler", "autoriser à passer, sans compteur consommable", "permiso para omitir, sin contador consumible", "дозвіл пропуску, без лічильника витрат", "允许跳过，无消耗计数"}, // unit_skip_flag
    {"current health", "текущее здоровье", "aktuelle Gesundheit", "santé actuelle", "salud actual", "поточне здоров’я", "当前生命值"}, // unit_current_health
    {"health points", "единицы здоровья", "Gesundheitspunkte", "points de santé", "puntos de salud", "одиниці здоров’я", "生命值"}, // unit_health
    {"fraction: 1 = 100%", "доля: 1 = 100%", "Anteil: 1 = 100%", "fraction : 1 = 100%", "fracción: 1 = 100%", "частка: 1 = 100%", "小数：1 = 100%"}, // unit_fraction
    {"multiplier", "множитель", "Multiplikator", "multiplicateur", "multiplicador", "множник", "倍率"}, // unit_multiplier
    {"fraction; normal cap 0.6", "доля; обычный предел 0.6", "Anteil; normale Grenze 0.6", "fraction ; limite normale 0.6", "fracción; límite normal 0.6", "частка; звичайна межа 0.6", "小数；正常上限 0.6"}, // unit_armor
    {"health per second", "здоровья в секунду", "Gesundheit pro Sekunde", "santé par seconde", "salud por segundo", "здоров’я за секунду", "每秒生命值"}, // unit_health_per_second
    {"count", "количество", "Anzahl", "nombre", "cantidad", "кількість", "数量"}, // unit_count
    {"health per enemy killed", "здоровья за врага", "Gesundheit pro besiegtem Gegner", "santé par ennemi éliminé", "salud por enemigo eliminado", "здоров’я за ворога", "每击杀一名敌人恢复的生命值"}, // unit_health_per_kill
    {"the game uses at most 3", "игра учитывает не более 3", "das Spiel nutzt höchstens 3", "le jeu en utilise 3 maximum", "el juego usa un máximo de 3", "гра враховує не більше ніж 3", "游戏最多计入 3 层"}, // unit_dot_limit
    {"Damage", "Урон", "Schaden", "Dégâts", "Daño", "Шкода", "伤害"}, // field_damage
    {"Run speed", "Скорость бега", "Lauftempo", "Vitesse de déplacement", "Velocidad de movimiento", "Швидкість бігу", "移动速度"}, // field_run_speed
    {"Spell cooldowns", "Перезарядка заклинаний", "Zauberabklingzeiten", "Recharge des sorts", "Reutilización de hechizos", "Відновлення заклинань", "法术冷却"}, // field_cooldown
    {"Pickup radius", "Радиус сбора", "Sammelradius", "Rayon de collecte", "Radio de recogida", "Радіус збору", "拾取范围"}, // field_pickup
    {"Area of effect", "Область действия", "Wirkungsbereich", "Zone d’effet", "Área de efecto", "Область дії", "作用范围"}, // field_area
    {"Luck", "Удача", "Glück", "Chance", "Suerte", "Удача", "幸运"}, // field_luck
    {"Run gold", "Золото забега", "Gold im Durchlauf", "Or de la partie", "Oro de la partida", "Золото забігу", "本次挑战金币"}, // field_gold
    {"Cache opening cost", "Цена открытия сундука", "Truhenöffnungskosten", "Coût d’ouverture du coffre", "Coste de abrir el cofre", "Ціна відкриття скрині", "宝箱开启费用"}, // field_chest_cost
    {"Free rerolls", "Бесплатные рероллы", "Kostenlose Neuwürfe", "Relances gratuites", "Renovaciones gratuitas", "Безкоштовні перекидання", "免费重选"}, // field_rerolls
    {"Allow skipping", "Разрешить скип", "Überspringen erlauben", "Autoriser à passer", "Permitir omitir", "Дозволити пропуск", "允许跳过"}, // field_skip
    {"Health", "Здоровье", "Gesundheit", "Santé", "Salud", "Здоров’я", "生命值"}, // field_health
    {"Maximum health", "Максимум здоровья", "Maximale Gesundheit", "Santé maximale", "Salud máxima", "Максимум здоров’я", "最大生命值"}, // field_max_health
    {"Critical strike chance", "Шанс критического удара", "Kritische Trefferchance", "Chances de coup critique", "Probabilidad de crítico", "Шанс критичного удару", "暴击几率"}, // field_crit_chance
    {"Critical strike damage", "Урон критического удара", "Kritischer Trefferschaden", "Dégâts critiques", "Daño crítico", "Шкода критичного удару", "暴击伤害"}, // field_crit_damage
    {"Experience gained", "Получаемый опыт", "Erhaltene Erfahrung", "Expérience gagnée", "Experiencia obtenida", "Отримуваний досвід", "经验获取"}, // field_experience
    {"Damage reduction", "Снижение входящего урона", "Schadensreduktion", "Réduction des dégâts", "Reducción de daño", "Зменшення отриманої шкоди", "伤害减免"}, // field_armor
    {"Health regeneration", "Восстановление здоровья", "Gesundheitsregeneration", "Régénération de santé", "Regeneración de salud", "Відновлення здоров’я", "生命恢复"}, // field_regeneration
    {"Gold gained", "Получаемое золото", "Erhaltenes Gold", "Or gagné", "Oro obtenido", "Отримуване золото", "金币获取"}, // field_gold_gain
    {"Additional targets", "Дополнительные цели", "Zusätzliche Ziele", "Cibles supplémentaires", "Objetivos adicionales", "Додаткові цілі", "额外目标"}, // field_targets
    {"Spell echo", "Повторное заклинание", "Zauberwiederholung", "Répétition de sort", "Repetición de hechizo", "Повторне заклинання", "法术回响"}, // field_echo
    {"Healing per kill", "Лечение за убийство", "Heilung pro besiegtem Gegner", "Soins par élimination", "Curación por eliminación", "Лікування за вбивство", "击杀治疗"}, // field_kill_heal
    {"Damage reflection", "Отражение урона", "Schadensreflexion", "Renvoi de dégâts", "Reflejo de daño", "Відбиття шкоди", "伤害反射"}, // field_thorns
    {"Additional DoT stacks", "Дополнительные стаки DoT", "Zusätzliche DoT-Stapel", "Cumuls DoT supplémentaires", "Acumulaciones DoT adicionales", "Додаткові стаки DoT", "额外持续伤害层数"}, // field_dot_stacks
    {"Buff power", "Сила баффов", "Buff-Stärke", "Puissance des bonus", "Potencia de mejoras", "Сила посилень", "增益强度"}, // field_buff_power
    {"Revives", "Возрождения", "Wiederbelebungen", "Résurrections", "Resurrecciones", "Відродження", "复活次数"}, // field_lives
};
static_assert(std::size(text_catalog) == static_cast<std::size_t>(ui_text_id::count));
static_assert(static_cast<std::size_t>(client_language::chinese_simplified) == 6);

} // namespace

const ui_strings::field_entry ui_strings::field_entries_[25] = {
    {"area", ui_text_id::field_area, ui_text_id::unit_multiplier_one},
    {"armor", ui_text_id::field_armor, ui_text_id::unit_armor},
    {"buff_power", ui_text_id::field_buff_power, ui_text_id::unit_bonus_fraction},
    {"chest_cost", ui_text_id::field_chest_cost, ui_text_id::unit_chest_coins},
    {"cooldown", ui_text_id::field_cooldown, ui_text_id::unit_cooldown},
    {"crit_chance", ui_text_id::field_crit_chance, ui_text_id::unit_fraction},
    {"crit_damage", ui_text_id::field_crit_damage, ui_text_id::unit_multiplier},
    {"damage", ui_text_id::field_damage, ui_text_id::unit_multiplier_one},
    {"dot_stacks", ui_text_id::field_dot_stacks, ui_text_id::unit_dot_limit},
    {"echo", ui_text_id::field_echo, ui_text_id::unit_fraction},
    {"experience", ui_text_id::field_experience, ui_text_id::unit_multiplier_one},
    {"gold", ui_text_id::field_gold, ui_text_id::unit_coins},
    {"gold_gain", ui_text_id::field_gold_gain, ui_text_id::unit_multiplier_one},
    {"health", ui_text_id::field_health, ui_text_id::unit_current_health},
    {"kill_heal", ui_text_id::field_kill_heal, ui_text_id::unit_health_per_kill},
    {"lives", ui_text_id::field_lives, ui_text_id::unit_count},
    {"luck", ui_text_id::field_luck, ui_text_id::unit_bonus_fraction},
    {"max_health", ui_text_id::field_max_health, ui_text_id::unit_health},
    {"pickup", ui_text_id::field_pickup, ui_text_id::unit_yards},
    {"regeneration", ui_text_id::field_regeneration, ui_text_id::unit_health_per_second},
    {"rerolls", ui_text_id::field_rerolls, ui_text_id::unit_remaining},
    {"run_speed", ui_text_id::field_run_speed, ui_text_id::unit_multiplier_one},
    {"skip", ui_text_id::field_skip, ui_text_id::unit_skip_flag},
    {"targets", ui_text_id::field_targets, ui_text_id::unit_count},
    {"thorns", ui_text_id::field_thorns, ui_text_id::unit_fraction},
};

const char* ui_strings::get(ui_text_id id) noexcept {
    return get(id, localization::current_language());
}

const char* ui_strings::get(ui_text_id id, client_language language) noexcept {
    const auto index = static_cast<std::size_t>(id);
    if (index >= std::size(text_catalog)) {
        return "";
    }
    const auto language_index = static_cast<std::size_t>(language);
    return text_catalog[index][language_index < 7 ? language_index : 0];
}

const char* ui_strings::translate_message(const char* message) noexcept {
    if (!message || !message[0]) {
        return "";
    }
    for (std::size_t index = 0; index < std::size(text_catalog); ++index) {
        for (const char* translation : text_catalog[index]) {
            if (std::strcmp(message, translation) == 0) {
                return get(static_cast<ui_text_id>(index));
            }
        }
    }
    return message;
}

const ui_strings::field_entry* ui_strings::find_field(const char* key) noexcept {
    if (!key) {
        return nullptr;
    }
    std::size_t first = 0;
    std::size_t last = std::size(field_entries_);
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2;
        const int comparison = std::strcmp(key, field_entries_[middle].key);
        if (comparison == 0) {
            return &field_entries_[middle];
        }
        if (comparison < 0) {
            last = middle;
        } else {
            first = middle + 1;
        }
    }
    return nullptr;
}

const char* ui_strings::field_name(const char* key) noexcept {
    return field_name(key, localization::current_language());
}

const char* ui_strings::field_name(const char* key, client_language language) noexcept {
    const auto* entry = find_field(key);
    return entry ? get(entry->name, language) : (key ? key : "");
}

const char* ui_strings::field_unit(const char* key) noexcept {
    return field_unit(key, localization::current_language());
}

const char* ui_strings::field_unit(const char* key, client_language language) noexcept {
    const auto* entry = find_field(key);
    return entry ? get(entry->unit, language) : "";
}

} // namespace azs
