/****************************************************************************
** Meta object code from reading C++ file 'TitleBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/TitleBar.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TitleBar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN8TitleBarE_t {};
} // unnamed namespace

template <> constexpr inline auto TitleBar::qt_create_metaobjectdata<qt_meta_tag_ZN8TitleBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "TitleBar",
        "minimizeRequested",
        "",
        "maximizeRequested",
        "closeRequested",
        "searchTriggered",
        "query",
        "helpRequested",
        "windowDragged",
        "QPoint",
        "globalDelta",
        "maximizeToggled"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'minimizeRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'maximizeRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'closeRequested'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchTriggered'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'helpRequested'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'windowDragged'
        QtMocHelpers::SignalData<void(const QPoint &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Signal 'maximizeToggled'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TitleBar, qt_meta_tag_ZN8TitleBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject TitleBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8TitleBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8TitleBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8TitleBarE_t>.metaTypes,
    nullptr
} };

void TitleBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TitleBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->minimizeRequested(); break;
        case 1: _t->maximizeRequested(); break;
        case 2: _t->closeRequested(); break;
        case 3: _t->searchTriggered((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->helpRequested(); break;
        case 5: _t->windowDragged((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 6: _t->maximizeToggled(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)()>(_a, &TitleBar::minimizeRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)()>(_a, &TitleBar::maximizeRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)()>(_a, &TitleBar::closeRequested, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)(const QString & )>(_a, &TitleBar::searchTriggered, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)()>(_a, &TitleBar::helpRequested, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)(const QPoint & )>(_a, &TitleBar::windowDragged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (TitleBar::*)()>(_a, &TitleBar::maximizeToggled, 6))
            return;
    }
}

const QMetaObject *TitleBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TitleBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8TitleBarE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int TitleBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void TitleBar::minimizeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void TitleBar::maximizeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void TitleBar::closeRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void TitleBar::searchTriggered(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void TitleBar::helpRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void TitleBar::windowDragged(const QPoint & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void TitleBar::maximizeToggled()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
