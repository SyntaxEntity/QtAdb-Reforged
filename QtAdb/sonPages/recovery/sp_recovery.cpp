#include "sp_recovery.h"
#include "ui_sp_recovery.h"
#include <QDir>
#include "textexplainer.h"
#include "uishadow.h"

sp_recovery::sp_recovery(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_recovery)
{
    ui->setupUi(this);

    thread = new adbThread(this);
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    shadowEffect_tips = new QGraphicsDropShadowEffect(this);








    applyDropShadow(ui->back_to_basePage);
    applyDropShadow(ui->runBtn);
    applyDropShadow(ui->showOutputBtn);
    applyDropShadow(ui->selectBtn);
    applyDropShadow(ui->output);
    // tips 的阴影颜色会在 setDev() 里按设备状态改动，所以要保留这个 effect 句柄
    shadowEffect_tips = applyDropShadow(ui->tips, Qt::red);

    ui->progressBar->setVisible(false);
    ui->output->setText(tr("暂时仅支持 .zip 格式\n进度将显示在设备端"));
}

sp_recovery::~sp_recovery()
{
    if(page != NULL)
    {
        delete page;
    }

    delete ui;
}

void sp_recovery::setDev(device device)
{
    dev = device;

    if(dev.state != DeviceState::Sideload)
    {
        ui->runBtn->setDisabled(true);
        shadowEffect_tips->setColor(Qt::green);
        ui->tips->setText(tr("请先打开设备的「Sideload」模式"));
    }
    else
    {
        ui->tips->setText(tr("「Sideload」为实验功能，请谨慎使用"));
    }
}

void sp_recovery::on_runBtn_clicked()
{
    ui->progressBar->setVisible(true);
    ui->output->setText(tr("正在刷入，请在设备端查看进度，切勿插拔设备"));

    QString command;

    if(zipPath != "")
    {
        command = "adb sideload ~ " + zipPath + " ~";
    }
    else
    {
        command = "adb sideload";
    }

    thread->initThread(command, dev, "#SIDELOAD#");
    connect(thread,SIGNAL(signal_output(QString)),this,SLOT(updateProgress(QString)));
    thread->start();

    this->setDisabled(true);

    zipPath.clear();
    labelDisplay.clear();

}

void sp_recovery::on_selectBtn_clicked()
{
    zipPath.clear();
    zipPath = QFileDialog::getOpenFileName(this, tr("选择zip包"), QDir::homePath(), tr("压缩包(*.zip)"));

    ui->filePaths->setText(zipPath);

}

void sp_recovery::on_showOutputBtn_clicked(bool checked)
{
    if(checked)
    {        page = new standardOutputPage(NULL);
        page->setWindowTitle(tr("命令行输出"));
        // 侧载实际由 thread（内部另有自己的 adbProcess）执行，
        // 原实现连接的却是本页的 process，所以输出窗口永远不会有内容。
        connect(thread,SIGNAL(signal_output(QString)),page,SLOT(update(QString)));
        page->show();
    }
    else if(page != nullptr)
    {
        // 同 sp_activator：显式析构调用 + 析构函数中的 delete page = 双重析构
        page->hide();
        page->deleteLater();
        page = nullptr;
    }
}

void sp_recovery::updateProgress(QString s)
{
    ui->progressBar->hide();
    ui->filePaths->clear();

    ui->output->setText(tr("侧载已结束，请在设备上查看结果"));
    ui->tips->setText(tr("设备将退出侧载模式，请刷新设备列表以返回"));
    ui->back_to_basePage->hide();
    this->setEnabled(true);
}
