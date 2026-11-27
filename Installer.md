# Installing Clipify

These instructions cover **Zorin OS 18.1 and Ubuntu 24.04 on X11**. Run `echo "$XDG_SESSION_TYPE"` in a terminal to check your session type.

## Install the .deb

From the folder containing the package, run:

```bash
sudo apt install ./clipify_0.1.5_amd64.deb
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

| Default | Action |
| --- | --- |
| `Super+V` | Open clipboard history at the pointer |
| `Super+Shift+S` | Select a screenshot region |
| `Ctrl+Print` | Capture the monitor under the pointer |
| `Super+Print` | Capture the focused window |
| `Alt+Print` | Select a region and copy its recognized text |

Other X11 desktops register available shortcuts when Clipify starts. To change a binding, click it under **Clipboard** or **Screen**, then press a key or key combination. Click the field again to cancel recording. Single-key bindings also apply globally. If your desktop already uses a shortcut, including GNOME's possible `Alt+Print` binding for window capture, free it in your desktop keyboard settings or choose an available binding in Clipify.

Screenshots are saved as PNGs in the `Screenshots` folder within your Pictures directory and copied to the clipboard. To change the folder, go to **Screen → Save folder → Choose folder…**. The gallery shows screenshots from the selected folder.

## Start in the tray at login

After installing the `.deb`, register the autostart entry for your user:

```bash
install -Dm644 /usr/share/clipify/io.github.riccify.Clipify-autostart.desktop \\
  "${XDG_CONFIG_HOME:-$HOME/.config}/autostart/io.github.riccify.Clipify-autostart.desktop"
```

For an installed source build, use `~/.local/share/clipify/io.github.riccify.Clipify-autostart.desktop` as the source file instead.

You can also start Clipify in the tray with `clipify --background`. **Open Window** in the tray menu or **Clipify** in the application menu opens the running instance. Choose **Quit** in the tray to exit.

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
