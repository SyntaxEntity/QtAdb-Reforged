#include "adbclient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>

AdbClient *AdbClient::instance()
{
    static AdbClient client;
    return &client;
}

QString AdbClient::adbProgramPath()
{
    static const QString resolved = [] {
        const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
        const QString bundled = appDir + QStringLiteral("/Resources/platform-tools/adb.exe");
#else
        const QString bundled = appDir + QStringLiteral("/Resources/platform-tools/adb");
#endif
        if(QFileInfo::exists(bundled))
        {
            return bundled;
        }
        return QStringLiteral("adb");
    }();
    return resolved;
}

QStringList AdbClient::tokenizeCommand(const QString &command, const QString &separator)
{
    const QStringList raw = command.split(QLatin1Char(' '));
    QStringList arguments;
    QString quoted;
    bool inQuote = false;

    for(const QString &token : raw)
    {
        if(token == separator)
        {
            if(inQuote)
            {
                arguments.append(quoted);
                quoted.clear();
                inQuote = false;
            }
            else
            {
                inQuote = true;
            }
            continue;
        }

        if(inQuote)
        {
            if(!quoted.isEmpty())
            {
                quoted += QLatin1Char(' ');
            }
            quoted += token;
        }
        else
        {
            arguments.append(token);
        }
    }

    // 分隔符未闭合时容错，不要把已收集的内容丢掉
    if(inQuote && !quoted.isEmpty())
    {
        arguments.append(quoted);
    }
    return arguments;
}

AdbClient::AdbClient(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_timeoutTimer(new QTimer(this))
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AdbClient::handleTimeout);
    connect(m_process, &QProcess::finished, this, &AdbClient::handleProcessFinished);

    // 启动失败（例如 PATH 里没有 adb）不会触发 finished()，需要单独收尾。
    // 这里用排队调用而不是直接调用：start() 是在 startNext() 内部同步发出的，
    // 直接处理会形成 startNext → start → errorOccurred → handleProcessFinished
    // → startNext 的递归。
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if(error == QProcess::FailedToStart && m_busy)
        {
            m_startFailed = true;
            QMetaObject::invokeMethod(this, [this] {
                handleProcessFinished(-1, QProcess::CrashExit);
            }, Qt::QueuedConnection);
        }
    });
}

AdbClient::~AdbClient()
{
    shutdown();
}

void AdbClient::shutdown()
{
    if(m_shuttingDown)
    {
        return;     // 幂等：main() 调用过之后析构再调用不会重复动作
    }

    // 退出时可能仍有在途/排队请求（例如设备挂起时的设备列表请求）。
    // 先立起关闭闸门，避免后续再启动定时器或子进程 —— 此刻事件分发器可能已经销毁，
    // Qt 会打印 "QObject::startTimer: current thread's event dispatcher has already been destroyed"，
    // 随后还可能留下 "QProcess: Destroyed while process (...) is still running"。
    m_shuttingDown = true;

    m_queue.clear();
    m_timeoutTimer->stop();

    // 一并断开所有未触发的 finished 连接（并释放其 lambda 捕获的回调）
    for(const QMetaObject::Connection &connection : std::as_const(m_connections))
    {
        disconnect(connection);
    }
    m_connections.clear();

    if(m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(3000);
    }

    // 在途请求也要放掉：它持着调用方的 std::function 与命令行参数 ——
    // 不清的话，进程退出时这份内存就是泄漏（LSan 把栈指到 runWithCallback）。
    m_current = Request();
    m_busy = false;
}

int AdbClient::enqueue(const QStringList &arguments, int timeoutMs, Explain explain)
{
    Request request;
    request.id = m_nextId++;
    request.arguments = arguments;
    request.timeoutMs = timeoutMs;
    request.explain = explain;

    m_queue.enqueue(request);
    if(!m_busy)
    {
        startNext();
    }
    return request.id;
}

int AdbClient::run(const QStringList &arguments, int timeoutMs)
{
    return enqueue(arguments, timeoutMs, Explain::Yes);
}

int AdbClient::run(const QStringList &arguments, const QString &standardInput, int timeoutMs)
{
    Request request;
    request.id = m_nextId++;
    request.arguments = arguments;
    request.standardInput = standardInput;
    request.timeoutMs = timeoutMs;

    m_queue.enqueue(request);
    if(!m_busy)
    {
        startNext();
    }
    return request.id;
}

int AdbClient::runForDevice(const QString &serial, const QStringList &arguments, int timeoutMs)
{
    QStringList full;
    if(!serial.isEmpty())
    {
        full << QStringLiteral("-s") << serial;
    }
    full += arguments;
    return run(full, timeoutMs);
}

void AdbClient::runForDeviceWithCallback(const QString &serial, const QStringList &arguments,
                                         const std::function<void(int, const QString &, const QString &)> &callback,
                                         int timeoutMs,
                                         Explain explain)
{
    QStringList full;
    if(!serial.isEmpty())
    {
        full << QStringLiteral("-s") << serial;
    }
    full += arguments;
    runWithCallback(full, callback, timeoutMs, explain);
}

int AdbClient::runCommand(const QString &command, int timeoutMs)
{
    QStringList arguments = tokenizeCommand(command);
    if(!arguments.isEmpty() && arguments.first() == QStringLiteral("adb"))
    {
        arguments.removeFirst();
    }
    return run(arguments, timeoutMs);
}

int AdbClient::runCommandForDevice(const QString &serial, const QString &command, int timeoutMs)
{
    QStringList arguments = tokenizeCommand(command);
    if(!arguments.isEmpty() && arguments.first() == QStringLiteral("adb"))
    {
        arguments.removeFirst();
    }
    return runForDevice(serial, arguments, timeoutMs);
}

int AdbClient::runCommandWithInput(const QString &command, const QString &standardInput, int timeoutMs)
{
    QStringList arguments = tokenizeCommand(command);
    if(!arguments.isEmpty() && arguments.first() == QStringLiteral("adb"))
    {
        arguments.removeFirst();
    }
    return run(arguments, standardInput, timeoutMs);
}

void AdbClient::runWithCallback(const QStringList &arguments,
                                const std::function<void(int, const QString &, const QString &)> &callback,
                                int timeoutMs,
                                Explain explain)
{
    const int requestId = enqueue(arguments, timeoutMs, explain);

    // 连接以**值语义**存进成员表，不 new：请求结束时按 id 取回并断开。
    // 原实现把 QMetaObject::Connection new 在堆上、只在其 lambda 被触发时 delete ——
    // 请求若因退出或取消而未触发回调（例如设备挂起时退出），那个对象就永远留下了。
    // 断开连接会同时销毁 lambda，于是它捕获的 callback 也一并释放。
    m_connections.insert(requestId, connect(this, &AdbClient::finished, this,
        [this, requestId, callback](int finishedId, int exitCode,
                                    const QString &output, const QString &errorOutput) {
            if(finishedId != requestId)
            {
                return;
            }
            disconnect(m_connections.take(requestId));
            callback(exitCode, output, errorOutput);
        }));
}

void AdbClient::runCommandWithCallback(const QString &command,
                                       const std::function<void(int, const QString &, const QString &)> &callback,
                                       int timeoutMs)
{
    QStringList arguments = tokenizeCommand(command);
    if(!arguments.isEmpty() && arguments.first() == QStringLiteral("adb"))
    {
        arguments.removeFirst();
    }
    runWithCallback(arguments, callback, timeoutMs);
}

void AdbClient::runCommandForDeviceWithCallback(const QString &serial, const QString &command,
                                                const std::function<void(int, const QString &, const QString &)> &callback,
                                                int timeoutMs)
{
    QStringList arguments = tokenizeCommand(command);
    if(!arguments.isEmpty() && arguments.first() == QStringLiteral("adb"))
    {
        arguments.removeFirst();
    }
    if(!serial.isEmpty())
    {
        arguments.prepend(serial);
        arguments.prepend(QStringLiteral("-s"));
    }
    runWithCallback(arguments, callback, timeoutMs);
}

void AdbClient::cancel(int requestId)
{
    if(m_busy && m_current.id == requestId)
    {
        m_process->kill();      // 触发 finished()，走正常收尾
        return;
    }

    for(int i = 0; i < m_queue.size(); ++i)
    {
        if(m_queue.at(i).id == requestId)
        {
            m_queue.removeAt(i);
            emit finished(requestId, -1, QString(), tr("请求已取消"));
            return;
        }
    }
}

void AdbClient::cancelAll()
{
    const QQueue<Request> pending = m_queue;
    m_queue.clear();
    for(const Request &request : pending)
    {
        emit finished(request.id, -1, QString(), tr("请求已取消"));
    }

    if(m_busy)
    {
        m_process->kill();
    }
}

void AdbClient::startNext()
{
    if(m_shuttingDown || m_queue.isEmpty())
    {
        m_busy = false;
        return;
    }

    m_busy = true;
    m_current = m_queue.dequeue();
    m_timedOut = false;
    m_startFailed = false;

    m_process->start(adbProgramPath(), m_current.arguments);
    m_timeoutTimer->start(m_current.timeoutMs);

    if(!m_current.standardInput.isEmpty())
    {
        // start() 之后立刻 write() 会失败（进程尚未就绪），因此等 started() 再写；
        // 写完关闭写通道，让子进程知道输入结束（例如 adb pair 的配对码）。
        connect(m_process, &QProcess::started, this, [this]() {
            m_process->write(m_current.standardInput.toLocal8Bit());
            m_process->closeWriteChannel();
        }, Qt::SingleShotConnection);
    }
}

void AdbClient::handleTimeout()
{
    if(!m_busy)
    {
        return;
    }

    m_timedOut = true;
    m_process->kill();
}

void AdbClient::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if(!m_busy)
    {
        return;     // 可能是启动失败后重复到达的通知
    }
    m_busy = false;
    m_timeoutTimer->stop();

    const QString output = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    QString errorOutput = QString::fromLocal8Bit(m_process->readAllStandardError());

    if(m_timedOut)
    {
        errorOutput += tr("[adb 命令超时 %1 ms，已终止该进程] %2")
                           .arg(m_current.timeoutMs)
                           .arg(m_current.arguments.join(QLatin1Char(' ')));
        exitCode = -1;
    }
    else if(m_startFailed)
    {
        errorOutput = tr("[无法启动 adb] 程序 = %1，原因 = %2")
                          .arg(adbProgramPath(), m_process->errorString());
        exitCode = -1;
    }
    else if(status == QProcess::CrashExit && exitCode == 0)
    {
        exitCode = -1;
    }

    if(!errorOutput.isEmpty())
    {
        qWarning().noquote() << errorOutput;
    }

    // 恢复原 adbProcess 的行为：对输出与错误输出做"解释"，后者会针对
    // "Permission denied""Not allowed to set the device owner …"等约 20 种情况
    // 弹出中文提示。后台取数类命令（adbThread / threadMonitor）用 Explain::No，
    // 与原实现里"后台线程不弹窗"（adbProcess::thread = true）保持一致。
    if(m_current.explain == Explain::Yes)
    {
        m_explainer.thread = false;             // 现在全部在 GUI 线程执行
        m_explainer.explainOutput(output, false);

        // 仅在确有错误输出时才解释错误：原实现挂在 readyReadStandardError 上，
        // 没有 stderr 就不会触发。若这里无条件调用，explainError 末尾的兜底分支
        // 会在每次成功命令之后弹出"未知错误！"，并把 explainOutput 给出的
        // 有意义提示顶掉（showMsgBox 会关闭上一个提示框）。
        if(!errorOutput.isEmpty())
        {
            m_explainer.explainError(errorOutput);
        }
    }

    emit finished(m_current.id, exitCode, output, errorOutput);

    // 注意：finished 的回调里可能立刻发起下一个请求（链式/嵌套回调，例如
    // "先取 proc/stat 再取 meminfo"），那种情况下 run() 已经把 m_busy 置回 true。
    // 如果这里无条件调用 startNext()，它会看到队列为空而把 m_busy 重新置为 false，
    // 于是刚启动的请求会被"遗忘"、结果永远不投递 —— 链式回调就此卡死。
    if(!m_busy)
    {
        startNext();
    }
}
