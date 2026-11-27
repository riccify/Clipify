# Installing Clipify

These instructions cover **Zorin OS 18.1 and Ubuntu 24.04 on X11**. Run `echo "$XDG_SESSION_TYPE"` in a terminal to check your session type.

## Install the .deb

From the folder containing the package, run:

```bash
sudo apt install ./clipify_0.1.6_amd64.deb
```

The package contains the prebuilt app. APT installs the required Qt runtime and the German and English **Fast** OCR models and adds Clipify to the application menu.

Open **Clipify** from the application menu. On Zorin or GNOME, Clipify sets up available global shortcuts automatically on first launch.

Clipify checks GitHub Releases on startup. Open **About → Download update** to download and install an update for the system-installed `.deb`. Clipify verifies the download, asks for system authorization, and restarts after a successful installation. If the in-app installer is unavailable, **Open download** opens the release download for manual installation.

## Build and run from source

Follow the [source build instructions](README.md#build-from-source) to install the dependencies, clone the repository, and compile Clipify. If you already have a checkout, run the CMake commands from its root. In the development workspace, the client checkout is named `CLI/`.

Run `./build/clipify` from the checkout. The build includes the GNOME shortcut setup helper.

To add the source build to your application menu and give it the Clipify taskbar icon, install it for your user account:

```bash
cmake --install build --prefix ~/.local
```

Make sure `~/.local/bin` is on your desktop session's `PATH`. This installation also provides the `clipify-setup-gnome` and `clipify-install-ocr-best` commands. If the application menu still displays the default icon, refresh the local GTK icon cache with `gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor`.

To produce a `.deb` yourself, install `dpkg-dev` and run this from the repository root after building:

```bash
cpack --config build/CPackConfig.cmake -G DEB -B build/packages
```

## Global shortcuts on GNOME

GNOME stores key bindings per user. On launch, Clipify adds missing actions and preserves your existing Clipify bindings. When GNOME already uses `Super+V`, Clipify tries `Super+Shift+V`. For other conflicts, Clipify shows a message and lets you choose a free shortcut.

If GNOME uses `Super+N` to focus notifications, Clipify tries `Super+Shift+N` for Quick Notes.

| Default | Action |
| --- | --- |
| `Super+V` | Open clipboard history at the pointer |
| `Super+N` | Open Quick Notes at the pointer |
| `Super+Shift+D` | Start or stop Whisper dictation |
| `Super+Shift+S` | Select a screenshot region |
| `Ctrl+Print` | Capture the monitor under the pointer |
| `Super+Print` | Capture the focused window |
| `Alt+Print` | Select a region and copy its recognized text |

Other X11 desktops register available shortcuts when Clipify starts. To change a binding, click it under **Clipboard**, **Screen**, or **Quick Notes**, then press a key or key combination. Click the field again to cancel recording. Single-key bindings also apply globally. If your desktop already uses a shortcut, including GNOME's possible `Alt+Print` binding for window capture, free it in your desktop keyboard settings or choose an available binding in Clipify.

Screenshots are saved as PNGs in the `Screenshots` folder within your Pictures directory and copied to the clipboard. To change the folder, go to **Screen → Save folder → Choose folder…**. The gallery shows screenshots from the selected folder.

Quick Notes saves Markdown files under `Documents/Clipify Notes/YYYY-MM-DD/HH-mm-ss.md` by default. To change the destination, go to **Quick Notes → Save folder → Choose folder…**. Notes saved in the previous folder remain in the overview.

## Start in the tray at login

Turn on **Settings → Auto Start** to launch Clipify in the tray at login. Turn it off to remove the user autostart entry.

You can also start Clipify in the tray with `clipify --background`. **Open Window** in the tray menu or **Clipify** in the application menu opens the running instance. Choose **Quit** in the tray to exit.

## Image conversion

The Debian package installs the Qt image format plugins used by **Converter**, including WebP and TIFF support. For source builds, install `qt6-image-formats-plugins` using the dependencies listed in the README. The Converter lists the output formats available in your Qt installation.

## Local Whisper dictation

Whisper records from the default microphone on Linux. X11 supports the configurable **Super+Shift+D** hotkey while Clipify is in the tray. Choose **Toggle recording** to stop with a second press, or **Hold to talk** to stop on release. The controls under **Whisper** also start and stop recording. Clipify copies the transcript and saves it to local history. For hotkey dictation on X11, it also pastes into the previously focused application when it still has focus. Recording files are temporary and are removed when the job ends. Transcription runs offline once the executable and model are present.

The current installer does not bundle Whisper yet. For a source build, install `pipewire-bin` on PipeWire systems or `alsa-utils` for the ALSA fallback. Build [whisper.cpp](https://github.com/ggml-org/whisper.cpp/tree/v1.8.7) with the following options from its checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DGGML_NATIVE=OFF
cmake --build build --target whisper-cli
sh models/download-ggml-model.sh base
```

Place `whisper-cli` next to the Clipify executable or on your `PATH`. Place `ggml-base.bin` under `whisper/` next to the Clipify executable or under `whisper/` in Clipify's user data directory. You can also select a compatible model in the app. The default **base** model supports multiple languages.

For future Debian packaging, ship the CPU-compatible `whisper-cli` and any required libraries under `/usr/lib/clipify/`, install the pinned multilingual model at `/usr/share/clipify/whisper/ggml-base.bin`, and include the microphone recording dependency and upstream license notices. Bundle or explicitly install the model so the first dictation works offline.

## Configuration backup

Clipify starts with `config.json` in the per-user application configuration directory. **Settings → Load Config** makes the selected JSON file active. Its path is shown under **Active config**, and Clipify saves subsequent changes to that file automatically. **Save Config** exports a copy. The export includes autostart, global shortcuts, save folders, clipboard popup size, and OCR mode. Clipboard history, screenshots, and Markdown files are stored separately.

The same directory contains `default-config.json` with the factory settings. **Settings → Restore factory settings** applies those defaults to the local config and makes it active. External config files and saved content are kept.

## Optional Accurate OCR models

Choose **Fast** or **Accurate** under **Clipboard**. Both recognize German and English locally and put the recognized text in the clipboard. The selected image stays in memory.

**Fast** uses the language packages included in the installation steps. **Accurate** requires a one-time download of the `tessdata_best` models. Install `curl` if needed, then run:

```bash
clipify-install-ocr-best
```

From a source checkout, you can also use `sh scripts/install-ocr-best.sh`. Both OCR modes work offline once the models are installed.

## Troubleshooting

- If Clipify reports a shortcut conflict, check your desktop's keyboard settings and the other bindings in Clipify.
- If the console reports a missing accessibility bus socket, signing out and back in may restore the session's AT-SPI socket.
- Windows and Wayland support are planned. The global shortcuts in this guide are for X11.
