#include "animationwidget.h"

#include <QAbstractItemView>
#include <QStandardItemModel>

QStandardItemModel *animationWidget::installedTableModel() const
{
    return qobject_cast<QStandardItemModel *>(m_installedModel.data());
}

void animationWidget::installTableModel(QAbstractItemView *view, QStandardItemModel *model)
{
    if(m_installedModel != nullptr && m_installedModel != model)
    {
        m_installedModel->deleteLater();
    }
    m_installedModel = model;
    view->setModel(model);
}

animationWidget::animationWidget(QWidget *p)
{
    // 第三个参数是 parent：原实现省略了它，动画没有父对象 —— 每个页面的这个动画都会漏。
    animation = new QPropertyAnimation(this, "geometry", this);
    setParent(p);
    parent = p;
    this->hide();
}

void animationWidget::playLoadAnimation()
{
    animation->setDuration(750);
    if(parentWidgetWidth() <= 600)
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 - 11, 600,wgtHeight));
    }
    else
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 - 11, parentWidgetWidth(),wgtHeight));
    }
    animation->setStartValue(QRect(301 + 50 - 29, 117 - 6 - 11, parentWidgetWidth()-50,wgtHeight));

    animation->setEasingCurve(QEasingCurve::OutQuart);

    this->show();
    animation->start();
    emit animationEnd();
}

void animationWidget::playLoadAnimation(int h)
{
    animation->setDuration(750);
    if(parentWidgetWidth() <= 600)
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 -11, 600,h));
    }
    else
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 - 11, parentWidgetWidth(),h));
    }
    animation->setStartValue(QRect(301 + 50 - 29, 117 - 6 - 11, parentWidgetWidth()-50,h));

    animation->setEasingCurve(QEasingCurve::OutQuart);

    this->show();
    animation->start();
    emit animationEnd();
}

void animationWidget::playLoadAnimation_sp(int h)
{
    animation->setDuration(500);
    if(parentWidgetWidth() <= 600)
    {
        animation->setEndValue(QRect(0,0, parentWidgetWidth(),h));
    }
    else
    {
        animation->setEndValue(QRect(0,0, parentWidgetWidth(),h));
    }
    animation->setStartValue(QRect(0,-10, parentWidgetWidth(),h));

    animation->setEasingCurve(QEasingCurve::OutQuart);

    this->show();
    animation->start();
    emit animationEnd();
}

void animationWidget::playLoadAnimation_bp(int h)
{
    animation->setDuration(500);
    if(parentWidgetWidth() <= 600)
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 -11, 600,h));
    }
    else
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 - 11, parentWidgetWidth(),h));
    }
    animation->setStartValue(QRect(301 - 29, 117 - 6 - 11 + 10, parentWidgetWidth(),h));

    animation->setEasingCurve(QEasingCurve::OutQuart);

    this->show();
    animation->start();
    emit animationEnd();
}

void animationWidget::playExitAnimation()
{
    animation->setDuration(300);
    if(parentWidgetWidth() <= 600)
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 - 11 + 50, parentWidgetWidth(),wgtHeight));
    }
    else
    {
        animation->setEndValue(QRect(301 - 29, 117 - 6 - 11 + 50, parentWidgetWidth(),wgtHeight));
    }
    animation->setStartValue(QRect(301 - 29, 117 - 6 - 11, parentWidgetWidth(),wgtHeight));

    animation->setEasingCurve(QEasingCurve::OutQuart);

    this->show();
    animation->start();
    emit animationEnd();
}

void animationWidget::playExitAnimation_sp()
{
    animation->setDuration(500);
    if(parentWidgetWidth() <= 600)
    {
        animation->setEndValue(QRect(0,-11, parentWidgetWidth(),wgtHeight));
    }
    else
    {
        animation->setEndValue(QRect(0,-11, parentWidgetWidth(),wgtHeight));
    }
    animation->setStartValue(QRect(0,0, parentWidgetWidth(),wgtHeight));

    animation->setEasingCurve(QEasingCurve::OutQuart);

    this->show();
    animation->start();
    emit animationEnd();
}

animationWidget::~animationWidget()
{
    // 原实现在此调用 playExitAnimation()，有三个问题：
    //   1) 紧接着就 delete animation，动画根本来不及播放，纯属无效工作；
    //   2) 会在析构过程中 this->show() 并 emit animationEnd()，
    //      使已连接的槽函数在对象半销毁状态下被调用；
    //   3) playExitAnimation() 会解引用 parent，而 tp_packageProfile 等页面
    //      是以 nullptr 为父对象创建的（sp_packages.cpp:103），
    //      一旦其析构函数执行就会空指针解引用崩溃。
    // 退出动画应由调用方在销毁【之前】显式播放，例如
    // basePage::spg_return_pushed() 已经显式调用了 playExitAnimation_sp()。
    delete animation;
    animation = nullptr;
}

int animationWidget::parentWidgetWidth() const
{
    // parent 可能为 nullptr（tp_packageProfile 即以 nullptr 创建），
    // 此时退回自身宽度，避免空指针解引用。
    // 注意：这里必须直接调用 parent->width()，不能再调 parentWidgetWidth()（会自递归）。
    return parent != nullptr ? parent->width() : this->width();
}

