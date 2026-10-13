#include "textexplainer.h"
#include "uistyles.h"

textExplainer::textExplainer()
{

}

QList<device> textExplainer::getDevList_windows(QString tmpStr)
{
    QStringList tmpList;
    QList<device> devList;

    tmpList = tmpStr.split("\n");


    for(int i = 1; i < tmpList.size() ; i++)
    {
        tmpList[i] = tmpList[i].simplified();

        if (tmpList[i] == "" || tmpList[i] == "\r")
        {
            continue;
        }

        QStringList tmp = tmpList[i].split(" ");

        // 只取"序列号"这一段的半行（设备状态切换过程中真实 adb 会输出）
        // 原实现直接访问 tmp[1] 起，越界即触发 QList 断言并 abort；
        // 这里跳过而不是崩溃。
        if(tmp.size() < 2 || tmp[0].isEmpty())
        {
            continue;
        }

        device dev;
        dev.addr = tmp[0];
        dev.state = parseDeviceState(tmp[1]);

        // -l 的附加字段并非总是存在（不同 adb 版本 / 设备状态 / 半行输出），
        // 缺什么补 "unknown"，语义与原来的离线/未授权分支保持一致。
        const QString unknown = QStringLiteral("unknown");

        if(dev.state == DeviceState::Offline || dev.state == DeviceState::Unauthorized)
        {
            dev.transport_id   = tmp.size() > 2 ? tmp[2] : unknown;
            dev.device_product = unknown;
            dev.model          = unknown;
            dev.device_debug   = unknown;
        }
        else
        {
            dev.device_product = tmp.size() > 2 ? tmp[2] : unknown;
            dev.model          = tmp.size() > 3 ? tmp[3] : unknown;
            dev.device_debug   = tmp.size() > 4 ? tmp[4] : unknown;
            dev.transport_id   = tmp.size() > 5 ? tmp[5] : unknown;
        }

        devList.append(dev);
    }
    return devList;
}

QString textExplainer::get_words_after(QString str , QString key)
{
    QString text = str;
    int index = str.lastIndexOf(key);
    text = text.right(text.size()-index-key.size());
    text = text.simplified();
    return text;
}

QString textExplainer::get_words_before(QString str , QString key)
{
    QString text = str;
    int index = str.indexOf(key);
    text = text.left(index);
    text = text.simplified();
    return text;
}

QString textExplainer::explain_cpu_output(QString str)
{
    QString text = str;
    QStringList tmpList = text.split("\n");
    // 原实现直接取 tmpList[tmpList.count()-2]：
    // 当 adb 输出不足两行时（命令失败、设备离线/未授权、或仅一行且无结尾换行）
    // 下标为负，Qt 下会触发 QList::operator[] 断言并 abort（实测由 ASan 复现）。
    if(tmpList.size() < 2)
    {
        return QString();
    }
    QString tmpInfo = tmpList[tmpList.count()-2];
    tmpInfo = tmpInfo.simplified();

    return get_words_after(tmpInfo, ":");
}


namespace {

// 报错文本 → 提示 的对照表。**顺序即优先级**（先匹配到的先生效），与原来那条
// if/else if 链完全一致；`twoPatterns` 为真时 a、b 两个子串必须同时出现；
// `message` 为 nullptr 表示"匹配到但不提示"（原实现里那些空分支：命中即停止往下匹配）。
// 文案用 QT_TR_NOOP 标记，运行时再 tr()，因此仍可被 lupdate 提取。
struct ErrorHint
{
    const char *title;
    const char *message;
    bool twoPatterns;
    const char *a;
    const char *b;
};

const ErrorHint kErrorHints[] = {
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 应用未安装 \n · 应用安装后未首次运行"),        false, "No such file or directory",                   nullptr },
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 请先运行应用"),                       false, "Permission denied",                           nullptr },
        { nullptr,                                   nullptr,                                       false, "daemon not running",                          nullptr },
        { nullptr,                                   nullptr,                                       false, "daemon started successfully",                 nullptr },
        { nullptr,                                   nullptr,                                       false, "doesn't match this client",                   nullptr },
        { QT_TR_NOOP("提示"),                          QT_TR_NOOP(" · 请先选择安装包"),                      false, "install requires an argument",                nullptr },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 当前设备状态无法安装应用"),                 true, "failed to install",                           "cmd: not found" },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 无法执行该指令"),                      true, "Shell cannot change component state for",     "cmd: not found" },
        { QT_TR_NOOP("无法操作"),                        QT_TR_NOOP(" · 无法删除默认用户"),                     false, "couldn't remove user id 0",                   nullptr },
        { QT_TR_NOOP("未选择"),                         QT_TR_NOOP(" · 请先选择一个用户"),                     false, "no user id specified",                        nullptr },
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 请先清除所有用户并关闭多用户模式  \n · 可在「软件包管理器」-「用户」中查看并删除用户  \n"), false, "Not allowed to set the device owner because there are already several users on the device", nullptr },
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 设备管理员应用未安装"),                   false, "Unknown admin",                               nullptr },
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 请先退出所有账户  \n · 可在「其它」-「账户」中查看设备中的账户  \n · 或在设备设置中查看并退出账户  \n"), false, "Not allowed to set the device owner because there are already some accounts on the device", nullptr },
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 存在冲突的设备管理员应用"),                 false, "Trying to set the device owner, but device owner is already set.", nullptr },
        { QT_TR_NOOP("失败"),                          QT_TR_NOOP(" · 请输入配对码"),                       false, "No pairing code provided",                    nullptr },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 当前设备状态无法执行该命令\n · 请确认当前设备状态（开机、REC等）能否执行该操作"), false, "cmd: not found",                              nullptr },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 当前设备状态无法执行该命令\n · 请确认当前设备状态（开机、REC等）能否执行该操作"), false, "dpm: not found",                              nullptr },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 当前设备状态无法执行该命令\n · 请确认当前设备状态（开机、REC等）能否执行该操作"), false, "am: not found",                               nullptr },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 当前设备状态无法执行该命令\n · 请确认当前设备状态（开机、REC等）能否执行该操作"), false, "input: not found",                            nullptr },
        { QT_TR_NOOP("提示"),                          QT_TR_NOOP(" · 程序上次运行时异常退出，您可以反馈此问题"),         false, "could not start server",                      nullptr },
        { QT_TR_NOOP("完成"),                          QT_TR_NOOP(" · 成功\n · 你之所以会看到这条提示，是因为ADB本身将成功的消息以错误输出方式输出\n · 您无需关注此错误"), false, "Done",                                        nullptr },
        { QT_TR_NOOP("错误"),                          QT_TR_NOOP(" · 参数有误"),                         false, "Bad arguments",                               nullptr },
};

} // namespace

bool textExplainer::explainError(QString err)
{
    err = err.simplified();

    // 原来的 19 个分支是一条长 if/else if 链；改成查表后，新增一条提示只需加一行数据。
    for(const ErrorHint &hint : kErrorHints)
    {
        const bool hit = hint.twoPatterns
                ? (err.contains(QLatin1String(hint.a)) && err.contains(QLatin1String(hint.b)))
                : err.contains(QLatin1String(hint.a));
        if(!hit)
        {
            continue;
        }
        if(hint.message != nullptr)
        {
            showMsgBox(tr(hint.title), tr(hint.message));
        }
        return true;    // 命中即结束（空分支同样不再往下匹配），与原来一致
    }

    // 未命中任何已知形态（原实现的最后一条 else）
    showMsgBox(tr("错误"), tr(" · 未知错误！\n") + err + tr("\n · 若此处出现并不完整的报错或一大串包含 '.android.os'、'java' 等字样的错误，您可以视情况忽视它，并查阅另一条报错\n"));
    return true;
}


bool textExplainer::explainOutput(QString op, bool thread)
{
    op = op.simplified();

    /*普通*/
    if(op.contains("No such file or directory") && thread == false)
    {
        showMsgBox(tr("失败"),tr(" · 应用未安装 \n · 应用安装后未首次运行"));
    }
    return true;
}

void textExplainer::showMsgBox(QString title, QString msg)
{
    if(!thread)
    {
        // 原实现每次 new 一个 QMessageBox 且从不释放（弹一次泄漏一个顶层窗口）；
        // 这里先关闭上一个，并让新窗口在关闭时自毁。
        if(msgBox != nullptr)
        {
            msgBox->close();        // 关闭即删除（见下面的 WA_DeleteOnClose）
            msgBox = nullptr;
        }

        msgBox = new QMessageBox;
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->setWindowIcon(QIcon(":/ico/image/ico/link.svg"));
        msgBox->setStyleSheet(UiStyles::kPlainDialog);
        msgBox->setWindowTitle(title);
        msgBox->setText(msg);
        msgBox->addButton(" ✓ ", QMessageBox::AcceptRole);
        msgBox->show();
    }
}

DeviceState parseDeviceState(const QString &state)
{
    if(state == "device")            return DeviceState::Online;
    if(state == "offline")           return DeviceState::Offline;
    if(state == "recovery")          return DeviceState::Recovery;
    if(state == "sideload")          return DeviceState::Sideload;
    if(state == "unauthorized")      return DeviceState::Unauthorized;
    return DeviceState::Unknown;
}

QString stateLabel(DeviceState state)
{
    switch(state)
    {
    case DeviceState::Online:       return QCoreApplication::translate("DeviceState", "[开机]");
    case DeviceState::Offline:      return QCoreApplication::translate("DeviceState", "[未响应]");
    case DeviceState::Recovery:     return QStringLiteral("[REC]");
    case DeviceState::Sideload:     return QCoreApplication::translate("DeviceState", "[侧载]");
    case DeviceState::Unauthorized: return QCoreApplication::translate("DeviceState", "[未授权]");
    case DeviceState::Unknown:      break;
    }
    return QCoreApplication::translate("DeviceState", "[未知]");
}

QStringList textExplainer::explainPermissionGroups(QString s)
{
    QStringList list;
    list = s.split("\n");

    return list;
}

QStringList textExplainer::explainAccounts(QString s)
{
    QStringList final;
    QStringList classific_as_users;
    classific_as_users = s.split("}:");

    for(int i = 1; i<classific_as_users.count(); i++)
    {
        QStringList tmpList;
        tmpList = get_words_before(classific_as_users[i],"AccountId,").split("Account ");
        if(!tmpList.isEmpty())
        {
            QString num = QString::number(i-1);
            tmpList[0] = tmpList[0].replace("Accounts:", tr("属于用户") + num + tr("的账户数量为："));
        }

        final.append(tmpList);
    }
    return final;
}
