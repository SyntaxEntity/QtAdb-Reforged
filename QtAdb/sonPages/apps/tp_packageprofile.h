#ifndef TP_PACKAGEPROFILE_H
#define TP_PACKAGEPROFILE_H

#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include "../../textexplainer.h"
#include "../../threads/adbthread.h"
#include "../../animationwidget.h"

namespace Ui {
class tp_packageProfile;
}

class tp_packageProfile : public animationWidget
{
    Q_OBJECT

public:
    explicit tp_packageProfile(QWidget *parent = nullptr, QString packageName = "");
    ~tp_packageProfile();

    QString name = "";
    device dev;
    void setDevice(device d);

private:
    Ui::tp_packageProfile *ui = nullptr;

    adbThread * thread_file = nullptr;
    adbThread * thread_installer = nullptr;
    textExplainer *explainer = nullptr;

    void setShadow(QWidget* wgt);

    bool uninstall_clicked = false;
    bool clear_clicked = false;
    bool enable_clicked = false;
    bool disable_clicked = false;

signals:
    void packageChanged();
    /*
    void lock();
    void unlock();*/
public slots:
    void set_file(QString);
    void set_installer(QString);

private slots:
    void on_uninstall_clicked();
    void on_clear_clicked();
    void on_enable_clicked();
    void on_disable_clicked();
};

#endif // TP_PACKAGEPROFILE_H
