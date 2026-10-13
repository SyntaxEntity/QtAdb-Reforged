#include "sp_wmsize.h"
#include "ui_sp_wmsize.h"
#include "adbclient.h"
#include <QPointer>
#include "textexplainer.h"
#include "uishadow.h"
#include "uistyles.h"

sp_wmsize::sp_wmsize(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_wmsize)
{
    ui->setupUi(this);

    explainer = new textExplainer;

    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    ui->verticalLayout->setAlignment(Qt::AlignVCenter);

    ui->back_to_basePage->setStyleSheet(UiStyles::kFlatButton);
    ui->runBtn->setStyleSheet(UiStyles::kFlatButton);












    applyDropShadow(ui->back_to_basePage);
    applyDropShadow(ui->runBtn);
    applyDropShadow(ui->refreshBtn);
    applyDropShadow(ui->dpiInfo);
    applyDropShadow(ui->currentDpi);
    applyDropShadow(ui->label_3);
    applyDropShadow(ui->label_4);
    applyDropShadow(ui->label_5);
}

sp_wmsize::~sp_wmsize()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void sp_wmsize::setWmInfo()
{
    // 原实现同步等待 adb（设备挂起时最长 20s，期间界面无响应）→ 改为异步回填。
    // 页面可能在请求返回前被销毁，故用 QPointer 保护。
    QPointer<sp_wmsize> self(this);
    AdbClient::instance()->runWithCallback(
        {"-s", dev.addr, "shell", "wm", "size"},
        [self](int, const QString &output, const QString &) {
            if(!self) {
                return;
            }
            QString text = output;      // replace() 非 const，需先取副本
            text.replace("Physical size", tr("默认")).replace("Override size", tr("当前"));
            self->ui->dpiInfo->setText(text.simplified());
        });
}

void sp_wmsize::on_runBtn_clicked()
{
    if(ui->lineEdit->text().toInt() && ui->lineEdit_2->text().toInt()){
        // 原实现在此处同步等待，并用 setDisabled(true/false) 包住整段等待；
        // 改为异步下发，等待期间同样禁用界面，完成后在回调里恢复。
        const QString size = ui->lineEdit->text().simplified() + "x" + ui->lineEdit_2->text().simplified();
        this->setDisabled(true);

        QPointer<sp_wmsize> self(this);
        AdbClient::instance()->runWithCallback(
            {"-s", dev.addr, "shell", "wm", "size", size},
            [self](int, const QString &, const QString &) {
                if(!self) {
                    return;
                }
                self->setDisabled(false);
                self->refresh();
                self->ui->lineEdit->setPlaceholderText(tr("已修改"));
                self->ui->lineEdit_2->setPlaceholderText(tr("已修改"));
            });
    }
    else if(!ui->lineEdit->text().toInt() || !ui->lineEdit_2->text().toInt())
    {
        ui->lineEdit->clear();
        ui->lineEdit_2->clear();
        ui->lineEdit->setPlaceholderText(tr("输入无效"));
        ui->lineEdit_2->setPlaceholderText(tr("输入无效"));
    }
    else
    {
        ui->lineEdit->clear();
        ui->lineEdit_2->clear();
        ui->lineEdit->setPlaceholderText("error");
        ui->lineEdit_2->setPlaceholderText("error");
    }
}

void sp_wmsize::refresh()
{
    setWmInfo();
    ui->lineEdit->clear();
    ui->lineEdit_2->clear();
}

void sp_wmsize::on_refreshBtn_clicked()
{
    refresh();
}

void sp_wmsize::setDev(device device)
{
    dev = device;
    setWmInfo();
}
