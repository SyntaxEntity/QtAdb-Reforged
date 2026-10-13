#include "devicenames.h"

#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

// 设备序列号可能包含分隔符（例如网络设备的 "192.168.1.5:5555"），
// 用百分号编码作为 INI 的键，避免与 QSettings 的层级分隔符冲突。
QString keyFor(const QString &addr)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(addr));
}

QString filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if(!dir.isEmpty() && !QDir().mkpath(dir))
    {
        // 目录建不出来时，后面的写入必然失败（setName 会据 status() 返回 false）。
        // 这里先给出一条更准确的提示，省得只看到"保存失败"而不知原因。
        qWarning() << "无法创建配置目录（别名将无法保存）:" << dir;
    }
    return dir + QStringLiteral("/device-names.ini");
}

} // namespace

QString DeviceNames::storagePath()
{
    return filePath();
}

QString DeviceNames::nameFor(const QString &addr)
{
    if(addr.isEmpty())
    {
        return QString();
    }

    QSettings settings(filePath(), QSettings::IniFormat);

    // 注意：键里出现的 '%25' 是双重转义 —— 我们先把地址百分号编码，
    // QSettings 写入 INI 时又会把 '%' 再转义一次；读回时两层都会还原，
    // 因此往返是正确的（见 .dsh/probe/devicenames_probe.cpp 的跨进程验证）。
    const QString encoded = settings.value(QStringLiteral("names/") + keyFor(addr)).toString();
    if(!encoded.isEmpty())
    {
        return encoded;
    }

    // 兼容手工编辑的配置文件：也接受未编码的地址直接作为键
    return settings.value(QStringLiteral("names/") + addr).toString();
}

bool DeviceNames::setName(const QString &addr, const QString &name)
{
    if(addr.isEmpty())
    {
        return false;
    }

    QSettings settings(filePath(), QSettings::IniFormat);
    const QString key = QStringLiteral("names/") + keyFor(addr);
    const QString trimmed = name.trimmed();

    if(trimmed.isEmpty())
    {
        settings.remove(key);       // 清空输入即取消命名
    }
    else
    {
        settings.setValue(key, trimmed);
    }
    settings.sync();

    // 写盘失败（目录只读、磁盘满、权限不足……）必须让调用方知道，
    // 否则界面会显示"已保存"，而重启后别名消失了。
    return settings.status() == QSettings::NoError;
}

QHash<QString, QString> DeviceNames::allNames()
{
    QHash<QString, QString> result;

    QSettings settings(filePath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("names"));
    for(const QString &key : settings.childKeys())
    {
        const QString addr = QUrl::fromPercentEncoding(key.toLatin1());
        result.insert(addr, settings.value(key).toString());
    }
    settings.endGroup();

    return result;
}
