#include "ScriptEngine.h"
#include "UInputTouchBridge.h"

#include <QFile>
#include <QTextStream>
#include <QKeySequence>
#include <cmath>

// Swipe steps per 100ms budget (each step ~25ms)
static constexpr int kSwipeStepIntervalMs = 25;

ScriptEngine::ScriptEngine(UInputTouchBridge *bridge, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
{
    m_stepTimer = new QTimer(this);
    m_stepTimer->setSingleShot(false);
    m_stepTimer->setInterval(kSwipeStepIntervalMs);
    connect(m_stepTimer, &QTimer::timeout, this, &ScriptEngine::onStepTimer);
}

ScriptEngine::~ScriptEngine()
{
    stop();
}

void ScriptEngine::setBridge(UInputTouchBridge *bridge)
{
    m_bridge = bridge;
}

bool ScriptEngine::loadScript(const QString &scriptText, QString *errorOut)
{
    stop();
    m_program.clear();
    m_loopStack.clear();
    m_ip = 0;

    QStringList lines = scriptText.split('\n', Qt::SkipEmptyParts);
    return compile(lines, errorOut);
}

bool ScriptEngine::loadScriptFile(const QString &filePath, QString *errorOut)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut) *errorOut = QString("Cannot open script file: %1").arg(filePath);
        return false;
    }
    return loadScript(QTextStream(&f).readAll(), errorOut);
}

bool ScriptEngine::compile(const QStringList &lines, QString *errorOut)
{
    // Stack of (loopBeginIndex, loopCount) for matching loopEnd
    QList<int> openLoopIndices;

    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        QString raw = lines[lineNum].trimmed();
        if (raw.isEmpty() || raw.startsWith('#') || raw.startsWith("//")) {
            continue; // skip comments / blank lines
        }

        QStringList parts = raw.split(' ', Qt::SkipEmptyParts);
        QString cmd = parts[0].toLower();
        Instruction instr;

        if (cmd == "tap") {
            if (parts.size() < 3) {
                if (errorOut) *errorOut = QString("Line %1: tap requires X Y").arg(lineNum + 1);
                return false;
            }
            instr.op = OpCode::Tap;
            instr.x1 = parts[1].toInt();
            instr.y1 = parts[2].toInt();

        } else if (cmd == "swipe") {
            if (parts.size() < 6) {
                if (errorOut) *errorOut = QString("Line %1: swipe requires X1 Y1 X2 Y2 DurationMs").arg(lineNum + 1);
                return false;
            }
            instr.op = OpCode::Swipe;
            instr.x1 = parts[1].toInt();
            instr.y1 = parts[2].toInt();
            instr.x2 = parts[3].toInt();
            instr.y2 = parts[4].toInt();
            instr.duration = qMax(50, parts[5].toInt());

        } else if (cmd == "wait") {
            if (parts.size() < 2) {
                if (errorOut) *errorOut = QString("Line %1: wait requires DelayMs").arg(lineNum + 1);
                return false;
            }
            instr.op = OpCode::Wait;
            instr.duration = qMax(1, parts[1].toInt());

        } else if (cmd == "keydown") {
            if (parts.size() < 2) {
                if (errorOut) *errorOut = QString("Line %1: keyDown requires Key").arg(lineNum + 1);
                return false;
            }
            instr.op = OpCode::KeyDown;
            instr.keyName = parts[1];

        } else if (cmd == "keyup") {
            if (parts.size() < 2) {
                if (errorOut) *errorOut = QString("Line %1: keyUp requires Key").arg(lineNum + 1);
                return false;
            }
            instr.op = OpCode::KeyUp;
            instr.keyName = parts[1];

        } else if (cmd == "entershoot") {
            instr.op = OpCode::EnterShoot;

        } else if (cmd == "exitshoot") {
            instr.op = OpCode::ExitShoot;

        } else if (cmd == "loop") {
            instr.op = OpCode::LoopBegin;
            instr.loopCount = (parts.size() >= 2) ? parts[1].toInt() : 1;
            openLoopIndices.append(m_program.size());
            m_program.append(instr);
            continue; // don't re-append below

        } else if (cmd == "loopend") {
            if (openLoopIndices.isEmpty()) {
                if (errorOut) *errorOut = QString("Line %1: loopEnd without matching loop").arg(lineNum + 1);
                return false;
            }
            int beginIdx = openLoopIndices.takeLast();
            instr.op = OpCode::LoopEnd;
            instr.loopTarget = beginIdx;
            m_program.append(instr);
            continue;

        } else {
            // Unknown command — emit warning but continue (NOP)
            instr.op = OpCode::Nop;
        }

        m_program.append(instr);
    }

    if (!openLoopIndices.isEmpty()) {
        if (errorOut) *errorOut = "Unclosed loop block (missing loopEnd)";
        return false;
    }

    return true;
}

void ScriptEngine::execute()
{
    if (m_program.isEmpty()) return;

    m_ip = 0;
    m_running = true;
    m_loopStack.clear();
    m_swipeInProgress = false;
    emit scriptStarted();

    // Kick off first instruction via timer to avoid blocking the event loop
    m_stepTimer->setInterval(1);
    m_stepTimer->start();
}

void ScriptEngine::stop()
{
    m_running = false;
    m_swipeInProgress = false;
    m_stepTimer->stop();
    m_loopStack.clear();
    m_ip = 0;
}

void ScriptEngine::onStepTimer()
{
    if (!m_running) {
        m_stepTimer->stop();
        return;
    }

    // Swipe sub-stepping
    if (m_swipeInProgress) {
        m_swipeStep++;
        qreal t = static_cast<qreal>(m_swipeStep) / m_swipeTotalSteps;
        int cx = m_swipeX1 + static_cast<int>((m_swipeX2 - m_swipeX1) * t);
        int cy = m_swipeY1 + static_cast<int>((m_swipeY2 - m_swipeY1) * t);

        if (m_swipeStep >= m_swipeTotalSteps) {
            if (m_bridge) {
                m_bridge->touchMove(8, cx, cy);
                m_bridge->touchUp(8);
            }
            m_swipeInProgress = false;
            finishCurrentInstruction();
        } else {
            if (m_bridge) {
                m_bridge->touchMove(8, cx, cy);
            }
        }
        return;
    }

    if (m_ip >= m_program.size()) {
        m_running = false;
        m_stepTimer->stop();
        emit scriptFinished();
        return;
    }

    executeInstruction(m_program[m_ip]);
}

void ScriptEngine::executeInstruction(const Instruction &instr)
{
    emit commandExecuted(QString("ip=%1 op=%2").arg(m_ip).arg(static_cast<int>(instr.op)));
    emit progressChanged(m_ip, m_program.size());

    switch (instr.op) {

    case OpCode::Tap:
        if (m_bridge) {
            m_bridge->touchDown(7, instr.x1, instr.y1);
            m_bridge->touchUp(7);
        }
        finishCurrentInstruction();
        break;

    case OpCode::Swipe: {
        m_swipeTotalSteps = qMax(1, instr.duration / kSwipeStepIntervalMs);
        m_swipeStep = 0;
        m_swipeX1 = instr.x1; m_swipeY1 = instr.y1;
        m_swipeX2 = instr.x2; m_swipeY2 = instr.y2;
        if (m_bridge) {
            m_bridge->touchDown(8, instr.x1, instr.y1);
        }
        m_swipeInProgress = true;
        m_stepTimer->setInterval(kSwipeStepIntervalMs);
        // Don't advance IP — onStepTimer will call finishCurrentInstruction() when done
        break;
    }

    case OpCode::Wait:
        m_stepTimer->stop();
        // Schedule resume after delay
        QTimer::singleShot(instr.duration, this, [this]() {
            if (m_running) {
                finishCurrentInstruction();
                m_stepTimer->setInterval(1);
                m_stepTimer->start();
            }
        });
        break;

    case OpCode::KeyDown:
        // Emit as text — InputTranslator is hooked up by MainWindow via signals
        // For now, emit commandExecuted which MainWindow can intercept
        emit commandExecuted(QString("keyDown:%1").arg(instr.keyName));
        finishCurrentInstruction();
        break;

    case OpCode::KeyUp:
        emit commandExecuted(QString("keyUp:%1").arg(instr.keyName));
        finishCurrentInstruction();
        break;

    case OpCode::EnterShoot:
        emit commandExecuted("enterShoot");
        finishCurrentInstruction();
        break;

    case OpCode::ExitShoot:
        emit commandExecuted("exitShoot");
        finishCurrentInstruction();
        break;

    case OpCode::LoopBegin: {
        // Push loop state: (beginIdx, remaining iterations)
        int remaining = (instr.loopCount < 0) ? -1 : instr.loopCount;
        m_loopStack.append(qMakePair(m_ip, remaining));
        finishCurrentInstruction();
        break;
    }

    case OpCode::LoopEnd: {
        if (m_loopStack.isEmpty()) {
            emit scriptError("loopEnd without matching loop state");
            stop();
            return;
        }
        auto &top = m_loopStack.last();
        if (top.second < 0) {
            // Infinite loop — jump back to begin+1
            m_ip = top.first + 1;
            m_stepTimer->setInterval(1);
            m_stepTimer->start();
        } else {
            top.second--;
            if (top.second > 0) {
                m_ip = top.first + 1;
                m_stepTimer->setInterval(1);
                m_stepTimer->start();
            } else {
                m_loopStack.removeLast();
                finishCurrentInstruction();
            }
        }
        break;
    }

    case OpCode::Nop:
    default:
        finishCurrentInstruction();
        break;
    }
}

void ScriptEngine::finishCurrentInstruction()
{
    m_ip++;
    if (m_ip >= m_program.size()) {
        m_running = false;
        m_stepTimer->stop();
        emit scriptFinished();
        return;
    }
    // Next instruction on next timer tick
    m_stepTimer->setInterval(1);
    if (!m_stepTimer->isActive()) {
        m_stepTimer->start();
    }
}

// ── Hotkey binding ────────────────────────────────────────────────────────────

void ScriptEngine::bindToHotkey(int qtKey, const QString &scriptText)
{
    m_hotkeys.insert(qtKey, scriptText);
}

void ScriptEngine::unbindHotkey(int qtKey)
{
    m_hotkeys.remove(qtKey);
}

bool ScriptEngine::triggerHotkey(int qtKey)
{
    if (!m_hotkeys.contains(qtKey)) return false;

    // Stop any currently running script before starting the new one
    stop();
    QString err;
    if (!loadScript(m_hotkeys[qtKey], &err)) {
        emit scriptError(err);
        return true;
    }
    execute();
    return true;
}
