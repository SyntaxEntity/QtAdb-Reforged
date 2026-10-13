#ifndef SP_FEATURES_H
#define SP_FEATURES_H

#include <QWidget>
#include <QStandardItemModel>
#include <QGraphicsDropShadowEffect>
#include <QScrollBar>
#include <QFile>
//#include <QAbstractItemView>
#include "../../threads/adbthread.h"
#include "../../animationwidget.h"
#include "textexplainer.h"

namespace Ui {
class sp_features;
}

class sp_features : public animationWidget
{
    Q_OBJECT

public:
    explicit sp_features(QWidget *parent = nullptr);
    ~sp_features();

    device dev;
    void setDev(device dev);

private slots:
    void on_refreshBtn_clicked();

public slots:
    void refresh_tableView(QString);

private:
    Ui::sp_features *ui = nullptr;
    adbThread *thread = nullptr;
    // 值成员：原先这里是未初始化的裸指针，8 处解引用均为未定义行为
    textExplainer explainer;
};

#endif // SP_FEATURES_H
