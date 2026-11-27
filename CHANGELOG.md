# Changelog

## 0.1.6 — 2026-11-27

### Added

- **Quick Notes** opens a Markdown editor at the pointer. Format text inline, save to a dated folder or choose a destination, and browse notes by date.
- **Converter** detects input image formats, accepts multiple files or clipboard images, and converts them locally. Search the target formats and copy, save, or batch-export the results.
- **Command Shortcuts** runs user-defined shell commands in the background. Record a global shortcut or test a command with **Run**. Imported commands require explicit approval.
- **Whisper** provides local dictation with toggle and hold-to-talk modes, a recording overlay, and a persistent transcript history. On X11, it can paste into the previously focused application when that application still has focus.
- **Settings** provides tray autostart, configuration import and export, an active-config path with a shortcut to its folder, and a factory reset. Setting changes save directly to the active JSON file.

### Improved

- Added animated, reusable dropdown and message-box components, consistent shortcut keycaps, and slide-out animations for removing entries.
- Added the Qt image format plugin dependency for WebP and TIFF support in the Debian package.

### Installation

- The Debian package targets 64-bit Zorin OS and Ubuntu 24.04 systems. The desktop features described above were tested on X11.
- Whisper still needs a separately installed runtime and model. See the [installation guide](https://github.com/riccify/Clipify/blob/v0.1.6/Installer.md#local-whisper-dictation).
