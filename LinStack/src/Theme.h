#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace Theme {

// Palette constants
inline const QColor ColorBgDark       = QColor(11, 15, 25);    // #0B0F19 Deep Cyber Dark
inline const QColor ColorBgDarkAlt    = QColor(13, 18, 29);    // #0D121D Deep Contrast
inline const QColor ColorBgPanel      = QColor(17, 24, 39);    // #111827 Panel / Titlebar / Sidebar
inline const QColor ColorBgSurface    = QColor(22, 31, 48);    // #161F30 Cards / Containers
inline const QColor ColorBgCardHover  = QColor(30, 41, 59);    // #1E293B Hover Card

inline const QColor ColorEmerald      = QColor(16, 185, 129);  // #10B981 Primary Emerald
inline const QColor ColorNeon         = QColor(0, 255, 102);   // #00FF66 Neon Phosphor Accent
inline const QColor ColorMint         = QColor(52, 211, 153);  // #34D399 Mint / Glow
inline const QColor ColorEmeraldDark  = QColor(5, 150, 105);   // #059669 Dark Emerald

inline const QColor ColorBorder       = QColor(31, 41, 55);    // #1F2937 Default Border
inline const QColor ColorBorderHover  = QColor(16, 185, 129, 140);
inline const QColor ColorBorderFocus  = QColor(0, 255, 102, 200);

inline const QColor ColorTextPrimary  = QColor(249, 250, 251); // #F9FAFB Pure Text
inline const QColor ColorTextMuted    = QColor(156, 163, 175); // #9CA3AF Muted Labels
inline const QColor ColorTextDim      = QColor(107, 114, 128); // #6B7280 Dim details

inline const QColor ColorDanger       = QColor(239, 68, 68);   // #EF4444 Close / Danger
inline const QColor ColorWarning      = QColor(245, 158, 11);  // #F59E0B Warning / Eco

enum class IconType {
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    WindowClose,
    Search,
    ClearText,
    Help,
    Fullscreen,
    VolumeUp,
    VolumeDown,
    Keymapper,
    EcoMode,
    InstallApk,
    Screenshot,
    Rotate,
    RamTrim,
    Settings,
    AndroidBack,
    AndroidHome,
    AndroidRecent,
    PlayStore,
    Gamepad,
    Cpu,
    Memory,
    Display,
    Audio,
    Phone,
    Shield,
    Check,
    Macro,
    FileManager,
    MultiInstance,
    AndroidBackTriangle,
    AndroidHomeCircle,
    AndroidRecentSquare,
    SwipeGesture,
    SmartCast
};

QIcon createIcon(IconType type, const QColor &color = ColorTextMuted, int size = 24);
QPixmap renderIconPixmap(IconType type, const QColor &color = ColorTextMuted, int size = 24);

QString cyberEmeraldStyleSheet();

} // namespace Theme

#endif // THEME_H
