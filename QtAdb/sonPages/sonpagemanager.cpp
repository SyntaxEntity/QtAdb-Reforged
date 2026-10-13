#include "sonpagemanager.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uistyles.h"

sonPageManager::sonPageManager()
{
    explainer = new textExplainer();
}

sonPageManager::~sonPageManager()
{
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

namespace {

// 子页工厂的两种形态（本文件的表里每一行都只是"new + 可选 setDev"）：
//   makeSonPageWithDev<T>：构造后调用 setDev(dev)，绝大多数页面如此；
//   makeSonPage<T>       ：只构造 —— advanced 的第一项就是这个形态（它原来那行
//                           setDev 是注释掉的，必须保持"不调用"）。
// 有了它们，表里每一行只剩"序号 + 类名"，新增子页不必再抄那 3 行样板。
template <typename T>
animationWidget *makeSonPageWithDev(QWidget *parent, device dev)
{
    T *page = new T(parent);
    page->setDev(dev);
    return page;
}

template <typename T>
animationWidget *makeSonPage(QWidget *parent, device dev)
{
    Q_UNUSED(dev);
    return new T(parent);
}

} // namespace

animationWidget* sonPageManager::selector(QWidget *parent , QString parentName, int key, device dev)
{
    // 父页名 → 子页工厂。原实现是一串 if-else，且未知父页名会 `throw "…"` ——
    // 抛的是 C 字符串、无人接住，等于直接 terminate。这里改为查表并对未知名返回 nullptr，
    // 调用方 basePage::slot_createSonPage() 本来就检查了 nullptr。
    // ⚠ 隐式约定：主页面里条目的先后顺序就是子页序号（第 i 个条目 ↔ 第 i 个子页）。
    //    调整条目顺序必须同步改这里的表，否则会"点 A 打开 B"且不报错。
    //    自测：QtAdbAsan --factories 的"条目↔子页 交叉校验"会逐条核对（36 组）。
    using Factory = animationWidget* (sonPageManager::*)(QWidget*, int, device);
    static const QHash<QString, Factory> factories = {
        { QStringLiteral("devInfo"),    &sonPageManager::createSonPageFor_devInfo    },
        { QStringLiteral("activator"),  &sonPageManager::createSonPageFor_activator  },
        { QStringLiteral("apps"),       &sonPageManager::createSonPageFor_apps       },
        { QStringLiteral("devControl"), &sonPageManager::createSonPageFor_devControl },
        { QStringLiteral("recovery"),   &sonPageManager::createSonPageFor_recovery   },
        { QStringLiteral("advanced"),   &sonPageManager::createSonPageFor_advanced   },
    };

    const auto it = factories.constFind(parentName);
    if(it == factories.constEnd())
    {
        qWarning() << "未知的父页面名，无法创建子页:" << parentName;
        return nullptr;
    }
    return (this->*(it.value()))(parent, key, dev);
}

animationWidget* sonPageManager::createSonPageFor_devInfo(QWidget *parent, int key, device dev)
{
    // 子页序号（0 基）→ 页面工厂。
    // 原实现是 switch(key + 1) 的逐个 case：新增一项既要改分支又要记住 +1 的偏移；
    // 改成表后新增只加一行；case 体原样作为 lambda 保存（文案仍在 tr() 中，可被 lupdate 提取）。
    using SonPageFactory = std::function<animationWidget*(QWidget*, device)>;
    static const QHash<int, SonPageFactory> pages = {
        {  1, makeSonPageWithDev<sp_wmsize> },
        {  2, makeSonPageWithDev<sp_dpiChanger> },
    };

    const auto it = pages.constFind(key);
    return it == pages.constEnd() ? nullptr : it.value()(parent, dev);
}

animationWidget* sonPageManager::createSonPageFor_activator(QWidget *parent, int key, device dev)
{
    // 18 个激活器页**只有四项数据不同**：图标样式、标题、说明、要执行的命令。
    // 原实现是 18 个近乎逐字重复的 case（每个 11 行）；现在改成数据表 + 一个通用构造函数，
    // 新增激活器只需加一行数据，不必再抄一遍构造流程。
    // 标题/说明用 QT_TR_NOOP 标记：文案仍可被 lupdate 提取，运行时经 tr() 翻译。
    struct ActivatorEntry
    {
        const char *icon;
        const char *title;
        const char *profile;
        QStringList command;
    };
    static const QHash<int, ActivatorEntry> entries = {
        {  0, { "border:0px;image:url(:/activatorApps/image/activatorApps/brevent.png);",
            QT_TR_NOOP("激活 黑阈"),
            QT_TR_NOOP("黑阈™不需要 root 也不需要解锁，能有效防止恶意应用在后台运行，从而让手机流畅省电。\n黑阈主要采取以下手段来限制应用后台：\n1. 停用状态。应用处于停用状态时，不会收到 Android 系统及 GMS 发送的广播，从而避免“被动运行”。\n2. 待机。待机以后，应用无法访问网络，执行任务与同步等。这减少Android 系统被动运行该应用，同时即使运行也减少其网络活动。\n"),
            AdbCmd::shell({"sh", "/data/data/me.piebridge.brevent/brevent.sh"}) } },
        {  1, { UiStyles::kActivatorTile_icebox,
            QT_TR_NOOP("激活 冰箱 - 设备管理员"),
            QT_TR_NOOP("使用**设备管理器**模式激活冰箱前，请按下列步骤操作：\n1. 确保手机上已安装冰箱，删除屏幕锁，索尼手机取出 SIM 卡，MIUI 开启「USB 调试（安全设置）」并关闭「MIUI 优化」\n2. **请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。\n3. **请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。\n"),
            AdbCmd::shell({"dpm", "set-device-owner", "com.catchingnow.icebox/.receiver.DPMReceiver"}) } },
        {  2, { UiStyles::kActivatorTile_icebox,
            QT_TR_NOOP("激活 冰箱 - 普通模式"),
            QT_TR_NOOP("如果您未能成功配置设备管理员模式的冰箱，则可以考虑使用普通 ADB 模式。\n- 优点：无需删除帐号等复杂步骤，直接使用\n- 缺点：每次重启手机后必须要重新激活，否则无法冻结解冻 App\n- 激活后需保持 USB 调试始终打开，设置 USB 连接设置为「仅充电」（MIUI 需开启「USB 调试（安全设置）」）\n"),
            AdbCmd::shell({"sh", "/sdcard/Android/data/com.catchingnow.icebox/files/start.sh"}) } },
        {  3, { UiStyles::kActivatorTile_blackroom,
            QT_TR_NOOP("激活 小黑屋 - 设备管理员"),
            QT_TR_NOOP("使用**设备管理器**模式激活小黑屋前，注意以下事项：\n1. 打开手机开发者选项与 USB 调试，将 USB 连接设置为「仅充电」（小米手机需开启开发者选项「USB 调试（安全设置）」）。\n2. **请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。\n3. **请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。\n"),
            AdbCmd::shell({"dpm", "set-device-owner", "web1n.stopapp/.receiver.AdminReceiver"}) } },
        {  4, { UiStyles::kActivatorTile_blackroom,
            QT_TR_NOOP("激活 小黑屋 - 麦克斯韦妖"),
            QT_TR_NOOP("麦克斯韦妖是小黑屋的一款冻结模式。借助它，小黑屋可以免 Root 权限冻结应用程序。  \n麦克斯韦妖引擎与 ROM 的兼容性很好，不会出现设备管理员模式与 ROM 之间的一些兼容性问题。  \n**麦克斯韦妖优点**  \n- 无需移除账号与用户等操作，激活成功率极高。  \n- 冻结速度快，点击即冻结。  \n\n**麦克斯韦妖缺点**  \n- 关机后即失效，因此每次开机需要重新激活。若不重新激活将无法打开已冻结 app。  \n"),
            AdbCmd::shell({"sh", "/storage/emulated/0/Android/data/web1n.stopapp/files/starter.sh"}) } },
        {  5, { "border:0px;image:url(:/activatorApps/image/activatorApps/shizuku.png);",
            QT_TR_NOOP("激活 Shizuku"),
            QT_TR_NOOP("Android 11 及以上支持无线调试，您可以直接在设备上启动 Shizuku。  \n对于未 root 设备，则需要借助 adb 启动。  \n**请注意**  \n- 设备重新启动后需要再次连接电脑。  \n在一些定制系统上 Shizuku 可能会随机停止，此时:  \n\n- 不要关闭“USB 调试”及“开发者选项”  \n- 保证在连接电脑期间 USB 使用模式不变。通常的做法是在“开发者选项”中将 USB 使用模式改为“仅充电”  \n- 连接 USB 后，若弹出类似“是否允许访问文件”的对话框，请不要点击它，因为点击后 USB 使用模式会发生变化。  \n- Sony 设备不要点击连接 USB 后弹出的对话框。  \n"),
            AdbCmd::shell({"sh", "/sdcard/Android/data/moe.shizuku.privileged.api/start.sh"}) } },
        {  6, { "border:0px;image:url(:/activatorApps/image/activatorApps/blackhole.png);",
            QT_TR_NOOP("激活 黑洞"),
            QT_TR_NOOP("**黑洞**已在各大平台下架且无法找到帮助文档。  \n但您仍可通过 QtAdb 激活它的设备管理员模式。  \n**请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。  \n**请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。  \n"),
            AdbCmd::shell({"dpm", "set-device-owner", "com.hld.apurikakusu/.receiver.DPMReceiver"}) } },
        {  7, { "border:0px;image:url(:/activatorApps/image/activatorApps/taichi.png);",
            QT_TR_NOOP("激活 太极"),
            QT_TR_NOOP("**阴阳之门的用途**  \n1. 可以大幅优化 太极 创建应用和升级应用的流程；不再需要手动确认，整个流程可以一气呵成。  \n2. 额外提供类似冰箱的冻结功能；可以冻结和隐藏应用。  \n\n**注意事项**  \n1. 首先确保您已安装太极。  \n2. **请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。  \n3. **请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。  \n"),
            AdbCmd::shell({"dpm", "set-device-owner", "me.weishu.exp/.DeviceAdmin"}) } },
        {  8, { "border:0px;image:url(:/activatorApps/image/activatorApps/island.png);",
            QT_TR_NOOP("激活 炼妖壶Island - 上帝模式"),
            QT_TR_NOOP("激活 Island 『上帝模式』：（可冻结任何应用，而不仅限于 Island 空间内的应用）  \n1. **请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。  \n2. **请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。  \n3. 如果你已经创建了 Island 空间（Work profile），也必须先在 Island - Settings - Setup 中移除。在激活『上帝模式』后，你可以再重新创建 Island 空间。  \n"),
            AdbCmd::shell({"dpm", "set-device-owner", "com.oasisfeng.island/.IslandDeviceAdminReceiver"}) } },
        {  9, { "border:0px;image:url(:/activatorApps/image/activatorApps/scene5.png);",
            QT_TR_NOOP("激活 Scene5"),
            QT_TR_NOOP("激活 Scene5 的 ADB 模式  \n可以使用大部分查看功能和部分修改功能  \n"),
            AdbCmd::shell({"sh", "/sdcard/Android/data/com.omarea.vtools/up.sh"}) } },
        { 10, { "border:0px;image:url(:/activatorApps/image/activatorApps/watchfps.png);",
            QT_TR_NOOP("激活 看帧数+"),
            QT_TR_NOOP("激活 看帧数+ 的 ADB 模式  \n在屏幕上直观的显示当前帧数。  \n方便、简单、美观。  \n不用复杂的操作，没有任何风险  \n"),
            AdbCmd::shell({"sh", "/sdcard/Download/watchfps/watchfps.sh"}) } },
        { 11, { "border:0px;image:url(:/activatorApps/image/activatorApps/permissiondog.png);",
            QT_TR_NOOP("激活 权限狗"),
            QT_TR_NOOP("激活 权限狗 的 ADB 模式  \n**什么是 AppOps?**  \nAppOps 是 Android 系统内存在的一个权限管理功能.与大家熟知的 运行时权限 不同,它至今没有开放管理入口,因此这项功能鲜为人知.  \n正因为包括许多开发者的人们不了解 AppOps,应用通常不会检测 AppOps 设置,我们得以借助它在不会被应用发现的条件下拒绝应用申请的权限.(例如某应用不授权定位权限就无法使用,我们可以设置此应用的 AppOps 定位权限为忽略.这样我们就可以在拒绝定位权限后正常使用此应用了.)  \n**权限狗又是什么?**  \n我们知道,Android 尚未开放 AppOps 管理功能.而权限狗就是一个能够管理 AppOps 设置的第三方工具.  \n"),
            AdbCmd::shell({"sh", "/storage/emulated/0/Android/data/com.web1n.permissiondog/files/starter.sh"}) } },
        { 12, { "border:0px;image:url(:/activatorApps/image/activatorApps/hail.png);",
            QT_TR_NOOP("激活 雹"),
            QT_TR_NOOP("激活 雹 的 「设备所有者 - 隐藏」 模式  \n此模式通过将雹设置为设备所有者 (Device Owner)，调用```DevicePolicyManager.setApplicationHidden```方法隐藏应用。  \n**请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。  \n**请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。  \n"),
            AdbCmd::shell({"dpm", "set-device-owner", "com.aistra.hail/.receiver.DeviceAdminReceiver"}) } },
        { 13, { UiStyles::kActivatorTile_bg,
            QT_TR_NOOP("为 BatteryGuru 授予 使用情况访问 权限"),
            QT_TR_NOOP("授予 BatteryGuru 使用情况访问权限  \n在某些定制 ROM 上可能不会生效  \n授予应用程序权限，否则 BatteryGuru 的一些功能将无法工作。  \n"),
            AdbCmd::shell({"pm", "grant", "com.paget96.batteryguru", "android.permission.PACKAGE_USAGE_STATS"}) } },
        { 14, { UiStyles::kActivatorTile_bg,
            QT_TR_NOOP("为 BatteryGuru 授予 修改安全系统设置 权限"),
            QT_TR_NOOP("授予 BatteryGuru 修改安全系统设置权限  \n在某些定制 ROM 上可能不会生效  \n用于更改一些系统设置，例如 Android Doze 参数或系统节电状态  \n也可以使用 Root  \n"),
            AdbCmd::shell({"pm", "grant", "com.paget96.batteryguru", "android.permission.WRITE_SECURE_SETTINGS"}) } },
        { 15, { UiStyles::kActivatorTile_bg,
            QT_TR_NOOP("为 BatteryGuru 授予 DUMP 权限"),
            QT_TR_NOOP("授予 BatteryGuru 读取 DUMP 权限  \n在某些定制 ROM 上可能不会生效  \n允许应用检查系统内部状态的权限，用来检测设置状态（例如空闲和 Doze 模式）。  \n也可以使用 Root  \n"),
            AdbCmd::shell({"pm", "grant", "com.paget96.batteryguru", "android.permission.DUMP"}) } },
        { 16, { "border:0px;image:url(:/activatorApps/image/activatorApps/apkinstaller.png);",
            QT_TR_NOOP("激活 安装狮"),
            QT_TR_NOOP("激活 安装狮 - DPM  \n安装狮的主要功能是安装APK和卸载应用  \n除了原生 Android 外的绝大部分定制系统由于系统的限制，无法将本应用设置为默认安装与卸载应用,而且发起安装时这些系统会直接让系统默认的软件包安装程序来处理，也没有很好的解决办法  \n这种情况推荐用本应用内的 选择文件安装 功能或用一些支持选择打开方式的文件管理器等来发起安装  \n安装前请关闭Google Play的安全扫描，请在开发者选项打开USB安装、关闭监控ADB安装应用等等以防止安装失败。  \n**请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。  \n**请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。  \n"),
            AdbCmd::shell({"dpm", "set-device-owner", "com.modosa.apkinstaller/.receiver.AdminReceiver"}) } },
        { 17, { "border:0px;image:url(:/activatorApps/image/activatorApps/default.gif);",
            QT_TR_NOOP("激活 Dhizuku"),
            QT_TR_NOOP("激活 Dhizuku  \nDhizuku 可被用于共享设备所有者权限(DeviceOwner)。  \n同一个系统只能存在一个应用被设置为设备所有者，授予后该应用不能卸载。  \n**请退出所有帐号**，如小米帐号、华为帐号、Google 帐号等，激活后可以再重新登录。可前往QtAdb的[其它] -> [帐户]查看并删除账号。  \n**请删除所有用户**，包括手机分身等，可前往QtAdb的[软件包管理器] -> [用户]中查看现有用户。  \n"),
            AdbCmd::shell({"dpm", "set-device-owner", "com.rosan.dhizuku/.server.DhizukuDAReceiver"}) } },
    };

    const auto it = entries.constFind(key);
    if(it == entries.constEnd())
    {
        return nullptr;
    }

    sp_activator *page = new sp_activator(parent);
    page->setDev(dev);
    page->setUp(it->icon, tr(it->title), tr(it->profile), it->command);
    return page;
}

animationWidget* sonPageManager::createSonPageFor_apps(QWidget *parent, int key, device dev)
{
    // 子页序号（0 基）→ 页面工厂。
    // 原实现是 switch(key + 1) 的逐个 case：新增一项既要改分支又要记住 +1 的偏移；
    // 改成表后新增只加一行；case 体原样作为 lambda 保存（文案仍在 tr() 中，可被 lupdate 提取）。
    using SonPageFactory = std::function<animationWidget*(QWidget*, device)>;
    static const QHash<int, SonPageFactory> pages = {
        {  0, makeSonPageWithDev<sp_installer> },
        {  1, makeSonPageWithDev<sp_permissionGroups> },
        {  2, makeSonPageWithDev<sp_permissions> },
        {  3, makeSonPageWithDev<sp_packages> },
        {  4, makeSonPageWithDev<sp_features> },
        {  5, makeSonPageWithDev<sp_libraries> },
        {  6, makeSonPageWithDev<sp_users> },
    };

    const auto it = pages.constFind(key);
    return it == pages.constEnd() ? nullptr : it.value()(parent, dev);
}

animationWidget* sonPageManager::createSonPageFor_devControl(QWidget *parent, int key, device dev)
{
    // 子页序号（0 基）→ 页面工厂。
    // 原实现是 switch(key + 1) 的逐个 case：新增一项既要改分支又要记住 +1 的偏移；
    // 改成表后新增只加一行；case 体原样作为 lambda 保存（文案仍在 tr() 中，可被 lupdate 提取）。
    using SonPageFactory = std::function<animationWidget*(QWidget*, device)>;
    static const QHash<int, SonPageFactory> pages = {
        {  0, makeSonPageWithDev<sp_devControl_power> },
        {  1, makeSonPageWithDev<btnEmulator> },
    };

    const auto it = pages.constFind(key);
    return it == pages.constEnd() ? nullptr : it.value()(parent, dev);
}

animationWidget* sonPageManager::createSonPageFor_recovery(QWidget *parent, int key, device dev)
{
    // 子页序号（0 基）→ 页面工厂。
    // 原实现是 switch(key + 1) 的逐个 case：新增一项既要改分支又要记住 +1 的偏移；
    // 改成表后新增只加一行；case 体原样作为 lambda 保存（文案仍在 tr() 中，可被 lupdate 提取）。
    using SonPageFactory = std::function<animationWidget*(QWidget*, device)>;
    static const QHash<int, SonPageFactory> pages = {
        {  0, makeSonPageWithDev<sp_recovery> },
    };

    const auto it = pages.constFind(key);
    return it == pages.constEnd() ? nullptr : it.value()(parent, dev);
}

animationWidget* sonPageManager::createSonPageFor_advanced(QWidget *parent, int key, device dev)
{
    // 子页序号（0 基）→ 页面工厂。
    // 原实现是 switch(key + 1) 的逐个 case：新增一项既要改分支又要记住 +1 的偏移；
    // 改成表后新增只加一行；case 体原样作为 lambda 保存（文案仍在 tr() 中，可被 lupdate 提取）。
    using SonPageFactory = std::function<animationWidget*(QWidget*, device)>;
    static const QHash<int, SonPageFactory> pages = {
        {  0, makeSonPage<sp_customize_cmd> },
        {  1, makeSonPageWithDev<sp_accounts> },
        {  2, makeSonPageWithDev<sp_captive_portal> },
        {  3, makeSonPageWithDev<sp_scales> },
        {  4, makeSonPageWithDev<sp_status_bar> },
        {  5, makeSonPageWithDev<sp_vibrate> },
    };

    const auto it = pages.constFind(key);
    return it == pages.constEnd() ? nullptr : it.value()(parent, dev);
}
