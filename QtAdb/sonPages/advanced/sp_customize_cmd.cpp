#include "sp_customize_cmd.h"
#include "ui_sp_customize_cmd.h"
#include "adbclient.h"
#include "uishadow.h"

sp_customize_cmd::sp_customize_cmd(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_customize_cmd)
{
    ui->setupUi(this);

    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));





    applyDropShadow(ui->runBtn);

    applyDropShadow(ui->textBrowser);

    applyDropShadow(ui->label);
    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->back_to_basePage);
}

sp_customize_cmd::~sp_customize_cmd()
{
    delete ui;
}


void sp_customize_cmd::on_runBtn_clicked()
{
    // 异步下发：原同步调用会阻塞界面直到命令结束（挂起时最长 20s）
    AdbClient::instance()->runCommand(ui->lineEdit->text());
}
