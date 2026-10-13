#include "sp_dpichanger.h"
#include "ui_sp_dpichanger.h"
#include "adbclient.h"
#include <QPointer>
#include "textexplainer.h"
#include "uishadow.h"
#include "uistyles.h"

sp_dpiChanger::sp_dpiChanger(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_dpiChanger)
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
}

sp_dpiChanger::~sp_dpiChanger()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void sp_dpiChanger::setDpiInfo()
{
    // 原实现同步等待 adb → 改为异步回填；页面可能已销毁，用 QPointer 保护
    QPointer<sp_dpiChanger> self(this);
    AdbClient::instance()->runWithCallback(
        {"-s", dev.addr, "shell", "wm", "density"},
        [self](int, const QString &output, const QString &) {
            if(!self) {
                return;
            }
            QString text = output;      // replace() 非 const，需先取副本
            text.replace("Physical density", tr("默认")).replace("Override density", tr("当前"));
            self->ui->dpiInfo->setText(text.simplified());
        });
}

void sp_dpiChanger::on_runBtn_clicked()
{
    if(ui->lineEdit->text().toInt()){
        // 原实现在此处同步等待，并用 setDisabled(true/false) 包住整段等待；
        // 改为异步下发，等待期间同样禁用界面，完成后在回调里恢复。
        const QString density = ui->lineEdit->text().simplified();
        this->setDisabled(true);

        QPointer<sp_dpiChanger> self(this);
        AdbClient::instance()->runWithCallback(
            {"-s", dev.addr, "shell", "wm", "density", density},
            [self](int, const QString &, const QString &) {
                if(!self) {
                    return;
                }
                self->setDisabled(false);
                self->refresh();
                self->ui->lineEdit->setPlaceholderText(tr("已修改"));
            });
    }
    else if(!ui->lineEdit->text().toInt())
    {
        ui->lineEdit->clear();
        ui->lineEdit->setPlaceholderText(tr("输入无效"));
    }
    else
    {
        ui->lineEdit->clear();
        ui->lineEdit->setPlaceholderText("error");
    }
}

void sp_dpiChanger::refresh()
{
    setDpiInfo();
    ui->lineEdit->clear();
}

void sp_dpiChanger::on_refreshBtn_clicked()
{
    refresh();
}

void sp_dpiChanger::setDev(device device)
{
    dev = device;
    setDpiInfo();
}
