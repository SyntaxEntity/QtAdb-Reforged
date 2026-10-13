#include "adbthread.h"
#include "../adbclient.h"

#include <QPointer>
#include "textexplainer.h"

#include <QDebug>

adbThread::adbThread(QObject* parent)
    : QObject(parent)
{
}

adbThread::~adbThread() = default;

void adbThread::initThread(QString cmd, device d)
{
    command = cmd;
    m_arguments.clear();
    dev = d;
    explain = false;
}

void adbThread::initThread_timer(QString cmd, device d)
{
    command = cmd;
    m_arguments.clear();
    dev = d;
    explain = false;
}

void adbThread::initThread(QString cmd, device d, QString key)
{
    command = cmd;
    m_arguments.clear();
    dev = d;
    explain = true;
    explainKey = key;
}

void adbThread::initThread(const QStringList &arguments, device d)
{
    command.clear();
    m_arguments = arguments;
    dev = d;
    explain = false;
}

void adbThread::initThread(const QStringList &arguments, device d, QString key)
{
    command.clear();
    m_arguments = arguments;
    dev = d;
    explain = true;
    explainKey = key;
}

QStringList adbThread::buildArguments() const
{
    // 前置 "-s <serial>"（与迁移前的 adbProcess::run(command, dev) 一致）
    QStringList arguments;
    arguments << QStringLiteral("-s") << dev.addr;

    if(!m_arguments.isEmpty())
    {
        // 参数表形式：直接用，不经过字符串切分
        arguments += m_arguments;
        return arguments;
    }

    // 整条命令串形式（#INSTALL#/#SIDELOAD# 的含空格路径仍走这里）：
    // #INSTALL# / #SIDELOAD# 用 "~" 作分隔符，其余用 "'"；并弹出命令串里的 "adb"（下标 2）。
    const bool hasSpacedPath = (explainKey == QLatin1String("#INSTALL#")
                                || explainKey == QLatin1String("#SIDELOAD#"));
    const QString separator = hasSpacedPath ? QStringLiteral("~") : QStringLiteral("'");

    arguments += AdbClient::tokenizeCommand(command, separator);

    if(arguments.size() > 2)
    {
        arguments.removeAt(2);      // 弹出 "adb"
    }
    return arguments;
}

void adbThread::start()
{
    // 用 QPointer 保护：请求返回前对象可能已被销毁（例如页面已关闭）
    QPointer<adbThread> self(this);
    // Explain::No：本类对应原来的后台线程路径（adbProcess::thread = true），
    // 取数类命令不弹"解释"提示框。
    AdbClient::instance()->runWithCallback(
        buildArguments(),
        [self](int, const QString &output, const QString &) {
            if(self)
            {
                self->deliver(output);
            }
        },
        AdbClient::kDefaultTimeoutMs,
        AdbClient::Explain::No);
}

void adbThread::deliver(const QString &output)
{
    // 以下分支与迁移前 adbThread::run() 的输出处理保持一致
    if(explain == false)
    {
        emit signal_output(output.simplified());
    }
    else if(explainKey == QLatin1String("#CPU#"))
    {
        emit signal_output(m_explainer.explain_cpu_output(output));
    }
    else if(explainKey == QLatin1String("#PERMISSIONGROUPS#"))
    {
        emit signal_output(output);
    }
    else if(explainKey == QLatin1String("#INSTALL#") || explainKey == QLatin1String("#SIDELOAD#"))
    {
        emit signal_output(output);
    }
    else if(explain == true)
    {
        // 兜底：把 explainKey 当作"分隔符"用（例如屏幕分辨率那项传的是 ": "），
        // 取分隔符之后的内容作为要显示的说明。
        //
        // 但解释键的集合是【封闭】的：只有上面三个分支里的四种。若传进来的形如
        // #XXX# 却不在这四种里，多半是键写错了、或新增了键却忘了在这里登记 ——
        // 那种"静默不解释"最难排查，所以留一条警告。
        if(explainKey.startsWith(QLatin1Char('#')) && explainKey.endsWith(QLatin1Char('#'))
           && explainKey.size() > 2)
        {
            qWarning() << "未登记的解释键（该输出不会被解释）:" << explainKey;
        }
        emit signal_output(m_explainer.get_words_after(output, explainKey));
    }

    emit finished();
}
