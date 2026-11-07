/****************************************************************************
** Meta object code from reading C++ file 'SelfHealingManager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/SelfHealingManager.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SelfHealingManager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18SelfHealingManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto SelfHealingManager::qt_create_metaobjectdata<qt_meta_tag_ZN18SelfHealingManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SelfHealingManager",
        "healingStarted",
        "",
        "actionDescription",
        "healingProgress",
        "percent",
        "statusText",
        "healingFinished",
        "success",
        "summary",
        "diagnosticsProgress",
        "checkName",
        "diagnosticsFinished",
        "HealthReport",
        "report",
        "repairStarted",
        "repairFinished",
        "logMessage",
        "msg",
        "handleDiagnosticsFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'healingStarted'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'healingProgress'
        QtMocHelpers::SignalData<void(int, const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'healingFinished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 }, { QMetaType::QString, 9 },
        }}),
        // Signal 'diagnosticsProgress'
        QtMocHelpers::SignalData<void(int, const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::QString, 11 },
        }}),
        // Signal 'diagnosticsFinished'
        QtMocHelpers::SignalData<void(const HealthReport &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'repairStarted'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'repairFinished'
        QtMocHelpers::SignalData<void(bool)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 },
        }}),
        // Slot 'handleDiagnosticsFinished'
        QtMocHelpers::SlotData<void(const HealthReport &)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SelfHealingManager, qt_meta_tag_ZN18SelfHealingManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SelfHealingManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18SelfHealingManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18SelfHealingManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN18SelfHealingManagerE_t>.metaTypes,
    nullptr
} };

void SelfHealingManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SelfHealingManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->healingStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->healingProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->healingFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 3: _t->diagnosticsProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->diagnosticsFinished((*reinterpret_cast<std::add_pointer_t<HealthReport>>(_a[1]))); break;
        case 5: _t->repairStarted(); break;
        case 6: _t->repairFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->handleDiagnosticsFinished((*reinterpret_cast<std::add_pointer_t<HealthReport>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(const QString & )>(_a, &SelfHealingManager::healingStarted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(int , const QString & )>(_a, &SelfHealingManager::healingProgress, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(bool , const QString & )>(_a, &SelfHealingManager::healingFinished, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(int , const QString & )>(_a, &SelfHealingManager::diagnosticsProgress, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(const HealthReport & )>(_a, &SelfHealingManager::diagnosticsFinished, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)()>(_a, &SelfHealingManager::repairStarted, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(bool )>(_a, &SelfHealingManager::repairFinished, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfHealingManager::*)(const QString & )>(_a, &SelfHealingManager::logMessage, 7))
            return;
    }
}

const QMetaObject *SelfHealingManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SelfHealingManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN18SelfHealingManagerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SelfHealingManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void SelfHealingManager::healingStarted(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SelfHealingManager::healingProgress(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void SelfHealingManager::healingFinished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void SelfHealingManager::diagnosticsProgress(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void SelfHealingManager::diagnosticsFinished(const HealthReport & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void SelfHealingManager::repairStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void SelfHealingManager::repairFinished(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void SelfHealingManager::logMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN14RecoveryDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto RecoveryDialog::qt_create_metaobjectdata<qt_meta_tag_ZN14RecoveryDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "RecoveryDialog",
        "autoHealRequested",
        "",
        "recheckRequested",
        "ignoreAndContinue"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'autoHealRequested'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'recheckRequested'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'ignoreAndContinue'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RecoveryDialog, qt_meta_tag_ZN14RecoveryDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject RecoveryDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14RecoveryDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14RecoveryDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14RecoveryDialogE_t>.metaTypes,
    nullptr
} };

void RecoveryDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RecoveryDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->autoHealRequested(); break;
        case 1: _t->recheckRequested(); break;
        case 2: _t->ignoreAndContinue(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RecoveryDialog::*)()>(_a, &RecoveryDialog::autoHealRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryDialog::*)()>(_a, &RecoveryDialog::recheckRequested, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryDialog::*)()>(_a, &RecoveryDialog::ignoreAndContinue, 2))
            return;
    }
}

const QMetaObject *RecoveryDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RecoveryDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14RecoveryDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int RecoveryDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void RecoveryDialog::autoHealRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void RecoveryDialog::recheckRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void RecoveryDialog::ignoreAndContinue()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
