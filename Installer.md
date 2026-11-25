# Clipify installieren

## Zorin OS 18.1 und Ubuntu 24.04

Der aktuelle Desktop-Client wird aus dem Quellcode gebaut. Die folgenden Schritte sind für eine X11-Sitzung vorgesehen. Mit `echo "$XDG_SESSION_TYPE"` lässt sich der Sitzungstyp prüfen.

### Pakete installieren

```bash
sudo apt update
sudo apt install git build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev libx11-dev libxtst6 \
  libqt6sql6-sqlite qml6-module-qtqml qml6-module-qtqml-workerscript \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-layouts qml6-module-qtquick-templates \
  qml6-module-qtquick-window
```

### Client bauen

```bash
git clone https://github.com/riccify/Clipify.git
cd Clipify
cmake -S . -B build -G Ninja
cmake --build build
```

Wenn das Repository bereits ausgecheckt ist, direkt in dessen Stammordner wechseln. Im Entwicklungsworkspace heißt dieser Ordner `CLI/`.

### Super+V auf Zorin und GNOME einrichten

GNOME verwaltet das Kürzel für das Benutzerkonto. Das Setup daher einmal pro Benutzer in der laufenden Desktop-Sitzung ausführen.

```bash
sudo apt install python3-gi libglib2.0-bin
/usr/bin/python3 scripts/setup-gnome-shortcut.py
```

Falls Clipify bereits läuft, die App über **Quit** im Tray beenden. Danach starten mit:

```bash
./build/clipify
```

Solange Clipify läuft, öffnet `Super+V` den Verlauf am Mauszeiger. Das Hauptfenster kann über das Tray-Menü wieder geöffnet werden. Auf anderen X11-Desktops registriert Clipify das Kürzel beim Start direkt, sofern es frei ist.

Unter **Clipboard → Clipboard history shortcut** auf die angezeigte Tastenkombination klicken und ein neues Kürzel drücken. `Esc` bricht die Aufnahme ab. Auf GNOME speichert Clipify die Auswahl in der Desktop-Belegung. Ein erneuter Aufruf des Setup-Skripts behält das gewählte Kürzel bei.

### Häufige Probleme

- Wenn das GNOME-Skript ein bereits belegtes `Super+V` meldet, die Belegung in den GNOME-Tastatureinstellungen prüfen.
- Eine Meldung über einen fehlenden Accessibility-Bus-Socket betrifft AT-SPI in der Desktop-Sitzung. Ab- und erneutes Anmelden kann den Socket wiederherstellen.
- Windows ist als Zielplattform vorgesehen. Für Windows und Wayland gibt es noch keine verifizierte Installationsanleitung mit funktionierendem Verlaufshotkey.
