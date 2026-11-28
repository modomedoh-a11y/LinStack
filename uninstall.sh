#!/usr/bin/env bash
echo "Uninstalling LinStack from /opt/linstack..."
sudo systemctl disable --now linstack-container.service 2>/dev/null || true
sudo rm -f /etc/systemd/system/linstack-container.service
sudo rm -f /usr/share/dbus-1/system.d/id.linstack.Container.conf
sudo rm -f /usr/share/dbus-1/system-services/id.linstack.Container.service
sudo rm -f /usr/share/polkit-1/actions/id.linstack.Container.policy
sudo rm -f /usr/local/bin/LinStack /usr/local/bin/linstack
sudo rm -f /usr/share/applications/LinStack.desktop /usr/share/applications/linstack.app.install.desktop /usr/share/icons/hicolor/512x512/apps/linstack.png
sudo rm -f /usr/share/applications/Linstack.desktop /usr/local/share/applications/Linstack.desktop
for dentry in Waydroid.desktop waydroid.desktop waydroid.install.desktop waydroid.app.install.desktop waydroid.market.desktop waydroid-helper.desktop; do
    sudo rm -f "/usr/local/share/applications/$dentry"
done
rm -f "$HOME/.local/share/applications/LinStack.desktop" "$HOME/.local/share/applications/Linstack.desktop" "$HOME/.local/share/icons/hicolor/512x512/apps/linstack.png"
if command -v update-desktop-database >/dev/null 2>&1; then
    sudo update-desktop-database /usr/local/share/applications 2>/dev/null || true
    sudo update-desktop-database /usr/share/applications 2>/dev/null || true
fi
sudo systemctl daemon-reload
sudo rm -rf /opt/linstack
echo "LinStack has been completely removed."
