/****************************************************************************
** Meta object code from reading C++ file 'KeymapperOverlay.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/KeymapperOverlay.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'KeymapperOverlay.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16KeymapperOverlayE_t {};
} // unnamed namespace

template <> constexpr inline auto KeymapperOverlay::qt_create_metaobjectdata<qt_meta_tag_ZN16KeymapperOverlayE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KeymapperOverlay",
        "modeChanged",
        "",
        "KeymapperOverlay::Mode",
        "newMode",
        "layoutSaved",
        "filePath",
        "layoutLoaded",
        "toastRequested",
        "message",
        "Mode",
        "Play",
        "Edit"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'modeChanged'
        QtMocHelpers::SignalData<void(KeymapperOverlay::Mode)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'layoutSaved'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'layoutLoaded'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'toastRequested'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Mode'
        QtMocHelpers::EnumData<enum Mode>(10, 10, QMC::EnumIsScoped).add({
            {   11, Mode::Play },
            {   12, Mode::Edit },
        }),
    };
    return QtMocHelpers::metaObjectData<KeymapperOverlay, qt_meta_tag_ZN16KeymapperOverlayE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KeymapperOverlay::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16KeymapperOverlayE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16KeymapperOverlayE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16KeymapperOverlayE_t>.metaTypes,
    nullptr
} };

void KeymapperOverlay::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KeymapperOverlay *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->modeChanged((*reinterpret_cast<std::add_pointer_t<KeymapperOverlay::Mode>>(_a[1]))); break;
        case 1: _t->layoutSaved((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->layoutLoaded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->toastRequested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (KeymapperOverlay::*)(KeymapperOverlay::Mode )>(_a, &KeymapperOverlay::modeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (KeymapperOverlay::*)(const QString & )>(_a, &KeymapperOverlay::layoutSaved, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (KeymapperOverlay::*)(const QString & )>(_a, &KeymapperOverlay::layoutLoaded, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (KeymapperOverlay::*)(const QString & )>(_a, &KeymapperOverlay::toastRequested, 3))
            return;
    }
}

const QMetaObject *KeymapperOverlay::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KeymapperOverlay::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16KeymapperOverlayE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int KeymapperOverlay::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void KeymapperOverlay::modeChanged(KeymapperOverlay::Mode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void KeymapperOverlay::layoutSaved(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void KeymapperOverlay::layoutLoaded(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void KeymapperOverlay::toastRequested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
