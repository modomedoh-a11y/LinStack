#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/LinStack"
OPT_DIR="/opt/linstack"

echo "=== Building LinStack ==="
cmake -S "$SRC_DIR" -B "$SRC_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SRC_DIR/build" -j"$(nproc)"

echo "=== Installing LinStack to $OPT_DIR (requires sudo) ==="
sudo bash -c "
    mkdir -p '$OPT_DIR/bin' '$OPT_DIR/resources' /var/lib/linstack/images \
        /usr/share/dbus-1/system.d /usr/share/dbus-1/system-services /usr/share/polkit-1/actions \
        /usr/share/applications /usr/share/icons/hicolor/512x512/apps

    # 1. Install binary
    cp -f '$SRC_DIR/build/LinStack' '$OPT_DIR/bin/LinStack'
    chmod 755 '$OPT_DIR/bin/LinStack'

    # 2. Install LinStack_core
    rm -rf '$OPT_DIR/LinStack_core'
    cp -a '$SRC_DIR/LinStack_core' '$OPT_DIR/LinStack_core'
    chmod +x '$OPT_DIR/LinStack_core/linstack.py'

    # 3. Install LinStack_Scripts
    if [ -d '$SCRIPT_DIR/LinStack_Scripts' ]; then
        rm -rf '$OPT_DIR/LinStack_Scripts'
        cp -a '$SCRIPT_DIR/LinStack_Scripts' '$OPT_DIR/LinStack_Scripts'
    elif [ -d '$SCRIPT_DIR/waydroid_script' ]; then
        rm -rf '$OPT_DIR/LinStack_Scripts'
        cp -a '$SCRIPT_DIR/waydroid_script' '$OPT_DIR/LinStack_Scripts'
    fi

    # 4. Preserve and sync resources
    if [ -f '$SRC_DIR/resources/system.img' ] && [ ! -f '$OPT_DIR/resources/system.img' ]; then
        echo 'Linking local system.img to $OPT_DIR/resources/system.img...'
        ln -sf '$SRC_DIR/resources/system.img' '$OPT_DIR/resources/system.img'
    fi
    if [ -f '$SRC_DIR/resources/vendor.img' ] && [ ! -f '$OPT_DIR/resources/vendor.img' ]; then
        echo 'Linking local vendor.img to $OPT_DIR/resources/vendor.img...'
        ln -sf '$SRC_DIR/resources/vendor.img' '$OPT_DIR/resources/vendor.img'
    fi

    # Link into /var/lib/linstack/images for container daemon
    if [ -f '$OPT_DIR/resources/system.img' ]; then
        ln -sf '$OPT_DIR/resources/system.img' /var/lib/linstack/images/system.img
    fi
    if [ -f '$OPT_DIR/resources/vendor.img' ]; then
        ln -sf '$OPT_DIR/resources/vendor.img' /var/lib/linstack/images/vendor.img
    fi

    # 5. Create system-wide executable symlinks
    ln -sf '$OPT_DIR/bin/LinStack' /usr/local/bin/LinStack
    ln -sf '$OPT_DIR/LinStack_core/linstack.py' /usr/local/bin/linstack

    # 6. Install App Icon and Desktop Launcher
    ICON_SRC='$OPT_DIR/LinStack_core/data/AppIcon.png'
    if [ -f \"\$ICON_SRC\" ]; then
        cp -f \"\$ICON_SRC\" /usr/share/icons/hicolor/512x512/apps/linstack.png
    fi

    cat > /usr/share/applications/LinStack.desktop <<EOF
[Desktop Entry]
Name=LinStack
Comment=Linux-native Android Emulator & Container
Exec=/opt/linstack/bin/LinStack
Icon=linstack
Terminal=false
Type=Application
Categories=Game;System;Emulator;Utility;
EOF

    # 7. Configure systemd & D-Bus system container service
    CORE_DIR='$OPT_DIR/LinStack_core'
    sed \"s|/usr/bin/linstack|\$CORE_DIR/linstack.py|g\" \"\$CORE_DIR/systemd/linstack-container.service\" > /etc/systemd/system/linstack-container.service
    cp -f \"\$CORE_DIR/dbus/id.linstack.Container.conf\" /usr/share/dbus-1/system.d/
    cp -f \"\$CORE_DIR/dbus/id.linstack.Container.policy\" /usr/share/polkit-1/actions/
    sed \"s|/usr/bin/linstack|\$CORE_DIR/linstack.py|g\" \"\$CORE_DIR/dbus/id.linstack.Container.service\" > /usr/share/dbus-1/system-services/id.linstack.Container.service

    systemctl daemon-reload
    systemctl enable --now linstack-container.service 2>/dev/null || true
"

# Also install launcher into current user local applications directory
mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/512x512/apps"
if [ -f "/opt/linstack/LinStack_core/data/AppIcon.png" ]; then
    cp -f "/opt/linstack/LinStack_core/data/AppIcon.png" "$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"
fi
cat > "$HOME/.local/share/applications/LinStack.desktop" <<EOF
[Desktop Entry]
Name=LinStack
Comment=Linux-native Android Emulator & Container
Exec=/opt/linstack/bin/LinStack
Icon=linstack
Terminal=false
Type=Application
Categories=Game;System;Emulator;Utility;
EOF

# 8. Generate clean uninstaller
cat > "$SCRIPT_DIR/uninstall.sh" <<EOF
#!/usr/bin/env bash
echo "Uninstalling LinStack from /opt/linstack..."
sudo systemctl disable --now linstack-container.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/linstack-container.service
sudo rm -f /usr/share/dbus-1/system.d/id.linstack.Container.conf
sudo rm -f /usr/share/dbus-1/system-services/id.linstack.Container.service
sudo rm -f /usr/share/polkit-1/actions/id.linstack.Container.policy
sudo rm -f /usr/local/bin/LinStack /usr/local/bin/linstack
sudo rm -f /usr/share/applications/LinStack.desktop /usr/share/icons/hicolor/512x512/apps/linstack.png
rm -f "\$HOME/.local/share/applications/LinStack.desktop" "\$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"
sudo systemctl daemon-reload
sudo rm -rf /opt/linstack
echo "LinStack has been completely removed."
EOF
chmod +x "$SCRIPT_DIR/uninstall.sh"
sudo cp -f "$SCRIPT_DIR/uninstall.sh" "$OPT_DIR/uninstall.sh" 2>/dev/null || true

echo "=== LinStack installation complete! ==="
echo "Launch LinStack from your application menu or run: LinStack"
