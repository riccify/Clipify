# Clipify

Clipify is a modular desktop toolbox for everyday shortcuts. It runs in the system tray and keeps clipboard history, OCR, screenshots, Markdown notes, image conversion, command shortcuts, and dictation in one app.

## Features

| Module | What it does |
| --- | --- |
| Clipboard | Local text and image history, live search, a keyboard-driven popup at the pointer, and offline OCR for screen selections |
| Screen | Region, monitor, and focused-window captures, a screenshot gallery, and a configurable save folder |
| Quick Notes | A Markdown editor at the pointer, inline formatting, quick saving, and notes grouped by date |
| Converter | Batch image conversion by drag-and-drop or paste, with single-image copy and a combined export |
| Commands | Custom global shortcuts for shell commands and a Run button for testing them |
| Whisper | Local dictation, transcript history, optional cloud text polishing, and paste into the focused app on X11 |
| Settings | Tray autostart, cloud LLM setup, JSON configuration import and export, and factory reset |

## Quick Install

For **Zorin OS 18.1 or Ubuntu 24.04 on X11** with a 64-bit Intel or AMD PC, download [clipify_0.1.7_amd64.deb](https://github.com/riccify/Clipify/releases/download/v0.1.7/clipify_0.1.7_amd64.deb), then open a terminal in the download folder and run:

```bash
sudo apt install ./clipify_0.1.7_amd64.deb
```

The package contains the prebuilt app. APT installs the required Qt runtime and the **Fast** OCR models for German and English, then adds **Clipify** to your app menu. **You can launch Clipify right away without compiling anything.**

**Whisper** requires a separate local runtime and model setup in this release. Set **Dictation history size** to keep the latest entries, or use **∞** for an unlimited history. See the [Whisper installation instructions](Installer.md#local-whisper-dictation).

**Zorin and GNOME:** Open Clipify from the app menu. Clipify sets up available global shortcuts in your account on first launch. If GNOME already uses `Super+V`, Clipify tries `Super+Shift+V`. You can change any binding in the app.

[Build from source](#build-from-source) · [Installation guide](Installer.md) · [Changelog](CHANGELOG.md)

## Clipboard history

- Press <kbd>Super</kbd> + <kbd>V</kbd> to open the history at your mouse pointer. Type to filter, use <kbd>↑</kbd> and <kbd>↓</kbd> to select, then press <kbd>Enter</kbd> to paste back into the previous window. <kbd>Esc</kbd> dismisses the popup.
- Open the main window to browse and manage your history, change the shortcut, or set how many entries the popup shows.
- Close the main window to keep Clipify running in the system tray. Choose **Quit** from the tray or **Exit** in the sidebar to stop it.

Clipboard history for text and images is stored locally in SQLite. Consecutive copies of the same text produce a single entry. The popup limit controls how many entries are shown, while saved history can be browsed in the main window.

Use **Pause history** in the tray or beside the history search, or press **Super+Shift+P**, to pause or resume saving copies. **History size** caps the number of entries stored in SQLite, with **∞** keeping all entries. **Clear history** deletes the saved entries. **Skip sensitive information** is enabled by default and skips clipboard items marked as secret and recognizable credential formats. Ordinary unmarked passwords can be missed.

Press <kbd>Alt</kbd> + <kbd>Print</kbd> to select an area and copy its text. Clipify recognizes German and English locally with Tesseract. Choose **Fast** or **Accurate** under **Clipboard**. The selected image stays in memory.

## Screenshots

- <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>S</kbd> opens the region selector on the monitor under your pointer.
- <kbd>Ctrl</kbd> + <kbd>Print</kbd> captures that whole monitor.
- <kbd>Super</kbd> + <kbd>Print</kbd> captures the focused window.

Clipify saves a numbered PNG in `Pictures/Screenshots` and copies the image to the clipboard. Use the eye icon on an image entry to open a preview. Open **Screen** to browse the screenshot gallery and change the shortcuts.

## Quick Notes

Press <kbd>Super</kbd> + <kbd>N</kbd> to write a note at your mouse pointer. If GNOME already uses that shortcut, Clipify tries <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>N</kbd>. Press <kbd>Space</kbd> or <kbd>Enter</kbd> after inline Markdown such as `**bold**` to format it. Press <kbd>Backspace</kbd> immediately afterward to restore the markup. Press <kbd>Ctrl</kbd> + <kbd>S</kbd> to save under `Documents/Clipify Notes/YYYY-MM-DD/HH-mm-ss.md`, <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>S</kbd> to choose a file location, or <kbd>Esc</kbd> to close without saving.

Open **Quick Notes** in the sidebar to browse notes by date, change the global shortcut, or choose the default save folder.

## Converter

Open **Converter**, select a target format, then drop in one or more image files or add files and images with **Ctrl+V**. Clipify detects each source format and converts the list locally. Use **Copy** or **Save** for a single result, or **Save all** to export the list into a folder.

## Command Shortcuts

Open **Commands** to add a shell command, optionally record a global shortcut, and test it with **Run**. Commands run in the background from your home folder. Global command shortcuts are available on X11. Commands imported through **Load Config** stay disabled until you enable them.

## Whisper

Press **Super+Shift+D** to start or stop local dictation on X11, or choose **Hold to talk** in Whisper. Clipify copies the transcript, saves it in **Dictation history**, and pastes into the previously focused application if it still has focus. See the [Whisper installation instructions](Installer.md#local-whisper-dictation) for the microphone tools, executable, and model.

Use **Super+Alt+P** to pause or resume saving dictations. The Whisper view also lets you search, clear, and limit the saved history.

For optional text polishing, choose **Cloud LLM** under **Settings → LLM Settings**, then select **OpenRouter**, **OpenAI**, or **Anthropic** and verify your API key. Pick a model from the searchable **Model for polishing text** menu in Whisper. **None** keeps dictation offline. When polishing is enabled, Clipify sends the transcript to the provider and uses the original transcript if polishing fails. Use **Prompt** to edit the Markdown instructions for the next dictation.

## Settings

Use **Auto Start** to launch Clipify in the tray at login. **Save Config** exports a JSON copy. **Load Config** makes the selected file active and saves later changes there automatically. **Restore factory settings** switches back to the local config and restores its defaults.

## Build from source

Install the build tools, Qt modules, and OCR dependencies:

```bash
sudo apt update
sudo apt install git build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-qpa-plugins qt6-image-formats-plugins libx11-dev libxtst6 libqt6sql6-sqlite \
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

Run `./build/clipify` from the repository root. On GNOME, first launch also configures available global shortcuts.

See [Installer.md](Installer.md) for tray autostart, optional **Accurate** OCR models, desktop integration, and instructions for building the `.deb`.

The desktop client uses **C++20, Qt 6 Quick/QML, CMake, and SQLite**.

## What's next

- Share screenshots with expiring links through the separate Clipify backend.
- Add and test desktop integration on Windows and Wayland.

[Clipify backend](https://github.com/riccify/ClipifyBackend)

## Contributing

Issues and pull requests are welcome. For a desktop integration bug, include your distribution, desktop environment, and session type (X11 or Wayland).

Clipify's source code is available under the [MIT License](LICENSE).
