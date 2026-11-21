# Desarrollo

## Requisitos

- Windows 10 u 11 x64.
- Visual Studio 2022 o Visual Studio Build Tools 2022 con la carga de trabajo "Desarrollo para el escritorio con C++" (incluye MSVC, Windows SDK, CMake y Ninja).
- Conexión a internet la primera vez: CMake descarga el paquete `Microsoft.Web.WebView2` de NuGet y verifica su SHA-256.

## Compilar y probar

```
build.cmd
```

El script busca Visual Studio con `vswhere`, configura con CMake y Ninja en `MinSizeRel`, compila, ejecuta las pruebas y escribe el reporte JUnit en `build\junit-core.xml`.

A mano, desde una "x64 Native Tools Command Prompt":

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build
build\wram_tests.exe --junit build\junit-core.xml
```

| Salida | Descripción |
|---|---|
| `build\WRAM.exe` | Aplicación |
| `build\wram_tests.exe` | Pruebas del núcleo |
| `build\res\settings.min.html` | Interfaz tal como queda incrustada |

## Interfaz de configuración

`src/ui/settings.html` se puede abrir directamente en Edge: sin el programa usa datos de muestra. El fragmento de la URL elige sección, tema e idioma (`es`, `en`, `pt`, `fr`, `zh`, `ja`):

```
settings.html#metrics&dark
settings.html#settings&light&en
```

Los logos se ven solo en `build\res\settings.min.html`, que es donde el build los incrusta.

## Logos e ícono

`assets/` guarda los logos con transparencia y su versión en base64. Se regeneran con:

```powershell
powershell -ExecutionPolicy Bypass -File tools/make-assets.ps1 -Mark <logo-blanco-sobre-oscuro> -Horizontal <horizontal-blanco-sobre-oscuro>
```

El script crea `assets/*.png`, `assets/*.b64` y `res/wram.ico`.

## Pruebas

`tests/test_main.cpp` cubre el codec de configuración (ida y vuelta, rangos, listas blancas, duplicados, tope de 10 posiciones, entrada sobredimensionada), el catálogo, las alertas y el formato de valores con punto y con coma decimal. Corre sin dependencias externas y acepta `--junit <archivo>`.

El workflow `.github/workflows/ci.yml` compila en `windows-latest`, ejecuta las pruebas, publica el reporte JUnit y sube `WRAM.exe` como artefacto.

## Ramas (gitflow)

| Rama | Uso |
|---|---|
| `main` | Versiones publicadas. Cada merge lleva una etiqueta `vX.Y.Z` |
| `develop` | Integración de lo que va a la próxima versión |
| `feature/<descripcion-corta>` | Trabajo nuevo, sale de `develop` y vuelve a `develop` |
| `release/X.Y.Z` | Preparación de versión: número de versión y CHANGELOG. Va a `main` y a `develop` |
| `hotfix/<descripcion-corta>` | Corrección urgente sobre `main`. Va a `main` y a `develop` |

Los pull requests de funcionalidades apuntan a `develop`.

## Commits

Formato: `tipo(alcance): descripción`

- Tipos: `feat`, `fix`, `test`, `docs`, `ci`, `refactor`, `chore`.
- Alcance: el módulo que se toca (`core`, `sensors`, `ui`, `app`, `build`, `docs`).
- Descripción en minúscula y como frase nominal: describe qué contiene el commit.

```
feat(sensors): temperatura de gpu con d3dkmt
fix(ui): ocultado inmediato de la franja en pantalla completa
docs(readme): guía de instalación con capturas
```

## Versiones

Versionado semántico. El número está en `CMakeLists.txt` (`project`), `res/wram.rc` (`VERSIONINFO`) y `src/app/App.cpp` (`kVersion`).
