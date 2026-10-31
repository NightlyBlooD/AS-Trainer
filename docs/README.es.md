# AS-Trainer 1.0.0

[English](../README.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md) · [Українська](README.uk.md) · [简体中文](README.zh-CN.md)

Un trainer C++20 / Dear ImGui para Azeroth Survivors en Windows x64. Permite modificar 25 parámetros, preparar la próxima partida, guardar perfiles INI, elegir recompensas automáticamente, obtener el conjunto legendario de clase y acelerar la simulación de 1 a 16×.

## Descargar e iniciar

Descarga **`AS-Trainer-1.0.0-windows-x64.zip`** desde [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases). Extrae todo el archivo. Mantén juntos `azs_overlay.dll`, `load_overlay.exe`, `load_overlay.cmd`, `signature_scan.exe`, `configs`, `signatures`, `fonts`, la documentación y las licencias. La DLL, el cargador y `SHA256SUMS.txt` también se ofrecen por separado.

1. Inicia la versión compatible del juego en modo ventana o sin bordes.
2. Ejecuta `load_overlay.cmd` o `load_overlay.exe` con los mismos permisos que el juego.
3. **Insert** muestra u oculta el panel. Ajusta los valores o carga un perfil en **Configuraciones**. Confirma los cambios de automatización con **Aplicar reglas**.
4. **End** cancela escrituras pendientes, bloqueos y marcas Inicio, desactiva la automatización y el modo de grupo, devuelve el tiempo a 1× y oculta el panel. Los valores ya cambiados en el juego y los archivos INI se conservan.

Con varios procesos: `load_overlay.exe --pid 12345`. La DLL permanece cargada hasta cerrar el juego. **Reinicia el juego antes de actualizarla**; volver a ejecutar el cargador no sustituye una DLL cargada.

## Compatibilidad e idioma

Los perfiles compatibles se identifican por el SHA-256 exacto de `AzerothSurvivors.exe`:

| Versión del juego | SHA-256 |
|---|---|
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

La DLL requiere un hash verificado, una disposición de objetos validada y la convención de llamadas nativa (ABI) correcta. También comprueba los bytes de código esenciales antes de instalar los hooks. Se rechazan otras versiones y no se modifica el EXE en disco. El perfil actual se ha comprobado con el ejecutable y objetos de prueba sin conexión. Aún no se han confirmado partidas reales completas con todas las funciones ni sesiones cooperativas reales.

La interfaz sigue el `language.txt` del cliente, normalmente `%APPDATA%\wenilla-rogue\language.txt`. Admite inglés, ruso, alemán, francés, español, ucraniano y chino simplificado. Los valores desconocidos usan inglés. Los cambios de idioma se actualizan sin reiniciar el trainer.

## Valores y perfiles

**Aplicar** programa una escritura única de un valor absoluto. Desde un menú prepara la siguiente partida y activa **Inicio**. **Fijar** vuelve a aplicar el valor tras los recálculos del juego y también activa Inicio. **Inicio** aplica el valor configurado en cada nueva partida; al desactivarlo, la fijación de ese campo no pasa a la siguiente. Los valores actuales aparecen cuando hay un personaje activo.

Los 25 campos incluyen daño, velocidad de carrera, reutilización, radio de recogida, área, suerte, oro, coste de cofres, repeticiones, omitir, salud actual/máxima, probabilidad/daño crítico, ganancias de experiencia/oro, reducción de daño, regeneración, objetivos adicionales, eco, curación por baja, espinas, acumulaciones de DoT, potencia de beneficios y resurrecciones.

| Valor | Significado |
|---|---|
| Daño / velocidad / área | `1` = normal; `2` = doble multiplicador |
| Reutilización | `0.5` = el doble de rápido; `0.1` = diez veces más rápido |
| Recogida | Yardas |
| Suerte | `0.2` = +20 %; `100` = +10000 % |
| Probabilidad crítica / eco / reducción | Fracciones: `0.5` = 50 % |
| Cofre / omitir | `0` = cofre gratis; omitir es un permiso |

Los perfiles están en `configs`, junto a la DLL cargada. Se guardan los valores confirmados, Fijar/Inicio y las reglas aplicadas, sin el texto sin confirmar de los editores. Cargar sustituye los ajustes; un archivo inválido se rechaza entero. **`default.ini`** se carga automáticamente al iniciar la DLL. Los ejemplos se cargan manualmente:

- [normal.ini](../configs/normal.ini): desactiva automatización y vuelve a 1×.
- [legendary_16x.ini](../configs/legendary_16x.ini): 16×, elección automática, conjunto legendario y valores fijados.
- [party_boost.ini](../configs/party_boost.ini): los 25 campos para el grupo del anfitrión, a 1×.

Los números son **objetivos absolutos**, no bonificaciones aditivas: daño `20` significa un multiplicador de daño de 20×.

## Recompensas y tiempo

Hechizos y joyas tienen interruptores y prioridades independientes. Usa claves internas de [reference_keys.txt](../configs/reference_keys.txt), como `wrath`, `hurricane`, `chosen` o `legendary:wrath`. Gana la clave que aparezca antes, después la mayor rareza y, en caso de empate, la primera carta. Sin coincidencias, decide la rareza. El retardo es de **50–5000 ms reales**. Solo se procesan ofertas abiertas; primero abre los niveles guardados con el botón del juego.

La opción del jefe final concede el conjunto legendario de armadura y armas de la **clase actual**, de la tabla Ahn'Qiraj/Naxxramas, tras el jefe final de **cualquier zona**: normalmente 9 objetos, 10 para Pícaro y Chamán. El gestor normal de botín los añade a la colección de equipo. Es independiente de las joyas de la partida y la rareza de los hechizos.

La aceleración usa subpasos nativos de simulación. A 16×, 16 minutos de juego activo duran aproximadamente un minuto real si el rendimiento lo permite. Las pausas y la selección de recompensas mantienen sus paradas normales; el retardo de selección sigue usando tiempo real. Aumenta la carga de CPU, por lo que la duración exacta depende del rendimiento.

## Cooperativo: solo el anfitrión

En tu propia sala, activa **Aplicar estadísticas a todo el grupo (solo anfitrión)** y aplica las reglas. Los mismos valores y opciones Fijar/Inicio se aplican a sus personajes. Los amigos conservan sus clases, inventarios, equipo, hechizos y progreso; no reciben una copia del contenido del anfitrión. El interruptor comparte valores numéricos, no la elección automática ni los inventarios.

Un trainer cargado únicamente en un invitado no puede modificar la simulación de otro anfitrión. El panel muestra el personaje del anfitrión y los diagnósticos del grupo. Las sesiones cooperativas reales con amigos **todavía no se han verificado**; las pruebas automáticas usan objetos de prueba y funciones nativas simuladas.

## Buscar firmas sin conexión

El ZIP incluye `signature_scan.exe` y los perfiles de `signatures`; no se publican como archivos de descarga separados. Desde la carpeta del archivo extraído, puedes analizar un EXE sin iniciar el juego ni cargar la DLL:

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-0.132.0.ini --output scan.json
```

Los perfiles usan bytes hexadecimales de estilo IDA con `?` / `??` como comodines de un byte completo. Cada regla exige una sola coincidencia. La resolución admite la propia coincidencia, un destino relativo a RIP, el inicio de la función obtenido de `.pdata` o un desplazamiento de campo con signo. El JSON UTF-8 incluye hashes, estado, posición en el archivo, RVA/VA y el desplazamiento de campo cuando corresponda.

Un hash distinto se indica con `hash_matches: false` y no impide el análisis sin conexión. Una RVA de código identifica una instrucción o función, no el valor de coma flotante de un atributo; un desplazamiento de campo es una posición en un objeto, no una dirección de memoria activa. El escáner puede examinar otras versiones, pero las firmas coincidentes por sí solas no confirman la disposición ni la ABI y no añaden compatibilidad automática con futuras versiones del juego.

## Compilar y publicar

Requisitos: Visual Studio 2022 con herramientas C++ x64, Windows SDK y CMake 3.24+. Desde la raíz del repositorio:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions usa Windows 2022 x64 Release para push y pull requests. Las etiquetas `vX.Y.Z` publican los archivos cuando la compilación y las pruebas terminan correctamente. Las partidas reales completas con todas las funciones siguen sin verificarse.

## Dependencias

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b): MIT. [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4): BSD 2-Clause. Las fuentes incluidas conservan sus licencias originales. Mantén los avisos al redistribuir. Win32, Direct3D 11 y BCrypt proceden del Windows SDK.
