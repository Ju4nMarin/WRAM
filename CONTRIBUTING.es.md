# Cómo contribuir

## Reportar un problema

Abre un issue en <https://github.com/Ju4nMarin/WRAM/issues> con:

- Versión de WRAM (Acerca de) y de Windows (`winver`).
- Qué esperabas y qué pasó.
- Pasos para reproducirlo y, si aplica, tu `config.ini` (`%APPDATA%\WRAM\config.ini`).
- Para métricas sin sensor: marca y modelo de la placa o de la GPU.

## Proponer cambios

1. Haz un fork y crea una rama desde `develop`: `feature/<descripcion-corta>`.
2. Compila y ejecuta las pruebas con `build.cmd`.
3. Usa el formato de commits de [docs/es/desarrollo.md](docs/es/desarrollo.md#commits).
4. Abre el pull request hacia `develop` describiendo qué cambia y cómo se probó. Si toca la interfaz, incluye capturas en tema claro y oscuro.

## Criterios

- El ejecutable se mantiene por debajo de 1 MB y sin dependencias de runtime.
- Sin permisos de administrador ni drivers.
- Una métrica nueva entra por el catálogo y un `IMetricProvider`, sin modificar los proveedores existentes.
- Todo dato que llega de la interfaz o del archivo pasa por `ConfigCodec`.
- Pruebas para la lógica nueva del núcleo.
- Textos nuevos de la interfaz en los seis idiomas (`I18N` en `settings.html`, `I18n.cpp` y `MetricCatalog.cpp`).
- Si un sensor no existe en el equipo, la métrica muestra "sin sensor"; no se inventan valores.

## Licencia

Al contribuir aceptas que tu código se publique bajo la licencia [MIT](LICENSE) del proyecto.
