#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_HOME="${APP_HOME:-$SCRIPT_DIR}"
SRC_DIR="$SCRIPT_DIR/LinStack"

echo "=== Installing LinStack ==="
echo "Repository Root: $SCRIPT_DIR"
echo "Application Target: $APP_HOME"

# 1. Build and install LinStack binary
cmake -S "$SRC_DIR" -B "$SRC_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SRC_DIR/build" -j"$(nproc)"
cmake --install "$SRC_DIR/build" --prefix "$APP_HOME"

# Locate core linstack.py and resources directory dynamically
if [ -f "$SCRIPT_DIR/LinStack/LinStack_core/linstack.py" ]; then
    CORE_SCRIPT="$SCRIPT_DIR/LinStack/LinStack_core/linstack.py"
    RES_DIR="$SCRIPT_DIR/LinStack/resources"
elif [ -f "$APP_HOME/LinStack_core/linstack.py" ]; then
    CORE_SCRIPT="$APP_HOME/LinStack_core/linstack.py"
    RES_DIR="$APP_HOME/resources"
elif [ -f "$APP_HOME/lib/linstack/LinStack_core/linstack.py" ]; then
    CORE_SCRIPT="$APP_HOME/lib/linstack/LinStack_core/linstack.py"
    RES_DIR="$APP_HOME/share/linstack/resources"
else
    CORE_SCRIPT="$SRC_DIR/LinStack_core/linstack.py"
    RES_DIR="$SRC_DIR/resources"
fi

EXEC_BIN="$APP_HOME/bin/LinStack"
if [ ! -f "$EXEC_BIN" ] && [ -f "$APP_HOME/LinStack" ]; then
    EXEC_BIN="$APP_HOME/LinStack"
fi

chmod +x "$EXEC_BIN" 2>/dev/null || true
chmod +x "$CORE_SCRIPT"
mkdir -p "$RES_DIR"

# 2. Desktop icon integration (~/.local/share/applications)
mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/512x512/apps"
ICON_SRC="$(dirname "$CORE_SCRIPT")/data/AppIcon.png"
if [ -f "$ICON_SRC" ]; then
    cp -f "$ICON_SRC" "$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"
fi

cat > "$HOME/.local/share/applications/LinStack.desktop" <<EOF
[Desktop Entry]
Name=LinStack
Comment=Linux-native Android Emulator
Exec=$EXEC_BIN
Icon=linstack
Terminal=false
Type=Application
Categories=Game;System;Emulator;
EOF

# 3. System container service registration (systemd + D-Bus + Polkit)
echo "=== Configuring system container service (requires sudo) ==="
sudo bash -c "
    mkdir -p /var/lib/linstack/images /usr/share/dbus-1/system.d /usr/share/dbus-1/system-services /usr/share/polkit-1/actions

    # Symlink images from resources if present to avoid duplicating disk space
    if [ -f '$RES_DIR/system.img' ]; then
        ln -sf '$RES_DIR/system.img' /var/lib/linstack/images/system.img
    fi
    if [ -f '$RES_DIR/vendor.img' ]; then
        ln -sf '$RES_DIR/vendor.img' /var/lib/linstack/images/vendor.img
    fi

    # CLI binary symlink
    ln -sf '$CORE_SCRIPT' /usr/local/bin/linstack

    # Service & DBus configurations pointing to dynamically resolved core script
    CORE_DIR=\"\$(dirname '$CORE_SCRIPT')\"
    sed \"s|/usr/bin/linstack|$CORE_SCRIPT|g\" \"\$CORE_DIR/systemd/linstack-container.service\" > /etc/systemd/system/linstack-container.service
    cp -f \"\$CORE_DIR/dbus/id.linstack.Container.conf\" /usr/share/dbus-1/system.d/
    cp -f \"\$CORE_DIR/dbus/id.linstack.Container.policy\" /usr/share/polkit-1/actions/
    sed \"s|/usr/bin/linstack|$CORE_SCRIPT|g\" \"\$CORE_DIR/dbus/id.linstack.Container.service\" > /usr/share/dbus-1/system-services/id.linstack.Container.service

    systemctl daemon-reload
    systemctl enable --now linstack-container.service
"

# 4. Clean uninstaller script
cat > "$APP_HOME/uninstall.sh" <<EOF
#!/usr/bin/env bash
echo "Removing LinStack container service and desktop integration..."
sudo systemctl disable --now linstack-container.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/linstack-container.service /usr/local/bin/linstack /usr/share/dbus-1/system.d/id.linstack.Container.conf /usr/share/dbus-1/system-services/id.linstack.Container.service /usr/share/polkit-1/actions/id.linstack.Container.policy
sudo systemctl daemon-reload
rm -f "$HOME/.local/share/applications/LinStack.desktop" "$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"
echo "LinStack system service and desktop entry have been removed."
EOF
chmod +x "$APP_HOME/uninstall.sh"

echo "=== LinStack installation complete! ==="
echo "Launch LinStack from your application menu or run: $EXEC_BIN"
