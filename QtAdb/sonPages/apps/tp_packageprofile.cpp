#include "tp_packageprofile.h"
#include "ui_tp_packageprofile.h"
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

tp_packageProfile::tp_packageProfile(QWidget *parent ,QString packageName) :
    animationWidget(parent),
    ui(new Ui::tp_packageProfile)
{
    ui->setupUi(this);

    this->setWindowTitle(tr("软件包 ' ")+packageName+" '");
    name = packageName;
    explainer = new textExplainer();

    thread_file = new adbThread(this);

    this->setStyleSheet("background-color:white;");


    thread_installer = new adbThread(this);

    ui->lineEdit_name->setText(name);
    ui->lineEdit_name->isReadOnly();
    ui->lineEdit_installer->isReadOnly();
    ui->lineEdit_path->isReadOnly();

    setShadow(ui->tips);
    setShadow(ui->uninstall);
    setShadow(ui->clear);
    setShadow(ui->enable);
    setShadow(ui->disable);
    setShadow(ui->tips_2);

}

tp_packageProfile::~tp_packageProfile()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void tp_packageProfile::set_file(QString s)
{
    ui->lineEdit_path->setText(explainer->get_words_after(s,"package:"));
}

void tp_packageProfile::set_installer(QString s)
{
    ui->lineEdit_installer->setText(explainer->get_words_after(s,"installer="));
}

void tp_packageProfile::on_uninstall_clicked()
{
    if(uninstall_clicked)
    {
        ui->tips->setText(tr("已执行「卸载」命令，本窗口将关闭"));

        // 命令由 AdbCmd 具名构造：参数数量与位置写错会直接编译不过
        AdbClient::instance()->runForDevice(dev.addr, AdbCmd::pmAction("uninstall", name));

        emit packageChanged();
        this->close();

        uninstall_clicked = false;
    }
    else
    {
        ui->tips->setText(tr("再次点击以卸载，卸载后本窗口将关闭"));
        uninstall_clicked = true;
    }
}


void tp_packageProfile::on_clear_clicked()
{
    if(clear_clicked)
    {
        ui->tips->setText(tr("已执行「清除应用数据」命令"));

        AdbClient::instance()->runForDevice(dev.addr, AdbCmd::pmAction("clear", name));

        clear_clicked = false;
    }
    else
    {
        ui->tips->setText(tr("再次点击以清除"));
        clear_clicked = true;
    }
}


void tp_packageProfile::on_enable_clicked()
{
    if(enable_clicked)
    {
        ui->tips->setText(tr("已执行「启用」命令"));

        AdbClient::instance()->runForDevice(dev.addr, AdbCmd::pmAction("enable", name));

        emit packageChanged();

        enable_clicked = false;
    }
    else
    {
        ui->tips->setText(tr("再次点击以启用"));
        enable_clicked = true;
    }
}


void tp_packageProfile::on_disable_clicked()
{
    if(disable_clicked)
    {
        ui->tips->setText(tr("已执行「停用」命令"));

        AdbClient::instance()->runForDevice(dev.addr, AdbCmd::pmAction("disable-user", name));

        emit packageChanged();

        disable_clicked = false;
    }
    else
    {
        ui->tips->setText(tr("再次点击以停用"));
        disable_clicked = true;
    }

}

void tp_packageProfile::setDevice(device d)
{
    dev = d;

    // -f 列出 apk 路径；name 是过滤串（可能为包名片段）
    QStringList command_file = AdbCmd::pmList("packages") + QStringList{QStringLiteral("-f"), name};
    thread_file->initThread(command_file, dev);
    connect(thread_file,SIGNAL(signal_output(QString)),this,SLOT(set_file(QString)));
    thread_file->start();

    // -i 列出安装来源
    QStringList command_installer = AdbCmd::pmList("packages") + QStringList{QStringLiteral("-i"), name};
    thread_installer->initThread(command_installer, dev);
    connect(thread_installer,SIGNAL(signal_output(QString)),this,SLOT(set_installer(QString)));
    thread_installer->start();
}

void tp_packageProfile::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}
