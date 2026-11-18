# Copyright 2021 Oliver Smith
# SPDX-License-Identifier: GPL-3.0-or-later
from shutil import which
import subprocess
import logging
import tools.helpers.run
from tools.interfaces import IPlatform


def host_get(args, prop):
    if which("getprop") is not None:
        command = ["getprop", prop]
        return subprocess.run(command, stdout=subprocess.PIPE).stdout.decode('utf-8').strip()
    else:
        return ""

def host_list(args, prefix=""):
    if which("getprop") is None:
        return {}
    output = subprocess.run(["getprop"], stdout=subprocess.PIPE).stdout.decode('utf-8', errors='ignore')
    result = {}
    for line in output.splitlines():
        line = line.strip()
        # getprop output format: [key]: [value]
        if not line.startswith("[" + prefix) or "]: [" not in line:
            continue
        key, value = line.split("]: [", 1)
        key = key[1:]
        value = value[:-1] if value.endswith("]") else value
        if key.startswith(prefix):
            result[key] = value
    return result

def host_set(args, prop, value):
    if which("setprop") is not None:
        command = ["setprop", prop, value]
        tools.helpers.run.user(args, command)

def get(args, prop):
    platformService = IPlatform.get_service(args)
    if platformService:
        val = platformService.getprop(prop, "")
        if not val:
            if prop.startswith("persist.linstack."):
                val = platformService.getprop("persist.waydroid." + prop[len("persist.linstack."):], "")
            elif prop.startswith("persist.waydroid."):
                val = platformService.getprop("persist.linstack." + prop[len("persist.waydroid."):], "")
            elif prop.startswith("linstack."):
                val = platformService.getprop("waydroid." + prop[len("linstack."):], "")
            elif prop.startswith("waydroid."):
                val = platformService.getprop("linstack." + prop[len("waydroid."):], "")
        return val
    else:
        logging.error("Failed to access IPlatform service")

def set(args, prop, value):
    platformService = IPlatform.get_service(args)
    if platformService:
        platformService.setprop(prop, value)
        if prop.startswith("persist.linstack."):
            platformService.setprop("persist.waydroid." + prop[len("persist.linstack."):], value)
        elif prop.startswith("persist.waydroid."):
            platformService.setprop("persist.linstack." + prop[len("persist.waydroid."):], value)
        elif prop.startswith("linstack."):
            platformService.setprop("waydroid." + prop[len("linstack."):], value)
        elif prop.startswith("waydroid."):
            platformService.setprop("linstack." + prop[len("waydroid."):], value)
    else:
        logging.error("Failed to access IPlatform service")

def file_get(args, file, prop):
    with open(file) as build_prop:
        for line in build_prop:
            line = line.strip()
            if len(line) == 0 or line[0] == "#":
                continue
            k,v = line.partition("=")[::2]
            if k == prop:
                return v
    return ""
