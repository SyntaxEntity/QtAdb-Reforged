#ifndef SP_ACTIVATOR_H
#define SP_ACTIVATOR_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <QScrollBar>
#include <QFile>
#include "../../textexplainer.h"      // device 类型（原先由 adbprocess.h 传递包含）
#include "../sonpagebase.h"
#include "../standardoutputpage.h"
#include "../../animationwidget.h"

namespace Ui {
class sp_activator;
}

class sp_activator : public animationWidget
{
    Q_OBJECT

public:
    explicit sp_activator(QWidget *parent = nullptr);
    ~sp_activator();

    void setUp(QString qss, QString title, QString profile, const QStringList &command);
    // 要执行的命令（参数表形式，由 AdbCmd:: 构造）
    QStringList command;
    device dev;
    standardOutputPage *page = NULL;

    void setDev(device device);

private slots:
    void on_runBtn_clicked();

    void on_showOutputBtn_clicked(bool checked);

private:
    Ui::sp_activator *ui = nullptr;

};

#endif // SP_ACTIVATOR_H
