#ifndef SP_PERMISSIONS_H
#define SP_PERMISSIONS_H

#include <QWidget>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>
#include <QStandardItemModel>
#include "../../textexplainer.h"
#include "../../threads/adbthread.h"
#include <QScrollBar>
#include <QFile>
#include "../../animationwidget.h"

namespace Ui {
class sp_permissions;
}

class sp_permissions : public animationWidget
{
    Q_OBJECT

public:
    explicit sp_permissions(QWidget *parent = nullptr);
    ~sp_permissions();

    device dev;
    void setDev(device dev);
    QWidget *pa = nullptr;

private slots:
    void on_back_to_basePage_2_clicked();
    void setArg();

    void on_refreshBtn_clicked();

    void on_open_another_window_clicked();

public slots:
    void refresh_tableView(QString);
    // 三种呈现方式（原来都写在 refresh_tableView 里）
    void fillGroupedTable(const QString &s);    // 按组整理（Mode::Grouped）
    void fillFilteredTable(const QString &s);   // 输出所有信息（Mode::Filtered）

private:
    Ui::sp_permissions *ui = nullptr;
    QString arg = "";

    // 呈现方式。原来用 5 个互斥布尔量表示，其中 arg_s / arg_d / arg_u 只写不读；
    // 分派只看"按组 / 过滤 / 其余"三种，故收成三值枚举。
    enum class Mode { Other, Grouped, Filtered };
    Mode mode = Mode::Other;

    // 必须初始化：on_open_another_window_clicked() 会把它交给 QTableView::setModel()，
    // 而列表刷新是异步的，用户可能在刷新完成前点击“查看”，
    // 原实现传入未初始化的野指针 → 崩溃。

    adbThread *thread = nullptr;


    // 必须初始化：这些成员在首次点击“选项”时才被创建，
    // 而释放旧对话框时要先判断指针有效性（原实现为未初始化裸指针）。
    QDialog *settings = nullptr;
    QButtonGroup *group = nullptr;
    QRadioButton *btn_g = nullptr;
    QRadioButton *btn_f = nullptr;
    QRadioButton *btn_s = nullptr;
    QRadioButton *btn_d = nullptr;
    QRadioButton *btn_u = nullptr;
    QPushButton *closeBtn = nullptr;

    void generalMode(QString s);
};

#endif // SP_PERMISSIONS_H
