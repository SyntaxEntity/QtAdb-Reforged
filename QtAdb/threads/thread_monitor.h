#ifndef THREAD_MONITOR_H
#define THREAD_MONITOR_H

#include <QObject>
#include "../textexplainer.h"

class QTimer;

// 设备 CPU / 内存监视器（每 2 秒采样一次 /proc/stat 与 /proc/meminfo）。
//
// 演进历史：
//   最初 run() 是空的，采样由 GUI 线程里的 QTimer 同步驱动（每 2 秒阻塞界面两次）；
//   第四轮改为真正的后台线程；
//   现在 AdbClient 本身已是异步的，于是线程也不再需要 ——
//   直接由定时器发起异步请求，采样之间互不阻塞，也不额外占用线程与 QProcess。
class thread_monitor : public QObject
{
    Q_OBJECT

public:
    explicit thread_monitor(device dev);
    ~thread_monitor() override;

    void start();       // 开始周期采样（并立即采样一次）
    // 当前监视的设备地址（供调用方判断是否需要重建监视器）
    QString deviceAddress() const { return dev.addr; }
    void stop();        // 停止采样，随后发出 finished()

signals:
    void signal_monitor(float cpu_use, float mem_use);
    // 停止后发出（供 deleteLater 使用，替代原 QThread::finished）
    void finished();

private:
    void onTimer();
    void computeAndEmit(const QString &cpuNow, const QString &memInfo);

    device dev;
    QTimer *m_timer = nullptr;
    QString m_cpuPrevious;      // 上一次的 /proc/stat 采样
    bool m_firstSample = true;
    bool m_samplePending = false;   // 上一轮尚未返回时跳过本轮，避免堆积
    textExplainer m_explainer;
};

#endif // THREAD_MONITOR_H
