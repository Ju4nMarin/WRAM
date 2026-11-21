# Contributing

Spanish version: [CONTRIBUTING.es.md](CONTRIBUTING.es.md).

## Reporting an issue

Open an issue at <https://github.com/Ju4nMarin/WRAM/issues> with:

- WRAM version (About) and Windows version (`winver`).
- What you expected and what happened.
- Steps to reproduce and, if relevant, your `config.ini` (`%APPDATA%\WRAM\config.ini`).
- For metrics without a sensor: motherboard or GPU make and model.

## Proposing changes

1. Fork the repository and create a branch from `develop`: `feature/<short-description>`.
2. Build and run the tests with `build.cmd`.
3. Follow the commit format in [docs/development.md](docs/development.md#commits).
4. Open the pull request against `develop`, describing what changes and how it was tested. If it touches the interface, include screenshots in light and dark theme.

## Guidelines

- The executable stays under 1 MB and without runtime dependencies.
- No administrator rights and no drivers.
- A new metric goes through the catalog and an `IMetricProvider`, without changing existing providers.
- Everything that comes from the interface or the file goes through `ConfigCodec`.
- Tests for new core logic.
- New interface strings in all six languages (`I18N` in `settings.html`, `I18n.cpp` and `MetricCatalog.cpp`).
- If a sensor does not exist on the PC, the metric shows "no sensor"; values are never made up.

## License

By contributing you agree that your code is published under the project's [MIT](LICENSE) license.
