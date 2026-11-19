#!/usr/bin/env bash
set -e

APP_HOME="$HOME/LinStack"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/LinStack"

echo "=== Installing LinStack into $APP_HOME ==="

# 1. بناء وتثبيت التطبيق والملفات داخل $HOME/LinStack
cmake -S "$SRC_DIR" -B "$SRC_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SRC_DIR/build" -j"$(nproc)"
cmake --install "$SRC_DIR/build" --prefix "$APP_HOME"

chmod +x "$APP_HOME/LinStack" "$APP_HOME/LinStack_core/linstack.py"

# 2. إنشاء أيقونة البرنامج في قائمة تطبيقات المستخدم (~/.local/share/applications)
mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/512x512/apps"
cp -f "$APP_HOME/LinStack_core/data/AppIcon.png" "$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"

cat > "$HOME/.local/share/applications/LinStack.desktop" <<EOF
[Desktop Entry]
Name=LinStack
Comment=Linux-native Android Emulator
Exec=$APP_HOME/LinStack
Icon=linstack
Terminal=false
Type=Application
Categories=Game;System;Emulator;
EOF

# 3. تسجيل خدمة الحاوية (systemd + D-Bus + Polkit) لتعمل من داخل $HOME/LinStack
echo "=== Configuring system container service (requires sudo) ==="
sudo bash -c "
    mkdir -p /var/lib/linstack/images /usr/share/dbus-1/system.d /usr/share/dbus-1/system-services /usr/share/polkit-1/actions

    # ربط صور الأندرويد من $APP_HOME/resources دون تكرار المساحة
    ln -sf '$APP_HOME/resources/system.img' /var/lib/linstack/images/system.img
    ln -sf '$APP_HOME/resources/vendor.img' /var/lib/linstack/images/vendor.img

    # رابط تنفيذي يشير إلى مجلد التطبيق في HOME
    ln -sf '$APP_HOME/LinStack_core/linstack.py' /usr/local/bin/linstack

    # نسخ وتعديل خدمة systemd لتشير إلى $APP_HOME
    sed 's|/usr/bin/linstack|$APP_HOME/LinStack_core/linstack.py|g' '$APP_HOME/LinStack_core/systemd/linstack-container.service' > /etc/systemd/system/linstack-container.service

    # نسخ ملفات D-Bus و Polkit وتوجيهها إلى $APP_HOME
    cp -f '$APP_HOME/LinStack_core/dbus/id.linstack.Container.conf' /usr/share/dbus-1/system.d/
    cp -f '$APP_HOME/LinStack_core/dbus/id.linstack.Container.policy' /usr/share/polkit-1/actions/
    sed 's|/usr/bin/linstack|$APP_HOME/LinStack_core/linstack.py|g' '$APP_HOME/LinStack_core/dbus/id.linstack.Container.service' > /usr/share/dbus-1/system-services/id.linstack.Container.service

    systemctl daemon-reload
    systemctl enable --now linstack-container.service
"

# 4. إنشاء سكريبت حذف نظيف داخل $HOME/LinStack
cat > "$APP_HOME/uninstall.sh" <<EOF
#!/usr/bin/env bash
echo "Removing LinStack completely..."
sudo systemctl disable --now linstack-container.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/linstack-container.service /usr/local/bin/linstack /usr/share/dbus-1/system.d/id.linstack.Container.conf /usr/share/dbus-1/system-services/id.linstack.Container.service /usr/share/polkit-1/actions/id.linstack.Container.policy
sudo rm -rf /var/lib/linstack
sudo systemctl daemon-reload
rm -f "$HOME/.local/share/applications/LinStack.desktop" "$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"
rm -rf "$APP_HOME"
echo "LinStack has been completely removed."
EOF
chmod +x "$APP_HOME/uninstall.sh"

echo "=== Done! Run $APP_HOME/LinStack or launch LinStack from your app menu. ==="
