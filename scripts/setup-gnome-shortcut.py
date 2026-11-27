#!/usr/bin/python3
"""Register Clipify's history and screenshot shortcuts in GNOME Settings."""

from gi.repository import Gio

media_keys_schema = "org.gnome.settings-daemon.plugins.media-keys"
custom_key_schema = media_keys_schema + ".custom-keybinding"
keybindings_key = "custom-keybindings"
prefix = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/"
service = "io.github.riccify.Clipify"
object_path = "/io/github/riccify/Clipify"

settings = Gio.Settings.new(media_keys_schema)
paths = settings.get_strv(keybindings_key)
configured = []
for name, label, default, service_suffix, object_suffix, method in (
    ("clipify", "Clipify History", "<Super>v", "", "", "ShowHistory"),
    ("clipify-screen", "Clipify Screenshot Selection", "<Super><Shift>s", ".Screen", "/Screen", "CaptureScreen"),
    ("clipify-full-screen", "Clipify Full Screenshot", "<Control>Print", ".FullScreen", "/FullScreen", "CaptureScreen"),
    ("clipify-focused-window", "Clipify Focused Window Screenshot", "<Super>Print", ".FocusedWindow", "/FocusedWindow", "CaptureScreen"),
    ("clipify-ocr", "Clipify Copy Text from Screenshot", "<Alt>Print", ".Ocr", "/Ocr", "CaptureScreen"),
):
    path = prefix + name + "/"
    shortcut = Gio.Settings.new_with_path(custom_key_schema, path)
    accelerator = (shortcut.get_string("binding") if path in paths else "") or default
    if name == "clipify-screen" and accelerator == "<Control><Shift><Super>s":
        accelerator = default
    for other_path in paths:
        other = Gio.Settings.new_with_path(custom_key_schema, other_path)
        if accelerator and other_path != path and other.get_string("binding").casefold() == accelerator.casefold():
            raise SystemExit(f"{accelerator} is already assigned to another custom shortcut.")
    if accelerator and any(item[3].casefold() == accelerator.casefold() for item in configured):
        raise SystemExit(f"{accelerator} is already assigned to another Clipify shortcut.")
    configured.append((path, shortcut, label, accelerator, service_suffix, object_suffix, method))

for path, shortcut, label, accelerator, service_suffix, object_suffix, method in configured:
    shortcut.set_string("name", label)
    shortcut.set_string("command", f"gdbus call --session --dest {service}{service_suffix} "
                        f"--object-path {object_path}{object_suffix} --method {service}.{method}")
    shortcut.set_string("binding", accelerator)
    if path not in paths:
        paths.append(path)

settings.set_strv(keybindings_key, paths)
Gio.Settings.sync()
