#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QTimer>
#include <QQueue>

class UInputTouchBridge;

/**
 * @brief ScriptEngine — parse and execute LinStack macro script language.
 *
 * Supported commands:
 *   tap X Y                     — single touch down+up at pixel coords
 *   swipe X1 Y1 X2 Y2 DurMs    — linear multi-step swipe
 *   wait DelayMs                — pause execution for N milliseconds
 *   keyDown Key                 — simulate key press (Qt key name or code)
 *   keyUp Key                   — simulate key release
 *   enterShoot                  — activate aim-lock (FPS mode)
 *   exitShoot                   — deactivate aim-lock
 *   loop Count ... loopEnd      — repeat block Count times (-1 for infinite)
 *
 * Example script:
 *   tap 540 960
 *   wait 200
 *   swipe 200 800 200 400 300
 *   loop 3
 *     tap 900 600
 *     wait 100
 *   loopEnd
 */
class ScriptEngine : public QObject
{
    Q_OBJECT

public:
    explicit ScriptEngine(UInputTouchBridge *bridge, QObject *parent = nullptr);
    ~ScriptEngine() override;

    void setBridge(UInputTouchBridge *bridge);

    // Load script source text; compiles into instruction list
    bool loadScript(const QString &scriptText, QString *errorOut = nullptr);

    // Load from file path
    bool loadScriptFile(const QString &filePath, QString *errorOut = nullptr);

    // Start executing the loaded script from the beginning
    void execute();

    // Stop any running script immediately
    void stop();

    bool isRunning() const { return m_running; }

    // Bind a Qt key code to a script (scriptText stored directly)
    void bindToHotkey(int qtKey, const QString &scriptText);
    void unbindHotkey(int qtKey);

    // Returns true if the key was bound and a script was started
    bool triggerHotkey(int qtKey);

    // Current bound hotkeys
    QMap<int, QString> hotkeys() const { return m_hotkeys; }

signals:
    void scriptStarted();
    void scriptFinished();
    void scriptError(const QString &message);
    void commandExecuted(const QString &command);
    void progressChanged(int current, int total);

private slots:
    void onStepTimer();

private:
    enum class OpCode {
        Tap,
        Swipe,
        Wait,
        KeyDown,
        KeyUp,
        EnterShoot,
        ExitShoot,
        LoopBegin,
        LoopEnd,
        Nop
    };

    struct Instruction {
        OpCode op = OpCode::Nop;
        int x1 = 0, y1 = 0;
        int x2 = 0, y2 = 0;
        int duration = 0;     // ms for Wait / Swipe
        int loopCount = 0;    // -1 for infinite (LoopBegin)
        int loopTarget = -1;  // index of matching LoopBegin (for LoopEnd)
        QString keyName;
    };

    bool compile(const QStringList &lines, QString *errorOut);
    void executeInstruction(const Instruction &instr);
    void startSwipeExecution(const Instruction &instr);
    void finishCurrentInstruction();

    UInputTouchBridge   *m_bridge   = nullptr;
    QList<Instruction>   m_program;
    int                  m_ip       = 0;    // instruction pointer
    bool                 m_running  = false;

    // Loop stack: (loopBeginIndex, remainingIterations)
    QList<QPair<int,int>> m_loopStack;

    // Step timer for non-blocking execution
    QTimer *m_stepTimer = nullptr;

    // Swipe sub-state
    bool   m_swipeInProgress = false;
    int    m_swipeStep       = 0;
    int    m_swipeTotalSteps = 0;
    int    m_swipeX1 = 0, m_swipeY1 = 0;
    int    m_swipeX2 = 0, m_swipeY2 = 0;

    // Hotkey map: Qt key code → script text
    QMap<int, QString> m_hotkeys;
};

#endif // SCRIPTENGINE_H
