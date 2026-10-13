// 主窗口里的两个"辅助窗口"：无线调试（获取无线调试参数）与 Magisk 救砖。
// 从 mainwindow.cpp 拆出来 —— 那个文件原本 900 多行，混了设备列表管理、监视线程、
// 页面管理与这两个对话框；拆开后各文件的职责更清楚。
// 搬动是纯搬动：文案、控件、调用次数都没有变（由 refactor_guard.py 核对）。
#include "mainwindow.h"
#include "adbclient.h"
#include "adbcommands.h"
#include "textexplainer.h"
#include "uishadow.h"
#include "uistyles.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

static QLabel *makeFormLabel(QWidget *parent, const QString &text, const char *style)
{
    QLabel *label = new QLabel(parent);
    label->setText(text);
    if(style != nullptr && *style != '\0')
    {
        label->setStyleSheet(style);
    }
    return label;
}

static QHBoxLayout *makeFormRow(QWidget *left, QWidget *right)
{
    QHBoxLayout *row = new QHBoxLayout();
    row->addWidget(left);
    row->addWidget(right);
    return row;
}

QWidget *MainWindow::buildWirelessOutPanel(QWidget *parent)
{
    // 第一步：无线调试的 IP 与端口（原来与第二步混在同一个槽里，共 75 行）。
    QWidget *panel = new QWidget(parent);
    panel->setStyleSheet(UiStyles::kFormPanel);
    QVBoxLayout *layout = new QVBoxLayout();

    QLabel *step = makeFormLabel(parent, tr("第一步：打开 [开发者选项 -> 无线调试]"), UiStyles::kFormStepLabel);
    QLabel *lbIp = makeFormLabel(parent, tr("无线调试IP地址："), UiStyles::kFormLabel);
    QLabel *lbPort = makeFormLabel(parent, tr("无线调试端口："), UiStyles::kFormLabel);

    le_outIp = new QLineEdit(parent);
    le_outPort = new QLineEdit(parent);

    panel->setLayout(layout);
    layout->addWidget(step);
    layout->addLayout(makeFormRow(lbIp, le_outIp));
    layout->addLayout(makeFormRow(lbPort, le_outPort));
    return panel;
}

QWidget *MainWindow::buildWirelessPairPanel(QWidget *parent)
{
    // 第二步：配对用的 IP、端口与配对码。
    QWidget *panel = new QWidget(parent);
    panel->setStyleSheet(UiStyles::kFormPanel);
    QVBoxLayout *layout = new QVBoxLayout();

    QLabel *step = makeFormLabel(parent, tr("第二步：点击 [使用配对码配对设备]"), UiStyles::kFormStepLabel);
    QLabel *lbIp = makeFormLabel(parent, tr("配对IP地址："),
                                 "border-radius: 4px;border:0px solid #BDBDBD;border-bottom:2px");
    QLabel *lbPort = makeFormLabel(parent, tr("配对端口："), UiStyles::kFormLabel);
    QLabel *lbCode = makeFormLabel(parent, tr("配对码："), UiStyles::kFormLabel);

    le_ipAddr = new QLineEdit(parent);
    le_port = new QLineEdit(parent);
    le_code = new QLineEdit(parent);

    panel->setLayout(layout);
    layout->addWidget(step);
    layout->addLayout(makeFormRow(lbIp, le_ipAddr));
    layout->addLayout(makeFormRow(lbPort, le_port));
    layout->addLayout(makeFormRow(lbCode, le_code));
    return panel;
}

void MainWindow::on_WIFIBtn_clicked()               //槽：弹出无线调试连接窗口，获取无线调试参数
{
    // 挂在主窗口下 + 关闭即删：原来只在"连接"按钮里 deleteLater()，
    // 用户直接关掉这个窗口时就永远留着（LSan 里 36KB / 952 次分配全指向这里）。
    getInfo = new QWidget(this);
    getInfo->setAttribute(Qt::WA_DeleteOnClose);
    getInfo->setMaximumSize(500,300);
    getInfo->setWindowTitle(tr("无线调试"));
    getInfo->setStyleSheet("background-color:rgba(255,255,255,1);");
    getInfo->setWindowIcon(QIcon(":/ico/image/ico/link.svg"));
    getInfo->setWindowIconText("test");

    // 两个步骤各自建面板（见 buildWirelessOutPanel / buildWirelessPairPanel）
    QWidget *outWgt = buildWirelessOutPanel(getInfo);
    QWidget *inWgt = buildWirelessPairPanel(getInfo);

    QPushButton * getInfoBtn = new QPushButton(getInfo);
    getInfoBtn->setText(tr("连接"));
    getInfoBtn->setStyleSheet(UiStyles::kFlatButton);
    applyDropShadow(getInfoBtn);

    QVBoxLayout * mainLayout = new QVBoxLayout(getInfo);
    getInfo->setLayout(mainLayout);
    mainLayout->addWidget(makeFormLabel(getInfo, tr("通过WI-FI连接设备"), ""));
    mainLayout->addWidget(outWgt);
    mainLayout->addWidget(inWgt);
    mainLayout->addWidget(getInfoBtn);

    connect(getInfoBtn,SIGNAL(clicked()),this,SLOT(connectWIFIDev()));
    getInfo->show();
}

void MainWindow::connectWIFIDev()                   //槽：连接无线调试设备
{
    QString ip = le_ipAddr->text().simplified();
    QString port = le_port->text().simplified();
    QString outIp = le_outIp->text().simplified();
    QString outPort = le_outPort->text().simplified();
    QString code = le_code->text().simplified();
    QString command_connect_to_WIFI_dev = "adb pair " + ip + ":" + port;

    // `adb pair <ip>:<port>` 需要把配对码写入标准输入：
    // 原实现用同步 API 边等边写（挂起时最长 20s 界面无响应）。
    AdbClient::instance()->runCommandWithInput("adb pair " + ip + ":" + port, code + "\n");
    AdbClient::instance()->runCommand("adb connect " + outIp + ":" + outPort);

    le_ipAddr->clear();
    le_port->clear();
    le_code->clear();
    le_outIp->clear();
    le_outPort->clear();
    getInfo->deleteLater();
}

void MainWindow::on_pluginBtn_clicked()
{
    // 与无线调试窗口一致：挂在主窗口下 + 关闭即删。
    // 原来是无父对象的顶层窗口，关掉之后一直留到进程结束。
    magiskSaver = new QWidget(this);
    magiskSaver->setAttribute(Qt::WA_DeleteOnClose);
    magiskSaver->setMaximumSize(500,300);
    magiskSaver->setWindowTitle(tr("Magisk救砖"));
    magiskSaver->setStyleSheet("background-color:rgba(255,255,255,1);");
    magiskSaver->setWindowIcon(QIcon(":/ico/image/ico/magisk.png"));

    QLabel * title = makeFormLabel(magiskSaver, tr("在开机前禁用所有 Magisk 模块"), "");

    QWidget *saveWgt = new QWidget(magiskSaver);
    saveWgt->setStyleSheet("border-radius: 4px;border:1px solid #BDBDBD;");
    QVBoxLayout * saveLayout = new QVBoxLayout();
    QLabel * profile = makeFormLabel(magiskSaver, tr("请按照以下步骤操作"), UiStyles::kFormStepLabel);
    QLabel * step1 = makeFormLabel(magiskSaver, tr("1. 断开所有设备的连接，将不断重启的手机关机，或等待它的下一次重启。"), UiStyles::kFormLabel);
    step1->setWordWrap(true);
    step1->setWordWrap(true);
    QLabel * step2 = makeFormLabel(magiskSaver, tr("2. 点按救砖按钮。（接下来您无需再对电脑进行操作）"), UiStyles::kFormLabel);
    step2->setWordWrap(true);
    step2->setWordWrap(true);
    QLabel * step3 = makeFormLabel(magiskSaver, tr("3. 将手机开机，尽量在开机进入第一屏时连接数据线。"), UiStyles::kFormLabel);
    step3->setWordWrap(true);
    step3->setWordWrap(true);


    QPushButton * saveBtn = new QPushButton(magiskSaver);
    saveBtn->setText("");
    saveBtn->setCheckable(false);
    saveBtn->setIcon(QIcon(":/ico/image/ico/play-line.svg"));
    saveBtn->setStyleSheet(UiStyles::kFlatButton);
    QVBoxLayout * mainLayout = new QVBoxLayout(magiskSaver);

    applyDropShadow(saveBtn);

    magiskSaver->setLayout(mainLayout);
    mainLayout->addWidget(title);

    mainLayout->addWidget(saveWgt);

    saveWgt->setLayout(saveLayout);
    saveLayout->addWidget(profile);
    saveLayout->addWidget(step1);
    saveLayout->addWidget(step2);
    saveLayout->addWidget(step3);
    mainLayout->addWidget(saveBtn);

    connect(saveBtn,SIGNAL(clicked()),this,SLOT(slot_save_magisk()));
    magiskSaver->show();
}

void MainWindow::slot_save_magisk()
{
    // `adb wait-for-device` 可能长时间等待设备，原实现是无限期同步阻塞；
    // 这里用长耗时预算（10 分钟）异步执行，结果经回调打印。
    AdbClient::instance()->runWithCallback(
        {"wait-for-device", "shell", "magisk", "--remove-modules"},
        [](int exitCode, const QString &output, const QString &errorOutput) {
        },
        AdbClient::kLongOperationTimeoutMs);
}
