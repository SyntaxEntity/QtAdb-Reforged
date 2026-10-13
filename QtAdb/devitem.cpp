#include "devitem.h"
#include "ui_devitem.h"
#include "textexplainer.h"
#include "uishadow.h"
#include "uistyles.h"

devItem::devItem(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::devItem)
{
    ui->setupUi(this);
    explainer = new textExplainer();
    applyDropShadow(ui->widget_phone);
    ui->widget_2->setStyleSheet(UiStyles::kToolTip);

}

devItem::~devItem()
{
    delete ui;
    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void devItem::setAddr(QString s)
{
    m_addr = s;                     // 记下真实序列号，供tr("命名")使用
    ui->label_addr_info->setText(tr("序列号\n") + s);
}
void devItem::setDevModel(QString s)
{
    m_modelName = explainer->get_words_after(s, ":");
    ui->label_model_info->setText(tr("型号\n") + m_modelName);
}

void devItem::setDevice(QString s)
{
    ui->label_device_info->setText(tr("设备代号\n") + explainer->get_words_after(s, ":"));
}

void devItem::setName(QString s)
{
    // 有别名就填进输入框；没有则把型号名作为占位提示。
    // 原实现只 setText(型号名)，而输入框又是隐藏的，用户无从下手。
    if(s.isEmpty())
    {
        ui->lineEdit->clear();
        ui->lineEdit->setPlaceholderText(m_modelName.isEmpty() ? tr("为设备命名")
                                                              : tr("为设备命名（默认 %1）").arg(m_modelName));
    }
    else
    {
        ui->lineEdit->setText(s);
    }
}

void devItem::setStat(DeviceState state)
{
    ui->label_stat_info->setText(stateLabel(state));
    if(state == DeviceState::Unauthorized)
    {
        ui->label_stat_info->setStyleSheet("background-color: rgba(237,90,101,0.6);"
                                           "border:0px solid #BDBDBD;"
                                           "border-radius:15px;");
    }
}

void devItem::on_lineEdit_editingFinished()
{
    // 原实现发的是标签文本（"序列号\n<addr>"），键里混进了标签文字；
    // 这里发真实序列号。
    emit addName(m_addr, ui->lineEdit->text());
}
