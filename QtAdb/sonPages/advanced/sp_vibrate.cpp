#include "sp_vibrate.h"
#include "qscrollbar.h"
#include "ui_sp_vibrate.h"
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QRegularExpression>
#include "qssloader.h"
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_vibrate::sp_vibrate(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_vibrate)
{
    ui->setupUi(this);

    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->scrollArea);

    // 同类输入框共用提示语
    const QList<QLineEdit *> onOffEdits = { ui->lineEdit_all, ui->lineEdit_all_charge,
                                            ui->lineEdit_all_haptic, ui->lineEdit_all_ring };
    for(QLineEdit *edit : onOffEdits)
    {
        edit->setPlaceholderText(tr("0关   1开"));
    }
    const QList<QLineEdit *> levelEdits = { ui->lineEdit_haptic, ui->lineEdit_haptic_hardware,
                                            ui->lineEdit_media, ui->lineEdit_alarm,
                                            ui->lineEdit_ring, ui->lineEdit_notification };
    for(QLineEdit *edit : levelEdits)
    {
        edit->setPlaceholderText(tr("请输入等级"));
    }

    // 同一正则；父对象设为输入框本身 —— 原实现无父对象而 setValidator() 不接管所有权，
    // 每建一次页面泄漏 10 个验证器（LSan 报的 52 个就是它们）。
    const QRegularExpression levelPattern(QStringLiteral("^[0-9]+(\\.[0-9]+)?$"));
    const QList<QLineEdit *> validatedEdits = { ui->lineEdit_alarm, ui->lineEdit_all,
                                                ui->lineEdit_all_charge, ui->lineEdit_all_haptic,
                                                ui->lineEdit_all_ring, ui->lineEdit_haptic,
                                                ui->lineEdit_haptic_hardware, ui->lineEdit_media,
                                                ui->lineEdit_notification, ui->lineEdit_ring };
    for(QLineEdit *edit : validatedEdits)
    {
        edit->setValidator(new QRegularExpressionValidator(levelPattern, edit));
    }

    // 投影：原实现逐条列举 32 个控件，漏了不会报错。改为遍历（覆盖集合由 --effects 探针核对）。
    for(QPushButton *button : findChildren<QPushButton *>())
    {
        setShadow(button);
    }
    for(QLabel *label : findChildren<QLabel *>())
    {
        if(label == ui->title || label == ui->icon)
        {
            continue;
        }
        setShadow(label);
    }



}

sp_vibrate::~sp_vibrate()
{
    delete ui;
}

void sp_vibrate::setDev(device device)
{
    dev = device;
    refresh();
}

void sp_vibrate::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}


// 起一个"读某个 system settings 键并把结果回填到指定槽"的临时线程。
// 收这段是因为 refresh() 里重复了 10 遍；槽用成员函数指针传，类型安全。
void sp_vibrate::requestSetting(const QString &systemKey, void (sp_vibrate::*slot)(QString))
{
    adbThread *thread = new adbThread(this);
    thread->initThread(AdbCmd::settingsGet("system", systemKey), dev);
    connect(thread, &adbThread::signal_output, this, slot);
    // 线程结束后自行释放（原实现每次刷新都新建线程并累积到页面销毁）
    connect(thread, &adbThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void sp_vibrate::refresh()
{
    // 10 个请求只有"键名 + 目标槽"不同：原来每个都是 5 行（连注释都重复了 10 遍）。
    // 具体见 requestSetting()。
    requestSetting(QStringLiteral("vibrate_on"), &sp_vibrate::setText_all);
    requestSetting(QStringLiteral("haptic_feedback_enabled"), &sp_vibrate::setText_all_haptics);
    requestSetting(QStringLiteral("vibrate_when_ringing"), &sp_vibrate::setText_all_ring);
    requestSetting(QStringLiteral("charging_vibration_enabled"), &sp_vibrate::setText_all_charge);
    requestSetting(QStringLiteral("haptic_feedback_intensity"), &sp_vibrate::setText_level_haptics);
    requestSetting(QStringLiteral("hardware_haptic_feedback_intensity"), &sp_vibrate::setText_level_haptics_hardware);
    requestSetting(QStringLiteral("notification_vibration_intensity"), &sp_vibrate::setText_level_notification);
    requestSetting(QStringLiteral("alarm_vibration_intensity"), &sp_vibrate::setText_level_alarm);
    requestSetting(QStringLiteral("media_vibration_intensity"), &sp_vibrate::setText_level_media);
    requestSetting(QStringLiteral("ring_vibration_intensity"), &sp_vibrate::setText_level_ring);
}


void sp_vibrate::setText_all(QString s)
{
    ui->current_all->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_all_haptics(QString s)
{
    ui->current_all_haptic->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_all_charge(QString s)
{
    ui->current_all_charge->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_all_ring(QString s)
{
    ui->current_all_ring->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_level_haptics(QString s)
{
    ui->current_haptic->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_level_haptics_hardware(QString s)
{
    ui->current_haptic_hardware->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_level_alarm(QString s)
{
    ui->current_alarm->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_level_media(QString s)
{
    ui->current_media->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_level_notification(QString s)
{
    ui->current_notification->setText(tr("当前：") + s.simplified());
}

void sp_vibrate::setText_level_ring(QString s)
{
    ui->current_ring->setText(tr("当前：") + s.simplified());
}

// 10 个"应用"按钮原本各自复制一份相同的代码，且都不检查输入有效性：
// 输入为空或非法（例如单独一个小数点）时会把垃圾值直接写进系统设置。
bool sp_vibrate::applySetting(const QString &key, QLineEdit *edit)
{
    if(edit == nullptr || !edit->hasAcceptableInput())
    {
        QMessageBox::information(this, tr("输入无效"), tr("请输入非负数值，例如 0、0.5 或 1。"));
        return false;
    }

    // 异步下发：原同步调用会阻塞界面直到命令结束
    AdbClient::instance()->runForDevice(dev.addr, AdbCmd::settingsPut("system", key, edit->text().simplified()));
    refresh();
    return true;
}

void sp_vibrate::on_pushButton_all_clicked()
{
    applySetting("vibrate_on", ui->lineEdit_all);
}

void sp_vibrate::on_pushButton_all_thaptic_clicked()
{
    applySetting("haptic_feedback_enabled", ui->lineEdit_all_haptic);
}

void sp_vibrate::on_pushButton_all_ring_clicked()
{
    applySetting("vibrate_when_ringing", ui->lineEdit_all_ring);
}

void sp_vibrate::on_pushButton_all_charge_clicked()
{
    applySetting("charging_vibration_enabled", ui->lineEdit_all_charge);
}

void sp_vibrate::on_pushButton_haptic_clicked()
{
    applySetting("haptic_feedback_intensity", ui->lineEdit_haptic);
}

void sp_vibrate::on_pushButton_haptic_hardware_clicked()
{
    applySetting("hardware_haptic_feedback_intensity", ui->lineEdit_haptic_hardware);
}

void sp_vibrate::on_pushButton_notificaton_clicked()
{
    applySetting("notification_vibration_intensity", ui->lineEdit_notification);
}

void sp_vibrate::on_pushButton_alarm_clicked()
{
    applySetting("alarm_vibration_intensity", ui->lineEdit_alarm);
}

void sp_vibrate::on_pushButton_media_clicked()
{
    applySetting("media_vibration_intensity", ui->lineEdit_media);
}

void sp_vibrate::on_pushButton_ring_clicked()
{
    applySetting("ring_vibration_intensity", ui->lineEdit_ring);
}


