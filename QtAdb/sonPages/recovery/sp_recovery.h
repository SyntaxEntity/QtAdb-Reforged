#ifndef SP_RECOVERY_H
#define SP_RECOVERY_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include "../standardoutputpage.h"
#include "../../threads/adbthread.h"
#include <QFileDialog>
#include "../../animationwidget.h"
#include "textexplainer.h"

namespace Ui {
class sp_recovery;
}

class sp_recovery : public animationWidget
{
    Q_OBJECT

public:
    explicit sp_recovery(QWidget *parent = nullptr);
    ~sp_recovery();

    device dev;

    void setDev(device dev);

    adbThread * thread = nullptr;

    QString labelDisplay;

private slots:
    void on_runBtn_clicked();

    void on_selectBtn_clicked();

    void on_showOutputBtn_clicked(bool checked);

    void updateProgress(QString);

private:
    Ui::sp_recovery *ui = nullptr;

    QString zipPath;

    standardOutputPage *page = NULL;

    QGraphicsDropShadowEffect *shadowEffect_tips = nullptr;
};

#endif // SP_RECOVERY_H
