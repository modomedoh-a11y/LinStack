<img align="left" src="LinStack/LinStack_core/data/AppIcon.png" width="64">
# LinStack

LinStack is a high-performance, Linux-native Android emulation and containerized gaming environment. It integrates an embedded **wlroots** Wayland compositor, zero-copy DMA-BUF OpenGL ES hardware rendering, an ultra-low latency `uinput` touch and keymapping bridge, and a modern Qt desktop UI.

---

## Features

- **Zero-Copy Hardware Rendering:** Direct Wayland surface composition using DMA-BUF and GLES2 textures for minimal latency and maximum frame rates.
- **Embedded wlroots Compositor:** Dedicated embedded compositor handling Android display pipelines independently from the host session.
- **Smart Container Initialization:**
  - Automated local fallback: Reuses local `system.img` and `vendor.img` in `LinStack/resources/` if already present.
  - Automated download & extract: Streams official Waydroid LineageOS Android 11 images directly from SourceForge with real-time progress and extraction.
  - Custom image support: Allows specifying custom system and vendor image URLs or OTA channels.
- **Integrated Keymapper & Touch Emulation:** Low-latency virtual touch emulation via Linux `uinput` with customizable on-screen control bindings.
- **Self-Healing Diagnostics:** Proactive diagnostics engine for Binder nodes, permissions, and container daemon status.

---

## System Requirements

- **Operating System:** Linux (x86_64 or aarch64)
- **Kernel Drivers:**
  - Android Binder IPC driver (`binderfs` or `binder_linux` module)
  - Anonymous Shared Memory (`ashmem_linux` or memfd fallback)
- **Display Server:** Wayland session (e.g., GNOME Wayland, KDE Plasma Wayland, Sway, Hyprland)
- **Container Infrastructure:** LXC, `dnsmasq`, `iptables` / `nftables`

### Required Packages

#### Debian / Ubuntu / Pop!_OS:
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config \
    qt6-base-dev libgl1-mesa-dev libwayland-dev wayland-protocols \
    libwlroots-dev libdrm-dev libepoxy-dev libxkbcommon-dev libinput-dev \
    lxc dnsmasq python3 python3-requests python3-dbus python3-gi \
    gir1.2-gtk-3.0 curl
```

#### Fedora:
```bash
sudo dnf install -y gcc-c++ cmake pkgconf-pkg-config \
    qt6-qtbase-devel mesa-libGL-devel wayland-devel wayland-protocols-devel \
    wlroots-devel libdrm-devel libepoxy-devel libxkbcommon-devel libinput-devel \
    lxc dnsmasq python3 python3-requests python3-dbus python3-gobject \
    gtk3 curl
```

#### Arch Linux:
```bash
sudo pacman -S --needed base-devel cmake pkgconf \
    qt6-base mesa wayland wayland-protocols wlroots \
    libdrm libepoxy libxkbcommon libinput lxc dnsmasq \
    python python-requests python-dbus python-gobject gtk3 curl
```

---

## Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/modomedoh-a11y/LinStack.git
   cd LinStack
   ```

2. **Run the installation script:**
   ```bash
   ./install.sh
   ```
   This will:
   - Compile the LinStack C++/Qt client and embedded wlroots engine in Release mode.
   - Install the application cleanly into `/opt/linstack` (`/opt/linstack/bin/LinStack`, `LinStack_core`, `LinStack_Scripts`, and `resources/`).
   - Create system-wide symlinks `/usr/local/bin/LinStack` and `/usr/local/bin/linstack`.
   - Install the main desktop launcher and icon into `/usr/share/applications/LinStack.desktop` and `~/.local/share/applications/LinStack.desktop`.
   - Configure D-Bus permissions, Polkit actions, and enable the systemd service (`linstack-container.service`).
   - Generate an automated `uninstall.sh` script.

---

## Container Initialization (`init`)

LinStack requires Android system and vendor partition images to boot the container. The initialization process is smart and automated:

### Option 1: Native Qt First-Run Wizard (Recommended)
1. Launch **LinStack** from your application menu or run:
   ```bash
   LinStack
   ```
2. If the container is not yet initialized, LinStack automatically displays the native **Qt Initialization Dialog** styled in the Cyber-Emerald theme.
3. The dialog comes pre-filled with official Waydroid LineageOS 18.1 (Android 11) image URLs:
   - **System URL:** `https://sourceforge.net/projects/waydroid/files/images/system/lineage/waydroid_x86_64/lineage-18.1-20250628-VANILLA-waydroid_x86_64-system.zip/download`
   - **Vendor URL:** `https://sourceforge.net/projects/waydroid/files/images/vendor/waydroid_x86_64/lineage-18.1-20250628-MAINLINE-waydroid_x86_64-vendor.zip/download`
   - **Helper Notice:**
     > *"These are links for Waydroid Lineage OS Android 11 you can put your android images links or you can copy them to LinStack/LinStack/resources as system.img and vendor.img"*
4. Click **Init**:
   - **Smart Local Fallback:** LinStack first checks if `system.img` and `vendor.img` already exist in `/opt/linstack/resources/` or the relative `LinStack/resources/` directory. If both files exist, downloading is bypassed and container initialization starts instantly.
   - **Download & Extract:** If files are missing, LinStack downloads the `.zip` archives directly from SourceForge (with redirect following and progress reporting), extracts `system.img` and `vendor.img`, removes temporary archives, performs container setup via `pkexec`, and transitions directly to the Android boot screen.

### Option 2: Terminal / Headless Initialization (CLI)
You can initialize directly from the terminal using elevated permissions:
```bash
sudo linstack init
```
Or specify custom image URLs:
```bash
sudo linstack init -s <SYSTEM_IMAGE_OR_ZIP_URL> -v <VENDOR_IMAGE_OR_ZIP_URL>
```

### Option 3: Offline / Manual Setup
If you have already downloaded `system.img` and `vendor.img`:
1. Copy both files into `/opt/linstack/resources/` (or `LinStack/resources/` prior to running `./install.sh`):
   ```bash
   sudo cp /path/to/system.img /opt/linstack/resources/system.img
   sudo cp /path/to/vendor.img /opt/linstack/resources/vendor.img
   ```
2. Launch `LinStack` or run `sudo linstack init`. LinStack will automatically detect the local `.img` files and initialize without downloading.

---

## Extras & Script Integration (`LinStack_Scripts`)

LinStack includes bundled scripts in `/opt/linstack/LinStack_Scripts` for installing Google Play Services (GApps/MicroG), Magisk root, Libndk / Houdini ARM translation bridges, and device spoofing:

```bash
cd /opt/linstack/LinStack_Scripts
# Or in repository root: cd LinStack_Scripts
python3 -m venv venv
venv/bin/pip install -r requirements.txt

# Interactive terminal manager:
sudo venv/bin/python3 main.py

# Or install specific components directly:
sudo venv/bin/python3 main.py install {libndk,microg,magisk}
```

---

## Running LinStack

After initialization, launch LinStack at any time:
- Desktop Application Menu: Search for **LinStack**.
- Command Line:
  ```bash
  LinStack
  ```

### CLI Service Controls
```bash
# Check container status
linstack status

# Start or stop the container session
linstack session start
linstack session stop

# Show real-time container logcat
linstack logcat

# Manage systemd service
sudo systemctl restart linstack-container.service
```

---

## Troubleshooting Guide

### 1. Missing Kernel Modules (`binder` & `ashmem`)
LinStack requires the Android Binder IPC kernel module. On modern Linux kernels (5.x / 6.x), Binder is typically built into the kernel or provided via `binderfs`:

- **Check if `binderfs` is already mounted:**
  ```bash
  ls -l /dev/binderfs /dev/binder
  ```
- **If `/dev/binder` or `/dev/binderfs` is missing:**
  Ensure the module is loaded:
  ```bash
  sudo modprobe binder_linux
  ```
  Mount `binderfs` if needed:
  ```bash
  sudo mkdir -p /dev/binderfs
  sudo mount -t binder binder /dev/binderfs
  sudo ln -sf /dev/binderfs/binder /dev/binder
  sudo ln -sf /dev/binderfs/hwbinder /dev/hwbinder
  sudo ln -sf /dev/binderfs/vndbinder /dev/vndbinder
  ```
- **Persistent configuration:** Add to `/etc/modules-load.d/linstack.conf`:
  ```
  binder_linux
  ```
  And add to `/etc/fstab`:
  ```
  binder   /dev/binderfs   binder   nofail   0   0
  ```

### 2. Wayland Session & GPU Rendering
LinStack requires a Wayland desktop environment (GNOME Wayland, KDE Plasma Wayland, Sway, Hyprland, etc.).

- **Verify Wayland session:**
  ```bash
  echo $WAYLAND_DISPLAY
  ```
  If this returns empty, log out and select a "Wayland" session from your display manager (GDM / SDDM).
- **Intel & AMD GPUs:**
  Out of the box support via standard Mesa DRI/GBM drivers (`mesa-dri-drivers`).
- **NVIDIA Proprietary Drivers:**
  NVIDIA proprietary drivers (525+ or 555+) require explicit EGL/GBM configuration:
  ```bash
  export GBM_BACKEND=nvidia-drm
  export __GLX_VENDOR_LIBRARY_NAME=nvidia
  export WLR_NO_HARDWARE_CURSORS=1
  export WAYLAND_DISPLAY=wayland-0
  ```
  If Android fails to render with hardware acceleration on NVIDIA, fall back to software rendering in `/var/lib/linstack/linstack.cfg`:
  ```ini
  [linstack]
  ro.hardware.gralloc = default
  ro.hardware.egl = swiftshader
  ```

### 3. Container Network & Firewall Conflicts
LinStack creates a virtual bridge (`linstack0` / `192.168.240.1`) and uses `dnsmasq` for DHCP and DNS resolution.

- **Missing Internet in Android container:**
  Verify the LinStack bridge interface:
  ```bash
  ip addr show linstack0
  ```
- **Firewall blocking packet forwarding:**
  - **`firewalld` users (Fedora/RHEL):**
    ```bash
    sudo firewall-cmd --zone=trusted --add-interface=linstack0 --permanent
    sudo firewall-cmd --reload
    ```
  - **`ufw` users (Ubuntu/Debian):**
    Set `DEFAULT_FORWARD_POLICY="ACCEPT"` in `/etc/default/ufw` and reload:
    ```bash
    sudo ufw reload
    ```
  - **`iptables` / `nftables`:**
    ```bash
    sudo iptables -A FORWARD -i linstack0 -j ACCEPT
    sudo iptables -A FORWARD -o linstack0 -j ACCEPT
    sudo iptables -t nat -A POSTROUTING -s 192.168.240.0/24 ! -d 192.168.240.0/24 -j MASQUERADE
    ```

### 4. Polkit & `pkexec` Permission Failures
During container initialization, LinStack invokes `pkexec` to run elevated setup actions without requiring terminal root access:

- If you receive `pkexec: must be setuid root`:
  ```bash
  sudo chmod 4755 /usr/bin/pkexec
  ```
- If Polkit authentication dialog does not pop up in your desktop environment, ensure your desktop session has an active Polkit agent running:
  - **GNOME:** Built-in.
  - **KDE Plasma:** `/usr/libexec/polkit-kde-authentication-agent-1`
  - **Hyprland / Sway:** Install and run `polkit-gnome` or `lxqt-policykit`.
- Alternatively, run initialization directly in terminal with `sudo`:
  ```bash
  sudo linstack init
  ```
# Some features are not working till now and it is still under development but usable.
---

## Uninstallation

To cleanly remove the LinStack system service, desktop shortcuts, and binary symlinks, run:
```bash
./uninstall.sh
```

---

## License

This project is licensed under the terms of the GNU General Public License v3.0 (GPL-3.0). See [LICENSE](LICENSE) for details.
