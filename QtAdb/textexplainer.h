#ifndef TEXTEXPLAINER_H
#define TEXTEXPLAINER_H
#include <QCoreApplication>   // Q_DECLARE_TR_FUNCTIONS / tr()
#include <QString>
#include <QList>
#include <QDebug>
#include <QStringList>
#include <QMessageBox>
#include <QPointer>
#include <QIcon>
#include <noticewidget.h>

// 设备状态。
// 原实现用中文字符串（"[开机]"/"[未响应]"/"[REC]"/"[侧载]"/"[未授权]"/"[未知]"）在逻辑里比较，
// 既容易写错、编译器也无法检查（多处是 4 个字符串的并联判断，重复了 4 遍）。
// 这里改为枚举：逻辑用枚举比较，界面文本统一由 stateLabel() 提供。
enum class DeviceState {
    Online,         // adb 输出 "device"       界面显示 "[开机]"
    Offline,        // adb 输出 "offline"      界面显示 "[未响应]"
    Recovery,       // adb 输出 "recovery"     界面显示 "[REC]"
    Sideload,       // adb 输出 "sideload"     界面显示 "[侧载]"
    Unauthorized,   // adb 输出 "unauthorized" 界面显示 "[未授权]"
    Unknown         // 其它                    界面显示 "[未知]"
};

// 解析 adb devices 输出里的状态字段
DeviceState parseDeviceState(const QString &state);
// 状态的界面显示文本（保持原有中文标签不变）
QString stateLabel(DeviceState state);

// 该状态下是否可以使用功能页面。
// 原实现的判定是"未响应 / REC / 侧载 / 未知 → 禁用页面"，其余（开机、未授权）可用；
// 这里保持同一语义，只是换成枚举表达。
inline bool isPageAvailable(DeviceState state)
{
    return state != DeviceState::Offline
        && state != DeviceState::Recovery
        && state != DeviceState::Sideload
        && state != DeviceState::Unknown;
}

struct device
{
    QString addr;
    DeviceState state = DeviceState::Unknown;   // 由 parseDeviceState() 解析得到
    QString device_product;
    QString model;
    QString device_debug;
    QString transport_id;
};

class textExplainer
{
    // 本类不是 QObject，但需要 tr()：用这个宏获得静态 tr()（Qt 官方推荐做法）
    Q_DECLARE_TR_FUNCTIONS(textExplainer)

public:
    textExplainer();

    QList<device> getDevList_windows(QString tmpStr);
    QString get_words_after(QString str , QString key);
    QString get_words_before(QString s, QString k);
    QString explain_cpu_output(QString str);

    void showMsgBox(QString title,QString msg);

    bool explainError(QString err);
    bool explainOutput(QString op, bool thread);

    // 提示框：关闭时自毁，QPointer 会自动置空（原实现每次 new 且从不释放）
    QPointer<QMessageBox> msgBox = nullptr;
    bool thread = false;

    QStringList explainPermissionGroups(QString s);
    QStringList explainAccounts(QString s);

private:

public slots:
};

#endif // TEXTEXPLAINER_H
