#ifndef ADBCOMMANDS_H
#define ADBCOMMANDS_H

#include <QString>
#include <QStringList>
#include <initializer_list>

// adb 命令的"具名构造"。
//
// 背景：命令原先以整条字符串的形式散落在各页面里
// （例如 "adb shell settings get system vibrate_on"），由执行层按空格与分隔符切分。
// 这样有两个问题：
//   1. 命令拼写错误编译器看不出来，只能靠运行时发现；
//   2. 切分规则（' 与 ~ 两个分隔符）容易被忽略，含空格的参数尤其容易踩坑。
//
// 这里把重复出现的命令收敛成具名函数，**直接产出 argv**，交给 AdbClient / adbThread 使用。
// 参数的数量与位置由函数签名固定，写错会直接编译不过。
namespace AdbCmd {

// adb shell <args...>
inline QStringList shell(std::initializer_list<QString> list)
{
    QStringList arguments;
    arguments << QStringLiteral("shell");
    for(const QString &item : list)
    {
        arguments << item;
    }
    return arguments;
}

// adb shell settings get <ns> <key>
inline QStringList settingsGet(const QString &ns, const QString &key)
{
    return shell({QStringLiteral("settings"), QStringLiteral("get"), ns, key});
}

// adb shell settings put <ns> <key> <value>
inline QStringList settingsPut(const QString &ns, const QString &key, const QString &value)
{
    return shell({QStringLiteral("settings"), QStringLiteral("put"), ns, key, value});
}

// adb shell settings delete <ns> <key>
inline QStringList settingsDelete(const QString &ns, const QString &key)
{
    return shell({QStringLiteral("settings"), QStringLiteral("delete"), ns, key});
}

// adb shell wm size / wm size <WxH>
inline QStringList wmSize()
{
    return shell({QStringLiteral("wm"), QStringLiteral("size")});
}
inline QStringList wmSize(const QString &size)
{
    return shell({QStringLiteral("wm"), QStringLiteral("size"), size});
}

// adb shell wm density / wm density <dpi>
inline QStringList wmDensity()
{
    return shell({QStringLiteral("wm"), QStringLiteral("density")});
}
inline QStringList wmDensity(const QString &dpi)
{
    return shell({QStringLiteral("wm"), QStringLiteral("density"), dpi});
}

// adb shell getprop <key>
inline QStringList getprop(const QString &key)
{
    return shell({QStringLiteral("getprop"), key});
}

// adb shell pm list <what>
inline QStringList pmList(const QString &what)
{
    return shell({QStringLiteral("pm"), QStringLiteral("list"), what});
}

// adb shell pm <action> <package>
inline QStringList pmAction(const QString &action, const QString &packageName)
{
    return shell({QStringLiteral("pm"), action, packageName});
}

// adb shell dumpsys <what>
inline QStringList dumpsys(const QString &what)
{
    return shell({QStringLiteral("dumpsys"), what});
}

// adb shell cat /proc/cpuinfo
inline QStringList cpuInfo()
{
    return shell({QStringLiteral("cat"), QStringLiteral("/proc/cpuinfo")});
}

// adb shell cat <path>
inline QStringList cat(const QString &path)
{
    return shell({QStringLiteral("cat"), path});
}

// adb shell input keyevent <keyCode>
inline QStringList inputKeyevent(const QString &keyCode)
{
    return shell({QStringLiteral("input"), QStringLiteral("keyevent"), keyCode});
}

// adb reboot [mode]
inline QStringList reboot(const QString &mode = QString())
{
    QStringList arguments;
    arguments << QStringLiteral("reboot");
    if(!mode.isEmpty())
    {
        arguments << mode;
    }
    return arguments;
}

// adb shell reboot -p （关机）
inline QStringList powerOff()
{
    return shell({QStringLiteral("reboot"), QStringLiteral("-p")});
}

// adb -s <serial> ... 由 AdbClient::runForDevice() 负责前置，
// 因此上面的构造函数都不包含 -s/-d/-e 这类设备选择参数。

} // namespace AdbCmd

#endif // ADBCOMMANDS_H
