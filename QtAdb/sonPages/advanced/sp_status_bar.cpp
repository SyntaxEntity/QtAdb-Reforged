#include "sp_status_bar.h"
#include "ui_sp_status_bar.h"
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_status_bar::sp_status_bar(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_status_bar)
{
    ui->setupUi(this);

    explainer = new textExplainer();
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    setShadow(ui->pushButton_hide);
    setShadow(ui->pushButton);
    setShadow(ui->pushButton_restore);
    setShadow(ui->hide_restore);
    setShadow(ui->hide_navigation);
    setShadow(ui->hide_status);
    setShadow(ui->back_to_basePage);
    setShadow(ui->pushButton_2);
}

sp_status_bar::~sp_status_bar()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void sp_status_bar::setDev(device device)
{
    dev = device;
}

void sp_status_bar::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}

void sp_status_bar::on_pushButton_hide_clicked()
{
    // 原实现固定用 cmd.append(",xxx") 拼接，导致写入的设置值带【前导逗号】
    // （形如 ",volume,wifi"），Android 会忽略整个值；且一个都没勾选时会写入空值，
    // 等于清空该设置。这里改为收集后用逗号连接，并拦下空选择。
    QStringList icons;
    if(ui->volume->isChecked())       icons << "volume";
    if(ui->location->isChecked())     icons << "location";
    if(ui->speakerphone->isChecked()) icons << "speakerphone";
    if(ui->wifi->isChecked())         icons << "wifi";
    if(ui->hotspot->isChecked())      icons << "hotspot";
    if(ui->alarm_clock->isChecked())  icons << "alarm_clock";
    if(ui->bluetooth->isChecked())    icons << "bluetooth";
    if(ui->airplane->isChecked())     icons << "airplane";
    if(ui->headphone->isChecked())    icons << "headphone";
    if(ui->cast->isChecked())         icons << "cast";
    if(ui->nfc->isChecked())          icons << "nfc";

    if(icons.isEmpty())
    {
        QMessageBox::information(NULL, tr("隐藏图标"), tr("请至少选择一个要隐藏的图标。"));
        return;
    }

    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, {"shell", "settings", "put", "secure", "icon_blacklist", icons.join(",")});
    refreshBtns();

    QMessageBox::information(NULL, tr("隐藏图标"),tr("已隐藏选中图标"));
}


void sp_status_bar::on_pushButton_restore_clicked()
{
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("secure", "icon_blacklist", "null"));
    QMessageBox::information(NULL, tr("取消隐藏图标"),tr("状态栏图标已全部取消隐藏"));
}

void sp_status_bar::on_hide_status_clicked()
{
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "policy_control", "immersive.status=*"));
    QMessageBox::information(NULL, tr("隐藏状态栏"),tr("已隐藏状态栏"));
}


void sp_status_bar::on_hide_navigation_clicked()
{
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "policy_control", "immersive.navigation=*"));
    QMessageBox::information(NULL, tr("隐藏导航栏"),tr("已隐藏导航栏"));
}

void sp_status_bar::on_hide_restore_clicked()
{
    //adb shell settings put global policy_control null
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "policy_control", "null"));
    QMessageBox::information(NULL, tr("重置"),tr("已将状态栏与导航栏取消隐藏"));
}

void sp_status_bar::on_pushButton_2_clicked()
{
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "policy_control", "immersive.full=*"));
    QMessageBox::information(NULL, tr("同时隐藏"),tr("已将导航栏与状态栏同时全局隐藏"));
}

void sp_status_bar::refreshBtns()
{
    ui->airplane->setChecked(false);
    ui->alarm_clock->setChecked(false);
    ui->bluetooth->setChecked(false);
    ui->cast->setChecked(false);
    // 原实现此处重复写了一次 bluetooth，而漏掉了 speakerphone（界面里存在该控件），
    // 导致"隐藏扬声器"勾选后不会被复位。
    ui->speakerphone->setChecked(false);
    ui->headphone->setChecked(false);
    ui->hotspot->setChecked(false);
    ui->location->setChecked(false);
    ui->nfc->setChecked(false);
    ui->wifi->setChecked(false);
    ui->volume->setChecked(false);
}

