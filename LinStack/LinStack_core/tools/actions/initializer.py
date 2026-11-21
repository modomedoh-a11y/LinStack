# Copyright 2021 Erfan Abdi
# SPDX-License-Identifier: GPL-3.0-or-later
import logging
import os
from tools import helpers
import tools.config
import stat
import sys
import threading
import multiprocessing
import select
import queue
import time
import dbus
import dbus.service
import argparse
import subprocess
import shutil
import zipfile
from pathlib import Path
from gi.repository import GLib

DEFAULT_SYSTEM_URL = "https://sourceforge.net/projects/waydroid/files/images/system/lineage/waydroid_x86_64/lineage-18.1-20250628-VANILLA-waydroid_x86_64-system.zip/download"
DEFAULT_VENDOR_URL = "https://sourceforge.net/projects/waydroid/files/images/vendor/waydroid_x86_64/lineage-18.1-20250628-MAINLINE-waydroid_x86_64-vendor.zip/download"
INFO_NOTE_TEXT = "These are links for Waydroid Lineage OS Android 11 you can put your android images links or you can copy them to LinStack/LinStack/resources as system.img and vendor.img"

def resolve_project_resources():
    """Dynamically resolve the project root and LinStack/resources directory."""
    cur = Path(__file__).resolve()
    for parent in [cur] + list(cur.parents):
        if (parent / ".git").exists() or (parent / "install.sh").exists() or (parent / "wlroots_engine").exists():
            res = parent / "LinStack" / "resources"
            return res
        if (parent / "LinStack" / "resources").exists() and (parent / "LinStack" / "LinStack_core").exists():
            return (parent / "LinStack" / "resources").resolve()
        if (parent / "resources").exists() and (parent / "linstack.py").exists():
            return (parent / "resources").resolve()

    cwd = Path.cwd().resolve()
    for parent in [cwd] + list(cwd.parents):
        if (parent / ".git").exists() or (parent / "install.sh").exists():
            return (parent / "LinStack" / "resources").resolve()
        if (parent / "LinStack" / "resources").exists():
            return (parent / "LinStack" / "resources").resolve()

    return (cur.parents[3] / "resources").resolve()

def download_image_zip(url, target_path, progress_prefix="Downloading"):
    """Download a zip archive from URL handling redirects cleanly with streaming progress."""
    logging.info("Connecting to %s...", url)
    headers = {"User-Agent": "curl/8.6.0"}
    download_success = False

    try:
        import requests
        with requests.get(url, stream=True, allow_redirects=True, headers=headers, timeout=30) as r:
            r.raise_for_status()
            total_size = int(r.headers.get("content-length", 0))
            downloaded = 0
            last_log_time = 0
            with open(target_path, "wb") as f:
                for chunk in r.iter_content(chunk_size=256 * 1024):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        now = time.time()
                        if now - last_log_time >= 0.5 or (total_size and downloaded >= total_size):
                            last_log_time = now
                            if total_size > 0:
                                percent = (downloaded / total_size) * 100
                                mb_done = downloaded / (1024 * 1024)
                                mb_total = total_size / (1024 * 1024)
                                sys.stdout.write(f"\r{progress_prefix}: {percent:5.1f}% ({mb_done:6.1f} MB / {mb_total:6.1f} MB)")
                                sys.stdout.flush()
                            else:
                                mb_done = downloaded / (1024 * 1024)
                                sys.stdout.write(f"\r{progress_prefix}: {mb_done:6.1f} MB")
                                sys.stdout.flush()
        sys.stdout.write("\n")
        sys.stdout.flush()
        download_success = True
        logging.info("Download completed successfully.")
    except Exception as e:
        sys.stdout.write("\n")
        sys.stdout.flush()
        logging.warning("Requests download encountered: %s. Falling back to curl...", e)

    if not download_success:
        if shutil.which("curl"):
            cmd = ["curl", "-L", "-f", "--progress-bar", "-o", str(target_path), url]
            res = subprocess.run(cmd)
            if res.returncode != 0:
                raise RuntimeError(f"curl failed to download image from {url}")
        else:
            raise RuntimeError(f"Failed to download image from {url} and curl is not available")

def extract_image_from_zip(zip_path, target_img_name, dest_dir):
    """Extract target image (system.img or vendor.img) from zip archive into dest_dir."""
    logging.info("Extracting %s into %s...", target_img_name, dest_dir)
    dest_path = os.path.join(dest_dir, target_img_name)
    with zipfile.ZipFile(zip_path, "r") as zf:
        names = zf.namelist()
        member_to_extract = None
        for name in names:
            if os.path.basename(name) == target_img_name:
                member_to_extract = name
                break
        if not member_to_extract:
            for name in names:
                if name.endswith(".img"):
                    member_to_extract = name
                    break

        if member_to_extract:
            with zf.open(member_to_extract) as src, open(dest_path, "wb") as dst:
                shutil.copyfileobj(src, dst)
        else:
            zf.extractall(dest_dir)

    try:
        if os.path.exists(zip_path):
            os.remove(zip_path)
    except Exception as e:
        logging.debug("Could not remove temp zip %s: %s", zip_path, e)

    try:
        if os.geteuid() == 0 and os.path.exists(dest_path):
            dir_st = os.stat(dest_dir)
            os.chown(dest_path, dir_st.st_uid, dir_st.st_gid)
    except Exception as e:
        logging.debug("Could not chown %s: %s", dest_path, e)

    logging.info("Extracted %s successfully.", target_img_name)

def ensure_images_available(args, system_url=None, vendor_url=None):
    """
    Smart Initialization Logic:
      Step A: Check if system.img and vendor.img exist in LinStack/resources/.
      Step B: If both exist, skip downloading and initialize using local files.
      Step C: If either is missing, download zip archive(s) and extract them into LinStack/resources/.
    """
    res_dir = resolve_project_resources()
    os.makedirs(res_dir, exist_ok=True)

    system_img_path = os.path.join(res_dir, "system.img")
    vendor_img_path = os.path.join(res_dir, "vendor.img")

    system_exists = os.path.isfile(system_img_path) and os.path.getsize(system_img_path) > 0
    vendor_exists = os.path.isfile(vendor_img_path) and os.path.getsize(vendor_img_path) > 0

    if system_exists and vendor_exists:
        logging.info("Step A/B: Found local system.img and vendor.img in %s.", res_dir)
        logging.info("Skipping download; initializing container using local images.")
        args.images_path = str(res_dir)
        return True

    logging.info("Step A: Checking local files in %s...", res_dir)
    if not system_exists:
        logging.info("system.img is missing in %s", res_dir)
    if not vendor_exists:
        logging.info("vendor.img is missing in %s", res_dir)

    sys_url = (system_url or getattr(args, "system_channel", None) or "").strip()
    if not sys_url:
        sys_url = DEFAULT_SYSTEM_URL
    vnd_url = (vendor_url or getattr(args, "vendor_channel", None) or "").strip()
    if not vnd_url:
        vnd_url = DEFAULT_VENDOR_URL

    if not system_exists:
        logging.info("Step C: Downloading system image archive from: %s", sys_url)
        temp_system_zip = os.path.join(res_dir, "temp_system_download.zip")
        download_image_zip(sys_url, temp_system_zip, progress_prefix="Downloading system image")
        extract_image_from_zip(temp_system_zip, "system.img", res_dir)

    if not vendor_exists:
        logging.info("Step C: Downloading vendor image archive from: %s", vnd_url)
        temp_vendor_zip = os.path.join(res_dir, "temp_vendor_download.zip")
        download_image_zip(vnd_url, temp_vendor_zip, progress_prefix="Downloading vendor image")
        extract_image_from_zip(temp_vendor_zip, "vendor.img", res_dir)

    args.images_path = str(res_dir)
    return True

def is_initialized(args):
    return os.path.isfile(args.config) and os.path.isdir(tools.config.defaults["rootfs"])

def get_vendor_type(args):
    vndk_str = helpers.props.host_get(args, "ro.vndk.version")
    vendorapi_str = helpers.props.host_get(args, "ro.vendor.build.version.sdk")
    ret = "MAINLINE"
    if vndk_str != "":
        vndk = int(vndk_str)
        if vndk > 19:
            halium_ver = vndk - 19
            if vndk > 31:
                halium_ver -= 1 # 12L -> Halium 12
            ret = "HALIUM_" + str(halium_ver)
            if vndk == 32:
                ret += "L"
    elif vendorapi_str != "":
        vendorapi = int(vendorapi_str)
        if vendorapi > 32:
            halium_ver = vendorapi - 20
            ret = "HALIUM_" + str(halium_ver)

    return ret

def setup_config(args):
    cfg = tools.config.load(args)
    args.arch = helpers.arch.host()
    cfg["linstack"]["arch"] = args.arch

    args.vendor_type = get_vendor_type(args)
    cfg["linstack"]["vendor_type"] = args.vendor_type

    helpers.drivers.setupBinderNodes(args)
    cfg["linstack"]["binder"] = args.BINDER_DRIVER
    cfg["linstack"]["vndbinder"] = args.VNDBINDER_DRIVER
    cfg["linstack"]["hwbinder"] = args.HWBINDER_DRIVER

    has_preinstalled_images = False
    if args.images_path and args.images_path not in tools.config.defaults["preinstalled_images_paths"]:
        tools.config.defaults["preinstalled_images_paths"].insert(0, args.images_path)
    for preinstalled_images in tools.config.defaults["preinstalled_images_paths"]:
        if os.path.isdir(preinstalled_images):
            system_path = os.path.join(preinstalled_images, "system.img")
            vendor_path = os.path.join(preinstalled_images, "vendor.img")
            system_exists = os.path.isfile(system_path) or (os.path.exists(system_path) and stat.S_ISBLK(os.stat(system_path).st_mode))
            vendor_exists = os.path.isfile(vendor_path) or (os.path.exists(vendor_path) and stat.S_ISBLK(os.stat(vendor_path).st_mode))
            if system_exists and vendor_exists:
                has_preinstalled_images = True
                args.images_path = preinstalled_images
                logging.info("Found local preinstalled images at %s", preinstalled_images)
                break
            else:
                logging.debug("Found directory %s but missing system or vendor image, ignoring...", preinstalled_images)

    if not args.images_path:
        args.images_path = tools.config.defaults["images_path"]
    cfg["linstack"]["images_path"] = args.images_path

    if has_preinstalled_images:
        cfg["linstack"]["system_ota"] = args.system_ota = "None"
        cfg["linstack"]["vendor_ota"] = args.vendor_ota = "None"
        cfg["linstack"]["system_datetime"] = tools.config.defaults["system_datetime"]
        cfg["linstack"]["vendor_datetime"] = tools.config.defaults["vendor_datetime"]
        tools.config.save(args, cfg)
        logging.info("Configured LinStack with local preinstalled images from %s", args.images_path)
        return True

    channels_cfg = tools.config.load_channels()
    if not args.system_channel:
        args.system_channel = channels_cfg["channels"].get("system_channel", "")
    if not args.vendor_channel:
        args.vendor_channel = channels_cfg["channels"].get("vendor_channel", "")
    if not args.rom_type:
        args.rom_type = channels_cfg["channels"].get("rom_type", "lineage")
    if not args.system_type:
        args.system_type = channels_cfg["channels"].get("system_type", "VANILLA")

    if not args.system_channel or not args.vendor_channel:
        logging.error("ERROR: No preinstalled images found at /var/lib/linstack/images and no valid OTA URLs configured.")
        logging.error("Please place system.img and vendor.img into /var/lib/linstack/images or specify --images_path")
        return False

    args.system_ota = args.system_channel + "/" + args.rom_type + \
        "/linstack_" + args.arch + "/" + args.system_type + ".json"
    system_request = helpers.http.retrieve(args.system_ota)
    if system_request[0] != 200:
        raise ValueError(
            "Failed to get system OTA channel: {}, error: {}".format(args.system_ota, system_request[0]))

    device_codename = helpers.props.host_get(args, "ro.product.device")
    args.vendor_type = None
    for vendor in [device_codename, get_vendor_type(args)]:
        vendor_ota = args.vendor_channel + "/linstack_" + \
            args.arch + "/" + vendor.replace(" ", "_") + ".json"
        vendor_request = helpers.http.retrieve(vendor_ota)
        if vendor_request[0] == 200:
            args.vendor_type = vendor
            args.vendor_ota = vendor_ota
            break

    if not args.vendor_type:
        raise ValueError(
            "Failed to get vendor OTA channel: {}".format(vendor_ota))

    if args.system_ota != cfg["linstack"].get("system_ota"):
        cfg["linstack"]["system_datetime"] = tools.config.defaults["system_datetime"]
    if args.vendor_ota != cfg["linstack"].get("vendor_ota"):
        cfg["linstack"]["vendor_datetime"] = tools.config.defaults["vendor_datetime"]

    cfg["linstack"]["vendor_type"] = args.vendor_type
    cfg["linstack"]["system_ota"] = args.system_ota
    cfg["linstack"]["vendor_ota"] = args.vendor_ota
    tools.config.save(args, cfg)
    return True

def init(args):
    already_init = is_initialized(args)
    if already_init and not args.force:
        logging.info("Already initialized. Re-verifying configuration with local images...")

    ensure_images_available(args, getattr(args, "system_channel", None), getattr(args, "vendor_channel", None))

    if not setup_config(args):
        return

    status = "STOPPED"
    session = None
    if os.path.exists(tools.config.defaults["lxc"] + "/linstack"):
        status = helpers.lxc.status(args)
    if status != "STOPPED":
        if "running_init_in_service" in args:
            session = args.session
            tools.actions.container_manager.stop(args, False)
        else:
            logging.info("Stopping container")
            try:
                container = tools.helpers.ipc.DBusContainerService()
                session = container.GetSession()
                container.Stop(False)
            except Exception as e:
                logging.debug(e)
                tools.actions.container_manager.stop(args, False)
    if args.images_path not in tools.config.defaults["preinstalled_images_paths"]:
        helpers.images.get(args)
    else:
        if args.force or not already_init:
            helpers.images.remove_overlay(args)
    if not os.path.isdir(tools.config.defaults["rootfs"]):
        os.mkdir(tools.config.defaults["rootfs"])
    if not os.path.isdir(tools.config.defaults["overlay"]):
        os.mkdir(tools.config.defaults["overlay"])
        os.mkdir(tools.config.defaults["overlay"]+"/vendor")
    if not os.path.isdir(tools.config.defaults["overlay_rw"]):
        os.mkdir(tools.config.defaults["overlay_rw"])
        os.mkdir(tools.config.defaults["overlay_rw"]+"/system")
        os.mkdir(tools.config.defaults["overlay_rw"]+"/vendor")
    helpers.drivers.probeAshmemDriver(args)
    helpers.lxc.setup_host_perms(args)
    helpers.lxc.set_lxc_config(args)
    helpers.lxc.make_base_props(args)
    if status != "STOPPED":
        try:
            if "running_init_in_service" in args:
                tools.actions.container_manager.do_start(args, session)
            else:
                logging.info("Starting container")
                container.Start(session)
        except Exception as e:
            logging.debug(e)
            logging.error("Failed to restart container. Please do so manually.")

class DbusInitializer(dbus.service.Object):
    def __init__(self, looper, bus, object_path, args):
        self.args = args
        self.looper = looper
        self.worker_thread = None
        dbus.service.Object.__init__(self, bus, object_path)

    @helpers.logging.log_exceptions
    @dbus.service.method("id.linstack.Initializer", in_signature='a{ss}', out_signature='', sender_keyword="sender", connection_keyword="conn")
    def Init(self, params, sender=None, conn=None):
        if self.worker_thread is not None:
            self.worker_thread.kill()
            self.worker_thread.join()

        channels_cfg = tools.config.load_channels()
        no_auth = params["system_channel"] == channels_cfg["channels"]["system_channel"] and \
                  params["vendor_channel"] == channels_cfg["channels"]["vendor_channel"]
        if no_auth or ensure_polkit_auth(sender, conn, "id.linstack.Initializer.Init"):
            self.worker_thread = remote_init_server(self.args, self, params)
        else:
            raise PermissionError("Polkit: Authentication failed")

    @helpers.logging.log_exceptions
    @dbus.service.method("id.linstack.Initializer", in_signature='', out_signature='')
    def Cancel(self):
        if self.worker_thread is not None:
            self.worker_thread.kill()
            self.worker_thread.join()

    @dbus.service.signal("id.linstack.Initializer", signature='s')
    def ProgressChanged(self, message):
        pass

    @dbus.service.signal("id.linstack.Initializer", signature='')
    def Finished(self):
        pass

    @dbus.service.signal("id.linstack.Initializer", signature='')
    def Interrupted(self):
        pass

def ensure_polkit_auth(sender, conn, privilege):
    dbus_info = dbus.Interface(conn.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus/Bus", False), "org.freedesktop.DBus")
    pid = dbus_info.GetConnectionUnixProcessID(sender)
    polkit = dbus.Interface(dbus.SystemBus().get_object("org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority", False), "org.freedesktop.PolicyKit1.Authority")
    try:
        (is_auth, _, _) = polkit.CheckAuthorization(
            ("unix-process", {
                "pid": dbus.UInt32(pid, variant_level=1),
                "start-time": dbus.UInt64(0, variant_level=1)}),
            privilege, {"AllowUserInteraction": "true"},
            dbus.UInt32(1),
            "",
            timeout=300)
        return is_auth
    except dbus.DBusException:
        raise PermissionError("Polkit: Authentication timed out")

class StdoutRedirect(logging.StreamHandler):
    def __init__(self, pipe):
        logging.StreamHandler.__init__(self)
        self.pipe = pipe
    def write(self, s):
        self.pipe.send(s)
    def flush(self):
        pass
    def emit(self, record):
        if record.levelno >= logging.INFO:
            self.write(self.format(record) + self.terminator)

def remote_init_proc_entry(args, pipe):
    out = StdoutRedirect(pipe)
    sys.stdout = sys.stderr = out

    helpers.logging.add_verbose_log_level()
    logging.getLogger().addHandler(out)

    try:
        init(args)
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(1)
    except Exception:
        logging.exception("Exception during init")
        sys.exit(1)
    finally:
        pipe.close()

def remote_init_server(_args, dbus_obj, params):
    args = argparse.Namespace()
    tools.prep_args(args)
    args.details_to_stdout = _args.details_to_stdout
    args.verbose = _args.verbose
    args.quiet = _args.quiet
    args.force = True
    args.images_path = ""
    args.rom_type = ""
    args.system_channel = params["system_channel"]
    args.vendor_channel = params["vendor_channel"]
    args.system_type = params["system_type"]
    args.running_init_in_service = True

    parent_conn, child_conn = multiprocessing.Pipe(False)
    p = multiprocessing.Process(target=remote_init_proc_entry, args=(args, child_conn,), daemon=True)
    p.start()

    def monitor_init(p, pipe):
        try:
            while True:
                dbus_obj.ProgressChanged(pipe.recv())
        except EOFError:
            pass

        p.join()
        if p.exitcode == 0:
            GLib.idle_add(dbus_obj.Finished)
        else:
            GLib.idle_add(dbus_obj.Interrupted)

    t = threading.Thread(target=monitor_init, args=(p, parent_conn,), daemon=True)
    t.kill = lambda: p.kill()
    t.start()
    return t


def remote_init_client(args):
    # Local imports cause Gtk is intrusive
    import gi
    gi.require_version("Gtk", "3.0")
    from gi.repository import Gtk

    class LinstackInitWindow(Gtk.Window):
        def __init__(self):
            super().__init__(title="Initialize Linstack")
            channels_cfg = tools.config.load_channels()

            self.set_default_size(720, 440)
            self.set_icon_name("linstack")

            grid = Gtk.Grid(row_spacing=8, column_spacing=8, margin=14, column_homogeneous=False)
            grid.set_hexpand(True)
            grid.set_vexpand(True)
            self.add(grid)

            sys_val = channels_cfg["channels"].get("system_channel") or DEFAULT_SYSTEM_URL
            vnd_val = channels_cfg["channels"].get("vendor_channel") or DEFAULT_VENDOR_URL

            sysOtaLabel = Gtk.Label(label="System Image URL")
            sysOtaLabel.set_xalign(0.0)
            sysOtaEntry = Gtk.Entry()
            sysOtaEntry.set_hexpand(True)
            sysOtaEntry.set_text(sys_val)
            grid.attach(sysOtaLabel, 0, 0, 1, 1)
            grid.attach_next_to(sysOtaEntry, sysOtaLabel, Gtk.PositionType.RIGHT, 2, 1)
            self.sysOta = sysOtaEntry.get_buffer()

            vndOtaLabel = Gtk.Label(label="Vendor Image URL")
            vndOtaLabel.set_xalign(0.0)
            vndOtaEntry = Gtk.Entry()
            vndOtaEntry.set_hexpand(True)
            vndOtaEntry.set_text(vnd_val)
            grid.attach(vndOtaLabel, 0, 1, 1, 1)
            grid.attach_next_to(vndOtaEntry, vndOtaLabel, Gtk.PositionType.RIGHT, 2, 1)
            self.vndOta = vndOtaEntry.get_buffer()

            # Informational Note Label
            infoLabel = Gtk.Label(label=INFO_NOTE_TEXT)
            infoLabel.set_line_wrap(True)
            infoLabel.set_max_width_chars(75)
            infoLabel.set_xalign(0.0)
            infoLabel.set_margin_top(4)
            infoLabel.set_margin_bottom(6)
            grid.attach(infoLabel, 0, 2, 3, 1)

            sysTypeLabel = Gtk.Label(label="Android Type")
            sysTypeLabel.set_xalign(0.0)
            sysTypesStore = Gtk.ListStore(str, str)
            sysTypesStore.append(["VANILLA", "Minimal Android"])
            sysTypesStore.append(["GAPPS", "Android with Google Apps"])

            sysTypeCombo = Gtk.ComboBox.new_with_model(sysTypesStore)
            renderer_text = Gtk.CellRendererText()
            sysTypeCombo.pack_start(renderer_text, True)
            sysTypeCombo.add_attribute(renderer_text, "text", 1)
            sysTypeCombo.set_active(0)
            grid.attach(sysTypeLabel, 0, 3, 1, 1)
            grid.attach_next_to(sysTypeCombo, sysTypeLabel, Gtk.PositionType.RIGHT, 2, 1)
            self.sysType = sysTypeCombo

            downloadBtn = Gtk.Button(label="Init")
            downloadBtn.connect("clicked", self.on_download_btn_clicked)
            grid.attach(downloadBtn, 1, 4, 1, 1)
            self.downloadBtn = downloadBtn

            doneBtn = Gtk.Button(label="Done")
            doneBtn.connect("clicked", lambda x: self.destroy())
            doneBtn.get_style_context().add_class('suggested-action')
            grid.attach_next_to(doneBtn, downloadBtn, Gtk.PositionType.RIGHT, 1, 1)
            self.doneBtn = doneBtn

            outScrolledWindow = Gtk.ScrolledWindow()
            outScrolledWindow.set_hexpand(True)
            outScrolledWindow.set_vexpand(True)
            outTextView = Gtk.TextView()
            outTextView.set_property('editable', False)
            outTextView.set_property('cursor-visible', False)
            outScrolledWindow.add(outTextView)
            grid.attach(outScrolledWindow, 0, 5, 3, 1)
            self.outScrolledWindow = outScrolledWindow
            self.outTextView = outTextView
            self.outBuffer = outTextView.get_buffer()
            self.outBuffer.create_mark("end", self.outBuffer.get_end_iter(), False)

            self.bus_signals = []
            self.initializing = False

            self.connect("destroy", self.on_destroy)

        def scroll_to_bottom(self):
            self.outTextView.scroll_mark_onscreen(self.outBuffer.get_mark("end"))

        def on_download_btn_clicked(self, widget):
            widget.set_sensitive(False)
            self.doneBtn.hide()
            self.outTextView.show()
            sysType = self.sysType.get_model()[self.sysType.get_active_iter()][0]
            self.run_init(self.sysOta.get_text().strip(), self.vndOta.get_text().strip(), sysType)

        def draw(self, s):
            if s.startswith('\r'):
                last = self.outBuffer.get_iter_at_line(self.outBuffer.get_line_count()-1)
                last.backward_char()
                self.outBuffer.delete(last, self.outBuffer.get_end_iter())
            self.outBuffer.insert(self.outBuffer.get_end_iter(), s)
            self.scroll_to_bottom()

        def on_progress(self, message):
            self.draw(message)

        def on_finished(self):
            self.initializing = False
            if is_initialized(args):
                self.doneBtn.show()
                self.draw("\nDone\n")

        def on_interrupted(self):
            self.initializing = False
            self.draw("\nInterrupted\n")

        def on_reply(self):
            self.downloadBtn.set_sensitive(True)

        def on_bus_error(self, e):
            err_str = str(e)
            if hasattr(e, "get_dbus_name") and e.get_dbus_name() == "org.freedesktop.DBus.Python.PermissionError":
                self.draw(e.get_dbus_message().splitlines()[-1] + "\n")
            elif "org.freedesktop.DBus.Error.ServiceUnknown" in err_str or "not provided" in err_str or "Connection refused" in err_str:
                self.draw("Container service is not currently active.\n"
                          "Please start the service with:\n"
                          "  sudo systemctl start linstack-container\n"
                          "Or run container initialization directly in terminal:\n"
                          "  sudo linstack init\n")
            else:
                self.draw(err_str + "\n")
            self.downloadBtn.set_sensitive(True)
            self.initializing = False

        def on_destroy(self, _):
            if self.initializing:
                try:
                    tools.helpers.ipc.DBusContainerService("/Initializer", "id.linstack.Initializer").Cancel()
                except Exception as e:
                    logging.debug("Unexpected error while cancelling initializer: %s", e)
            Gtk.main_quit()

        def run_init(self, systemOta, vendorOta, systemType):
            for signal in self.bus_signals:
                signal.remove()

            self.draw("\nWaiting for linstack container service...\n")
            self.bus_signals = []
            self.initializing = True

            try:
                initializer = tools.helpers.ipc.DBusContainerService("/Initializer", "id.linstack.Initializer")

                self.bus_signals.append(initializer.connect_to_signal("ProgressChanged", self.on_progress))
                self.bus_signals.append(initializer.connect_to_signal("Finished", self.on_finished))
                self.bus_signals.append(initializer.connect_to_signal("Interrupted", self.on_interrupted))

                params = {
                    "system_channel": systemOta,
                    "vendor_channel": vendorOta,
                    "system_type": systemType
                }
                initializer.Init(params, reply_handler=self.on_reply, error_handler=self.on_bus_error)
            except dbus.DBusException as e:
                self.on_bus_error(e)
            except Exception as e:
                self.draw(f"{e}\n")
                self.downloadBtn.set_sensitive(True)
                self.initializing = False

    GLib.set_prgname("Linstack")
    win = LinstackInitWindow()

    win.show_all()
    win.outTextView.hide()
    win.doneBtn.hide()

    Gtk.main()
