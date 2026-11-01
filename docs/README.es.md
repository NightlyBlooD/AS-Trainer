# AS-Trainer 1.1.0

[English](../README.md) · [Русский](README.ru.md) · [Deutsch](README.de.md) · [Français](README.fr.md) · [Español](README.es.md) · [Українська](README.uk.md) · [简体中文](README.zh-CN.md)

Un trainer C++20 / Dear ImGui para Azeroth Survivors en Windows x64. Permite modificar hasta 26 parámetros, preparar la próxima partida, guardar perfiles INI, elegir recompensas automáticamente, obtener el conjunto legendario de clase y acelerar la simulación de 1 a 16×.

## Descargar e iniciar

Descarga **`AS-Trainer-1.1.0-windows-x64.zip`** desde [GitHub Releases](https://github.com/NightlyBlooD/AS-Trainer/releases). Extrae todo el archivo. Mantén juntos `azs_overlay.dll`, `load_overlay.exe`, `load_overlay.cmd`, `signature_scan.exe`, `configs`, `signatures`, `fonts`, la documentación y las licencias. La DLL, el cargador y `SHA256SUMS.txt` también se ofrecen por separado.

1. Inicia la versión compatible del juego en modo ventana o sin bordes.
2. Ejecuta `load_overlay.cmd` o `load_overlay.exe` con los mismos permisos que el juego.
3. **Insert** muestra u oculta el panel. Ajusta los valores o carga un perfil en **Configuraciones**. Confirma los cambios de automatización con **Aplicar reglas**.
4. **End** cancela escrituras pendientes, bloqueos y marcas Inicio, desactiva la automatización y el modo de grupo, devuelve el tiempo a 1× y oculta el panel. Los valores ya cambiados en el juego y los archivos INI se conservan.

Con varios procesos: `load_overlay.exe --pid 12345`. La DLL permanece cargada hasta cerrar el juego. **Reinicia el juego antes de actualizarla**; volver a ejecutar el cargador no sustituye una DLL cargada.

## Compatibilidad e idioma

Versiones compatibles de `AzerothSurvivors.exe` y su SHA-256:

| Versión del juego | SHA-256 |
|---|---|
| `v0.135.1 (90898d5e3)` | `71e5f1767279db4bd428d1cd9c5d50d37759b72295386a69f9e3f6e70bc59249` |
| `v0.132.0 (04e2a2d1c)` | `e60b03aae379c9c3c4c728bba1b3dbaeac161007e34a8c09fc61c061f9cceb9c` |
| `v0.131.0 (e7d9dd47)` | `5dd6819d836748ccf809ab76fe6509c9ee793e087939b5d036c430d835a14e63` |

Se conservan los perfiles de v0.131.0 y v0.132.0. Al iniciar, el catálogo adaptativo comprueba una vez 92 reglas: destinos nativos únicos, todos los desplazamientos de campos necesarios, el tamaño del registro de cada participante y pruebas explícitas de ABI. Un hash desconocido puede admitirse si supera la comprobación completa. Antes de instalar los hooks también se verifican los bytes del código cargado. Las firmas ausentes o ambiguas y las ABI incompatibles se rechazan; no se garantiza compatibilidad con todas las versiones futuras. El EXE en disco no se modifica.

La interfaz y los diagnósticos siguen el `language.txt` del cliente, normalmente `%APPDATA%\wenilla-rogue\language.txt`. Admite los 14 idiomas del cliente: inglés (`en`), ruso (`ru`), alemán (`de`), francés (`fr`), español (`es`), ucraniano (`uk`), chino simplificado (`zh`), polaco (`pl`), checo (`cs`), portugués de Brasil (`pt`), rumano (`ro`), turco (`tr`), japonés (`ja`) y coreano (`ko`). Los valores desconocidos usan inglés. Los cambios de idioma se actualizan en un segundo sin reiniciar el trainer. La fuente incluida también cubre las traducciones japonesas y coreanas.

## Valores y perfiles

**Aplicar** programa una escritura única de un valor absoluto. Desde un menú prepara la siguiente partida y activa **Inicio**. **Fijar** vuelve a aplicar el valor tras los recálculos del juego y también activa Inicio. **Inicio** aplica el valor configurado en cada nueva partida; al desactivarlo, la fijación de ese campo no pasa a la siguiente. Los valores actuales aparecen cuando hay un personaje activo.

Los 25 campos comunes incluyen daño, velocidad de carrera, reutilización, radio de recogida, área, suerte, oro, coste de cofres, repeticiones, omitir, salud actual/máxima, probabilidad/daño crítico, ganancias de experiencia/oro, reducción de daño, regeneración, objetivos adicionales, eco, curación por baja, potencia de beneficios, acumulaciones de DoT, velocidad de ticks DoT y resurrecciones. Las v0.135.1 y v0.132.0 incluyen robo de vida y ofrecen 26 campos; la v0.131.0 mantiene 25.

| Valor | Significado |
|---|---|
| Daño / velocidad / área | `1` = normal; `2` = doble multiplicador |
| Reutilización | `0.5` = el doble de rápido; `0.1` = diez veces más rápido |
| Recogida | Yardas |
| Suerte | `0.2` = +20 %; `100` = +10000 % |
| Probabilidad crítica / eco / reducción | Fracciones: `0.5` = 50 % |
| Cofre / omitir | `0` = cofre gratis; omitir es un permiso |
| Robo de vida | Fracción del daño realmente infligido: `0.25` = 25 % de curación; rango del trainer 0–10, no un límite nativo demostrado |

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

Un trainer cargado únicamente en un invitado no puede modificar la simulación de otro anfitrión. El panel muestra el personaje del anfitrión y los diagnósticos del grupo.

## Buscar firmas sin conexión

El ZIP incluye `signature_scan.exe` y los perfiles de `signatures`; no se publican como archivos de descarga separados. Desde la carpeta del archivo extraído, puedes analizar un EXE sin iniciar el juego ni cargar la DLL:

```powershell
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --auto --output compatibility.json
.\signature_scan.exe --exe "PATH/AzerothSurvivors.exe" --profile signatures/azeroth-survivors-adaptive.ini --output scan.json
```

`--auto` comprueba los mismos contratos sin conexión integrados en la DLL, sin instalar hooks. `--profile` solo analiza el perfil indicado y no autoriza su uso en ejecución.

Los perfiles usan bytes hexadecimales de estilo IDA con `?` / `??` como comodines de un byte completo. Cada regla exige una sola coincidencia. La resolución admite la propia coincidencia, un destino relativo a RIP, el inicio de la función obtenido de `.pdata` o un desplazamiento de campo con signo. El JSON UTF-8 incluye hashes, estado, posición en el archivo, RVA/VA y el desplazamiento de campo cuando corresponda.

Con `--profile`, un hash distinto se indica con `hash_matches: false` y no impide el análisis diagnóstico. Una RVA identifica una instrucción o función, no un valor de atributo; un desplazamiento de campo es una posición dentro del objeto, no una dirección activa. Las firmas aisladas no confirman la disposición completa ni la ABI. `--auto` exige el contrato completo; los bytes del código cargado se verifican al iniciar la DLL.

## Compilar y publicar

Requisitos: Visual Studio 2022 con herramientas C++ x64, Windows SDK y CMake 3.24+. Desde la raíz del repositorio:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

GitHub Actions usa Windows 2022 x64 Release para push y pull requests. Las etiquetas `vX.Y.Z` publican los archivos cuando la compilación y las pruebas terminan correctamente.

## Dependencias

[Dear ImGui v1.91.9b](https://github.com/ocornut/imgui/tree/v1.91.9b): MIT. [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook/tree/v1.3.4): BSD 2-Clause. Las fuentes incluidas conservan sus licencias originales. Mantén los avisos al redistribuir. Win32, Direct3D 11 y BCrypt proceden del Windows SDK.
