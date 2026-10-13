#ifndef SP_INSTALLER_H
#define SP_INSTALLER_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <QFileDialog>
#include "../../threads/adbthread.h"
#include <QScrollBar>
#include <QFile>
#include "../../animationwidget.h"
#include "textexplainer.h"

namespace Ui {
class sp_installer;
}

class sp_installer : public animationWidget
{
    Q_OBJECT

public:
    explicit sp_installer(QWidget *parent = nullptr);
    ~sp_installer();

    device dev;

    void setDev(device dev);

    adbThread *thread = nullptr;

    QString labelDisplay;

private slots:
    void on_runBtn_clicked();

    void on_selectBtn_clicked();

    void stop_progressBar(QString);

private:
    Ui::sp_installer *ui = nullptr;

    QString apkPath;
    /*
    QLabel *movieLable;*/
};

#endif // SP_INSTALLER_H
