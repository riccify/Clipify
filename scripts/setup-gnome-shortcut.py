#!/usr/bin/python3
"""Register Clipify's global shortcuts in GNOME Settings."""

import re
import sys

from gi.repository import Gio

media_keys_schema = "org.gnome.settings-daemon.plugins.media-keys"
custom_key_schema = media_keys_schema + ".custom-keybinding"
keybindings_key = "custom-keybindings"
prefix = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/"
service = "io.github.riccify.Clipify"
object_path = "/io/github/riccify/Clipify"

def normalized(binding):
    modifiers = re.findall(r"<([^>]+)>", binding.casefold())
    aliases = {"ctrl": "control", "ctl": "control", "mod1": "alt", "mod4": "super"}
    return tuple(sorted(aliases.get(modifier, modifier) for modifier in modifiers)), re.sub(r"<[^>]+>", "", binding.casefold())


settings = Gio.Settings.new(media_keys_schema)
paths = settings.get_strv(keybindings_key)
occupied = {}
schemas = Gio.SettingsSchemaSource.get_default()
for schema_id in (
    "org.gnome.shell.keybindings",
    "org.gnome.desktop.wm.keybindings",
    "org.gnome.mutter.keybindings",
    media_keys_schema,
):
    schema = schemas.lookup(schema_id, True)
    if schema is None:
        continue
    desktop = Gio.Settings.new(schema_id)
    for key in schema.list_keys():
        if key == keybindings_key:
            continue
        value = desktop.get_value(key)
        if value.get_type_string() in ("s", "as"):
            bindings = [value.unpack()] if value.get_type_string() == "s" else value.unpack()
            for binding in bindings:
                if binding and binding != "disabled":
                    occupied[normalized(binding)] = key

for path in paths:
    shortcut = Gio.Settings.new_with_path(custom_key_schema, path)
    binding = shortcut.get_string("binding")
    if binding:
        occupied[normalized(binding)] = shortcut.get_string("name") or path

for name, label, default, service_suffix, object_suffix, method in (
    ("clipify", "Clipify History", "<Super>v", "", "", "ShowHistory"),
    ("clipify-screen", "Clipify Screenshot Selection", "<Super><Shift>s", ".Screen", "/Screen", "CaptureScreen"),
    ("clipify-full-screen", "Clipify Full Screenshot", "<Control>Print", ".FullScreen", "/FullScreen", "CaptureScreen"),
    ("clipify-focused-window", "Clipify Focused Window Screenshot", "<Super>Print", ".FocusedWindow", "/FocusedWindow", "CaptureScreen"),
    ("clipify-whisper", "Clipify Whisper Dictation", "<Super><Shift>d", ".Whisper", "/Whisper", "ToggleDictation"),
    ("clipify-notes", "Clipify Quick Notes", "<Super>n", ".Notes", "/Notes", "OpenNotes"),
    ("clipify-ocr", "Clipify Copy Text from Screenshot", "<Alt>Print", ".Ocr", "/Ocr", "CaptureScreen"),
):
    path = prefix + name + "/"
    shortcut = Gio.Settings.new_with_path(custom_key_schema, path)
    if path in paths:
        continue

    accelerator = default
    conflict = occupied.get(normalized(accelerator))
    fallback = {"clipify": "<Super><Shift>v", "clipify-notes": "<Super><Shift>n"}.get(name)
    if conflict and fallback and normalized(fallback) not in occupied:
        accelerator = fallback
        print(f"{label}: {default} is already used by {conflict}. Clipify uses {fallback}.", file=sys.stderr)
    elif conflict:
        print(f"{label}: {accelerator} is already used by {conflict}. Choose a free shortcut in Clipify.", file=sys.stderr)
        accelerator = ""
    if accelerator:
        occupied[normalized(accelerator)] = label

    shortcut.set_string("name", label)
    shortcut.set_string("command", f"gdbus call --session --dest {service}{service_suffix} "
                        f"--object-path {object_path}{object_suffix} --method {service}.{method}")
    shortcut.set_string("binding", accelerator)
    paths.append(path)

if paths != settings.get_strv(keybindings_key):
    settings.set_strv(keybindings_key, paths)
Gio.Settings.sync()
