#include "sp_scales.h"
#include "ui_sp_scales.h"
#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QRegularExpression>
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_scales::sp_scales(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_scales)
{
    ui->setupUi(this);

    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    setShadow(ui->back_to_basePage);
    setShadow(ui->pushButton);
    setShadow(ui->pushButton_animation);
    setShadow(ui->pushButton_transition);
    setShadow(ui->pushButton_window);
    setShadow(ui->current_window);
    setShadow(ui->current_animator);
    setShadow(ui->current_transition);
    setShadow(ui->label_animator);
    setShadow(ui->label_window);
    setShadow(ui->label_transition);

    ui->lineEdit_window->setPlaceholderText(tr("修改为"));
    ui->lineEdit_animator->setPlaceholderText(tr("修改为"));
    ui->lineEdit_transition->setPlaceholderText(tr("修改为"));

    // 三个输入框共用同一正则；父对象设为输入框本身 ——
    // 原实现无父对象，而 QLineEdit::setValidator() 不接管所有权，
    // 每建一次页面就泄漏 3 个验证器（LSan 报的栈就指向这里）。
    const QRegularExpression scalePattern(QStringLiteral("^[0-9]+(\\.[0-9]+)?$"));
    const QList<QLineEdit *> scaleEdits = { ui->lineEdit_transition, ui->lineEdit_animator, ui->lineEdit_window };
    for(QLineEdit *edit : scaleEdits)
    {
        edit->setValidator(new QRegularExpressionValidator(scalePattern, edit));
    }
}

sp_scales::~sp_scales()
{
    delete ui;
}

void sp_scales::setDev(device device)
{
    dev = device;
    refresh();
}


void sp_scales::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}

void sp_scales::refresh()
{


    adbThread *thread_animator = new adbThread(this);
    thread_animator->initThread(AdbCmd::settingsGet("global", "animator_duration_scale"), dev);
    QEventLoop::connect(thread_animator,SIGNAL(signal_output(QString)),this,SLOT(setText_animator(QString)));
    // 线程结束后自行释放：原实现每次刷新都新建线程并一直累积到页面销毁
    connect(thread_animator,SIGNAL(finished()),thread_animator,SLOT(deleteLater()));
    thread_animator->start();

    adbThread *thread_transition = new adbThread(this);
    thread_transition->initThread(AdbCmd::settingsGet("global", "transition_animation_scale"), dev);
    QEventLoop::connect(thread_transition,SIGNAL(signal_output(QString)),this,SLOT(setText_transition(QString)));
    // 线程结束后自行释放：原实现每次刷新都新建线程并一直累积到页面销毁
    connect(thread_transition,SIGNAL(finished()),thread_transition,SLOT(deleteLater()));
    thread_transition->start();

    adbThread *thread_window = new adbThread(this);
    thread_window->initThread(AdbCmd::settingsGet("global", "window_animation_scale"), dev);
    QEventLoop::connect(thread_window,SIGNAL(signal_output(QString)),this,SLOT(setText_window(QString)));
    // 线程结束后自行释放：原实现每次刷新都新建线程并一直累积到页面销毁
    connect(thread_window,SIGNAL(finished()),thread_window,SLOT(deleteLater()));
    thread_window->start();

    //ui->current_animator->setText("当前：" + process->run(AdbCmd::settingsGet("global", "animator_duration_scale"),dev).simplified());
    //ui->current_transition->setText("当前：" + process->run(AdbCmd::settingsGet("global", "transition_animation_scale"),dev).simplified());
    //ui->current_window->setText("当前：" + process->run(AdbCmd::settingsGet("global", "window_animation_scale"),dev).simplified());
    ui->lineEdit_transition->clear();
    ui->lineEdit_window->clear();
    ui->lineEdit_animator->clear();
}

void sp_scales::on_pushButton_animation_clicked()
{
    // 输入校验：原实现直接取文本框内容拼接命令，空值或非法值会被原样写进系统设置
    // （校验器此前还拒绝输入 0，导致"关闭动画"这一常用取值无法设置）。
    if(!ui->lineEdit_animator->hasAcceptableInput())
    {
        QMessageBox::information(this, tr("输入无效"), tr("请输入非负数值，例如 0、0.5 或 1。"));
        return;
    }
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "animator_duration_scale", ui->lineEdit_animator->text().simplified()));
    refresh();
}

void sp_scales::on_pushButton_transition_clicked()
{
    if(!ui->lineEdit_transition->hasAcceptableInput())
    {
        QMessageBox::information(this, tr("输入无效"), tr("请输入非负数值，例如 0、0.5 或 1。"));
        return;
    }
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "transition_animation_scale", ui->lineEdit_transition->text().simplified()));
    refresh();
}

void sp_scales::on_pushButton_window_clicked()
{
    if(!ui->lineEdit_window->hasAcceptableInput())
    {
        QMessageBox::information(this, tr("输入无效"), tr("请输入非负数值，例如 0、0.5 或 1。"));
        return;
    }
    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("global", "window_animation_scale", ui->lineEdit_window->text().simplified()));
    refresh();
}

void sp_scales::setText_animator(QString s)
{
    ui->current_animator->setText(tr("当前：") + s.simplified());
}

void sp_scales::setText_transition(QString s)
{
    ui->current_transition->setText(tr("当前：") + s.simplified());
}

void sp_scales::setText_window(QString s)
{
    ui->current_window->setText(tr("当前：") + s.simplified());
}
