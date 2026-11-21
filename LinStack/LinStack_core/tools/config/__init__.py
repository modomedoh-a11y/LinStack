# Copyright 2021 Oliver Smith
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import pwd

#
# Exported functions
#
from tools.config.load import load, load_channels
from tools.config.save import save

#
# Exported variables (internal configuration)
#
version = "1.6.3"
tools_src = os.path.normpath(os.path.realpath(__file__) + "/../../..")

# Keys saved in the config file (mostly what we ask in 'linstack init')
config_keys = ["arch",
               "images_path",
               "vendor_type",
               "system_datetime",
               "vendor_datetime",
               "suspend_action",
               "mount_overlays",
               "auto_adb"]

from pathlib import Path
import os

DEFAULT_SYSTEM_URL = "https://sourceforge.net/projects/waydroid/files/images/system/lineage/waydroid_x86_64/lineage-18.1-20250628-VANILLA-waydroid_x86_64-system.zip/download"
DEFAULT_VENDOR_URL = "https://sourceforge.net/projects/waydroid/files/images/vendor/waydroid_x86_64/lineage-18.1-20250628-MAINLINE-waydroid_x86_64-vendor.zip/download"

def _get_preinstalled_images_paths():
    paths = [
        "/var/lib/linstack/images",
    ]
    # 0. Check explicit environment override
    env_img = os.environ.get("LINSTACK_IMAGES_PATH")
    if env_img and os.path.isdir(env_img):
        resolved_env = str(Path(env_img).resolve())
        if resolved_env not in paths:
            paths.insert(0, resolved_env)

    # 1. Check relative to project root / portable build directories & user homes
    try:
        core_dir = Path(__file__).resolve().parent.parent.parent
        proj_root = None
        for p in [core_dir] + list(core_dir.parents):
            if (p / ".git").exists() or (p / "install.sh").exists() or (p / "wlroots_engine").exists():
                proj_root = p
                break
            if (p / "LinStack" / "resources").exists() and (p / "LinStack" / "LinStack_core").exists():
                proj_root = p
                break

        candidates = []
        if proj_root:
            candidates.append(proj_root / "LinStack" / "resources")
            candidates.append(proj_root / "resources")
        candidates.extend([
            core_dir.parent / "resources",
            core_dir.parent / "LinStack" / "resources",
            core_dir / "resources",
            Path.cwd() / "LinStack" / "resources",
            Path.cwd() / "resources",
            Path.cwd().parent / "LinStack" / "resources",
            Path.cwd().parent / "resources",
        ])
        home = Path.home()
        candidates.extend([
            home / "LinStack" / "LinStack" / "resources",
            home / "LinStack" / "resources",
        ])
        for c in candidates:
            c_resolved = str(c.resolve()) if c.exists() else str(c)
            if c.is_dir() and (c / "system.img").exists() and c_resolved not in paths:
                paths.append(c_resolved)
    except Exception:
        pass

    # 2. Standard system installation paths
    system_paths = [
        "/usr/share/linstack/resources",
        "/usr/local/share/linstack/resources",
        "/usr/share/linstack/images",
        "/etc/linstack-extra/images",
        "/usr/share/linstack-extra/images",
    ]
    for sp in system_paths:
        if sp not in paths:
            paths.append(sp)
    return paths

# Config file/commandline default values
# $WORK gets replaced with the actual value for args.work (which may be
# overridden on the commandline)
defaults = {
    "arch": "arm64",
    "work": os.environ.get("LINSTACK_WORK_DIR", "/var/lib/linstack"),
    "vendor_type": "MAINLINE",
    "system_datetime": "0",
    "vendor_datetime": "0",
    "preinstalled_images_paths": _get_preinstalled_images_paths(),
    "suspend_action": "freeze",
    "mount_overlays": "True",
    "auto_adb": "False",
    "container_xdg_runtime_dir": "/run/xdg",
    "container_wayland_display": "wayland-0",
}
defaults["images_path"] = defaults["work"] + "/images"
defaults["rootfs"] = defaults["work"] + "/rootfs"
defaults["overlay"] = defaults["work"] + "/overlay"
defaults["overlay_rw"] = defaults["work"] + "/overlay_rw"
defaults["overlay_work"] = defaults["work"] + "/overlay_work"
defaults["data"] = defaults["work"] + "/data"
defaults["lxc"] = defaults["work"] + "/lxc"
defaults["host_perms"] = defaults["work"] + "/host-permissions"
defaults["container_pulse_runtime_path"] = defaults["container_xdg_runtime_dir"] + "/pulse"

session_defaults = {
    "user_name": pwd.getpwuid(os.getuid()).pw_name,
    "user_id": str(os.getuid()),
    "group_id": str(os.getgid()),
    "host_user": os.path.expanduser("~"),
    "pid": str(os.getpid()),
    "xdg_data_home": str(os.environ.get('XDG_DATA_HOME', os.path.expanduser("~") + "/.local/share")),
    "xdg_runtime_dir": str(os.environ.get('XDG_RUNTIME_DIR')),
    "wayland_display": os.environ.get('WAYLAND_DISPLAY') or "wayland-linstack-0",
    "pulse_runtime_path": str(os.environ.get('PULSE_RUNTIME_PATH')),
    "state": "STOPPED",
    "lcd_density": "0",
    "background_start": "true"
}
session_defaults["linstack_user_state"] = session_defaults["xdg_data_home"] + "/linstack"
session_defaults["linstack_data"] = session_defaults["linstack_user_state"] + "/data"
if session_defaults["pulse_runtime_path"] == "None":
    session_defaults["pulse_runtime_path"] = session_defaults["xdg_runtime_dir"] + "/pulse"

channels_defaults = {
    "config_path": "/usr/share/linstack-extra/channels.cfg",
    "system_channel": DEFAULT_SYSTEM_URL,
    "vendor_channel": DEFAULT_VENDOR_URL,
    "rom_type": "lineage",
    "system_type": "VANILLA"
}
channels_config_keys = ["system_channel",
                        "vendor_channel",
                        "rom_type",
                        "system_type"]
