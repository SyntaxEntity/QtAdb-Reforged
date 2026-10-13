#ifndef ADBCLIENT_H
#define ADBCLIENT_H

#include <QObject>
#include <QProcess>     // handleProcessFinished 用到 QProcess::ExitStatus
#include <QHash>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <functional>

#include "textexplainer.h"      // 输出/错误的"解释"（可能弹出提示框）

class QTimer;

// 异步 ADB 客户端（M1 迁移的落点）。
//
// 背景：原有的 adbProcess 是同步 API（waitForReadyRead + waitForFinished），
// 约 70 处控件槽直接调用它 —— 设备无响应时会把 GUI 线程阻塞到超时（最长 20s）。
// AdbClient 把等待交还给事件循环：调用方立即返回，结果经 finished 信号投递。
//
// 设计要点
//   * 单实例、单 QProcess，请求入队后串行执行（同一条 adb 通道不适合并发抢占）；
//   * 每个请求独立超时；超时则 kill 子进程，仍照常发出 finished（exitCode = -1）；
//   * 支持取消：cancel(requestId) / cancelAll()。
class AdbClient : public QObject
{
    Q_OBJECT

public:
    // 是否对命令的标准输出/标准错误做"解释"（textExplainer 会针对特定输出弹出中文提示，
    // 例如"应用未安装""请先运行应用""请先退出所有账户"等）。
    //
    // 原实现里这件事由 adbProcess::run() 完成，并靠 adbProcess::thread 标志区分：
    // 后台线程发起的命令不弹窗。迁移到 AdbClient 后请求都从 GUI 线程发出，
    // 于是用这个标志保留原有区别 —— adbThread / threadMonitor 这类"取数据"的
    // 后台命令用 No，用户在界面上直接触发的操作用 Yes。
    enum class Explain { Yes, No };
    ~AdbClient() override;
    static constexpr int kDefaultTimeoutMs = 20000;        // 普通命令
    static constexpr int kLongOperationTimeoutMs = 600000; // 安装 / 侧载 / 推送 / 拉取

    static AdbClient *instance();

    // adb 可执行文件路径：优先使用随程序分发的 platform-tools，否则回退到 PATH。
    // （adbProcess 也复用这里，避免两套解析逻辑漂移。）
    static QString adbProgramPath();

    // 把命令串切成 argv：以空格分词；单独出现的一个 separator
    // 用于把其中含空格的若干词合并成一个参数（例如
    // "adb install ~ /path/with space.apk ~" 用 "~" 作分隔符）。
    // 这是原 adbProcess 的解析规则，迁移后集中到这里，避免多处漂移。
    static QStringList tokenizeCommand(const QString &command,
                                       const QString &separator = QStringLiteral("'"));

    // 入队一个请求并立即返回请求号；完成后发出 finished(requestId, ...)
    int run(const QStringList &arguments, int timeoutMs = kDefaultTimeoutMs);
    // 同上，并在子进程就绪后把 standardInput 写入其标准输入（随后关闭写通道）。
    // 用于 `adb pair` 这类需要交互输入配对码的命令。
    int run(const QStringList &arguments, const QString &standardInput,
            int timeoutMs = kDefaultTimeoutMs);
    // 指定设备的请求：等价于 run({"-s", serial, ...arguments})
    int runForDevice(const QString &serial, const QStringList &arguments,
                     int timeoutMs = kDefaultTimeoutMs);
    // 指定设备 + 一次性回调
    void runForDeviceWithCallback(const QString &serial, const QStringList &arguments,
                                  const std::function<void(int exitCode, const QString &output,
                                                           const QString &errorOutput)> &callback,
                                  int timeoutMs = kDefaultTimeoutMs,
                                  Explain explain = Explain::Yes);
    // 便捷入口：整条命令串（按空白切分，自动去掉开头的 "adb"），供迁移期使用
    int runCommand(const QString &command, int timeoutMs = kDefaultTimeoutMs);
    // 同上，但指定设备（等价于 run({"-s", serial, ...})）
    int runCommandForDevice(const QString &serial, const QString &command,
                            int timeoutMs = kDefaultTimeoutMs);
    // 整条命令串 + 标准输入
    int runCommandWithInput(const QString &command, const QString &standardInput,
                            int timeoutMs = kDefaultTimeoutMs);

    // 一次性回调：该请求完成时在 GUI 线程调用 callback（随后自动断开连接）。
    // 供"结果要参与界面判断"的旧同步调用点迁移使用。
    // 注意：callback 里访问界面对象时请用 QPointer 保护（页面可能已被销毁）。
    void runWithCallback(const QStringList &arguments,
                         const std::function<void(int exitCode, const QString &output,
                                                  const QString &errorOutput)> &callback,
                         int timeoutMs = kDefaultTimeoutMs,
                         Explain explain = Explain::Yes);
    // 整条命令串 + 一次性回调（自动去掉开头的 "adb"）
    void runCommandWithCallback(const QString &command,
                                const std::function<void(int exitCode, const QString &output,
                                                         const QString &errorOutput)> &callback,
                                int timeoutMs = kDefaultTimeoutMs);
    // 指定设备的整条命令串 + 一次性回调
    void runCommandForDeviceWithCallback(const QString &serial, const QString &command,
                                         const std::function<void(int exitCode, const QString &output,
                                                                  const QString &errorOutput)> &callback,
                                         int timeoutMs = kDefaultTimeoutMs);

    void cancel(int requestId);
    void cancelAll();

    // 退出前调用：立起关闭闸门、清空队列与在途请求（释放其回调）、终止子进程。
    // 由 main() 在 exec() 返回后显式调用 —— 若只依赖析构，那一刻静态对象已被销毁，
    // Qt 的事件分发器可能也没了（且 m_current 里的 std::function 会一直留到进程结束）。
    void shutdown();

signals:
    // exitCode 为 -1 表示失败/超时/被取消，具体原因见 standardError
    void finished(int requestId, int exitCode,
                  const QString &standardOutput, const QString &standardError);

private:
    explicit AdbClient(QObject *parent = nullptr);

    struct Request {
        int id = 0;
        QStringList arguments;
        QString standardInput;
        int timeoutMs = kDefaultTimeoutMs;
        Explain explain = Explain::Yes;
    };

    void startNext();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleTimeout();

    QProcess *m_process = nullptr;
    QTimer   *m_timeoutTimer = nullptr;
    QQueue<Request> m_queue;

    // 请求号 → finished 连接。用**值语义**保存在这里，而不是 new 一个
    // QMetaObject::Connection：请求若因退出或取消而没触发回调，
    // 堆上那个对象就再也没人释放（LSan 的栈指向 runWithCallback）。
    QHash<int, QMetaObject::Connection> m_connections;
    Request  m_current;
    bool     m_busy = false;
    bool     m_timedOut = false;
    bool     m_startFailed = false;
    // 析构时置位：避免在事件分发器已销毁之后仍启动定时器 / 子进程
    bool     m_shuttingDown = false;
    int      m_nextId = 1;
    // 对输出/错误做解释（与原 adbProcess 内嵌的 explainer 等价）
    textExplainer m_explainer;
    // 统一的入队入口（带"是否解释"标志）
    int enqueue(const QStringList &arguments, int timeoutMs, Explain explain);
};

#endif // ADBCLIENT_H
