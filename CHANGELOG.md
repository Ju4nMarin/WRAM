# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and semantic versioning.

## 1.7.0

### Added

- "Notify me about new versions", on by default: once a day WRAM checks GitHub and, if there is a new version, shows a Windows notification (once per version) and a dot on the tray icon and next to Settings. Clicking the notification opens Settings at the update button. It never checks during full screen, games, presentations or Do not disturb, and it never installs on its own.

### Changed

- The file description of `WRAM.exe` is now "WRAM", so notifications and Task Manager show the name cleanly.

## 1.6.0

### Added

- Updates from the app: "Check for updates" in Settings looks for the latest version on GitHub and "Update to" downloads it, verifies its SHA-256, version and PE header, replaces `WRAM.exe` and restarts the app. GitHub is contacted only when the button is pressed.

## 1.5.2

### Added

- Full screen: overall opacity of the indicator (numbers, labels and background), from 10% to 100%, separate from the background opacity.

### Fixed

- Removed the link between the strip and the taskbar window added in 1.5.1. It could leave WRAM and explorer.exe not responding. While the Start menu or search is open the strip stays below the taskbar again.

### Changed

- Executable size and measured usage updated in the README and the architecture guide.

## 1.5.1

### Fixed

- The strip no longer disappears behind the taskbar while the Start menu or search is open: the taskbar is now the owner window of the strip.
- If explorer.exe restarts and the strip window is lost along with its taskbar, it is created again.

### Changed

- App screenshots moved to `docs/img/app/` so the README shows the current ones.

## 1.5.0

### Added

- Sidebar card to support the project with a donation; in a narrow window it becomes a smiley button.

### Changed

- README and documentation screenshots updated.

## 1.4.0

### Added

- Full screen: background opacity of the indicator, from 0% to 100%.
- About: link to support the project with a donation on PayPal.

### Changed

- Taskbar preview in a narrow window: it keeps the real proportions between the app icons, the distance, the strip and the tray, scaled down together. After making the window larger again it returns to real size.

## 1.3.1

### Fixed

- Taskbar preview in a narrow window: the strip keeps its real size and the distance from the tray is shortened and marked with its real value, instead of shrinking the whole preview until it was unreadable.

## 1.3.0

### Added

- "Show on all displays" option: every taskbar gets its own strip, next to its clock on secondary displays.

### Changed

- After changing the icon alignment, position or size of the taskbar, the strip settles in place within a fraction of a second instead of waiting for the periodic scan.

## 1.2.0

### Added

- Taskbar on the left or right: metrics stacked one below the other, as wide as the taskbar, above the tray.
- Small taskbar size: the text shrinks just enough to fit.
- Automatically hidden taskbar: the strip comes back as soon as the taskbar slides in.
- Taskbar compatibility table in the usage guide.

### Fixed

- The taskbar preview shrinks proportionally in a narrow window instead of cutting off its content.

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
