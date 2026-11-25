#!/usr/bin/python3
"""Register Clipify's Super+V shortcut in GNOME Settings."""

from gi.repository import Gio

media_keys_schema = "org.gnome.settings-daemon.plugins.media-keys"
custom_key_schema = media_keys_schema + ".custom-keybinding"
keybindings_key = "custom-keybindings"
clipify_path = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/clipify/"
accelerator = "<Super>v"
command = (
    "gdbus call --session --dest io.github.riccify.Clipify "
    "--object-path /io/github/riccify/Clipify --method io.github.riccify.Clipify.ShowHistory"
)

settings = Gio.Settings.new(media_keys_schema)
paths = settings.get_strv(keybindings_key)
clipify = Gio.Settings.new_with_path(custom_key_schema, clipify_path)
if clipify_path in paths:
    accelerator = clipify.get_string("binding") or accelerator
for path in paths:
    shortcut = Gio.Settings.new_with_path(custom_key_schema, path)
    if path != clipify_path and shortcut.get_string("binding").casefold() == accelerator.casefold():
        raise SystemExit(f"{accelerator} is already assigned to another custom shortcut.")

clipify.set_string("name", "Clipify History")
clipify.set_string("command", command)
clipify.set_string("binding", accelerator)
if clipify_path not in paths:
    settings.set_strv(keybindings_key, [*paths, clipify_path])
Gio.Settings.sync()
