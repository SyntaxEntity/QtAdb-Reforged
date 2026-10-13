#ifndef ADBTHREAD_H
#define ADBTHREAD_H

#include <QObject>
#include "../textexplainer.h"

// 一次性 adb 请求（异步）。
//
// 历史：本类原为 QThread 子类，在 run() 里用同步的 adbProcess 执行命令。
// 现在改为基于 AdbClient 的异步请求器，**不再创建线程**：
//   * 保留了原有调用方式（initThread / start / signal_output），页面无需改动；
//   * 不再占用线程与 QProcess 资源，也不再需要 adbProcess；
//   * 命令顺序仍由 AdbClient 的请求队列保证。
// 类名与文件名沿用，避免大面积改动调用点。
class adbThread : public QObject
{
    Q_OBJECT
public:
    explicit adbThread(QObject* parent = nullptr);
    ~adbThread() override;

    void initThread(QString cmd, device dev);
    // explainKey 是【封闭集合】：#CPU# / #PERMISSIONGROUPS# / #INSTALL# / #SIDELOAD#，
    // 以及"分隔符"用法（例如 ": "，表示取该分隔符之后的内容作为说明）。
    // 传了 #XXX# 形式却不在集合里的键，deliver() 会给出 qWarning（见那里的说明）。
    void initThread(QString cmd, device dev, QString explainKey);
    void initThread_timer(QString cmd, device dev);

    // 直接给参数表（推荐）：命令由 AdbCmd 中的具名函数构造，
    // 不再经过"整条字符串 → 按空格/分隔符切分"这一步，避免切分规则带来的坑。
    void initThread(const QStringList &arguments, device dev);
    void initThread(const QStringList &arguments, device dev, QString explainKey);

    // 发起请求（原为 QThread::start）；完成后发出 signal_output，随后发出 finished
    void start();

    bool explain = false;
    QString command;            // 整条命令串形式（#INSTALL#/#SIDELOAD# 仍走这条路）
    QStringList m_arguments;    // 参数表形式（优先）
    QString explainKey;
    device dev;

signals:
    void signal_output(QString);
    // 请求结束（替代原 QThread::finished，供 deleteLater 使用）
    void finished();

private:
    // 构造与迁移前 adbProcess::run(command, dev) 等价的 argv
    QStringList buildArguments() const;
    void deliver(const QString &output);

    textExplainer m_explainer;
};

#endif // ADBTHREAD_H
