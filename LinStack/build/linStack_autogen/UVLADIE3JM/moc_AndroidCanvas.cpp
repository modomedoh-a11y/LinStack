/****************************************************************************
** Meta object code from reading C++ file 'AndroidCanvas.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/AndroidCanvas.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AndroidCanvas.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13AndroidCanvasE_t {};
} // unnamed namespace

template <> constexpr inline auto AndroidCanvas::qt_create_metaobjectdata<qt_meta_tag_ZN13AndroidCanvasE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AndroidCanvas",
        "windowEmbedded",
        "",
        "WId",
        "winId",
        "windowReleased",
        "appLaunchRequested",
        "packageName",
        "displayName",
        "apkDropped",
        "filePath",
        "searchStoreRequested",
        "query",
        "handleAppCardClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'windowEmbedded'
        QtMocHelpers::SignalData<void(WId)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'windowReleased'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'appLaunchRequested'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
        // Signal 'apkDropped'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'searchStoreRequested'
        QtMocHelpers::SignalData<void(const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'handleAppCardClicked'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(13, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 7 }, { QMetaType::QString, 8 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AndroidCanvas, qt_meta_tag_ZN13AndroidCanvasE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AndroidCanvas::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13AndroidCanvasE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13AndroidCanvasE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13AndroidCanvasE_t>.metaTypes,
    nullptr
} };

void AndroidCanvas::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AndroidCanvas *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->windowEmbedded((*reinterpret_cast<std::add_pointer_t<WId>>(_a[1]))); break;
        case 1: _t->windowReleased(); break;
        case 2: _t->appLaunchRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->apkDropped((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->searchStoreRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->handleAppCardClicked((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AndroidCanvas::*)(WId )>(_a, &AndroidCanvas::windowEmbedded, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AndroidCanvas::*)()>(_a, &AndroidCanvas::windowReleased, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AndroidCanvas::*)(const QString & , const QString & )>(_a, &AndroidCanvas::appLaunchRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AndroidCanvas::*)(const QString & )>(_a, &AndroidCanvas::apkDropped, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AndroidCanvas::*)(const QString & )>(_a, &AndroidCanvas::searchStoreRequested, 4))
            return;
    }
}

const QMetaObject *AndroidCanvas::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AndroidCanvas::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13AndroidCanvasE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int AndroidCanvas::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void AndroidCanvas::windowEmbedded(WId _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void AndroidCanvas::windowReleased()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void AndroidCanvas::appLaunchRequested(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void AndroidCanvas::apkDropped(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void AndroidCanvas::searchStoreRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
