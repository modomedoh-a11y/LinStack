import configparser
import os
import sys
# import dbus
from tools.helper import run
from tools.logger import Logger

# def DBusContainerService(object_path="/ContainerManager", intf="id.linstack.ContainerManager"):
#     return dbus.Interface(dbus.SystemBus().get_object("id.linstack.Container", object_path), intf)

# def DBusSessionService(object_path="/SessionManager", intf="id.linstack.SessionManager"):
#     return dbus.Interface(dbus.SessionBus().get_object("id.linstack.Session", object_path), intf)

# def use_dbus():
#     try:
#         DBusContainerService()
#     except:
#         return False
#     return True

def use_overlayfs():
    cfg = configparser.ConfigParser()
    cfg_file = os.environ.get("LINSTACK_CONFIG", "/var/lib/linstack/linstack.cfg")
    if not os.path.isfile(cfg_file):
        Logger.error("Cannot locate linstack config file, reinit wayland and try again!")
        sys.exit(1)
    cfg.read(cfg_file)
    if "linstack" not in cfg:
        Logger.error("Required entry in config was not found, Cannot continue!")
    if "mount_overlays" not in cfg["linstack"]:
        return False
    if cfg["linstack"]["mount_overlays"]=="True":
        return True
    return False


# def get_session():
#     return DBusContainerService().GetSession()

def stop():
    # if use_dbus():
    #     session = DBusContainerService().GetSession()
    #     if session:
    #         DBusContainerService().Stop(False)
    # else:
        run(["linstack", "container", "stop"])


def is_running():
        return "Session:\tRUNNING" in run(["linstack", "status"]).stdout.decode()

def upgrade():
    run(["linstack", "upgrade", "-o"], ignore=r"\[.*\] Stopping container\n\[.*\] Starting container")