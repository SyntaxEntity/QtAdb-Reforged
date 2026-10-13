#ifndef BASEPAGE_H
#define BASEPAGE_H

#include <QWidget>
#include <QGraphicsEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QScroller>
#include <QScrollBar>

#include "pagelistitem.h"
#include "sonPages/sonpagemanager.h"
#include "animationwidget.h"
#include "textexplainer.h"

namespace Ui {
class basePage;
}

class basePage : public animationWidget
{
    Q_OBJECT
signals:
    void creatingSonPage();

public:
    explicit basePage(QWidget *parent = nullptr);
    ~basePage();


    animationWidget *sonPage = nullptr;
    QString myName;
    sonPageManager *SPManager = nullptr;
    // 允许打开的子页序号集合。注意：沿用原实现的约定 —— 存放的是"序号+1"，
    // 因为 0 在原实现里表示"空槽"（见下面的 isEnable）。
    // 原实现是 int enableVal[20] 加上一个"拷贝 20 个 int"的接口：
    // 固定上限 20，且接口只收裸指针、拿不到长度，调用方一旦传短数组就会越界读。
    // 改为 QList<int> 后这两点都消失了。
    QList<int> enableVal;
    //pageMaker *maker;
    device dev;


    void addItemsToList(pageListItem *itemWidget);
    void setFather(QWidget *parent);
    QString whoAmI();
    void whoYouAre(QString name);
    // 传入"子页序号+1"的集合（沿用原约定；空集合表示不允许打开任何子页）
    void setEnableValue(const QList<int> &val);

    // 允许打开全部子页。等价于 setEnableValue({1, 2, …, 条目数})，
    // 但不必手工维护那串数字 —— 原实现把 1..18 写死在调用处，新增条目时极易忘记同步。
    void enableAllItems();

    // 子页序号 k 是否被允许打开（见 enableVal 的约定）。
    // 公开出来是为了让自测能直接核对"哪些子页被允许"，而不必靠点击去试。
    bool isEnable(int k) const;
    void setDev(device dev);



    void self_castrate();

private:
    Ui::basePage *ui = nullptr;


    QTimer *listTimer = nullptr;
    QPropertyAnimation *animation_listHide = nullptr;

    //QGraphicsOpacityEffect* exitOpacity;

public slots:
    //void playExitAnimation();

    //void refresh_listItem_effect();

private slots:
    void slot_createSonPage(int key);
    void slot_destroySonPage();
    void unlock();
    void slot_spgAnimationEnd();
    void spg_return_pushed();
};

#endif // BASEPAGE_H
