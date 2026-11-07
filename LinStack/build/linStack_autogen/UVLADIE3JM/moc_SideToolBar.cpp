/****************************************************************************
** Meta object code from reading C++ file 'SideToolBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/SideToolBar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SideToolBar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11SideToolBarE_t {};
} // unnamed namespace

template <> constexpr inline auto SideToolBar::qt_create_metaobjectdata<qt_meta_tag_ZN11SideToolBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SideToolBar",
        "fullscreenClicked",
        "",
        "volumeUpClicked",
        "volumeDownClicked",
        "keymapperToggled",
        "active",
        "macroClicked",
        "rotateClicked",
        "ecoModeToggled",
        "installApkClicked",
        "screenshotClicked",
        "fileManagerClicked",
        "ramTrimClicked",
        "multiInstanceClicked",
        "settingsClicked",
        "androidBackClicked",
        "androidHomeClicked",
        "androidRecentsClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'fullscreenClicked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'volumeUpClicked'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'volumeDownClicked'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'keymapperToggled'
        QtMocHelpers::SignalData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Signal 'macroClicked'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'rotateClicked'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ecoModeToggled'
        QtMocHelpers::SignalData<void(bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Signal 'installApkClicked'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'screenshotClicked'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'fileManagerClicked'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ramTrimClicked'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'multiInstanceClicked'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'settingsClicked'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'androidBackClicked'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'androidHomeClicked'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'androidRecentsClicked'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SideToolBar, qt_meta_tag_ZN11SideToolBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SideToolBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SideToolBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SideToolBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11SideToolBarE_t>.metaTypes,
    nullptr
} };

void SideToolBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SideToolBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->fullscreenClicked(); break;
        case 1: _t->volumeUpClicked(); break;
        case 2: _t->volumeDownClicked(); break;
        case 3: _t->keymapperToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->macroClicked(); break;
        case 5: _t->rotateClicked(); break;
        case 6: _t->ecoModeToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->installApkClicked(); break;
        case 8: _t->screenshotClicked(); break;
        case 9: _t->fileManagerClicked(); break;
        case 10: _t->ramTrimClicked(); break;
        case 11: _t->multiInstanceClicked(); break;
        case 12: _t->settingsClicked(); break;
        case 13: _t->androidBackClicked(); break;
        case 14: _t->androidHomeClicked(); break;
        case 15: _t->androidRecentsClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::fullscreenClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::volumeUpClicked, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::volumeDownClicked, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)(bool )>(_a, &SideToolBar::keymapperToggled, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::macroClicked, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::rotateClicked, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)(bool )>(_a, &SideToolBar::ecoModeToggled, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::installApkClicked, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::screenshotClicked, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::fileManagerClicked, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::ramTrimClicked, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::multiInstanceClicked, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::settingsClicked, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::androidBackClicked, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::androidHomeClicked, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (SideToolBar::*)()>(_a, &SideToolBar::androidRecentsClicked, 15))
            return;
    }
}

const QMetaObject *SideToolBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SideToolBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SideToolBarE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SideToolBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void SideToolBar::fullscreenClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void SideToolBar::volumeUpClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SideToolBar::volumeDownClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void SideToolBar::keymapperToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void SideToolBar::macroClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void SideToolBar::rotateClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void SideToolBar::ecoModeToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void SideToolBar::installApkClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void SideToolBar::screenshotClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void SideToolBar::fileManagerClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void SideToolBar::ramTrimClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void SideToolBar::multiInstanceClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void SideToolBar::settingsClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void SideToolBar::androidBackClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void SideToolBar::androidHomeClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void SideToolBar::androidRecentsClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}
QT_WARNING_POP
