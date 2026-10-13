#ifndef DEVITEM_H
#define DEVITEM_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
//#include "abstract/comboboxlistwidget.h"
#include "textexplainer.h"

namespace Ui {
class devItem;
}

class devItem : public QWidget
{
    Q_OBJECT

public:
    explicit devItem(QWidget *parent = nullptr);
    ~devItem();

    void setAddr(QString);
    void setDevModel(QString);
    void setDevice(QString);
    void setName(QString);
    // 展示设备状态：文本由 stateLabel() 提供，判定用枚举（不再比较中文字符串）
    void setStat(DeviceState state);

    // 设备序列号（原实现把标签文本「序列号\n<addr>」当作地址发给出去，导致保存的键是错的）
    QString address() const { return m_addr; }

    textExplainer *explainer = nullptr;

private:
    QString m_addr;         // 真实序列号
    QString m_modelName;    // 型号名（用于输入框的占位提示）

public:

private slots:
    void on_lineEdit_editingFinished();

private:
    Ui::devItem *ui = nullptr;

signals:
    void addName(QString,QString);
};

#endif // DEVITEM_H
