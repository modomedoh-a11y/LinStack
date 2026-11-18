# Copyright 2021 Erfan Abdi
# SPDX-License-Identifier: GPL-3.0-or-later
import logging
import zipfile
import json
import hashlib
import shutil
import os
import stat
import tools.config
from contextlib import suppress
from tools import helpers
from shutil import which

# takes an open file object
def sha256sum(f):
    h = hashlib.sha256()
    b = bytearray(128*1024)
    mv = memoryview(b)
    for n in iter(lambda: f.readinto(mv), 0):
        h.update(mv[:n])
    f.seek(0)
    return h.hexdigest()


def get(args):
    cfg = tools.config.load(args)
    system_ota = cfg["linstack"]["system_ota"]
    system_request = helpers.http.retrieve(system_ota)
    if system_request[0] != 200:
        raise ValueError(
            "Failed to get system OTA channel: {}, error: {}".format(args.system_ota, system_request[0]))
    system_responses = json.loads(system_request[1].decode('utf8'))["response"]
    if len(system_responses) < 1:
        raise ValueError("No images found on system channel")

    for system_response in system_responses:
        if system_response['datetime'] > int(cfg["linstack"]["system_datetime"]):
            images_zip = helpers.http.download(
                args, system_response['url'], system_response['filename'], cache=False)
            logging.info("Validating system image")
            with open(images_zip, 'rb') as f:
                if sha256sum(f) != system_response['id']:
                    with suppress(OSError):
                        os.remove(images_zip)
                    raise ValueError("Downloaded system image hash doesn't match, expected: {}".format(
                        system_response['id']))
                logging.info("Extracting to " + args.images_path)
                with zipfile.ZipFile(f, 'r') as zip_ref:
                    zip_ref.extractall(args.images_path)
            cfg["linstack"]["system_datetime"] = str(system_response['datetime'])
            tools.config.save(args, cfg)
            os.remove(images_zip)
            break

    vendor_ota = cfg["linstack"]["vendor_ota"]
    vendor_request = helpers.http.retrieve(vendor_ota)
    if vendor_request[0] != 200:
        raise ValueError(
            "Failed to get vendor OTA channel: {}, error: {}".format(vendor_ota, vendor_request[0]))
    vendor_responses = json.loads(vendor_request[1].decode('utf8'))["response"]
    if len(vendor_responses) < 1:
        raise ValueError("No images found on vendor channel")

    for vendor_response in vendor_responses:
        if vendor_response['datetime'] > int(cfg["linstack"]["vendor_datetime"]):
            images_zip = helpers.http.download(
                args, vendor_response['url'], vendor_response['filename'], cache=False)
            logging.info("Validating vendor image")
            with open(images_zip, 'rb') as f:
                if sha256sum(f) != vendor_response['id']:
                    with suppress(OSError):
                        os.remove(images_zip)
                    raise ValueError("Downloaded vendor image hash doesn't match, expected: {}".format(
                        vendor_response['id']))
                logging.info("Extracting to " + args.images_path)
                with zipfile.ZipFile(f, 'r') as zip_ref:
                    zip_ref.extractall(args.images_path)
            cfg["linstack"]["vendor_datetime"] = str(vendor_response['datetime'])
            tools.config.save(args, cfg)
            os.remove(images_zip)
            break
    remove_overlay(args)

def validate(args, channel, f):
    # Verify that the zip comes from the channel
    cfg = tools.config.load(args)
    channel_url = cfg["linstack"][channel]
    channel_request = helpers.http.retrieve(channel_url)
    if channel_request[0] != 200:
        return False
    channel_responses = json.loads(channel_request[1].decode('utf8'))["response"]
    chksum = sha256sum(f)
    for build in channel_responses:
        if chksum == build['id']:
            return True
    logging.warning(f"Could not verify the image {f.name} against {channel_url}")
    return False

def replace(args, system_zip, system_time, vendor_zip, vendor_time):
    cfg = tools.config.load(args)
    args.images_path = cfg["linstack"]["images_path"]
    if os.path.exists(system_zip):
        with open(system_zip, 'rb') as f:
            if validate(args, "system_ota", f):
                with zipfile.ZipFile(f, 'r') as zip_ref:
                    zip_ref.extractall(args.images_path)
                cfg["linstack"]["system_datetime"] = str(system_time)
            else:
                logging.warning("Failed to validate update system image, ignoring")
        os.remove(system_zip)
    if os.path.exists(vendor_zip):
        with open(vendor_zip, 'rb') as f:
            if validate(args, "vendor_ota", f):
                with zipfile.ZipFile(f, 'r') as zip_ref:
                    zip_ref.extractall(args.images_path)
                cfg["linstack"]["vendor_datetime"] = str(vendor_time)
            else:
                logging.warning("Failed to validate update vendor image, ignoring")
        os.remove(vendor_zip)
    tools.config.save(args, cfg)
    remove_overlay(args)

def remove_overlay(args):
    if os.path.isdir(tools.config.defaults["overlay_rw"]):
        shutil.rmtree(tools.config.defaults["overlay_rw"])
    if os.path.isdir(tools.config.defaults["overlay_work"]):
        shutil.rmtree(tools.config.defaults["overlay_work"])

def make_prop(args, cfg, full_props_path):
    if not os.path.isfile(args.work + "/linstack_base.prop"):
        raise RuntimeError("linstack_base.prop Not found")
    with open(args.work + "/linstack_base.prop") as f:
        props = f.read().splitlines()
    if not props:
        raise RuntimeError("linstack_base.prop is broken!!?")

    def add_prop(key, cfg_key):
        value = cfg[cfg_key]
        if value != "None":
            value = value.replace("/mnt/", "/mnt_extra/")
            props.append(key + "=" + value)

    add_prop("linstack.host.user", "user_name")
    add_prop("waydroid.host.user", "user_name")
    add_prop("linstack.host.uid", "user_id")
    add_prop("waydroid.host.uid", "user_id")
    add_prop("linstack.host.gid", "group_id")
    add_prop("waydroid.host.gid", "group_id")
    add_prop("linstack.host_data_path", "linstack_data")
    add_prop("waydroid.host_data_path", "linstack_data")
    add_prop("linstack.background_start", "background_start")
    add_prop("waydroid.background_start", "background_start")
    wayland_disp = cfg.get("wayland_display")
    if not wayland_disp or wayland_disp == "None":
        wayland_disp = os.environ.get("WAYLAND_DISPLAY", "wayland-linstack-0")
    if not wayland_disp or wayland_disp == "None":
        wayland_disp = "wayland-linstack-0"
    props.append("linstack.host.wayland_display=" + wayland_disp)
    props.append("waydroid.host.wayland_display=" + wayland_disp)
    props.append("persist.linstack.multi_windows=false")
    props.append("persist.waydroid.multi_windows=false")
    if "width" in cfg:
        props.append("persist.linstack.width=" + str(cfg["width"]))
        props.append("persist.waydroid.width=" + str(cfg["width"]))
    if "height" in cfg:
        props.append("persist.linstack.height=" + str(cfg["height"]))
        props.append("persist.waydroid.height=" + str(cfg["height"]))
    props.append("linstack.xdg_runtime_dir=" + tools.config.defaults["container_xdg_runtime_dir"])
    props.append("waydroid.xdg_runtime_dir=" + tools.config.defaults["container_xdg_runtime_dir"])
    props.append("linstack.pulse_runtime_path=" + tools.config.defaults["container_pulse_runtime_path"])
    props.append("waydroid.pulse_runtime_path=" + tools.config.defaults["container_pulse_runtime_path"])
    props.append("linstack.wayland_display=" + tools.config.defaults["container_wayland_display"])
    props.append("waydroid.wayland_display=" + tools.config.defaults["container_wayland_display"])
    if which("linstack-sensord") is None:
        props.append("linstack.stub_sensors_hal=1")
        props.append("waydroid.stub_sensors_hal=1")
    dpi = cfg["lcd_density"]
    if dpi != "0":
        props.append("ro.sf.lcd_density=" + dpi)

    with open(full_props_path, "w") as f:
        for prop in props:
            f.write(prop + "\n")
    os.chmod(full_props_path, 0o644)

def mount_rootfs(args, images_dir, session):
    cfg = tools.config.load(args)
    # Validate system.img exists at images_dir; if moved or missing, dynamically find it
    system_img_path = os.path.join(images_dir, "system.img")
    system_exists = os.path.isfile(system_img_path) or (os.path.exists(system_img_path) and stat.S_ISBLK(os.stat(system_img_path).st_mode))
    if not system_exists:
        logging.warning("System image not found at %s; searching dynamic candidate locations...", images_dir)
        search_candidates = []
        env_p = os.environ.get("LINSTACK_IMAGES_PATH")
        if env_p:
            search_candidates.append(env_p)
        search_candidates.extend(tools.config.defaults.get("preinstalled_images_paths", []))

        # Check user home paths
        try:
            home = os.path.expanduser("~")
            search_candidates.extend([
                os.path.join(home, "Downloads/LinStack/LinStack/resources"),
                os.path.join(home, "LinStack/LinStack/resources"),
                os.path.join(home, "LinStack/resources"),
            ])
        except Exception:
            pass

        for cand in search_candidates:
            cand_sys = os.path.join(cand, "system.img")
            if os.path.isfile(cand_sys) or (os.path.exists(cand_sys) and stat.S_ISBLK(os.stat(cand_sys).st_mode)):
                logging.info("Dynamically recovered images_dir to: %s", cand)
                images_dir = cand
                cfg["linstack"]["images_path"] = cand
                try:
                    tools.config.save(args, cfg)
                except Exception as e:
                    logging.warning("Could not persist recovered images_path: %s", e)
                break

    helpers.mount.mount(args, images_dir + "/system.img",
                        tools.config.defaults["rootfs"], umount=True, options=["loop"])
    if cfg["linstack"]["mount_overlays"] == "True":
        try:
            helpers.mount.mount_overlay(args, [tools.config.defaults["overlay"],
                                               tools.config.defaults["rootfs"]],
                                    tools.config.defaults["rootfs"],
                                    upper_dir=tools.config.defaults["overlay_rw"] + "/system",
                                    work_dir=tools.config.defaults["overlay_work"] + "/system")
        except RuntimeError:
            cfg["linstack"]["mount_overlays"] = "False"
            tools.config.save(args, cfg)
            logging.warning("Mounting overlays failed. The feature has been disabled.")

    helpers.mount.mount(args, images_dir + "/vendor.img",
                           tools.config.defaults["rootfs"] + "/vendor", options=["loop"])
    if cfg["linstack"]["mount_overlays"] == "True":
        helpers.mount.mount_overlay(args, [tools.config.defaults["overlay"] + "/vendor",
                                           tools.config.defaults["rootfs"] + "/vendor"],
                                    tools.config.defaults["rootfs"] + "/vendor",
                                    upper_dir=tools.config.defaults["overlay_rw"] + "/vendor",
                                    work_dir=tools.config.defaults["overlay_work"] + "/vendor")

    for egl_path in ["/vendor/lib/egl", "/vendor/lib64/egl"]:
        if os.path.isdir(egl_path):
            helpers.mount.bind(
                args, egl_path, tools.config.defaults["rootfs"] + egl_path)
    if helpers.mount.ismount("/odm"):
        helpers.mount.bind(
            args, "/odm", tools.config.defaults["rootfs"] + "/odm_extra")
    else:
        if os.path.isdir("/vendor/odm"):
            helpers.mount.bind(
                args, "/vendor/odm", tools.config.defaults["rootfs"] + "/odm_extra")

    make_prop(args, session, args.work + "/linstack.prop")
    for prop_name in ["waydroid.prop", "linstack.prop"]:
        prop_target = tools.config.defaults["rootfs"] + "/vendor/" + prop_name
        if os.path.exists(prop_target):
            try:
                helpers.mount.bind_file(args, args.work + "/linstack.prop", prop_target)
            except Exception as e:
                logging.warning("Failed to bind-mount to %s: %s", prop_target, e)

def umount_rootfs(args):
    helpers.mount.umount_all(args, tools.config.defaults["rootfs"])
