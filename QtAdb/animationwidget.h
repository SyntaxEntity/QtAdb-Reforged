#ifndef ANIMATIONWIDGET_H
#define ANIMATIONWIDGET_H

#include <QObject>

#include <QPointer>
#include <QWidget>
#include <QGraphicsEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace Ui {
class animationWidget;
}

class QAbstractItemView;
class QStandardItemModel;

class animationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit animationWidget(QWidget *parent = nullptr);
    // 必须为虚析构：basePage 持有 animationWidget *sonPage 并通过基类指针 delete
    // （basepage.cpp:190）。若析构非虚，则全部 20 个子页面的析构函数永远不会执行
    // （其中的 delete ui / delete process 全成死代码），且属未定义行为。
    virtual ~animationWidget();

    int wgtHeight = 427;

    void playLoadAnimation();
    void playLoadAnimation(int h);
    void playLoadAnimation_sp(int h);
    void playLoadAnimation_bp(int h);
    void playExitAnimation();
    void playExitAnimation_sp();

    // 父控件宽度。parent 可能为 nullptr（tp_packageProfile 即以 nullptr 为父创建），
    // 原实现直接写 parent->width()，会在这些页面上空指针解引用。
    int parentWidgetWidth() const;

    bool isBasePage;
    QWidget *parent = nullptr;

    QPropertyAnimation *animation = nullptr;
    /*
    QPropertyAnimation * opacityExitAnimation;*/

    // 把一个新模型装上视图，并释放上一个由本函数接管的模型。
    // 背景：这些列表页原来都只做 `view->setModel(new QStandardItemModel(this))`，
    // 旧模型仅被丢掉引用、仍作为子对象挂在页面上，于是每刷新一次就多留一个
    // 装着整张表的模型（--repeat 探针实测：6 次刷新后遗留 7 个）。
    // deleteLater 而非 delete：此刻事件循环里可能还有针对旧模型的排队事件（布局/重绘）。
    void installTableModel(QAbstractItemView *view, QStandardItemModel *model);
    // 定义在 .cpp：本头文件不包含 QStandardItemModel，保持编译依赖最小
    QStandardItemModel *installedTableModel() const;


signals:
    void animationEnd();

private:
    // 用 QObject 指针保存：QPointer<T> 需要完整类型，而本头文件只有前置声明
    QPointer<QObject> m_installedModel;
};


#endif // ANIMATIONWIDGET_H
