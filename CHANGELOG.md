# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and semantic versioning.

## 1.1.2

### Fixed

- "Distance from the tray" applies instantly while dragging, and the px value shown is the distance actually used.
- The taskbar preview shows the distance at 1:1 scale from the tray, with the sample app icons where the real ones end, and never places the strip over them.

## 1.1.1

### Changed

- The "approximate" tag is no longer shown next to the values; the About section still explains which readings depend on the motherboard.

## 1.1.0

### Changed

- "Distance from the tray" stops before the taskbar app icons; the slider maximum follows where the last app icon ends.

- Shorter "no sensor" text in Japanese and French so it fits in the value box.
- GitHub Actions updated to their latest versions (checkout v7.0.1, upload-artifact v7.0.2, attest-build-provenance v4.2.2, action-junit-report v6.7.1) and pinned by commit SHA.
- WebView2 SDK updated to 1.0.4370.26.

### Added

- Dependabot checks the GitHub Actions every week.

## 1.0.0

### Added

- Strip next to the system tray with up to 10 metrics in one or two rows.
- 14 metrics: CPU usage, temperature and clock; RAM usage and RAM in use; GPU usage and temperature; VRAM; disk activity; download; upload; battery; uptime and processes.
- Per-metric alert threshold that changes the value color.
- Settings window with Metrics, Appearance, Full screen, Settings and About sections, in light, dark or system theme.
- Interface in Spanish, English, Portuguese, French, Simplified Chinese and Japanese.
- Full-screen behavior: hidden, or visible in one of 8 screen spots. Reacts instantly, including F11 and videos.
- Taskbar preview with a light or dark taskbar selector.
- Release builds from CI with a Sigstore build provenance attestation.
- Startup with Windows without administrator rights.
- Decimal separator from the regional settings.
- Core tests with JUnit report and CI workflow.
