#include "btnemulator.h"
#include "ui_btnemulator.h"

#include <QLabel>
#include <QPushButton>
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"
#include "uistyles.h"

btnEmulator::btnEmulator(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::btnEmulator)
{
    ui->setupUi(this);
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));
    explainer = new textExplainer();
    this->setStyleSheet(UiStyles::kToolTip);


    setStyles();
}

btnEmulator::~btnEmulator()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void btnEmulator::setDev(device device)
{
    dev = device;
}

void btnEmulator::on_btn_back_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("4"));
}

void btnEmulator::on_btn_home_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("3"));
}

void btnEmulator::on_btn_menu_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("82"));
}

void btnEmulator::on_btn_changeWindow_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("187"));
}

void btnEmulator::on_btn_power_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("26"));
}

void btnEmulator::on_btn_volUp_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("24"));
}

void btnEmulator::on_btn_volDown_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("25"));
}

void btnEmulator::on_btn_mute_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("164"));
}

void btnEmulator::on_btn_shoot_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("27"));
}

void btnEmulator::on_btn_play_stop_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("85"));
}

void btnEmulator::on_btn_stop_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("86"));
}

void btnEmulator::on_last_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("88"));
}

void btnEmulator::on_btn_play_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("126"));
}

void btnEmulator::on_btn_pause_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("127"));
}

void btnEmulator::on_btn_next_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("87"));
}

void btnEmulator::on_btn_light_down_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("220"));
}

void btnEmulator::on_btn_light_up_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("221"));
}

void btnEmulator::on_btn_sleep_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("223"));
}

void btnEmulator::on_btn_sleep2_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("276"));
}

void btnEmulator::on_btn_delight_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("224"));
}

void btnEmulator::on_btn_call_AI_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("231"));
}

void btnEmulator::on_btn_hang_up_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("6"));
}

void btnEmulator::on_btn_settings_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("176"));
}

void btnEmulator::on_btn_contacts_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("207"));
}

void btnEmulator::on_btn_phone_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("5"));
}

void btnEmulator::on_btn_browser_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("64"));
}

void btnEmulator::on_btn_music_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("209"));
}

void btnEmulator::on_btn_calender_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("208"));
}

void btnEmulator::on_btn_calc_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::inputKeyevent("210"));
}

void btnEmulator::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}

void btnEmulator::setStyles()
{
    // 原来逐个列举了 36 个控件（30 个按钮 + 6 个标签）来加阴影。那份清单必须与 .ui
    // 手工保持一致：新增按钮忘了加、或改了 objectName，都不会报错，只是"这个按钮没阴影"。
    // 改为遍历子控件后清单不复存在，新控件自动获得阴影。
    // 覆盖集合由 --effects 探针核对：重构前后都是 30 个按钮 + 6 个标签。
    for(QPushButton *button : findChildren<QPushButton *>())
    {
        setShadow(button);
    }
    for(QLabel *label : findChildren<QLabel *>())
    {
        setShadow(label);
    }

    // 5 个说明标签的底色（原来同一行写了 5 遍）
    const QList<QLabel *> noteLabels = { ui->label, ui->label_2, ui->label_3, ui->label_4, ui->label_5 };
    for(QLabel *label : noteLabels)
    {
        label->setStyleSheet("border:0px;background-color:rgba(255,255,255,0.7);");
    }
}
