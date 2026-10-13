#ifndef PAGEMAKER_H
#define PAGEMAKER_H

#include <QCoreApplication>
#include <QHash>
#include <functional>   // Q_DECLARE_TR_FUNCTIONS / tr()
#include <QThread>
#include "basepage.h"
#include "pagelistitem.h"
#include "textexplainer.h"
#include "about.h"

#include "threads/adbthread.h"
#include "animationwidget.h"

class pageMaker
{
    // 本类不是 QObject，但需要 tr()（Qt 官方推荐做法）
    Q_DECLARE_TR_FUNCTIONS(pageMaker)

public:
    //pageMaker();
    pageMaker();
    ~pageMaker();

    textExplainer *explainer = nullptr;

    basePage *createPageWithKey(int key, QWidget *parent, device dev);


    basePage *createPage_devInfo(QWidget *parent, device dev);
    basePage *createPage_devInfo_rec(QWidget *parent, device dev);

    basePage *createPage_acvitator(QWidget *parent, device dev);
    basePage *createPage_apps(QWidget *parent,device dev);

    basePage *createPage_devControl(QWidget *parent,device dev);

    basePage *createPage_recovery(QWidget *parent, device dev);

    basePage *createPage_advanced(QWidget *parent, device dev);

    basePage *createPage_about(QWidget *parent);

    // 往页面上加一个条目。这段"新建 → 设标题/说明 → 设图标 →（可选）标记可点选 →
    // 加入列表"的固定组合在 createPage_* 里出现了近 40 次，收成一个函数。
    // 返回条目指针，便于调用方追加自己的配置。
    pageListItem *addPageItem(basePage *page, const QString &title, const QString &profile,
                              const QString &icon, bool selectable = true);

    // "标题 + 图标 + 异步查询结果自动填进说明"的条目（走 adbThread 的 signal_output 路径）。
    // createPage_devInfo 里这段 9 行组合出现了 6 次。
    void addInfoItem(basePage *page, device dev, const QString &title, const QString &icon,
                     const QStringList &arguments, const QString &explainKey = QString(),
                     bool selectable = false);

    // 同上，但走 AdbClient 回调路径（Recovery 状态的设备信息页用这条）。
    // 先填"读取中…"占位，结果到达后回填，避免创建页面时同步等待 adb。
    // transform 用于把原始输出转成要显示的说明；不给则用 output.simplified()。
    void addInfoItemAsync(basePage *page, device dev, const QString &title, const QString &icon,
                          const QStringList &arguments, bool selectable = false,
                          std::function<QString(const QString &)> transform = {});

    basePage *createPage_disabled(QWidget *parent);


    //activatorPage *createSonPage_activator(QWidget *parent, device dev);
    // 具名 struct（原为 typedef struct{...}pageListItemStruct;）：
    // 匿名 struct 一旦带上成员初始化器就不再是 C 兼容类型，
    // GCC 会报 -Wnon-c-typedef-for-linkage。
    struct pageListItemStruct
    {
        QWidget* parent = nullptr;
        pageListItem* item = nullptr;
        adbThread *thread = nullptr;
    };
private:
    /*youtPhone*/
    pageListItemStruct *initStruct(QWidget* parent);

    // 页面序号 → 工厂。原实现是一个 switch，且"设备状态不支持则返回不可用页"的判断
    // 在 3 个 case 里各写了一遍；改成表后：新增页面只加一行，门禁逻辑集中在 gated()。
    using PageFactory = std::function<basePage*(QWidget*, device)>;
    QHash<int, PageFactory> m_pageFactories;
    void registerPageFactories();
    // 包装一个工厂：设备状态不可用时统一返回"不可用"页
    PageFactory gated(basePage* (pageMaker::*factory)(QWidget*, device));



};

#endif // PAGEMAKER_H
