<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-horizontal-light.png">
    <img src="assets/logo-horizontal-dark.png" alt="WRAM" width="320">
  </picture>
</p>

<p align="center">
  Monitor del sistema para la barra de tareas de Windows.<br>
  Hasta 10 métricas en vivo junto a la bandeja, en un solo ejecutable de unos 400 KB.
</p>

<p align="center">
  <a href="https://github.com/Ju4nMarin/WRAM/releases/latest">Descargar</a> ·
  <a href="docs/es/instalacion.md">Instalación</a> ·
  <a href="docs/es/uso.md">Uso</a> ·
  <a href="docs/es/arquitectura.md">Arquitectura</a> ·
  <a href="docs/es/desarrollo.md">Desarrollo</a> ·
  <a href="README.md">English</a>
</p>

<p align="center">
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white">
  <img alt="Windows" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=for-the-badge">
  <img alt="Win32 API" src="https://img.shields.io/badge/Win32-API-0078D4?style=for-the-badge">
  <img alt="Direct2D" src="https://img.shields.io/badge/Direct2D-DirectWrite-5E5E5E?style=for-the-badge">
  <img alt="WebView2" src="https://img.shields.io/badge/WebView2-Edge-0C59A4?style=for-the-badge">
  <br>
  <img alt="HTML5" src="https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white">
  <img alt="CSS3" src="https://img.shields.io/badge/CSS3-663399?style=for-the-badge&logo=css&logoColor=white">
  <img alt="JavaScript" src="https://img.shields.io/badge/JavaScript-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black">
  <img alt="CMake" src="https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white">
  <img alt="MSVC" src="https://img.shields.io/badge/MSVC-Visual%20Studio%202022-5C2D91?style=for-the-badge">
  <img alt="GitHub Actions" src="https://img.shields.io/badge/GitHub%20Actions-2088FF?style=for-the-badge&logo=githubactions&logoColor=white">
  <img alt="Sigstore" src="https://img.shields.io/badge/Sigstore-attested-2E2F71?style=for-the-badge">
</p>

<p align="center">
  <a href="https://github.com/Ju4nMarin/WRAM/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/Ju4nMarin/WRAM?style=flat-square"></a>
  <a href="https://github.com/Ju4nMarin/WRAM/actions/workflows/ci.yml"><img alt="CI" src="https://img.shields.io/github/actions/workflow/status/Ju4nMarin/WRAM/ci.yml?branch=main&style=flat-square&label=CI"></a>
  <a href="LICENSE"><img alt="License MIT" src="https://img.shields.io/badge/license-MIT-green?style=flat-square"></a>
  <img alt="Size" src="https://img.shields.io/badge/WRAM.exe-%3C%20450%20KB-blue?style=flat-square">
</p>

## Qué hace

WRAM dibuja una franja de texto a la izquierda de la bandeja del sistema con las métricas que elijas: uso de CPU, RAM y GPU, temperaturas, red, disco, batería y más. Se configura desde una ventana propia en tema claro u oscuro, y los cambios se ven al instante en la barra.

<picture>
  <source media="(prefers-color-scheme: light)" srcset="docs/img/taskbar-light.png">
  <img src="docs/img/taskbar-dark.png" alt="Franja de WRAM en la barra de tareas de Windows 11, junto a la bandeja">
</picture>

- 14 métricas disponibles, hasta 10 a la vez, en una o dos filas.
- Umbral de aviso por métrica: el valor cambia de color al cruzarlo.
- En pantalla completa puede quedar oculta o visible en el recuadro de pantalla que elijas, y reacciona en el acto al entrar y al salir.
- Interfaz en español, inglés, portugués, francés, chino simplificado y japonés.
- Sin permisos de administrador, sin drivers y sin runtime: un solo `WRAM.exe`.
- En reposo usa cerca de 9 MB de memoria y 0,2 % de un núcleo de CPU.

![Métricas en tema oscuro](docs/img/metricas-oscuro.png)

![Métricas en tema claro](docs/img/metricas-claro.png)

| Pantalla completa | Configuración |
|---|---|
| ![Pantalla completa](docs/img/pantalla-completa-oscuro.png) | ![Configuración](docs/img/configuracion-claro.png) |

## Métricas

| Métrica | Fuente |
|---|---|
| Uso de CPU | Contador `% Processor Utility`, el mismo del Administrador de tareas |
| Temperatura de CPU | Zona térmica ACPI. Aproximada; en algunas placas no existe y se muestra "sin sensor" |
| Frecuencia de CPU | `Processor Frequency` × `% Processor Performance` |
| Uso de RAM y RAM en uso | `GlobalMemoryStatusEx` |
| Uso de GPU | Contador `GPU Engine`, motor 3D más ocupado |
| Temperatura de GPU | `D3DKMTQueryAdapterInfo`, la lectura que el driver expone a Windows (NVIDIA, AMD o Intel) |
| VRAM en uso | Contador `GPU Adapter Memory` |
| Actividad de disco | Contador `PhysicalDisk`, disco más ocupado |
| Descarga y subida | Contador `Network Interface`, suma de interfaces |
| Batería | `GetSystemPowerStatus` |
| Tiempo encendido y procesos | `GetTickCount64` y contador `System\Processes` |

## Instalación rápida

1. Descarga `WRAM.exe` desde [Releases](https://github.com/Ju4nMarin/WRAM/releases/latest).
2. Muévelo a una carpeta fija, por ejemplo `%LOCALAPPDATA%\Programs\WRAM`.
3. Ejecútalo. La primera vez se abre la configuración.

La guía completa, con requisitos, inicio con Windows y desinstalación, está en [docs/es/instalacion.md](docs/es/instalacion.md).

## Requisitos

| | |
|---|---|
| Sistema | Windows 10 versión 1903 o posterior, o Windows 11 |
| Arquitectura | x64 |
| Ventana de configuración | Microsoft Edge WebView2 Runtime (incluido en Windows 11 y en Windows 10 actualizado) |

## Compilar

Con Visual Studio 2022 o Build Tools (carga de trabajo de C++):

```
build.cmd
```

Deja `build\WRAM.exe` y ejecuta las pruebas. Detalles en [docs/es/desarrollo.md](docs/es/desarrollo.md).

## Documentación

- [Instalación](docs/es/instalacion.md)
- [Uso y configuración](docs/es/uso.md)
- [Arquitectura](docs/es/arquitectura.md)
- [Seguridad](docs/es/seguridad.md)
- [Desarrollo, ramas y commits](docs/es/desarrollo.md)
- [Cómo contribuir](CONTRIBUTING.es.md)
- [Cambios por versión](CHANGELOG.md)

## Autor

Desarrollado por Juan Guillermo Marín Córdoba ([@Ju4nMarin](https://github.com/Ju4nMarin)).

## Licencia

[MIT](LICENSE) © 2026 Juan Guillermo Marín Córdoba.
