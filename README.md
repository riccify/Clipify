# Clipify

**Your clipboard history, one shortcut away.**

Clipify is a modular desktop toolbox for everyday shortcuts. Its first module remembers text you copy so you can bring back a snippet, link, or command without leaving the app you're working in.

## Clipboard history

- Press <kbd>Super</kbd> + <kbd>V</kbd> to open the history at your mouse pointer. Type to filter, use <kbd>↑</kbd> and <kbd>↓</kbd> to select, then press <kbd>Enter</kbd> to paste back into the previous window. <kbd>Esc</kbd> dismisses the popup.
- Open the main window to browse and manage your history, change the shortcut, or set how many entries the popup shows.
- Close the main window to keep Clipify running in the system tray. Choose **Quit** from the tray or **Exit** in the sidebar to stop it.

Text history is stored locally in SQLite. Consecutive copies of the same text produce a single entry. The popup limit controls how many entries are shown, while saved history can be browsed in the main window.

## Get started

The current build is tested on Linux/X11, including Zorin OS with GNOME. Build from source and follow [Installer.md](Installer.md) for dependencies and the one-time GNOME shortcut setup.

Once those dependencies are installed, run this from the repository root:

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/clipify
```

The desktop client uses **C++20, Qt 6 Quick/QML, CMake, and SQLite**.

## What's next

- Capture a screen region and copy it to the clipboard.
- Share screenshots with expiring links through the separate Clipify backend.
- Dictate text with local transcription.
- Add and test desktop integration on Windows and Wayland.

[Clipify backend](https://github.com/riccify/ClipifyBackend)

## Contributing

Issues and pull requests are welcome. For a desktop integration bug, include your distribution, desktop environment, and session type (X11 or Wayland).

Clipify's source code is available under the [MIT License](LICENSE).
