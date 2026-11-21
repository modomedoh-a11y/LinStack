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
   - Build the LinStack C++ client and embedded compositor libraries with CMake.
   - Install the executable into `./bin/LinStack`.
   - Install user desktop entries and application icons (`~/.local/share/applications/LinStack.desktop`).
   - Register and start the background system container service (`linstack-container.service`).

---

## Container Initialization (`init`)

LinStack requires Android system and vendor partition images to boot the container. You have three flexible options to initialize:

### Option 1: Graphical Initialization (GUI)
1. Launch **LinStack** from your application menu or run:
   ```bash
   linstack init --client
   ```
2. The initialization window opens pre-filled with official LineageOS Android 11 images:
   - **System URL:** `https://sourceforge.net/projects/waydroid/files/images/system/lineage/waydroid_x86_64/lineage-18.1-20250628-VANILLA-waydroid_x86_64-system.zip/download`
   - **Vendor URL:** `https://sourceforge.net/projects/waydroid/files/images/vendor/waydroid_x86_64/lineage-18.1-20250628-MAINLINE-waydroid_x86_64-vendor.zip/download`
3. Click the **Init** button:
   - **Smart Check:** LinStack checks if `system.img` and `vendor.img` already exist in `LinStack/resources/`.
   - If present, downloading is skipped and the container initializes immediately.
   - If missing, LinStack downloads the archives, extracts `system.img` and `vendor.img` into `LinStack/resources/`, cleans up temporary zip files, and completes initialization.

### Option 2: Terminal Initialization (CLI)
You can initialize directly from your terminal:
```bash
sudo linstack init
```
Or specify custom image URLs:
```bash
sudo linstack init -s <SYSTEM_IMAGE_OR_ZIP_URL> -v <VENDOR_IMAGE_OR_ZIP_URL>
```

### Option 3: Offline / Manual Setup
If you already have `system.img` and `vendor.img` downloaded:
1. Copy both files directly into the repository resources directory:
   ```bash
   cp /path/to/system.img LinStack/resources/system.img
   cp /path/to/vendor.img LinStack/resources/vendor.img
   ```
2. Run `sudo linstack init` or click **Init** in the GUI. LinStack will detect the local images and initialize instantly without downloading.

---

## Running LinStack

After initialization, you can launch LinStack at any time:

- From your desktop application menu: Search for **LinStack**.
- From terminal:
  ```bash
  ./bin/LinStack
  ```

### CLI Service Controls
```bash
# Check container status
linstack status

# Start or stop the container session
linstack session start
linstack session stop

# Restart container service
sudo systemctl restart linstack-container.service
```

---

## Uninstallation

To cleanly remove the LinStack system service, desktop shortcuts, and binary symlinks, run:
```bash
./uninstall.sh
```

---

## License

This project is licensed under the terms of the GNU General Public License v3.0 (GPL-3.0). See [LICENSE](LICENSE) for details.
