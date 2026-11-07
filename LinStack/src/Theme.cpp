#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFontDatabase>

namespace Theme {

QPixmap renderIconPixmap(IconType type, const QColor &color, int size)
{
    // Render at 2x scale for crisp HiDPI
    const int scale = 2;
    QPixmap pixmap(size * scale, size * scale);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.scale(scale, scale);

    QPen pen(color);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setWidthF(1.75);
    p.setPen(pen);

    const qreal w = size;
    const qreal h = size;

    switch (type) {
    case IconType::WindowMinimize: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(w * 0.25, h * 0.55), QPointF(w * 0.75, h * 0.55));
        break;
    }
    case IconType::WindowMaximize: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QRectF rect(w * 0.26, h * 0.26, w * 0.48, h * 0.48);
        p.drawRoundedRect(rect, 1.5, 1.5);
        break;
    }
    case IconType::WindowRestore: {
        pen.setWidthF(1.6);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Back square
        p.drawRoundedRect(QRectF(w * 0.35, h * 0.22, w * 0.42, h * 0.42), 1.5, 1.5);
        // Front square with dark fill
        p.setBrush(ColorBgPanel);
        p.drawRoundedRect(QRectF(w * 0.22, h * 0.35, w * 0.42, h * 0.42), 1.5, 1.5);
        break;
    }
    case IconType::WindowClose: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        qreal cPad = w * 0.30;
        p.drawLine(QPointF(cPad, cPad), QPointF(w - cPad, h - cPad));
        p.drawLine(QPointF(w - cPad, cPad), QPointF(cPad, h - cPad));
        break;
    }
    case IconType::Search: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        qreal r = w * 0.22;
        QPointF center(w * 0.42, h * 0.42);
        p.drawEllipse(center, r, r);
        p.drawLine(QPointF(center.x() + r * 0.707, center.y() + r * 0.707),
                   QPointF(w * 0.78, h * 0.78));
        break;
    }
    case IconType::ClearText: {
        pen.setWidthF(1.6);
        p.setPen(pen);
        p.setBrush(ColorBgSurface);
        p.drawEllipse(QPointF(w * 0.5, h * 0.5), w * 0.35, h * 0.35);
        qreal c = w * 0.16;
        p.drawLine(QPointF(w * 0.5 - c, h * 0.5 - c), QPointF(w * 0.5 + c, h * 0.5 + c));
        p.drawLine(QPointF(w * 0.5 + c, h * 0.5 - c), QPointF(w * 0.5 - c, h * 0.5 + c));
        break;
    }
    case IconType::Help: {
        pen.setWidthF(1.6);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(w * 0.5, h * 0.5), w * 0.36, h * 0.36);
        QPainterPath path;
        path.moveTo(w * 0.40, h * 0.38);
        path.cubicTo(w * 0.40, h * 0.28, w * 0.60, h * 0.28, w * 0.60, h * 0.40);
        path.cubicTo(w * 0.60, h * 0.48, w * 0.50, h * 0.50, w * 0.50, h * 0.58);
        p.drawPath(path);
        p.setBrush(color);
        p.drawEllipse(QPointF(w * 0.5, h * 0.68), 1.2, 1.2);
        break;
    }
    case IconType::Fullscreen: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        qreal s = w * 0.22;
        qreal l = w * 0.18;
        // Top-left
        p.drawLine(QPointF(s, s + l), QPointF(s, s));
        p.drawLine(QPointF(s, s), QPointF(s + l, s));
        // Top-right
        p.drawLine(QPointF(w - s - l, s), QPointF(w - s, s));
        p.drawLine(QPointF(w - s, s), QPointF(w - s, s + l));
        // Bottom-left
        p.drawLine(QPointF(s, h - s - l), QPointF(s, h - s));
        p.drawLine(QPointF(s, h - s), QPointF(s + l, h - s));
        // Bottom-right
        p.drawLine(QPointF(w - s - l, h - s), QPointF(w - s, h - s));
        p.drawLine(QPointF(w - s, h - s), QPointF(w - s, h - s - l));
        break;
    }
    case IconType::VolumeUp: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(color);
        QPainterPath cone;
        cone.moveTo(w * 0.22, h * 0.42);
        cone.lineTo(w * 0.34, h * 0.42);
        cone.lineTo(w * 0.50, h * 0.26);
        cone.lineTo(w * 0.50, h * 0.74);
        cone.lineTo(w * 0.34, h * 0.58);
        cone.lineTo(w * 0.22, h * 0.58);
        cone.closeSubpath();
        p.drawPath(cone);

        p.setBrush(Qt::NoBrush);
        QRectF arc1(w * 0.36, h * 0.32, w * 0.32, h * 0.36);
        p.drawArc(arc1, -55 * 16, 110 * 16);
        QRectF arc2(w * 0.28, h * 0.20, w * 0.54, h * 0.60);
        p.drawArc(arc2, -55 * 16, 110 * 16);
        break;
    }
    case IconType::VolumeDown: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(color);
        QPainterPath cone;
        cone.moveTo(w * 0.22, h * 0.42);
        cone.lineTo(w * 0.34, h * 0.42);
        cone.lineTo(w * 0.50, h * 0.26);
        cone.lineTo(w * 0.50, h * 0.74);
        cone.lineTo(w * 0.34, h * 0.58);
        cone.lineTo(w * 0.22, h * 0.58);
        cone.closeSubpath();
        p.drawPath(cone);

        p.setBrush(Qt::NoBrush);
        QRectF arc1(w * 0.36, h * 0.32, w * 0.32, h * 0.36);
        p.drawArc(arc1, -55 * 16, 110 * 16);
        break;
    }
    case IconType::Keymapper: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(w * 0.18, h * 0.25, w * 0.64, h * 0.50), 3.0, 3.0);
        // Key grid
        p.drawLine(QPointF(w * 0.30, h * 0.38), QPointF(w * 0.36, h * 0.38));
        p.drawLine(QPointF(w * 0.44, h * 0.38), QPointF(w * 0.50, h * 0.38));
        p.drawLine(QPointF(w * 0.58, h * 0.38), QPointF(w * 0.70, h * 0.38));

        p.drawLine(QPointF(w * 0.26, h * 0.50), QPointF(w * 0.34, h * 0.50));
        p.drawLine(QPointF(w * 0.42, h * 0.50), QPointF(w * 0.56, h * 0.50));
        p.drawLine(QPointF(w * 0.64, h * 0.50), QPointF(w * 0.72, h * 0.50));

        p.drawLine(QPointF(w * 0.32, h * 0.62), QPointF(w * 0.66, h * 0.62));
        break;
    }
    case IconType::EcoMode: {
        pen.setWidthF(1.6);
        p.setPen(pen);
        p.setBrush(color);
        QPainterPath bolt;
        bolt.moveTo(w * 0.56, h * 0.18);
        bolt.lineTo(w * 0.32, h * 0.52);
        bolt.lineTo(w * 0.48, h * 0.52);
        bolt.lineTo(w * 0.42, h * 0.82);
        bolt.lineTo(w * 0.68, h * 0.46);
        bolt.lineTo(w * 0.52, h * 0.46);
        bolt.closeSubpath();
        p.drawPath(bolt);
        break;
    }
    case IconType::InstallApk: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Package box isometric
        QPainterPath box;
        box.moveTo(w * 0.50, h * 0.20);
        box.lineTo(w * 0.80, h * 0.36);
        box.lineTo(w * 0.80, h * 0.68);
        box.lineTo(w * 0.50, h * 0.84);
        box.lineTo(w * 0.20, h * 0.68);
        box.lineTo(w * 0.20, h * 0.36);
        box.closeSubpath();
        p.drawPath(box);
        p.drawLine(QPointF(w * 0.50, h * 0.20), QPointF(w * 0.50, h * 0.52));
        p.drawLine(QPointF(w * 0.20, h * 0.36), QPointF(w * 0.50, h * 0.52));
        p.drawLine(QPointF(w * 0.80, h * 0.36), QPointF(w * 0.50, h * 0.52));
        p.drawLine(QPointF(w * 0.50, h * 0.52), QPointF(w * 0.50, h * 0.84));
        break;
    }
    case IconType::Screenshot: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Camera body
        QPainterPath cam;
        cam.moveTo(w * 0.20, h * 0.36);
        cam.lineTo(w * 0.34, h * 0.36);
        cam.lineTo(w * 0.40, h * 0.26);
        cam.lineTo(w * 0.60, h * 0.26);
        cam.lineTo(w * 0.66, h * 0.36);
        cam.lineTo(w * 0.80, h * 0.36);
        cam.lineTo(w * 0.80, h * 0.74);
        cam.lineTo(w * 0.20, h * 0.74);
        cam.closeSubpath();
        p.drawPath(cam);
        p.drawEllipse(QPointF(w * 0.50, h * 0.54), w * 0.14, w * 0.14);
        break;
    }
    case IconType::Rotate: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QRectF arcR(w * 0.22, h * 0.22, w * 0.56, h * 0.56);
        p.drawArc(arcR, 30 * 16, 150 * 16);
        p.drawArc(arcR, 210 * 16, 150 * 16);
        // Arrow heads
        p.setBrush(color);
        QPainterPath ah1;
        ah1.moveTo(w * 0.74, h * 0.34);
        ah1.lineTo(w * 0.86, h * 0.38);
        ah1.lineTo(w * 0.78, h * 0.48);
        ah1.closeSubpath();
        p.drawPath(ah1);

        QPainterPath ah2;
        ah2.moveTo(w * 0.26, h * 0.66);
        ah2.lineTo(w * 0.14, h * 0.62);
        ah2.lineTo(w * 0.22, h * 0.52);
        ah2.closeSubpath();
        p.drawPath(ah2);
        break;
    }
    case IconType::RamTrim: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Speedometer arc
        QRectF gauge(w * 0.20, h * 0.22, w * 0.60, h * 0.60);
        p.drawArc(gauge, -20 * 16, 220 * 16);
        // Needle
        pen.setColor(ColorNeon);
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(w * 0.50, h * 0.55), QPointF(w * 0.68, h * 0.36));
        p.setBrush(ColorNeon);
        p.drawEllipse(QPointF(w * 0.50, h * 0.55), 2.2, 2.2);
        break;
    }
    case IconType::Settings: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // 8-tooth gear
        QPainterPath gear;
        const int teeth = 8;
        const qreal rInner = w * 0.24;
        const qreal rOuter = w * 0.36;
        const QPointF c(w * 0.5, h * 0.5);

        for (int i = 0; i < teeth; ++i) {
            qreal a1 = (i * 360.0 / teeth) * M_PI / 180.0;
            qreal a2 = ((i * 360.0 / teeth) + 14.0) * M_PI / 180.0;
            qreal a3 = ((i * 360.0 / teeth) + 24.0) * M_PI / 180.0;
            qreal a4 = (((i + 1) * 360.0 / teeth)) * M_PI / 180.0;

            QPointF p1(c.x() + rInner * std::cos(a1), c.y() + rInner * std::sin(a1));
            QPointF p2(c.x() + rOuter * std::cos(a2), c.y() + rOuter * std::sin(a2));
            QPointF p3(c.x() + rOuter * std::cos(a3), c.y() + rOuter * std::sin(a3));
            QPointF p4(c.x() + rInner * std::cos(a4), c.y() + rInner * std::sin(a4));

            if (i == 0) gear.moveTo(p1);
            else gear.lineTo(p1);
            gear.lineTo(p2);
            gear.lineTo(p3);
            gear.lineTo(p4);
        }
        gear.closeSubpath();
        p.drawPath(gear);
        p.drawEllipse(c, w * 0.12, w * 0.12);
        break;
    }
    case IconType::AndroidBack: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.setBrush(color);
        QPainterPath triangle;
        triangle.moveTo(w * 0.34, h * 0.50);
        triangle.lineTo(w * 0.64, h * 0.28);
        triangle.lineTo(w * 0.64, h * 0.72);
        triangle.closeSubpath();
        p.drawPath(triangle);
        break;
    }
    case IconType::AndroidHome: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.setBrush(color);
        QPainterPath house;
        house.moveTo(w * 0.50, h * 0.26);
        house.lineTo(w * 0.74, h * 0.46);
        house.lineTo(w * 0.66, h * 0.46);
        house.lineTo(w * 0.66, h * 0.72);
        house.lineTo(w * 0.34, h * 0.72);
        house.lineTo(w * 0.34, h * 0.46);
        house.lineTo(w * 0.26, h * 0.46);
        house.closeSubpath();
        p.drawPath(house);
        break;
    }
    case IconType::AndroidRecent: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(w * 0.32, h * 0.32, w * 0.36, h * 0.36), 2.5, 2.5);
        break;
    }
    case IconType::PlayStore: {
        p.setPen(Qt::NoPen);
        // Play store stylized cyber triangle
        QPainterPath f1;
        f1.moveTo(w * 0.24, h * 0.20);
        f1.lineTo(w * 0.76, h * 0.50);
        f1.lineTo(w * 0.54, h * 0.62);
        f1.lineTo(w * 0.24, h * 0.45);
        f1.closeSubpath();
        p.setBrush(ColorEmerald);
        p.drawPath(f1);

        QPainterPath f2;
        f2.moveTo(w * 0.24, h * 0.80);
        f2.lineTo(w * 0.76, h * 0.50);
        f2.lineTo(w * 0.54, h * 0.38);
        f2.lineTo(w * 0.24, h * 0.55);
        f2.closeSubpath();
        p.setBrush(ColorNeon);
        p.drawPath(f2);
        break;
    }
    case IconType::Gamepad: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QRectF body(w * 0.20, h * 0.30, w * 0.60, h * 0.44);
        p.drawRoundedRect(body, 6.0, 6.0);
        // D-pad left
        p.drawLine(QPointF(w * 0.35, h * 0.46), QPointF(w * 0.35, h * 0.58));
        p.drawLine(QPointF(w * 0.29, h * 0.52), QPointF(w * 0.41, h * 0.52));
        // Action buttons right
        p.setBrush(color);
        p.drawEllipse(QPointF(w * 0.62, h * 0.48), 1.8, 1.8);
        p.drawEllipse(QPointF(w * 0.70, h * 0.55), 1.8, 1.8);
        break;
    }
    case IconType::Cpu: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(ColorBgSurface);
        p.drawRoundedRect(QRectF(w * 0.30, h * 0.30, w * 0.40, h * 0.40), 2.0, 2.0);
        // Inner die
        p.setBrush(ColorEmerald);
        p.drawRect(QRectF(w * 0.40, h * 0.40, w * 0.20, h * 0.20));
        // Pins
        p.drawLine(QPointF(w * 0.38, h * 0.22), QPointF(w * 0.38, h * 0.30));
        p.drawLine(QPointF(w * 0.50, h * 0.22), QPointF(w * 0.50, h * 0.30));
        p.drawLine(QPointF(w * 0.62, h * 0.22), QPointF(w * 0.62, h * 0.30));
        p.drawLine(QPointF(w * 0.38, h * 0.70), QPointF(w * 0.38, h * 0.78));
        p.drawLine(QPointF(w * 0.50, h * 0.70), QPointF(w * 0.50, h * 0.78));
        p.drawLine(QPointF(w * 0.62, h * 0.70), QPointF(w * 0.62, h * 0.78));
        break;
    }
    case IconType::Memory: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(w * 0.22, h * 0.32, w * 0.56, h * 0.36), 2.0, 2.0);
        // Chips
        p.setBrush(color);
        p.drawRect(QRectF(w * 0.30, h * 0.40, w * 0.10, h * 0.16));
        p.drawRect(QRectF(w * 0.45, h * 0.40, w * 0.10, h * 0.16));
        p.drawRect(QRectF(w * 0.60, h * 0.40, w * 0.10, h * 0.16));
        break;
    }
    case IconType::Display: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(w * 0.20, h * 0.25, w * 0.60, h * 0.44), 2.5, 2.5);
        p.drawLine(QPointF(w * 0.50, h * 0.69), QPointF(w * 0.50, h * 0.78));
        p.drawLine(QPointF(w * 0.36, h * 0.78), QPointF(w * 0.64, h * 0.78));
        break;
    }
    case IconType::Audio: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QRectF arcH(w * 0.24, h * 0.22, w * 0.52, h * 0.52);
        p.drawArc(arcH, 0, 180 * 16);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(w * 0.20, h * 0.46, w * 0.14, h * 0.28), 2.0, 2.0);
        p.drawRoundedRect(QRectF(w * 0.66, h * 0.46, w * 0.14, h * 0.28), 2.0, 2.0);
        break;
    }
    case IconType::Phone: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(w * 0.28, h * 0.18, w * 0.44, h * 0.64), 3.5, 3.5);
        p.drawLine(QPointF(w * 0.44, h * 0.25), QPointF(w * 0.56, h * 0.25));
        p.drawEllipse(QPointF(w * 0.50, h * 0.74), 1.6, 1.6);
        break;
    }
    case IconType::Shield: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath sh;
        sh.moveTo(w * 0.50, h * 0.20);
        sh.lineTo(w * 0.76, h * 0.30);
        sh.lineTo(w * 0.76, h * 0.56);
        sh.cubicTo(w * 0.76, h * 0.74, w * 0.50, h * 0.84, w * 0.50, h * 0.84);
        sh.cubicTo(w * 0.50, h * 0.84, w * 0.24, h * 0.74, w * 0.24, h * 0.56);
        sh.lineTo(w * 0.24, h * 0.30);
        sh.closeSubpath();
        p.drawPath(sh);
        break;
    }
    case IconType::Check: {
        pen.setWidthF(2.2);
        p.setPen(pen);
        p.drawLine(QPointF(w * 0.24, h * 0.52), QPointF(w * 0.44, h * 0.72));
        p.drawLine(QPointF(w * 0.44, h * 0.72), QPointF(w * 0.78, h * 0.30));
        break;
    }
    case IconType::Macro: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Play tape / scroll
        p.drawRoundedRect(QRectF(w * 0.22, h * 0.26, w * 0.56, h * 0.48), 3.0, 3.0);
        // Play triangle inside
        p.setBrush(color);
        QPainterPath tp;
        tp.moveTo(w * 0.42, h * 0.38);
        tp.lineTo(w * 0.62, h * 0.50);
        tp.lineTo(w * 0.42, h * 0.62);
        tp.closeSubpath();
        p.drawPath(tp);
        break;
    }
    case IconType::FileManager: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath fld;
        fld.moveTo(w * 0.20, h * 0.32);
        fld.lineTo(w * 0.44, h * 0.32);
        fld.lineTo(w * 0.52, h * 0.40);
        fld.lineTo(w * 0.80, h * 0.40);
        fld.lineTo(w * 0.80, h * 0.72);
        fld.lineTo(w * 0.20, h * 0.72);
        fld.closeSubpath();
        p.drawPath(fld);
        break;
    }
    case IconType::MultiInstance: {
        pen.setWidthF(1.6);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Back instance
        p.drawRoundedRect(QRectF(w * 0.34, h * 0.20, w * 0.44, h * 0.52), 2.5, 2.5);
        // Front instance
        p.setBrush(ColorBgPanel);
        p.drawRoundedRect(QRectF(w * 0.20, h * 0.34, w * 0.44, h * 0.52), 2.5, 2.5);
        break;
    }
    case IconType::AndroidBackTriangle: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath tri;
        tri.moveTo(w * 0.34, h * 0.50);
        tri.lineTo(w * 0.64, h * 0.30);
        tri.lineTo(w * 0.64, h * 0.70);
        tri.closeSubpath();
        p.drawPath(tri);
        break;
    }
    case IconType::AndroidHomeCircle: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(w * 0.50, h * 0.50), w * 0.20, w * 0.20);
        break;
    }
    case IconType::AndroidRecentSquare: {
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(w * 0.32, h * 0.32, w * 0.36, h * 0.36), 2.0, 2.0);
        break;
    }
    case IconType::SwipeGesture: {
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        // Curved swipe arrow
        QRectF arcRect(w * 0.24, h * 0.26, w * 0.52, h * 0.52);
        p.drawArc(arcRect, 45 * 16, 180 * 16);
        // Arrow head
        p.setBrush(color);
        QPainterPath ah;
        ah.moveTo(w * 0.70, h * 0.32);
        ah.lineTo(w * 0.82, h * 0.36);
        ah.lineTo(w * 0.76, h * 0.48);
        ah.closeSubpath();
        p.drawPath(ah);
        break;
    }
    case IconType::SmartCast: {
        pen.setWidthF(1.7);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(w * 0.50, h * 0.50), w * 0.32, w * 0.32);
        p.drawEllipse(QPointF(w * 0.50, h * 0.50), w * 0.12, w * 0.12);
        p.drawLine(QPointF(w * 0.50, h * 0.14), QPointF(w * 0.50, h * 0.30));
        p.drawLine(QPointF(w * 0.50, h * 0.70), QPointF(w * 0.50, h * 0.86));
        p.drawLine(QPointF(w * 0.14, h * 0.50), QPointF(w * 0.30, h * 0.50));
        p.drawLine(QPointF(w * 0.70, h * 0.50), QPointF(w * 0.86, h * 0.50));
        break;
    }
    }

    return pixmap;
}

QIcon createIcon(IconType type, const QColor &color, int size)
{
    return QIcon(renderIconPixmap(type, color, size));
}

QString cyberEmeraldStyleSheet()
{
    return QString(R"(
        /* Global LinStack Cyber Emerald Theme */
        QWidget {
            background-color: transparent;
            color: #F9FAFB;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            font-size: 13px;
            selection-background-color: #10B981;
            selection-color: #0B0F19;
        }

        /* Tooltip styling */
        QToolTip {
            background-color: #111827;
            color: #F9FAFB;
            border: 1px solid #10B981;
            border-radius: 4px;
            padding: 5px 8px;
            font-size: 11px;
            font-weight: 500;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: #0D121D;
            width: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #1F2937;
            min-height: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #10B981;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QScrollBar:horizontal {
            background: #0D121D;
            height: 8px;
            margin: 0;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #1F2937;
            min-width: 24px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #10B981;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }

        /* Push Buttons */
        QPushButton {
            background-color: #161F30;
            color: #F9FAFB;
            border: 1px solid #1F2937;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
            outline: none;
        }
        QPushButton:hover {
            background-color: #1E293B;
            border: 1px solid #10B981;
            color: #00FF66;
        }
        QPushButton:pressed {
            background-color: #059669;
            color: #FFFFFF;
            border: 1px solid #00FF66;
        }
        QPushButton:disabled {
            background-color: #111827;
            color: #4B5563;
            border: 1px solid #1F2937;
        }

        /* Primary Action Button */
        QPushButton[primary="true"] {
            background-color: #10B981;
            color: #0B0F19;
            font-weight: 700;
            border: 1px solid #00FF66;
        }
        QPushButton[primary="true"]:hover {
            background-color: #00FF66;
            color: #064E3B;
            border: 1px solid #FFFFFF;
        }
        QPushButton[primary="true"]:pressed {
            background-color: #059669;
            color: #FFFFFF;
        }

        /* LineEdit Inputs */
        QLineEdit {
            background-color: #0D121D;
            color: #F9FAFB;
            border: 1px solid #1F2937;
            border-radius: 6px;
            padding: 6px 10px;
            selection-background-color: #10B981;
            selection-color: #0B0F19;
        }
        QLineEdit:focus {
            border: 1px solid #10B981;
            background-color: #111827;
        }

        /* ComboBox */
        QComboBox {
            background-color: #0D121D;
            color: #F9FAFB;
            border: 1px solid #1F2937;
            border-radius: 6px;
            padding: 6px 12px;
            min-width: 6em;
        }
        QComboBox:hover {
            border: 1px solid #10B981;
        }
        QComboBox:focus {
            border: 1px solid #00FF66;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #1F2937;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background-color: #111827;
        }
        QComboBox::down-arrow {
            width: 0px;
            height: 0px;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #10B981;
            margin-right: 2px;
        }
        QComboBox QAbstractItemView {
            background-color: #111827;
            color: #F9FAFB;
            border: 1px solid #10B981;
            selection-background-color: #10B981;
            selection-color: #0B0F19;
            outline: none;
            padding: 4px;
        }

        /* Slider */
        QSlider::groove:horizontal {
            border: none;
            height: 4px;
            background: #1F2937;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #10B981;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #00FF66;
            border: 2px solid #0B0F19;
            width: 14px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #FFFFFF;
            border: 2px solid #10B981;
        }

        /* Checkbox */
        QCheckBox {
            spacing: 8px;
            color: #E5E7EB;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #374151;
            border-radius: 4px;
            background-color: #0D121D;
        }
        QCheckBox::indicator:hover {
            border: 1px solid #10B981;
        }
        QCheckBox::indicator:checked {
            background-color: #10B981;
            border: 1px solid #00FF66;
            image: none;
        }

        /* Radio Button */
        QRadioButton {
            spacing: 8px;
            color: #E5E7EB;
        }
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #374151;
            border-radius: 8px;
            background-color: #0D121D;
        }
        QRadioButton::indicator:hover {
            border: 1px solid #10B981;
        }
        QRadioButton::indicator:checked {
            background-color: #10B981;
            border: 3px solid #0D121D;
        }

        /* Tab Widget */
        QTabWidget::pane {
            border: 1px solid #1F2937;
            background-color: #111827;
            border-radius: 6px;
            top: -1px;
        }
        QTabBar::tab {
            background: #0D121D;
            color: #9CA3AF;
            border: 1px solid #1F2937;
            border-bottom: none;
            padding: 8px 16px;
            margin-right: 4px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: 500;
        }
        QTabBar::tab:hover {
            color: #10B981;
            background: #161F30;
        }
        QTabBar::tab:selected {
            color: #00FF66;
            background: #111827;
            border-color: #1F2937;
            border-bottom: 2px solid #10B981;
            font-weight: 700;
        }
    )");
}

} // namespace Theme
