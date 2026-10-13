#include "sp_installer.h"
#include "ui_sp_installer.h"
#include <QDir>
#include "textexplainer.h"
#include "uishadow.h"

sp_installer::sp_installer(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_installer)
{
    ui->setupUi(this);
    setParent(parent);
    ui->progressBar->hide();
    this->repaint();
    parent->repaint();

    thread = new adbThread(this);

    // 连接只建立一次。原实现把它写在点击槽里：每点一次"安装"就多加一条连接，
    // 于是同一个输出会被处理多次（stop_progressBar 被反复调用）。
    connect(thread,SIGNAL(signal_output(QString)),this,SLOT(stop_progressBar(QString)));

    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));








    applyDropShadow(ui->back_to_basePage);
    applyDropShadow(ui->runBtn);



    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->selectBtn);
    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->tips, Qt::green);

}

sp_installer::~sp_installer()
{
    delete ui;
}

void sp_installer::setDev(device device)
{
    dev = device;
}
void sp_installer::on_runBtn_clicked()
{
    QString command;
    if(apkPath != "")
    {
        command = "adb install ~ " + apkPath + " ~";
    }
    else
    {
        command = "adb install";
    }


    thread->initThread(command,dev,"#INSTALL#");
    thread->start();
    ui->progressBar->show();
    this->setDisabled(true);
    apkPath.clear();
    labelDisplay.clear();
    ui->filePaths->clear();
}

void sp_installer::on_selectBtn_clicked()
{
    apkPath.clear();
    apkPath = QFileDialog::getOpenFileName(this, tr("选择安装包"), QDir::homePath(), tr("安装包(*.apk)"));

    ui->filePaths->setText(apkPath);

}

void sp_installer::stop_progressBar(QString str)
{
    if(str.contains("Success"))
    {
        ui->progressBar->hide();
        this->setEnabled(true);
        ui->filePaths->setText(tr("安装成功"));
    }
    else
    {
        ui->progressBar->hide();
        this->setEnabled(true);
        ui->filePaths->setText(tr("出错"));
    }
}
