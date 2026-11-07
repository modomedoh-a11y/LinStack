/****************************************************************************
** Meta object code from reading C++ file 'DiagnosticsEngine.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/DiagnosticsEngine.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DiagnosticsEngine.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17DiagnosticsWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto DiagnosticsWorker::qt_create_metaobjectdata<qt_meta_tag_ZN17DiagnosticsWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DiagnosticsWorker",
        "progressUpdated",
        "",
        "percent",
        "checkName",
        "checkCompleted",
        "DiagnosticCheckResult",
        "result",
        "diagnosticsFinished",
        "HealthReport",
        "report",
        "performChecks"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'progressUpdated'
        QtMocHelpers::SignalData<void(int, const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::QString, 4 },
        }}),
        // Signal 'checkCompleted'
        QtMocHelpers::SignalData<void(const DiagnosticCheckResult &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'diagnosticsFinished'
        QtMocHelpers::SignalData<void(const HealthReport &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Slot 'performChecks'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DiagnosticsWorker, qt_meta_tag_ZN17DiagnosticsWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DiagnosticsWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DiagnosticsWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DiagnosticsWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17DiagnosticsWorkerE_t>.metaTypes,
    nullptr
} };

void DiagnosticsWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DiagnosticsWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progressUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->checkCompleted((*reinterpret_cast<std::add_pointer_t<DiagnosticCheckResult>>(_a[1]))); break;
        case 2: _t->diagnosticsFinished((*reinterpret_cast<std::add_pointer_t<HealthReport>>(_a[1]))); break;
        case 3: _t->performChecks(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DiagnosticsWorker::*)(int , const QString & )>(_a, &DiagnosticsWorker::progressUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagnosticsWorker::*)(const DiagnosticCheckResult & )>(_a, &DiagnosticsWorker::checkCompleted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagnosticsWorker::*)(const HealthReport & )>(_a, &DiagnosticsWorker::diagnosticsFinished, 2))
            return;
    }
}

const QMetaObject *DiagnosticsWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DiagnosticsWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DiagnosticsWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DiagnosticsWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
void DiagnosticsWorker::progressUpdated(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void DiagnosticsWorker::checkCompleted(const DiagnosticCheckResult & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void DiagnosticsWorker::diagnosticsFinished(const HealthReport & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN17DiagnosticsEngineE_t {};
} // unnamed namespace

template <> constexpr inline auto DiagnosticsEngine::qt_create_metaobjectdata<qt_meta_tag_ZN17DiagnosticsEngineE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DiagnosticsEngine",
        "progressUpdated",
        "",
        "percent",
        "checkName",
        "checkCompleted",
        "DiagnosticCheckResult",
        "result",
        "diagnosticsFinished",
        "HealthReport",
        "report",
        "handleDiagnosticsFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'progressUpdated'
        QtMocHelpers::SignalData<void(int, const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::QString, 4 },
        }}),
        // Signal 'checkCompleted'
        QtMocHelpers::SignalData<void(const DiagnosticCheckResult &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'diagnosticsFinished'
        QtMocHelpers::SignalData<void(const HealthReport &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Slot 'handleDiagnosticsFinished'
        QtMocHelpers::SlotData<void(const HealthReport &)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DiagnosticsEngine, qt_meta_tag_ZN17DiagnosticsEngineE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DiagnosticsEngine::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DiagnosticsEngineE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DiagnosticsEngineE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17DiagnosticsEngineE_t>.metaTypes,
    nullptr
} };

void DiagnosticsEngine::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DiagnosticsEngine *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progressUpdated((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->checkCompleted((*reinterpret_cast<std::add_pointer_t<DiagnosticCheckResult>>(_a[1]))); break;
        case 2: _t->diagnosticsFinished((*reinterpret_cast<std::add_pointer_t<HealthReport>>(_a[1]))); break;
        case 3: _t->handleDiagnosticsFinished((*reinterpret_cast<std::add_pointer_t<HealthReport>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DiagnosticsEngine::*)(int , const QString & )>(_a, &DiagnosticsEngine::progressUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagnosticsEngine::*)(const DiagnosticCheckResult & )>(_a, &DiagnosticsEngine::checkCompleted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DiagnosticsEngine::*)(const HealthReport & )>(_a, &DiagnosticsEngine::diagnosticsFinished, 2))
            return;
    }
}

const QMetaObject *DiagnosticsEngine::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DiagnosticsEngine::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17DiagnosticsEngineE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DiagnosticsEngine::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
void DiagnosticsEngine::progressUpdated(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void DiagnosticsEngine::checkCompleted(const DiagnosticCheckResult & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void DiagnosticsEngine::diagnosticsFinished(const HealthReport & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
