# Copyright 2025 Alessandro Astone
# SPDX-License-Identifier: GPL-3.0-or-later
import tools.config
import dbus

import logging
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time

LINSTACK = sys.argv[0]
TARBALL = "linstack-bugreport.tar.xz"

def logcat(output_path):
    with open(output_path, "w+") as fd:
        return subprocess.Popen(["sudo", LINSTACK, "logcat"], stdout=fd, stderr=sys.stderr, stdin=None)

def dmesg(params, output_path):
    with open(output_path, "w+") as fd:
        return subprocess.Popen(["sudo", "dmesg", "-T"] + params, stdout=fd, stderr=fd, stdin=None)

def linstack_session(output_path):
    with open(output_path, "w+") as fd:
        return subprocess.Popen([LINSTACK, "session", "start"], stdout=fd, stderr=fd, stdin=None, start_new_session=True)

def clear_line():
    sys.stdout.write("\33[2K\r")

def sleep_progress(seconds):
    bar_length = 50
    time_increment = 0.1
    spinner_chars = ['|', '/', '-', '\\']

    iteration = 0
    time_slept = 0

    try:
        while time_slept < seconds:
            spinner_char = spinner_chars[iteration % len(spinner_chars)]
            filled_length = int(bar_length * time_slept // seconds)
            sys.stdout.write(f"[{'=' * filled_length}{' ' * (bar_length - filled_length)}] {spinner_char}\r")
            sys.stdout.flush()

            time.sleep(time_increment)
            time_slept += time_increment
            iteration += 1
        clear_line()
    except KeyboardInterrupt:
        clear_line()
        print("Interrupted.")


def bugreport(args):
    tmp = tempfile.mkdtemp()

    print("""\
The following information will be collected:
  - System kernel logs (kmsg)
  - Android system and user logs (logcat)
  - Linstack container manager logs (/var/lib/linstack/linstack.log)
  - Linstack configuration files (/var/lib/linstack/*)

The information will be stored on your machine.

Please authenticate as administrator in order to read system logs.
""")

    try:
        subprocess.run(["sudo", "-v"], check=True)
        print()
    except FileNotFoundError:
        print("The 'sudo' command is not available. Cannot read system logs.")
        return
    except subprocess.CalledProcessError:
        print("Authentication failed or was cancelled.")
        return

    procs = []
    logfiles = []
    def logfile(name):
        file = os.path.join(tmp, name)
        logfiles.append(file)
        return file

    session = None
    try:
        session = tools.helpers.ipc.DBusContainerService().GetSession()
    except dbus.DBusException:
        pass

    if not session:
        print("Linstack session not found. Trying to start one...")
        procs.append(linstack_session(logfile("session.txt")))
        sleep_progress(10)
        try:
            session = tools.helpers.ipc.DBusContainerService().GetSession()
        except dbus.DBusException:
            pass

    if session:
        print("\n\
\033[1mPlease try to reproduce the problem now.\033[0m\n\
Linstack will collect logs for up to 5 minutes. You may interrupt this operation earlier.\n\
")
        procs.append(logcat(logfile("logcat.txt")))
        procs.append(dmesg(["-w"], logfile("dmesg.txt")))
        sleep_progress(5 * 60)
    else:
        print("Session did not start\n")
        procs.append(dmesg([], logfile("dmesg.txt")))

    for p in procs:
        p.terminate()
    for p in procs:
        p.wait()

    print("Creating archive...")

    try:
        with tarfile.open(TARBALL, "w:xz", preset=9) as tar:
            files = [
                "/var/lib/linstack/linstack.log",
                "/var/lib/linstack/linstack.cfg",
                "/var/lib/linstack/linstack_base.prop",
                "/var/lib/linstack/linstack.prop",
                "/var/lib/linstack/lxc",
            ] + logfiles

            for f in files:
                try:
                    tar.add(f, arcname=os.path.basename(f), recursive=True)
                except Exception as e:
                    logging.debug(f"Failed to archive {f}: {repr(e)}")

    except Exception as e:
        raise e
    finally:
        shutil.rmtree(tmp)

    print(f"Created \033[1m{TARBALL}\033[0m")
