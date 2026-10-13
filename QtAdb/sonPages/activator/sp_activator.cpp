#include "sp_activator.h"
#include "ui_sp_activator.h"
#include "qssloader.h"
#include "adbclient.h"
#include <QPointer>
#include "textexplainer.h"
#include "uishadow.h"

sp_activator::sp_activator(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_activator)
{
    ui->setupUi(this);

    //spg_return_pushed()
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(spg_return_pushed()));











    applyDropShadow(ui->textBrowser);
    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->back_to_basePage);
    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->runBtn);
    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->showOutputBtn);
    // 补回：第 48 轮的批量替换只删未加，丢掉了这一处阴影（见 §六十）
    applyDropShadow(ui->refreshBtn);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->textBrowser);
    ui->textBrowser->setTextInteractionFlags(Qt::NoTextInteraction);
}

sp_activator::~sp_activator()
{
    if(page != NULL)
    {
        delete page;
    }

    delete ui;
}

void sp_activator::setUp(QString qss, QString title, QString profile, const QStringList &cmd)
{
    ui->appIcon->setStyleSheet(qss);
    ui->appName->setText(title);
    ui->textBrowser->setMarkdown(profile);
    command = cmd;
}

void sp_activator::on_runBtn_clicked()
{
    // 异步下发；若输出窗口已打开，把命令输出转发给它。
    // （原实现是同步执行、由 adbProcess::outputGet 驱动输出窗口；
    //   adbProcess 删除后改为在回调里转发，功能不变。）
    QPointer<standardOutputPage> pagePtr(page);
    AdbClient::instance()->runForDeviceWithCallback(
        dev.addr, command,
        [pagePtr](int, const QString &output, const QString &errorOutput) {
            if(pagePtr)
            {
                pagePtr->update(output + errorOutput);
            }
        });
}

void sp_activator::setDev(device device)
{
    dev = device;
}

void sp_activator::on_showOutputBtn_clicked(bool checked)
{
    if(checked)
    {
        page = new standardOutputPage(NULL);
        page->setWindowTitle(tr("命令行输出"));
        page->show();
    }
    else if(page != nullptr)
    {
        // 原实现为 page->~standardOutputPage(); —— 在堆对象上显式调用析构函数，
        // 而 ~sp_activator() 还会 delete page，导致 delete ui 被执行两次（双重析构）。
        // 正确做法：隐藏后交给事件循环延迟删除，并立即置空以断开成员引用。
        page->hide();
        page->deleteLater();
        page = nullptr;
    }
}
