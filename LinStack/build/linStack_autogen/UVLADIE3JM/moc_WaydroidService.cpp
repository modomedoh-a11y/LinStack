/****************************************************************************
** Meta object code from reading C++ file 'LinstackService.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/LinstackService.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LinstackService.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15LinstackServiceE_t {};
} // unnamed namespace

template <> constexpr inline auto LinstackService::qt_create_metaobjectdata<qt_meta_tag_ZN15LinstackServiceE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "LinstackService",
        "containerStateChanged",
        "",
        "LinstackService::ContainerState",
        "state",
        "sessionStateChanged",
        "LinstackService::SessionState",
        "adbConnectionStatusChanged",
        "connected",
        "errorOccurred",
        "LinstackService::ServiceError",
        "error",
        "message",
        "remediation",
        "diagnosticFinished",
        "KernelDiagnostic",
        "diagnostic",
        "logMessage",
        "category",
        "scriptOperationStarted",
        "operation",
        "scriptOperationProgress",
        "percent",
        "statusText",
        "scriptOperationFinished",
        "success",
        "details",
        "apkInstallStarted",
        "apkPath",
        "apkInstallProgress",
        "status",
        "apkInstallFinished",
        "handleSessionProcessFinished",
        "exitCode",
        "QProcess::ExitStatus",
        "exitStatus",
        "handleSessionProcessError",
        "QProcess::ProcessError",
        "handleSessionStdout",
        "handleSessionStderr",
        "handleAdbSocketConnected",
        "handleAdbSocketError",
        "ContainerState",
        "Stopped",
        "Starting",
        "Running",
        "Stopping",
        "Failed",
        "SessionState",
        "Idle",
        "Active",
        "Terminated",
        "ServiceError",
        "NoError",
        "KernelBinderMissing",
        "BinderfsNotMounted",
        "SystemdServiceFailed",
        "LinstackSessionFailed",
        "LinstackScriptsNotFound",
        "PythonScriptExecutionFailed",
        "AdbConnectionRefused",
        "AdbInstallFailed",
        "PermissionDenied"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'containerStateChanged'
        QtMocHelpers::SignalData<void(LinstackService::ContainerState)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'sessionStateChanged'
        QtMocHelpers::SignalData<void(LinstackService::SessionState)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 4 },
        }}),
        // Signal 'adbConnectionStatusChanged'
        QtMocHelpers::SignalData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(LinstackService::ServiceError, const QString &, const QStringList &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 }, { QMetaType::QString, 12 }, { QMetaType::QStringList, 13 },
        }}),
        // Signal 'diagnosticFinished'
        QtMocHelpers::SignalData<void(const KernelDiagnostic &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 15, 16 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 18 }, { QMetaType::QString, 12 },
        }}),
        // Signal 'scriptOperationStarted'
        QtMocHelpers::SignalData<void(const QString &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 },
        }}),
        // Signal 'scriptOperationProgress'
        QtMocHelpers::SignalData<void(const QString &, int, const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 }, { QMetaType::Int, 22 }, { QMetaType::QString, 23 },
        }}),
        // Signal 'scriptOperationFinished'
        QtMocHelpers::SignalData<void(const QString &, bool, const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 }, { QMetaType::Bool, 25 }, { QMetaType::QString, 26 },
        }}),
        // Signal 'apkInstallStarted'
        QtMocHelpers::SignalData<void(const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Signal 'apkInstallProgress'
        QtMocHelpers::SignalData<void(const QString &, int, const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 }, { QMetaType::Int, 22 }, { QMetaType::QString, 30 },
        }}),
        // Signal 'apkInstallFinished'
        QtMocHelpers::SignalData<void(const QString &, bool, const QString &)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 }, { QMetaType::Bool, 25 }, { QMetaType::QString, 12 },
        }}),
        // Slot 'handleSessionProcessFinished'
        QtMocHelpers::SlotData<void(int, QProcess::ExitStatus)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 33 }, { 0x80000000 | 34, 35 },
        }}),
        // Slot 'handleSessionProcessError'
        QtMocHelpers::SlotData<void(QProcess::ProcessError)>(36, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 37, 11 },
        }}),
        // Slot 'handleSessionStdout'
        QtMocHelpers::SlotData<void()>(38, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleSessionStderr'
        QtMocHelpers::SlotData<void()>(39, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleAdbSocketConnected'
        QtMocHelpers::SlotData<void()>(40, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleAdbSocketError'
        QtMocHelpers::SlotData<void()>(41, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'ContainerState'
        QtMocHelpers::EnumData<enum ContainerState>(42, 42, QMC::EnumIsScoped).add({
            {   43, ContainerState::Stopped },
            {   44, ContainerState::Starting },
            {   45, ContainerState::Running },
            {   46, ContainerState::Stopping },
            {   47, ContainerState::Failed },
        }),
        // enum 'SessionState'
        QtMocHelpers::EnumData<enum SessionState>(48, 48, QMC::EnumIsScoped).add({
            {   49, SessionState::Idle },
            {   44, SessionState::Starting },
            {   50, SessionState::Active },
            {   46, SessionState::Stopping },
            {   51, SessionState::Terminated },
            {   47, SessionState::Failed },
        }),
        // enum 'ServiceError'
        QtMocHelpers::EnumData<enum ServiceError>(52, 52, QMC::EnumIsScoped).add({
            {   53, ServiceError::NoError },
            {   54, ServiceError::KernelBinderMissing },
            {   55, ServiceError::BinderfsNotMounted },
            {   56, ServiceError::SystemdServiceFailed },
            {   57, ServiceError::LinstackSessionFailed },
            {   58, ServiceError::LinstackScriptsNotFound },
            {   59, ServiceError::PythonScriptExecutionFailed },
            {   60, ServiceError::AdbConnectionRefused },
            {   61, ServiceError::AdbInstallFailed },
            {   62, ServiceError::PermissionDenied },
        }),
    };
    return QtMocHelpers::metaObjectData<LinstackService, qt_meta_tag_ZN15LinstackServiceE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject LinstackService::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LinstackServiceE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LinstackServiceE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15LinstackServiceE_t>.metaTypes,
    nullptr
} };

void LinstackService::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LinstackService *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->containerStateChanged((*reinterpret_cast<std::add_pointer_t<LinstackService::ContainerState>>(_a[1]))); break;
        case 1: _t->sessionStateChanged((*reinterpret_cast<std::add_pointer_t<LinstackService::SessionState>>(_a[1]))); break;
        case 2: _t->adbConnectionStatusChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<LinstackService::ServiceError>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[3]))); break;
        case 4: _t->diagnosticFinished((*reinterpret_cast<std::add_pointer_t<KernelDiagnostic>>(_a[1]))); break;
        case 5: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->scriptOperationStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->scriptOperationProgress((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 8: _t->scriptOperationFinished((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 9: _t->apkInstallStarted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->apkInstallProgress((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 11: _t->apkInstallFinished((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 12: _t->handleSessionProcessFinished((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 13: _t->handleSessionProcessError((*reinterpret_cast<std::add_pointer_t<QProcess::ProcessError>>(_a[1]))); break;
        case 14: _t->handleSessionStdout(); break;
        case 15: _t->handleSessionStderr(); break;
        case 16: _t->handleAdbSocketConnected(); break;
        case 17: _t->handleAdbSocketError(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(LinstackService::ContainerState )>(_a, &LinstackService::containerStateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(LinstackService::SessionState )>(_a, &LinstackService::sessionStateChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(bool )>(_a, &LinstackService::adbConnectionStatusChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(LinstackService::ServiceError , const QString & , const QStringList & )>(_a, &LinstackService::errorOccurred, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const KernelDiagnostic & )>(_a, &LinstackService::diagnosticFinished, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & , const QString & )>(_a, &LinstackService::logMessage, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & )>(_a, &LinstackService::scriptOperationStarted, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & , int , const QString & )>(_a, &LinstackService::scriptOperationProgress, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & , bool , const QString & )>(_a, &LinstackService::scriptOperationFinished, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & )>(_a, &LinstackService::apkInstallStarted, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & , int , const QString & )>(_a, &LinstackService::apkInstallProgress, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (LinstackService::*)(const QString & , bool , const QString & )>(_a, &LinstackService::apkInstallFinished, 11))
            return;
    }
}

const QMetaObject *LinstackService::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LinstackService::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15LinstackServiceE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int LinstackService::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 18;
    }
    return _id;
}

// SIGNAL 0
void LinstackService::containerStateChanged(LinstackService::ContainerState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void LinstackService::sessionStateChanged(LinstackService::SessionState _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void LinstackService::adbConnectionStatusChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void LinstackService::errorOccurred(LinstackService::ServiceError _t1, const QString & _t2, const QStringList & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}

// SIGNAL 4
void LinstackService::diagnosticFinished(const KernelDiagnostic & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void LinstackService::logMessage(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void LinstackService::scriptOperationStarted(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void LinstackService::scriptOperationProgress(const QString & _t1, int _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3);
}

// SIGNAL 8
void LinstackService::scriptOperationFinished(const QString & _t1, bool _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2, _t3);
}

// SIGNAL 9
void LinstackService::apkInstallStarted(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void LinstackService::apkInstallProgress(const QString & _t1, int _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3);
}

// SIGNAL 11
void LinstackService::apkInstallFinished(const QString & _t1, bool _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
