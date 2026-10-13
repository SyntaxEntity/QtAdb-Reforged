#ifndef SP_PACKAGES_H
#define SP_PACKAGES_H

#include <QWidget>
#include <QWidget>
#include <QGraphicsDropShadowEffect>
#include <QStandardItemModel>
#include <QFileDialog>
#include "../../threads/adbthread.h"
#include "tp_packageprofile.h"
#include <QScrollBar>
#include <QFile>
#include "../../animationwidget.h"
#include "textexplainer.h"

/*
struct package
{
};*/

namespace Ui {
class sp_packages;
}

class sp_packages : public animationWidget
{
    Q_OBJECT

public:
    explicit sp_packages(QWidget *parent = nullptr);
    ~sp_packages();

    device dev;
    void setDev(device dev);

    QWidget *pa = nullptr;


private slots:
    void on_refreshBtn_clicked();

    void on_listView_clicked(const QModelIndex &index);
    void setArgs();

public slots:
    void refresh_tableView(QString);

private:
    Ui::sp_packages *ui = nullptr;
    adbThread *thread = nullptr;
    // 值成员：原先这里是未初始化的裸指针，解引用属未定义行为
    textExplainer explainer;

    QString arg = "";
};
#endif // SP_PACKAGES_H
