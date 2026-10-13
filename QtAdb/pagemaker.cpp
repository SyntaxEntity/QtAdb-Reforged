#include "pagemaker.h"
#include "adbclient.h"
#include <QPointer>
#include "textexplainer.h"
#include "adbcommands.h"

pageMaker::pageMaker()
{
    explainer = new textExplainer();
    registerPageFactories();
}

pageMaker::PageFactory pageMaker::gated(basePage* (pageMaker::*factory)(QWidget*, device))
{
    return [this, factory](QWidget *parent, device dev) -> basePage* {
        return isPageAvailable(dev.state) ? (this->*factory)(parent, dev)
                                          : createPage_disabled(parent);
    };
}

void pageMaker::registerPageFactories()
{
    // 与原来的 switch 逐条等价（见 §32 的状态×序号对照表）：
    //   key 0 仅"开机"用常规版、"REC"用恢复版，其余状态不可用；
    //   key 1/2/5 受 isPageAvailable() 门禁；key 3/4/6 不做门禁。
    m_pageFactories = {
        { 0, [this](QWidget *parent, device dev) -> basePage* {
              if(dev.state == DeviceState::Online)   { return createPage_devInfo(parent, dev); }
              if(dev.state == DeviceState::Recovery) { return createPage_devInfo_rec(parent, dev); }
              return createPage_disabled(parent);
          } },
        { 1, gated(&pageMaker::createPage_acvitator) },
        { 2, gated(&pageMaker::createPage_apps) },
        { 3, [this](QWidget *parent, device dev) -> basePage* { return createPage_devControl(parent, dev); } },
        { 4, [this](QWidget *parent, device dev) -> basePage* { return createPage_recovery(parent, dev); } },
        { 5, gated(&pageMaker::createPage_advanced) },
        { 6, [this](QWidget *parent, device dev) -> basePage* {
              Q_UNUSED(dev);
              return createPage_about(parent);
          } },
    };
}

pageMaker::~pageMaker()
{
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

// 同上，但走 AdbClient 回调路径（Recovery 状态的设备信息页用这条）：
// 先填"读取中…"占位，结果到达后回填 —— 原实现在创建页面时同步等待 adb，
// 设备挂起时界面会卡住（最长 20s），这也是 M1 异步化要解决的问题。
void pageMaker::addInfoItemAsync(basePage *page, device dev, const QString &title, const QString &icon,
                                 const QStringList &arguments, bool selectable,
                                 std::function<QString(const QString &)> transform)
{
    pageListItem *item = addPageItem(page, title, tr("读取中…"), icon, selectable);

    // 用 QPointer 保护：页面可能在请求返回前就被销毁
    QPointer<pageListItem> itemPtr(item);
    AdbClient::instance()->runForDeviceWithCallback(
        dev.addr, arguments,
        [itemPtr, title, transform](int, const QString &output, const QString &) {
            if(!itemPtr)
            {
                return;
            }
            // 未提供转换函数时用简化后的原始输出（与原实现一致）
            itemPtr->setText(title, transform ? transform(output) : output.simplified());
        });
}

// "标题 + 图标 + 异步查询结果自动填进说明"的条目：走 adbThread 的 signal_output 路径。
// 这段 9 行组合（initStruct → 设标题/图标 → 起线程 → 连接填充槽 → 入列表 → 释放结构体）
// 原来在 createPage_devInfo 里重复了 6 次。
void pageMaker::addInfoItem(basePage *page, device dev, const QString &title, const QString &icon,
                            const QStringList &arguments, const QString &explainKey, bool selectable)
{
    pageListItemStruct *entry = initStruct(page);
    entry->item->setText_title(title);
    entry->item->setPic(icon);
    if(explainKey.isEmpty())
    {
        entry->thread->initThread(arguments, dev);
    }
    else
    {
        entry->thread->initThread(arguments, dev, explainKey);
    }
    QEventLoop::connect(entry->thread, SIGNAL(signal_output(QString)),
                        entry->item, SLOT(slot_setText_profile(QString)));
    entry->thread->start();
    if(selectable)
    {
        entry->item->setSelectable();
    }
    page->addItemsToList(entry->item);
    delete entry;      // 结构体本身不再需要（item/thread 已各有宿主）
}

// 往页面上加一个条目。这段"新建 → 设标题/说明 → 设图标 →（可选）标记可点选 →
// 加入列表"的固定组合原来在 createPage_* 里重复了 40 次（每个条目 5～6 行）。
// 返回条目指针，便于调用方追加自己的配置（例如异步回填时需要持有它）。
// ⚠ 隐式约定：条目加入的先后顺序就是子页序号（第 i 个条目 ↔ 第 i 个子页），
//    调整顺序必须同步改 sonPageManager 的表；--factories 探针会逐条核对。
pageListItem *pageMaker::addPageItem(basePage *page, const QString &title, const QString &profile,
                                      const QString &icon, bool selectable)
{
    pageListItem *item = new pageListItem(page);
    item->setText(title, profile);
    item->setPic(icon);
    if(selectable)
    {
        item->setSelectable();
    }
    page->addItemsToList(item);
    return item;
}

basePage* pageMaker::createPageWithKey(int key, QWidget *parent, device dev)
{
    // 原实现开头有一段 `if(dev.addr.toInt() == -1) k = 6;`：设备序列号不是数字时
    // toInt() 返回 0，只有序列号字面量为 "-1" 才会命中，实际不可达（"没有设备"那条路径
    // 由 initBasePage() 直接传 key 6），属死代码，已删除。
    const auto it = m_pageFactories.constFind(key);
    if(it == m_pageFactories.constEnd())
    {
        // 保留"未知序号返回空"的原行为，但留下日志：调用方若直接 addWidget(nullptr)
        // 会崩，日志能让这种潜在问题一眼可见（界面只会传 0..6，正常不触发）。
        qWarning() << "未知的页面序号:" << key;
        return nullptr;
    }
    return it.value()(parent, dev);
}

basePage* pageMaker::createPage_devInfo(QWidget *parent, device dev)
{

    basePage *devInfo = new basePage(parent);
    devInfo->isBasePage = true;
    devInfo->whoYouAre("devInfo");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    devInfo->setEnableValue({2, 3});
    devInfo->setDev(dev);
    /***********************************/

    addInfoItem(devInfo, dev, tr("设备型号 | DeviceModel"),
              ":/ico/image/ico/profile-line.svg", AdbCmd::getprop("ro.product.model"));

    addInfoItem(devInfo, dev, tr("屏幕分辨率"),
              ":/ico/image/ico/fullscreen-line.svg", AdbCmd::wmSize(), ": ", true);

    //adb shell wm density
    addInfoItem(devInfo, dev, tr("屏幕密度 | DPI"),
              ":/ico/image/ico/grid-line.svg", AdbCmd::wmDensity(), QString(), true);

    //adb shell settings get secure android_id
    addInfoItem(devInfo, dev, "Android Id",
              ":/ico/image/ico/barcode-line.svg", AdbCmd::settingsGet("secure", "android_id"));

    //adb shell getprop ro.build.version.release
    addInfoItem(devInfo, dev, tr("安卓版本"),
              ":/ico/image/ico/android-line.svg", AdbCmd::getprop("ro.build.version.release"));

    addInfoItem(devInfo, dev, tr("处理器 | CPU"),
              ":/ico/image/ico/cpu-line.svg", AdbCmd::cpuInfo(), "#CPU#");

    /**************别删**********************/
    /****************别删***************/
    return devInfo;
}

basePage* pageMaker::createPage_devInfo_rec(QWidget *parent, device dev)
{

    basePage *devInfo = new basePage(parent);
    devInfo->isBasePage = true;
    devInfo->whoYouAre("devInfo");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    devInfo->setEnableValue({});
    devInfo->setDev(dev);
    /**/

    addInfoItemAsync(devInfo, dev, tr("设备型号 | DeviceModel"),
                     ":/ico/image/ico/profile-line.svg", { "shell", "getprop", "ro.product.model" });


    //adb shell getprop ro.build.version.release
    addInfoItemAsync(devInfo, dev, tr("Recovery 版本"),
                  ":/ico/image/ico/android-line.svg", { "shell", "getprop", "ro.build.version.release"});

    // /proc/cpuinfo 需要解析后再显示，这里传一个转换函数
    addInfoItemAsync(devInfo, dev, tr("处理器 | CPU"), ":/ico/image/ico/cpu-line.svg",
                     { "shell", "cat", "/proc/cpuinfo" }, false,
                     [](const QString &output) {
                         // textExplainer 是无状态的纯解析工具，就地构造即可
                         textExplainer parser;
                         return parser.explain_cpu_output(output);
                     });
    /**/
    return devInfo;
}

basePage* pageMaker::createPage_acvitator(QWidget *parent, device dev)
{
    basePage *activator = new basePage(parent);
    activator->isBasePage = true;
    activator->whoYouAre("activator");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    activator->setDev(dev);

    addPageItem(activator, tr("黑阈"), "adb -d shell sh /data/data/me.piebridge.brevent/brevent.sh",
              ":/activatorApps/image/activatorApps/brevent.png", true);

    addPageItem(activator, tr("冰箱 - 设备管理员"), "adb shell dpm set-device-owner com.catchingnow.icebox/.receiver.DPMReceiver",
              ":/activatorApps/image/activatorApps/icebox.png", true);

    addPageItem(activator, tr("冰箱 - 普通模式"), "adb shell sh /sdcard/Android/data/com.catchingnow.icebox/files/start.sh",
              ":/activatorApps/image/activatorApps/icebox.png", true);

    addPageItem(activator, tr("小黑屋 - 设备管理员"), "adb shell dpm set-device-owner web1n.stopapp/.receiver.AdminReceiver",
              ":/activatorApps/image/activatorApps/blackroom.png", true);

    addPageItem(activator, tr("小黑屋 - 麦克斯韦妖"), "adb shell sh /storage/emulated/0/Android/data/web1n.stopapp/files/starter.sh",
              ":/activatorApps/image/activatorApps/blackroom.png", true);

    addPageItem(activator, "Shizuku", "adb shell sh /sdcard/Android/data/moe.shizuku.privileged.api/start.sh",
              ":/activatorApps/image/activatorApps/shizuku.png", true);

    addPageItem(activator, tr("黑洞"), "adb shell dpm set-device-owner com.hld.apurikakusu/.receiver.DPMReceiver",
              ":/activatorApps/image/activatorApps/blackhole.png", true);

    addPageItem(activator, tr("太极"), "adb shell dpm set-device-owner me.weishu.exp/.DeviceAdmin",
              ":/activatorApps/image/activatorApps/taichi.png", true);

    addPageItem(activator, "Island", "adb -d shell dpm set-device-owner com.oasisfeng.island/.IslandDeviceAdminReceiver",
              ":/activatorApps/image/activatorApps/island.png", true);

    addPageItem(activator, "Scene", "adb shell sh /data/user/0/com.omarea.vtools/files/up.sh",
              ":/activatorApps/image/activatorApps/scene5.png", true);

    addPageItem(activator, tr("看帧数+"), "adb shell sh /sdcard/Download/watchfps/watchfps.sh",
              ":/activatorApps/image/activatorApps/watchfps.png", true);

    addPageItem(activator, tr("权限狗"), "adb shell sh /storage/emulated/0/Android/data/com.web1n.permissiondog/files/starter.sh",
              ":/activatorApps/image/activatorApps/permissiondog.png", true);

    addPageItem(activator, tr("雹"), "adb shell dpm set-device-owner com.aistra.hail/.receiver.DeviceAdminReceiver",
              ":/activatorApps/image/activatorApps/hail.png", true);

    addPageItem(activator, tr("BatteryGuru - 使用情况访问权限"), "adb shell pm grant com.paget96.batteryguru android.permission.PACKAGE_USAGE_STATS",
              ":/activatorApps/image/activatorApps/bg.png", true);

    pageListItem *bg_secure = new pageListItem(activator);   //15
    bg_secure->setText(tr("BatteryGuru - 修改安全系统设置"),"adb shell pm grant com.paget96.batteryguru android.permission.WRITE_SECURE_SETTINGS");
    bg_secure->setPic(":/activatorApps/image/activatorApps/bg.png");
    bg_secure->setSelectable();
    activator->addItemsToList(bg_secure);

    addPageItem(activator, tr("BatteryGuru - 读取 DUMP"), "adb shell pm grant com.paget96.batteryguru android.permission.DUMP",
              ":/activatorApps/image/activatorApps/bg.png", true);

    addPageItem(activator, tr("安装狮"), "adb shell dpm set-device-owner com.modosa.apkinstaller/.receiver.AdminReceiver",
              ":/activatorApps/image/activatorApps/apkinstaller.png", true);

    addPageItem(activator, "Dhizuku", "adb shell dpm set-device-owner com.rosan.dhizuku/.server.DhizukuDAReceiver",
              ":/activatorApps/image/activatorApps/default.gif", true);
    activator->enableAllItems();

    return activator;
}

basePage* pageMaker::createPage_apps(QWidget *parent, device dev)
{
    basePage *apps = new basePage(parent);
    apps->isBasePage = true;
    apps->whoYouAre("apps");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    apps->setDev(dev);

    addPageItem(apps, tr("安装应用"), "adb install <filename>",
              ":/ico/image/ico/install-line.svg", true);

    pageListItem *permissionGroups = new pageListItem(apps);
    // 第二参数是【显示文本】（pageListItem::setText 只写到标签上，不会被执行），保持原文案
    permissionGroups->setText(tr("已知权限组"),"adb shell pm list permission-groups");
    permissionGroups->setPic(":/ico/image/ico/about/information-line.svg");
    permissionGroups->setSelectable();
    apps->addItemsToList(permissionGroups);

    addPageItem(apps, tr("已知权限"), "adb shell pm list permissions",
              ":/ico/image/ico/about/information-line.svg", true);

    addPageItem(apps, tr("软件包"), "adb shell pm list packages",
              ":/ico/image/ico/android-line.svg", true);

    addPageItem(apps, tr("系统功能"), "adb shell pm list features",
              ":/ico/image/ico/tools-line.svg", true);

    addPageItem(apps, tr("库"), "adb shell pm list libraries",
              ":/ico/image/ico/stack-line.svg", true);

    addPageItem(apps, tr("用户"), "adb shell pm list users",
              ":/ico/image/ico/user-line.svg", true);
    apps->enableAllItems();

    return apps;
}

basePage* pageMaker::createPage_devControl(QWidget *parent, device dev)
{
    if(!isPageAvailable(dev.state))
    {
        basePage *devControl = new basePage(parent);
        devControl->isBasePage = true;
        devControl->whoYouAre("devControl");
        // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
        devControl->setDev(dev);

        addPageItem(devControl, tr("电源"), "adb reboot <n>",
                  ":/ico/image/ico/shut-down-line.svg", true);

        devControl->enableAllItems();
        return devControl;
    }
    else
    {
        basePage *devControl = new basePage(parent);
        devControl->isBasePage = true;
        devControl->whoYouAre("devControl");
        // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
        devControl->setDev(dev);

        addPageItem(devControl, tr("电源"), "adb reboot <n>",
                  ":/ico/image/ico/shut-down-line.svg", true);

        addPageItem(devControl, tr("按键模拟"), "adb shell input keyevent <key>",
                  ":/ico/image/ico/drag-move-line.svg", true);


        devControl->enableAllItems();
        return devControl;
    }
}


basePage* pageMaker::createPage_recovery(QWidget *parent, device dev)
{
    basePage *recovery = new basePage(parent);
    recovery->isBasePage = true;
    recovery->whoYouAre("recovery");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    recovery->setDev(dev);

    addPageItem(recovery, tr("侧载"), "adb sideload <filename>",
              ":/ico/image/ico/refund-line.svg", true);
    recovery->enableAllItems();

    return recovery;
}

basePage* pageMaker::createPage_advanced(QWidget *parent, device dev)
{
    basePage *advanced = new basePage(parent);
    advanced->isBasePage = true;
    advanced->whoYouAre("advanced");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    advanced->setDev(dev);

    addPageItem(advanced, tr("运行自定义命令"), "<customize command>",
              ":/ico/image/ico/code-s-slash-line.svg", true);

    addPageItem(advanced, tr("账户"), "adb shell dumpsys account",
              ":/ico/image/ico/user-settings-line.svg", true);

    addPageItem(advanced, tr("去除叹号"), "adb shell settings put global captive_portal_http_url <server avilable>",
              ":/ico/image/ico/signal-wifi-error-line.svg", true);

    addPageItem(advanced, tr("过渡动画"), "adb shell settings put global <animation type>",
              ":/ico/image/ico/donut-chart-line.svg", true);

    addPageItem(advanced, tr("状态栏与导航栏"), "adb shell settings put secure icon_blacklist <icon name>",
              ":/ico/image/ico/layout-top-2-line.svg", true);

    addPageItem(advanced, tr("分级调节震动强度"), "adb shell settings put system <vibrator> <value>",
              ":/ico/image/ico/volume-vibrate-line.svg", true);
    advanced->enableAllItems();

    //:/ico/image/ico/layout-top-2-line.svg

    return advanced;
}

basePage* pageMaker::createPage_about(QWidget *parent)
{
    about* abt = new about(parent);
    return abt;
}

basePage* pageMaker::createPage_disabled(QWidget *parent)
{
    basePage *disabled = new basePage(parent);
    disabled->isBasePage = true;
    disabled->whoYouAre("disabled");
    // 允许打开的子页序号集合（原实现是 int val[20]，见 basePage::setEnableValue）
    disabled->setEnableValue({});
    /**/

    pageListItem *productModel = new pageListItem(disabled);
    productModel->setText(tr("设备未开机"),tr("功能暂不可用"));
    productModel->setPic(":/ico/image/ico/close-circle-line.svg");
    disabled->addItemsToList(productModel);

    return disabled;
}

pageMaker::pageListItemStruct* pageMaker::initStruct(QWidget* parent)
{
    pageListItemStruct *itemStruct = new pageListItemStruct();
    itemStruct->item = new pageListItem();
    itemStruct->item->setParent(parent);
    // 让 item 做宿主：item 随页面销毁，thread 随之释放。
    // 原实现里结构体/item 的 thread 都没有宿主、调用方也不释放，
    // 每创建一个列表项就泄漏一个结构体 + 一个 textExplainer + 一个 adbThread
    //（而列表项在每次页面创建时都会重新建立）。
    itemStruct->thread = new adbThread(itemStruct->item);
    return itemStruct;
}
