# Copyright 2021 Erfan Abdi
# SPDX-License-Identifier: GPL-3.0-or-later
import logging
import os
import shutil
import time
import tools.config
import tools.helpers.props
import tools.helpers.ipc
from tools.interfaces import IPlatform
from tools.interfaces import IStatusBarService
import dbus

def install(args):
    import sys
    try:
        tools.helpers.ipc.DBusSessionService()

        cm = tools.helpers.ipc.DBusContainerService()
        session = cm.GetSession()
        if session["state"] == "FROZEN":
            cm.Unfreeze()

        tmp_dir = tools.config.session_defaults["linstack_data"] + "/linstack_tmp"
        os.makedirs(tmp_dir, mode=0o755, exist_ok=True)
        os.chmod(tmp_dir, 0o755)

        dest_apk = tmp_dir + "/base.apk"
        shutil.copyfile(args.PACKAGE, dest_apk)
        os.chmod(dest_apk, 0o644)

        installed = False
        platformService = IPlatform.get_service(args)
        if platformService:
            ret = platformService.installApp("/data/linstack_tmp/base.apk")
            if ret == 0 or ret is None:
                installed = True
        if not installed:
            ip = tools.helpers.net.get_device_ip_address() or "192.168.245.2"
            tools.helpers.run.user(args, ["adb", "connect", f"{ip}:5555"], check=False)
            code = tools.helpers.run.user(args, ["adb", "-s", f"{ip}:5555", "install", "-r", args.PACKAGE], check=False)
            if code == 0:
                installed = True

        if os.path.exists(dest_apk):
            os.remove(dest_apk)

        if session["state"] == "FROZEN":
            cm.Freeze()

        if not installed:
            logging.error("Failed to install APK package")
            sys.exit(1)
    except (dbus.DBusException, KeyError):
        logging.error("LinStack session is stopped")
        sys.exit(1)

def remove(args):
    try:
        tools.helpers.ipc.DBusSessionService()

        cm = tools.helpers.ipc.DBusContainerService()
        session = cm.GetSession()
        if session["state"] == "FROZEN":
            cm.Unfreeze()

        platformService = IPlatform.get_service(args)
        if platformService:
            ret = platformService.removeApp(args.PACKAGE)
            if ret != 0:
                logging.error("Failed to uninstall package: {}".format(args.PACKAGE))
        else:
            logging.error("Failed to access IPlatform service")

        if session["state"] == "FROZEN":
            cm.Freeze()
    except dbus.DBusException:
        logging.error("LinStack session is stopped")

def maybeLaunchLater(args, launchNow):
    try:
        tools.helpers.ipc.DBusSessionService()
        try:
            tools.helpers.ipc.DBusContainerService().Unfreeze()
        except Exception:
            logging.error("Failed to unfreeze container. Trying to launch anyways...")
        launchNow()
    except dbus.DBusException:
        logging.error("Starting linstack session")
        tools.actions.session_manager.start(args, launchNow, background=False)

def launch(args):
    def justLaunch():
        platformService = IPlatform.get_service(args)
        if platformService:
            platformService.setprop("linstack.active_apps", args.PACKAGE)
            platformService.setprop("waydroid.active_apps", args.PACKAGE)
            platformService.launchApp(args.PACKAGE)
            multiwin = platformService.getprop(
                "persist.waydroid.multi_windows",
                platformService.getprop("persist.linstack.multi_windows", "false")
            )
            if multiwin == "false":
                platformService.settingsPutString(
                    2, "policy_control", "immersive.status=*")
            else:
                platformService.settingsPutString(
                    2, "policy_control", "immersive.full=*")
        else:
            logging.error("Failed to access IPlatform service")
    maybeLaunchLater(args, justLaunch)

def list(args):
    try:
        tools.helpers.ipc.DBusSessionService()

        cm = tools.helpers.ipc.DBusContainerService()
        session = cm.GetSession()
        if session["state"] == "FROZEN":
            cm.Unfreeze()

        platformService = IPlatform.get_service(args)
        if platformService:
            appsList = platformService.getAppsInfo()
            for app in appsList:
                print("Name: " + app["name"])
                print("packageName: " + app["packageName"])
                print("categories:")
                for cat in app["categories"]:
                    print("\t" + cat)
        else:
            logging.error("Failed to access IPlatform service")

        if session["state"] == "FROZEN":
            cm.Freeze()
    except dbus.DBusException:
        logging.error("LinStack session is stopped")

def showFullUI(args):
    def justShow():
        platformService = IPlatform.get_service(args)
        if platformService:
            platformService.setprop("linstack.active_apps", "Waydroid")
            platformService.setprop("waydroid.active_apps", "Waydroid")
            platformService.settingsPutString(2, "policy_control", "null*")
            # HACK: Refresh display contents
            statusBarService = IStatusBarService.get_service(args)
            if statusBarService:
                statusBarService.expand()
                time.sleep(0.5)
                statusBarService.collapse()
        else:
            logging.error("Failed to access IPlatform service")
    maybeLaunchLater(args, justShow)

def intent(args):
    def justLaunch():
        platformService = IPlatform.get_service(args)
        if platformService:
            ret = platformService.launchIntent(args.ACTION, args.URI)
            if ret == "":
                return
            pkg = ret if ret != "android" else "Waydroid"
            platformService.setprop("linstack.active_apps", pkg)
            platformService.setprop("waydroid.active_apps", pkg)
            multiwin = platformService.getprop(
                "persist.waydroid.multi_windows",
                platformService.getprop("persist.linstack.multi_windows", "false")
            )
            if multiwin == "false":
                platformService.settingsPutString(
                    2, "policy_control", "immersive.status=*")
            else:
                platformService.settingsPutString(
                    2, "policy_control", "immersive.full=*")
        else:
            logging.error("Failed to access IPlatform service")
    maybeLaunchLater(args, justLaunch)
