# Clipify

**Your clipboard history, one shortcut away.**

Clipify is a modular desktop toolbox for everyday shortcuts. It remembers copied text and images, captures screenshots, and copies text from a screen selection using local OCR.

## Quick Install

For **Zorin OS 18.1 or Ubuntu 24.04 on X11** with a 64-bit Intel or AMD PC, download [clipify_0.1.0_amd64.deb](https://github.com/riccify/Clipify/releases/download/v0.1.0/clipify_0.1.0_amd64.deb), then open a terminal in the download folder and run:

```bash
sudo apt install ./clipify_0.1.0_amd64.deb
```

The package contains the prebuilt app. APT installs the required Qt runtime and the **Fast** OCR models for German and English, then adds **Clipify** to your app menu. **You can launch Clipify right away without compiling anything.**

**Zorin and GNOME:** To enable global shortcuts such as `Super+V`, run `clipify-setup-gnome` once in your desktop session, then launch Clipify from the app menu. Restart Clipify if it was already running.

[Build from source](#build-from-source) · [Detailed installation guide](Installer.md)

## Clipboard history

- Press <kbd>Super</kbd> + <kbd>V</kbd> to open the history at your mouse pointer. Type to filter, use <kbd>↑</kbd> and <kbd>↓</kbd> to select, then press <kbd>Enter</kbd> to paste back into the previous window. <kbd>Esc</kbd> dismisses the popup.
- Open the main window to browse and manage your history, change the shortcut, or set how many entries the popup shows.
- Close the main window to keep Clipify running in the system tray. Choose **Quit** from the tray or **Exit** in the sidebar to stop it.

Clipboard history for text and images is stored locally in SQLite. Consecutive copies of the same text produce a single entry. The popup limit controls how many entries are shown, while saved history can be browsed in the main window.

Press <kbd>Alt</kbd> + <kbd>Print</kbd> to select an area and copy its text. Clipify recognizes German and English locally with Tesseract. Choose **Fast** or **Accurate** under **Clipboard**. The selected image stays in memory.

## Screenshots

- <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>S</kbd> opens the region selector on the monitor under your pointer.
- <kbd>Ctrl</kbd> + <kbd>Print</kbd> captures that whole monitor.
- <kbd>Super</kbd> + <kbd>Print</kbd> captures the focused window.

Clipify saves a numbered PNG in `Pictures/Screenshots` and copies the image to the clipboard. Use the eye icon on an image entry to open a preview. Open **Screen** to browse the screenshot gallery and change the shortcuts.

## Build from source

Install the build tools, Qt modules, and OCR dependencies:

```bash
sudo apt update
sudo apt install git build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-qpa-plugins libx11-dev libxtst6 libqt6sql6-sqlite \
  qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtquick \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates qml6-module-qtquick-window \
  python3-gi libglib2.0-bin tesseract-ocr tesseract-ocr-deu tesseract-ocr-eng
```

Clone and build Clipify:

```bash
git clone https://github.com/riccify/Clipify.git
cd Clipify
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Zorin or GNOME, run `/usr/bin/python3 scripts/setup-gnome-shortcut.py` once. Then run `./build/clipify` from the repository root.

See [Installer.md](Installer.md) for tray autostart, optional **Accurate** OCR models, desktop integration, and instructions for building the `.deb`.

The desktop client uses **C++20, Qt 6 Quick/QML, CMake, and SQLite**.

## What's next

- Share screenshots with expiring links through the separate Clipify backend.
- Dictate text with local transcription.
- Add and test desktop integration on Windows and Wayland.

[Clipify backend](https://github.com/riccify/ClipifyBackend)

## Contributing

Issues and pull requests are welcome. For a desktop integration bug, include your distribution, desktop environment, and session type (X11 or Wayland).

Clipify's source code is available under the [MIT License](LICENSE).
