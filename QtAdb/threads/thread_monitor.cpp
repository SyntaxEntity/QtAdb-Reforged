#include "thread_monitor.h"
#include "../adbclient.h"

#include <QPointer>
#include <QTimer>
#include "textexplainer.h"
#include "adbcommands.h"

namespace {
constexpr int kSampleIntervalMs = 2000;   // 采样周期（与原来的定时器/线程一致）
}

thread_monitor::thread_monitor(device d)
    : dev(d)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(kSampleIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &thread_monitor::onTimer);
}

thread_monitor::~thread_monitor() = default;

void thread_monitor::start()
{
    if(m_timer->isActive())
    {
        return;
    }
    m_timer->start();
    onTimer();          // 立即采样一次，缩短首屏等待
}

void thread_monitor::stop()
{
    m_timer->stop();
    emit finished();
}

void thread_monitor::onTimer()
{
    if(m_samplePending)
    {
        return;         // 设备较慢时不堆积请求
    }
    m_samplePending = true;

    // 两条命令有依赖关系（第二个回调里要用到第一个的输出），因此嵌套回调。
    // 全部经由 AdbClient 异步执行，不阻塞界面。
    // Explain::No：周期采样属于取数命令，不弹"解释"提示框。
    QPointer<thread_monitor> self(this);
    // 必须用 runForDeviceWithCallback 带上 -s <序列号>：
    // 原实现用了不带设备的 runWithCallback，多设备时命令行没有 -s，
    // adb 会挑"默认设备"（多台时直接报 more than one device），
    // 于是 CPU/内存可能取自另一台设备。ADB 层所有设备相关请求都要带 -s。
    AdbClient::instance()->runForDeviceWithCallback(
        dev.addr, AdbCmd::cat("proc/stat"),
        [self](int, const QString &cpuNow, const QString &) {
            if(!self)
            {
                return;
            }
            QPointer<thread_monitor> nested = self;
            AdbClient::instance()->runForDeviceWithCallback(
                nested->dev.addr, AdbCmd::cat("/proc/meminfo"),
                [nested, cpuNow](int, const QString &memInfo, const QString &) {
                    if(!nested)
                    {
                        return;
                    }
                    nested->m_samplePending = false;
                    nested->computeAndEmit(cpuNow, memInfo);
                },
                AdbClient::kDefaultTimeoutMs,
                AdbClient::Explain::No);
        },
        AdbClient::kDefaultTimeoutMs,
        AdbClient::Explain::No);
}

void thread_monitor::computeAndEmit(const QString &cpuNow, const QString &memInfo)
{
    // 首次采样（或上次没有有效基准）只用于建立基准
    if(m_firstSample || m_cpuPrevious.isEmpty() || cpuNow.isEmpty() || memInfo.isEmpty())
    {
        m_cpuPrevious = cpuNow;
        m_firstSample = false;
        return;
    }

    QStringList listNow = cpuNow.split("\n")[0].split(" ");
    listNow.removeAll("");
    QStringList listPrev = m_cpuPrevious.split("\n")[0].split(" ");
    listPrev.removeAll("");

    m_cpuPrevious = cpuNow;     // 解析失败也要推进基准，否则会一直拿同一次采样比较

    // 只有形如 "cpu user nice system idle iowait irq softirq" 才能安全取到下标 7
    if(listNow.size() < 8 || listPrev.size() < 8)
    {
        return;
    }

    const int totalNow = listNow[1].toInt() + listNow[2].toInt() + listNow[3].toInt()
                       + listNow[4].toInt() + listNow[5].toInt() + listNow[6].toInt()
                       + listNow[7].toInt();
    const int totalPrev = listPrev[1].toInt() + listPrev[2].toInt() + listPrev[3].toInt()
                        + listPrev[4].toInt() + listPrev[5].toInt() + listPrev[6].toInt()
                        + listPrev[7].toInt();
    const int idleNow = listNow[4].toInt();
    const int idlePrev = listPrev[4].toInt();

    // 两次采样的总时间差为 0（计数器未推进或回绕）时不能做除法
    if(totalNow <= totalPrev)
    {
        return;
    }

    const float cpuUse = 1.0f - (float)(idleNow - idlePrev) / (float)(totalNow - totalPrev);

    // /proc/meminfo 至少需要前 3 行（MemTotal / MemFree / MemAvailable）
    const QStringList memLines = memInfo.split("\n");
    if(memLines.size() < 3)
    {
        return;
    }

    const int memTotal = m_explainer.get_words_before(m_explainer.get_words_after(memLines[0], "MemTotal:"), "kB").simplified().toInt();
    const int memAvailable = m_explainer.get_words_before(m_explainer.get_words_after(memLines[2], "MemAvailable:"), "kB").simplified().toInt();

    if(memTotal <= 0)
    {
        return;     // 解析失败时防止除零
    }
    emit signal_monitor(cpuUse, (float)(memTotal - memAvailable) / (float)memTotal);
}
