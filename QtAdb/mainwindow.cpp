#include "mainwindow.h"
#include "devicenames.h"
#include "qobjectdefs.h"
#include "ui_mainwindow.h"
#include "adbclient.h"
#include <QFileInfo>
#include "textexplainer.h"
#include "uishadow.h"
#include "uistyles.h"


// 两个对话框（无线调试 / Magisk 救砖）都要摆一片"标签 + 输入框"，
// 原来每个对话框各写一遍"建控件 → 设文本 → 设样式"。这里收成文件级助手。


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initEnvironmentPATH();

    /*加载动画GIF*/

    /*currentPage指针*/
    currentPage = NULL;

    /*devList 中的当前设备索引*/
    current_device = 0;
    ui->dynamic_island->setText(tr("请先选择设备"));
    ui->dynamic_island->setStyleSheet(UiStyles::kSemiWhitePanel);
    //ui->comboBox->setPlaceholderText("点击此处选择设备");

    /*连接信号与槽*/
    connect(this->ui->comboBox,SIGNAL(currentIndexChanged(int)),this,SLOT(setCurrentDevice(int)));  //更改当前设备
    //connect(this->ui->indexList,SIGNAL(currentRowChanged(int)),this,SLOT(playLoadingGif()));      //播放加载动画
    connect(this->ui->indexList,SIGNAL(currentRowChanged(int)),this,SLOT(initBasePage(int)));       //创建并更改当前页面

    /*设置界面*/
    addIndexItems();    //设置左侧目录
    setStyles();        //设置样式
    view = new comboboxListWidget(ui->comboBox);
    ui->comboBox->setModel(view->model());
    ui->comboBox->setView(view);

    /*初始化对象*/
    explainer = new textExplainer();
    maker = new pageMaker();
    listener = new usb_listener();

    connect(listener, SIGNAL(DeviceChanged()),this,SLOT(refreshDevListLater()));
    connect(this, SIGNAL(adbDeviceChanged()),this,SLOT(DeviceChanged()));

    qApp->installNativeEventFilter(listener);

    this->setAttribute(Qt::WA_NativeWindow);

    /*设备热插拔：轮询兜底*/
    // Windows 的 usb_listener 只对 USB 事件生效（无线调试设备不会触发），
    // Linux 下没有原生通知（usb_listener 直接返回 false）。
    // 这里用定时器轮询走【与原生通知完全相同】的路径（slot_refreshDevList 会置 liangYi=true），
    // 因此只有设备列表真的变化时才重建下拉框，不会打断用户当前选择的设备。
    // refreshDevList() 内部有"已有请求在途则跳过"的保护，不会堆积请求。
    m_deviceWatcher = new QTimer(this);
    connect(m_deviceWatcher, &QTimer::timeout, this, &MainWindow::slot_refreshDevList);
    m_deviceWatcher->start(2500);

    /*启动监听*/

    /*启动ADB与设备列表都不再阻塞界面*/
    connect(AdbClient::instance(), SIGNAL(finished(int,int,QString,QString)),
            this, SLOT(onAdbRequestFinished(int,int,QString,QString)));

    // 原实现用的是 "adb server"（并不是合法命令，只会打印用法），这里改为 start-server；
    // 二者都改为异步发起：设备无响应时窗口立刻可见，而不是等 20 秒。
    AdbClient::instance()->run({QStringLiteral("start-server")});
    refreshDevList();

    /*用户未选择设备前，锁定界面*/
    this->update();

    initBasePage(6);
    currentPage->playLoadAnimation(427);
    lock();

    reset_monitorBars();
}

MainWindow::~MainWindow()
{
    // 先请求监视线程退出（它在后台循环采样）
    stopMonitorThread();

    // 停止热插拔轮询，避免退出过程中再发起请求
    if(m_deviceWatcher != nullptr)
    {
        m_deviceWatcher->stop();
    }

    // listener 以 qApp 为宿主注册了原生事件过滤器，必须在删除对象【之前】摘除，
    // 否则 qApp 会持有悬空指针；原实现直接不删除，造成每次退出泄漏一个对象。
    if(listener != nullptr)
    {
        qApp->removeNativeEventFilter(listener);
        delete listener;
        listener = nullptr;
    }

    ui->indexList->clear();
    delete explainer;
    delete maker;
    delete ui;
}

void MainWindow::initEnvironmentPATH()              //方法：设置环境变量
{
    // 注：原实现在此处调用 QTextCodec::setCodecForLocale()。
    //     Qt 6 已把 QTextCodec 移出 QtCore（只在 Qt5Compat 中保留），而 Qt 6 内部
    //     统一使用 UTF-8，该调用既无必要（且编解码器名 "UTF_8" 本身有误、会返回
    //     nullptr 并把编码重置为本地编码），故删除。删除后不再需要 core5compat 模块。

    QString envPath = qgetenv("PATH");      //获取当前环境变量


    QString appDirPath = QApplication::applicationDirPath();    //获取程序所在位置
    if(appDirPath.contains(QRegularExpression("[\u4e00-\u9fa5]")))
    {
        QMessageBox::warning(NULL, tr("路径问题"),tr("请将程序置于纯英文路径下，否则将无法正确设置ADB环境\n您当前的路径为：\n")
                                             + appDirPath);
    }

    // 按平台定位随程序分发的 platform-tools 目录。修正原实现的三个问题：
    //   1) PATH 分隔符在 Windows 上是 ';'、在类 Unix 上是 ':'，原实现硬编码 ';'，
    //      导致 Linux/macOS 下注入的目录永远不会被搜索（表现为 adb 找不到）；
    //   2) 原实现用反斜杠拼路径，且 Linux 分支指向仓库中并不存在的
    //      "platform-tools-linux" 目录；
    //   3) 原实现把 QString userPath 分别声明在三个互斥的 #ifdef 中，
    //      平台都不匹配时 userPath 未声明 → 编译失败。
    QString userPath;
#ifdef Q_OS_WIN32
    userPath = appDirPath + "/Resources/platform-tools";
#elif defined(Q_OS_MACOS)
    userPath = appDirPath + "/platform-tools-OSX";
#else
    userPath = appDirPath + "/Resources/platform-tools";
#endif

    // 目录不存在时不污染 PATH，直接回退到系统 PATH 中的 adb
    if(QFileInfo::exists(userPath))
    {
        envPath += QDir::listSeparator() + QDir::toNativeSeparators(userPath);
        qputenv("PATH",envPath.toUtf8());         //更改环境变量
    }
    else
    {
        qWarning() << tr("未找到随程序分发的 platform-tools 目录，将使用系统 PATH 中的 adb：") << userPath;
    }
}

// 方法：请求刷新设备列表。
// 设备列表改为【异步】获取：原实现直接在 GUI 线程里同步等待 `adb devices -l`
// （最长 20s），设备无响应时窗口要等 20 秒才显示出来。
// 现在只发起请求，结果到达后在 onAdbRequestFinished() 中更新界面。
void MainWindow::refreshDevList()
{
    if(m_deviceListRequest != 0)
    {
        return;         // 已有请求在途，避免重复排队
    }
    m_deviceListRequest = AdbClient::instance()->run({QStringLiteral("devices"), QStringLiteral("-l")});
}

void MainWindow::onAdbRequestFinished(int requestId, int exitCode,
                                      const QString &standardOutput, const QString &standardError)
{
    if(requestId != m_deviceListRequest)
    {
        return;         // 不是设备列表请求（其它模块也可能使用 AdbClient）
    }
    m_deviceListRequest = 0;

    if(exitCode != 0 && !standardError.isEmpty())
    {
        qWarning().noquote() << tr("获取设备列表失败：") << standardError;
    }

    applyDeviceList(standardOutput);
}

// 方法：用 `adb devices -l` 的输出更新设备列表与界面（原 refreshDevList 的正文）
// ── 设备列表的两个纯判定函数（从 applyDeviceList 里抽出来，便于单独读懂与复用）──

// 列表是否发生了"需要重建界面"的变化：数量、序列号或状态任一不同。
// 注意必须比较 state：设备从"未授权"变为"已授权"时界面要跟着刷新。
static bool deviceListChanged(const QList<device> &before, const QList<device> &after)
{
    if(before.size() != after.size())
    {
        return true;    // 含"原来没设备 / 现在有设备"以及数量增减
    }
    for(int i = 0; i < before.size(); ++i)
    {
        if(before[i].addr != after[i].addr || before[i].state != after[i].state)
        {
            return true;
        }
    }
    return false;
}

// "未响应"设备在列表中的下标（下拉列表要把它们标出来）
static QList<int> offlineIndexes(const QList<device> &list)
{
    QList<int> off;
    for(int i = 0; i < list.size(); ++i)
    {
        if(list[i].state == DeviceState::Offline)
        {
            off.append(i);
        }
    }
    return off;
}

// 重建设备下拉框：先断开 currentIndexChanged，避免 clear() 触发 setCurrentDevice()
// 去操作已经不存在的设备；重建后再连回来。
// 原实现把这段在"逐设备循环"和"liangYi 分支"里各写了一遍。
void MainWindow::rebuildDeviceCombo(const QList<int> &offline)
{
    disconnect(this->ui->comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setCurrentDevice(int)));
    ui->comboBox->clear();
    view->setUpList(devList, offline);
    connect(this->ui->comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setCurrentDevice(int)));
}

// 设备列表变化后要跟着更新的一切：自动选中、离线项置灰、顶部标签与监视器。
void MainWindow::updateDeviceIndicators(const QList<int> &offline)
{
    // 刷新会先 clear() 下拉框再重填，此刻 currentIndex 变为 -1；
    // 而 slot_taiChi() 只在 currentIndex != -1 时才 unlock() ——
    // 于是"刷新之后界面一直保持锁定"，必须手动再点一次下拉框才能恢复。
    // 检测到设备时自动选中第一个（与原实现 current_device = 0 的假设一致）。
    if(ui->comboBox->currentIndex() < 0 && ui->comboBox->count() > 0)
    {
        ui->comboBox->setCurrentIndex(0);
    }

    /*将离线设备设为不可选*/
    for(int i = 0; i < offline.count(); i++)
    {
        QVariant v(0);
        ui->comboBox->setItemData(offline[i], v, Qt::UserRole - 1);
    }

    /*若没有设备，重置*/
    if(devList.isEmpty())
    {
        current_device = -1;         //重设当前设备
        // 设备全部消失时停止监视器（否则它会一直对着已拔出的设备发请求）
        stopMonitorThread();
        ui->dynamic_island->setText(tr("请先选择设备"));
        ui->dynamic_island->setStyleSheet(UiStyles::kSemiWhitePanel);
        return;
    }

    current_device = 0;
    // 顶部标签是"当前设备"始终可见的位置：有别名时显示别名
    const QString alias = DeviceNames::nameFor(devList[current_device].addr);
    ui->dynamic_island->setText(alias.isEmpty() ? devList[current_device].addr : alias);
    // 未授权设备用红色标出，其余用常规底色
    const bool unauthorized = (devList[current_device].state == DeviceState::Unauthorized);
    ui->dynamic_island->setStyleSheet(unauthorized
            ? "background-color:rgba(237,90,101,1);border-radius:4px;border:0px;"
            : UiStyles::kSemiWhitePanel);
}

void MainWindow::applyDeviceList(const QString &output)
{
    // ① 取变化前的快照（用于判断"设备列表有没有变"）
    const QList<device> before = devList;

    // ② 重新解析
    devList = explainer->getDevList_windows(output);

    // ③ 有变化才发信号（比较逻辑见 deviceListChanged()）
    const bool changed = deviceListChanged(before, devList);
    if(changed)
    {
        emit adbDeviceChanged();
    }

    // ④ 需要时重建下拉框。
    //    原实现把重建放在 `for(int i = 0; i < devList.size(); i++)` 里，而循环体
    //    根本不使用 i —— 等于"有几台设备就重建几次"（每次都 clear + 重建控件）。
    //    最后状态与只重建一次完全相同，故改为一次判断。
    const QList<int> offline = offlineIndexes(devList);
    if(changed || !liangYi)
    {
        rebuildDeviceCombo(offline);
    }
    liangYi = false;        //重置两仪

    // ⑤ 顶部标签、离线置灰、自动选中、监视器
    updateDeviceIndicators(offline);
}

void MainWindow::on_refreshButton_clicked()         //槽：按下刷新按钮
{
    // 刷新只做三件事：重新拉取设备列表、重置占用率显示、按需重启监视器。
    //
    // 这里【不重建页面、也不 lock()】：
    //  * 设备若发生变化，applyDeviceList → setCurrentDevice() 本来就会重建页面；
    //  * 原实现是 lock() + initBasePage(6)，而 key 6 是"关于"页 —— 每次刷新都会
    //    把用户从正在看的页面强行带走，并且一旦解锁条件不满足就会永久锁死界面
    //    （见 §12 记录的"刷新后界面一直锁定"）。
    refreshDevList();
    reset_monitorBars();
    resetMonitorThread(ui->comboBox->currentIndex());
}

void MainWindow::resetMonitorThread(int index)
{
    if(index < 0 || index >= devList.size())
    {
        // 索引无效多半是"刷新进行中、下拉框被短暂清空"造成的：
        // 此时不能停掉监视器，否则每次刷新都会丢掉采样基准，
        // CPU/内存显示要等约 4 秒（两次采样）才恢复。
        // 只有当设备列表确实为空时才停止。
        if(devList.isEmpty())
        {
            stopMonitorThread();
        }
        return;
    }

    // 设备没变就保持现有监视器继续采样。
    // 采集 CPU 占用率需要"两次采样求差"，重建监视器等于丢掉基准 ——
    // 而每次点"刷新"都会走到这里，于是刷新之后 CPU/内存要等约 4 秒才恢复显示。
    if(thread_mon != nullptr && thread_mon->deviceAddress() == devList[index].addr)
    {
        return;
    }

    stopMonitorThread();

    // 原实现使用 devList[current_device]（旧索引）创建监视线程：
    // 在 setCurrentDevice() 中先创建线程、后更新 current_device，
    // 于是切换设备后监控的其实是【上一台】设备。这里改用传入的 index。
    thread_mon = new thread_monitor(devList[index]);
    // 监视器结束后由它自己释放
    connect(thread_mon,SIGNAL(finished()),thread_mon,SLOT(deleteLater()));
    connect(thread_mon,SIGNAL(signal_monitor(float,float)),this,SLOT(slot_update_monitor(float,float)));
    thread_mon->start();

    ui->progressBar_CPU->setRange(0,100);
    ui->progressBar_RAM->setRange(0,100);
}

// 请求监视线程退出。
// 原实现用 GUI 线程里的 QTimer 驱动采样（getInfo() 是普通槽），线程本身是空转的；
// 现在线程自己循环采样，这里只需通知停止并断开信号。
void MainWindow::stopMonitorThread()
{
    if(thread_mon == nullptr)
    {
        return;
    }

    thread_mon->stop();
    // 只断开与 MainWindow 的连接；线程内部 finished()→deleteLater() 的连接保留，
    // 这样线程结束后仍能自行释放。
    thread_mon->disconnect(this);
    thread_mon = nullptr;
}

void MainWindow::setCurrentDevice(int index)        //槽：改变所选设备
{

    // 原实现把“停止旧线程 + 启动新线程”整段复制了一遍，且用旧索引创建监视对象；
    // 统一交给 resetMonitorThread()，并传入本次选中的索引。
    resetMonitorThread(index);

    if(index >= 0)
    {
        if(currentPage != NULL)
        {
            delete currentPage;
            currentPage = NULL;
        }

        current_device = index;

        if(ui->indexList->currentRow() == 0)
        {
            initBasePage(0);
        }
        else
        {
            ui->indexList->setCurrentRow(0);
        }

        // 同上：优先显示别名，其次型号名
        const QString alias2 = DeviceNames::nameFor(devList[current_device].addr);
        ui->dynamic_island->setText(alias2.isEmpty()
                                        ? explainer->get_words_after(devList[current_device].model, ":")
                                        : alias2);
    }
    unlock();
}

void MainWindow::initBasePage(int key)              //槽：生成basePages
{
    // indexList 被清空时会发出 currentRowChanged(-1)（"没有选中行"）。
    // 原实现把整个函数体包在 `while(key != -1) { ... break; }` 里，等于这种情况什么也不做，
    // 这里保留该语义（并且不做 lock()，避免与正在进行的动画互相干扰）。
    if(key < 0)
    {
        return;
    }

    this->lock();

    // ① 销毁上一个页面（其子控件随页面一起释放）
    if(currentPage != nullptr)
    {
        delete currentPage;
        currentPage = nullptr;
    }

    // ② 选目标设备：没有设备时用一个占位设备，并固定打开"关于"页（key 6）。
    //    这里同时挡住 current_device 越界（它由 applyDeviceList / setCurrentDevice 维护，
    //    正常不会是 -1 与"列表非空"同时出现，但越界访问 QList 会直接崩）。
    device target;
    int pageKey = key;
    if(devList.isEmpty() || current_device < 0 || current_device >= devList.size())
    {
        target.addr = "#EMPTY#";
        pageKey = 6;
    }
    else
    {
        target = devList[current_device];
    }

    // ③ 创建页面。
    //    原实现先 new 一个 basePage、设 wgtHeight、连信号，然后在两个分支里都立刻
    //    delete 掉再换成 maker 创建的页面 —— 那第一个页面纯属白建（连的槽也随之丢弃），
    //    每次导航都多一次分配；而且那个 `wgtHeight = ...` 打在即将删除的对象上，是死赋值
    //    （真实页面一直用 animationWidget 的默认值 427，且动画用的是显式传入的高度）。
    currentPage = maker->createPageWithKey(pageKey, ui->page, target);
    if(currentPage == nullptr)
    {
        // createPageWithKey 对未知序号返回 nullptr；直接 addWidget(nullptr) 会崩。
        qWarning() << "创建页面失败，页面序号 =" << pageKey;
        this->unlock();          // 上面 lock() 过，这里必须解锁，否则界面永久锁死
        return;
    }

    connect(currentPage, SIGNAL(animationEnd()), this, SLOT(slot_taiChi()));
    ui->verticalLayout_2->addWidget(currentPage);
    currentPage->playLoadAnimation(ui->widget_height->height());

    // ④ 重设"动画结束"定时器。
    //    上一次的若仍在等待（快速连续切换页面），先取消：多个定时器会各自触发
    //    slot_taiChi()，而槽里只释放成员指向的那一个，其余的会一直留到窗口销毁。
    if(taiChiTimer != nullptr)
    {
        taiChiTimer->stop();
        taiChiTimer->deleteLater();
    }
    taiChiTimer = new QTimer(this);
    connect(taiChiTimer, SIGNAL(timeout()), this, SLOT(slot_taiChi()));
    taiChiTimer->setSingleShot(true);
    taiChiTimer->start(700);
}

void MainWindow::addItemToIndex(indexListItem *itemWidget)  //方法：向indexist中添加单个item
{
    QListWidgetItem * tmpItem = new QListWidgetItem();
    ui->indexList->addItem(tmpItem);
    ui->indexList->setItemWidget(tmpItem, itemWidget);
}

void MainWindow::addIndexItems()                    //方法：初始化向indexList中添加的item，完成indexList创建。
{
    indexListItem *devInfo = new indexListItem(this->ui->indexList);
    devInfo->setText(tr("设备信息"));
    devInfo->setPic("image:url(:/ico/image/ico/smartphone-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(devInfo);

    indexListItem *activator = new indexListItem(this->ui->indexList);
    activator->setText(tr("激活"));
    activator->setPic("image:url(:/ico/image/ico/pulse-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(activator);

    indexListItem *apps = new indexListItem(this->ui->indexList);
    apps->setText(tr("软件包管理器"));
    apps->setPic("image:url(:/ico/image/ico/google-play-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(apps);

    indexListItem *devControl = new indexListItem(this->ui->indexList);
    devControl->setText(tr("设备控制"));
    devControl->setPic("image:url(:/ico/image/ico/remote-control-2-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(devControl);

    indexListItem *recovery = new indexListItem(this->ui->indexList);
    recovery->setText(tr("Recovery"));
    recovery->setPic("image:url(:/ico/image/ico/registered-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(recovery);

    //:/ico/image/ico/code-s-slash-line.svg //05
    indexListItem *advanced = new indexListItem(this->ui->indexList);
    advanced->setText(tr("其它"));
    advanced->setPic("image:url(:/ico/image/ico/code-s-slash-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(advanced);

    indexListItem *about = new indexListItem(this->ui->indexList);
    about->setText(tr("关于"));
    about->setPic("image:url(:/ico/image/ico/about/information-line.svg);background-color:rgba(255,255,255,0);");
    addItemToIndex(about);

    ui->indexList->setResizeMode(QListView::Adjust);
}

void MainWindow::lock()                             //方法：标记"暂无设备"（不再禁用控件）
{
    // 原来这里是 indexList/page 的 setEnabled(false)。后果是：只要设备列表为空，
    // 整个界面就永久锁死 —— 连不需要设备的"关于"页都进不去，而且没有任何提示。
    // 现在改为【只提示、不禁用】：
    //   * 页面本身已能优雅处理"无设备"（setCurrentDevice 里有 #EMPTY# 兜底）；
    //   * 设备到位后 setCurrentDevice() 会用别名/型号覆盖这行提示。
    // 顶部本就显示 .ui 里的默认提示（"请先选择设备"，且已有英文翻译），
    // 因此这里什么都不用做 —— 设备到位后 setCurrentDevice() 会用别名/型号覆盖它。
}

void MainWindow::unlock()                           //方法：清除"暂无设备"提示（与 lock 对称）
{
    // 不再改动任何控件的 enabled 状态 —— 提示文本由 setCurrentDevice() 负责替换。
}

void MainWindow::setStyles()                        //方法：设置样式
{
    // 仓库名带 -Reforged 以区分上游；可执行文件仍叫 QtAdb（配置目录因此保持不变）
    setWindowTitle("QtAdb Reforged");
    setWindowIcon(QIcon(":/ico/image/ico/logo.svg"));

    this->setWindowOpacity(0.99);

    ui->horizontalLayout_3->setAlignment(Qt::AlignLeft);
    ui->verticalLayout_2->setAlignment(Qt::AlignTop);

    ui->indexList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->indexList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    ui->indexList->setStyleSheet("QListWidget{background-color:rgba(255,255,255,0);border-width:0px;border-radius:4px;outline:0px;}"
                                 "QListWidget::item{background-color:rgba(255,255,255,0.9);border-width:0px;border-radius:4px;margin:4px 0px 0px 0px;}"
                                 "QListWidget::item:hover{background-color:#f5f6f9;padding:5px;}"
                                 "QListWidget::item::selected{background-color:#f5f6f9;color:black;padding:10px;}"
                                 "QListView::item{height:35px;}"
                                 );

    ui->comboBox->setStyleSheet("QComboBox{color:black; border:0px solid #BDBDBD; border-radius:4px; background-color:rgba(255,255,255,0.9);image:url(:/ico/image/ico/devices.svg);background-color:rgba(255,255,255,0.9);}"
                                "QComboBox::drop-down{border: 0px solid rgba(255,255,255,0);background-color:rgba(255,255,255,0);border-radius: 8px;}"
                                "QComboBox QAbstractItemView{border:1px solid #BDBDBD;border-radius:0px 0px 0px 0px;outline: 0px;background-color:rgba(255,255,255,0);}"
                                "QComboBox QAbstractItemView::item{height:300px;border:0px solid #BDBDBD;border-radius:0px 0px 0px 0px;background-color:rgba(255,255,255,255);}"//height:30px;
                                "QComboBox QAbstractItemView::item:hover{height:300px;border:0px solid #BDBDBD;border-radius:0px 0px 0px 0px;color:black;background-color:rgba(255,255,255,255);}"
                                "QComboBox QAbstractItemView::item:selected{height:300px;border:0px solid #BDBDBD;border-radius:0px 0px 0px 0px;color:black;background-color:rgba(255,255,255,255);}"
                                );

    ui->fakeSpacer->setStyleSheet("background-color:transparent;");


    ui->comboBox->setMaxVisibleItems(1);
    ui->comboBox->view()->window()->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    ui->comboBox->view()->window()->setAttribute(Qt::WA_TranslucentBackground);
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

    // 主窗口这 10 个控件的投影（原实现就在本函数里；§五十四 补回时曾错放到
    // on_pluginBtn_clicked 内，导致"要点一次插件按钮才有阴影" —— 见 §六十五）。
    const QList<QWidget *> shadowed = { ui->refreshButton, ui->adbKillerBtn, ui->WIFIBtn, ui->WSABtn,
                                        ui->cmdBtn, ui->dynamic_island, ui->comboBox,
                                        ui->widget_mon_CPU, ui->widget_mon_RAM, ui->pluginBtn };
    for(QWidget *widget : shadowed)
    {
        applyDropShadow(widget);
    }
}

void MainWindow::initSonPage(int key)               //槽：生成子页面
{
    maker->createPageWithKey(key,currentPage,devList[current_device]);
}

void MainWindow::on_adbKillerBtn_clicked()          //槽：杀死ADB
{
    QMessageBox * msgBox = new QMessageBox;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);   // 关闭即删：原来弹一次泄漏一个顶层窗口
    msgBox->setWindowIcon(QIcon(":/ico/image/ico/close-circle-line.svg"));
    msgBox->setStyleSheet(UiStyles::kPlainDialog);
    msgBox->setWindowTitle(tr("杀死ADB"));
    msgBox->setText(tr("· 已尝试杀死ADB进程\n· 请勿连续执行！！！"));
    msgBox->addButton(" ✓ ", QMessageBox::AcceptRole);
    msgBox->show();

    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runCommand("adb kill-server");
}


void MainWindow::on_WSABtn_clicked()                //槽：连接WSA
{
    AdbClient::instance()->runCommand("adb connect 127.0.0.1:58526");

    QMessageBox * msgBox = new QMessageBox;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);   // 关闭即删：原来弹一次泄漏一个顶层窗口
    msgBox->setWindowIcon(QIcon(":/ico/image/ico/link.svg"));
    msgBox->setStyleSheet(UiStyles::kPlainDialog);
    msgBox->setWindowTitle("WSA");
    msgBox->setText(tr("· 已尝试连接，请刷新设备列表并选择WSA\n· 请谨慎对WSA使用某些功能"));
    msgBox->addButton(" ✓ ", QMessageBox::AcceptRole);
    msgBox->show();
}

void MainWindow::slot_taiChi()
{
    taiChi = !taiChi;
    // 本槽正是在定时器的 timeout 信号里被调用的：直接 delete 会销毁"正在发信号的对象"，
    // 改用 deleteLater()，并把成员置空（否则下一次 initBasePage 会看到悬垂指针）。
    if(taiChiTimer != nullptr)
    {
        taiChiTimer->deleteLater();
        taiChiTimer = nullptr;
    }

    if(ui->comboBox->currentIndex() != -1)
    {
        this->unlock();
    }
}

void MainWindow::on_cmdBtn_clicked()
{
    QString appDirPath = QApplication::applicationDirPath();
    QString batPath = appDirPath + "/Resources/platform-tools/open-cmd-here.bat";
    QProcess batProcess;
    batProcess.start(batPath);
    batProcess.waitForFinished();
}

void MainWindow::DevicePlugIn()
{
}

void MainWindow::DevicePlugOut()
{
}

void MainWindow::DeviceChanged()
{
    if(firstBoot == true)
    {
        firstBoot = false;
    }
    else
    {
        QMessageBox::StandardButton result=QMessageBox::warning(NULL, tr("检测到USB热插拔"),tr("您可能插入或拔出了一台ADB设备\n将对设备列表进行刷新\n（⚠️这会中断当前正在执行的任务）"));

        if(result)
        {
            //这里要delete adbprocess
            on_refreshButton_clicked();
        }
        else
        {

        }
    }
}

void MainWindow::slot_refreshDevList()
{
    liangYi = true;
    refreshDevList();
}


void MainWindow::refreshDevListLater()
{
    // 延迟 1 秒再刷新：给系统枚举新设备留出时间。
    // 原实现每次都 new 一个 QTimer 且从不释放（deleteLater 那行被注释掉了），
    // 每来一次热插拔事件就泄漏一个定时器；这里挂到 this 上并在触发后释放。
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        timer->deleteLater();
        slot_refreshDevList();
    });
    timer->start(1000);
}

void MainWindow::slot_update_monitor(float cpu_useage, float mem_useage)
{

    int cpu_credit = (int)(cpu_useage*100);
    ui->progressBar_CPU->setValue(cpu_credit);

    int mem_credit = (int)(mem_useage*100);
    ui->progressBar_RAM->setValue(mem_credit);
}

void MainWindow::reset_monitorBars()
{
    ui->progressBar_CPU->setValue(0);
    ui->progressBar_RAM->setValue(0);
}


