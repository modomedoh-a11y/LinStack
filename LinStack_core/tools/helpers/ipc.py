# Copyright 2022 Alessandro Astone
# SPDX-License-Identifier: GPL-3.0-or-later

# Currently implemented as FIFO
import os
import dbus

def DBusContainerService(object_path="/ContainerManager", intf="id.linstack.ContainerManager"):
    return dbus.Interface(dbus.SystemBus().get_object("id.linstack.Container", object_path), intf)

def DBusSessionService(object_path="/SessionManager", intf="id.linstack.SessionManager"):
    return dbus.Interface(dbus.SessionBus().get_object("id.linstack.Session", object_path), intf)
