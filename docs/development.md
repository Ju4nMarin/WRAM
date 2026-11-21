# Development

## Requirements

- Windows 10 or 11 x64.
- Visual Studio 2022 or Visual Studio Build Tools 2022 with the "Desktop development with C++" workload (MSVC, Windows SDK, CMake and Ninja).
- Internet access the first time: CMake downloads the `Microsoft.Web.WebView2` package from NuGet and verifies its SHA-256.

## Build and test

```
build.cmd
```

The script finds Visual Studio with `vswhere`, configures with CMake and Ninja in `MinSizeRel`, builds, runs the tests and writes the JUnit report to `build\junit-core.xml`.

By hand, from an "x64 Native Tools Command Prompt":

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build
build\wram_tests.exe --junit build\junit-core.xml
```

| Output | Description |
|---|---|
| `build\WRAM.exe` | Application |
| `build\wram_tests.exe` | Core tests |
| `build\res\settings.min.html` | Interface as embedded in the executable |

## Settings interface

`src/ui/settings.html` can be opened directly in Edge: without the program it uses sample data. The URL fragment selects section, theme and language (`es`, `en`, `pt`, `fr`, `zh`, `ja`):

```
settings.html#metrics&dark&en
settings.html#settings&light&zh
```

Logos only show in `build\res\settings.min.html`, where the build embeds them.

## Translations

| Part | File |
|---|---|
| Interface | `I18N` object in `src/ui/settings.html` |
| Metric names, sources and default labels | `src/core/MetricCatalog.cpp` |
| Tray menu and native messages | `src/core/I18n.cpp` |

A new language needs an entry in all three, a value in `Lang` and in `ResolveLang`, and an option in the language selector.

## Logos and icon

`assets/` holds the transparent logos and their base64 versions. Regenerate them with:

```powershell
powershell -ExecutionPolicy Bypass -File tools/make-assets.ps1 -Mark <white-mark-on-dark> -Horizontal <white-horizontal-on-dark>
```

The script writes `assets/*.png`, `assets/*.b64` and `res/wram.ico`.

## Tests

`tests/test_main.cpp` covers the configuration codec (round trip, ranges, allow lists, duplicates, the 10-position cap, oversized input), the catalog, alerts and value formatting with dot and comma decimals. It has no external dependencies and accepts `--junit <file>`.

The `.github/workflows/ci.yml` workflow builds on `windows-latest`, runs the tests, publishes the JUnit report and uploads `WRAM.exe` as an artifact.

## Branches (gitflow)

| Branch | Purpose |
|---|---|
| `main` | Published versions. Every merge gets a `vX.Y.Z` tag |
| `develop` | Integration for the next version |
| `feature/<short-description>` | New work, branches from `develop` and returns to `develop` |
| `release/X.Y.Z` | Release preparation: version number and changelog. Merges into `main` and `develop` |
| `hotfix/<short-description>` | Urgent fix on `main`. Merges into `main` and `develop` |

Feature pull requests target `develop`.

## Commits

Format: `type(scope): description`

- Types: `feat`, `fix`, `test`, `docs`, `ci`, `refactor`, `chore`.
- Scope: the module touched (`core`, `sensors`, `ui`, `app`, `build`, `docs`).
- Lowercase description written as a noun phrase that states what the commit contains.

## Versions

Semantic versioning. The number lives in `CMakeLists.txt` (`project`), `res/wram.rc` (`VERSIONINFO`) and `src/app/App.cpp` (`kVersion`).
