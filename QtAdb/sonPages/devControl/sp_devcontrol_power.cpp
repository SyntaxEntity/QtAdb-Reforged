#include "sp_devcontrol_power.h"
#include "ui_sp_devcontrol_power.h"
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_devControl_power::sp_devControl_power(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_devControl_power)
{
    ui->setupUi(this);
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));
    explainer = new textExplainer();


    applyDropShadow(ui->back_to_basePage);

    applyDropShadow(ui->btn_powerDown);

    applyDropShadow(ui->btn_rebootToSystem);

    applyDropShadow(ui->btn_rebootToRecovery);

    applyDropShadow(ui->btn_rebootToFastboot);
}

sp_devControl_power::~sp_devControl_power()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void sp_devControl_power::on_btn_powerDown_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::powerOff());
}

void sp_devControl_power::setDev(device device)
{
    dev = device;
}

void sp_devControl_power::on_btn_rebootToSystem_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::reboot());
}

void sp_devControl_power::on_btn_rebootToRecovery_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::reboot(QStringLiteral("recovery")));
}

void sp_devControl_power::on_btn_rebootToFastboot_clicked()
{
    // 异步下发：原同步调用会让界面一直等到命令结束（挂起时最长 20s）
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::reboot(QStringLiteral("bootloader")));
}
