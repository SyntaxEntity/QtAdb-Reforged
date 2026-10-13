#ifndef UISHADOW_H
#define UISHADOW_H

#include <QColor>

class QWidget;

// 给控件加统一的投影（偏移 0,0、模糊半径 5），颜色默认灰色。
//
// 说明：全工程有 37 处都是同样四行 ——
//     QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect(this);
//     e->setOffset(0,0); e->setColor(Qt::gray); e->setBlurRadius(5);
//     wgt->setGraphicsEffect(e);
// 其中 35 处参数完全相同（灰色），另 2 处用橙色/天蓝做强调。
// 收成一个函数后，调整整体观感只需改这里一处。
//
// 返回该 effect，便于调用方追加别致的设置（当前调用方都不需要）。
class QGraphicsDropShadowEffect;
QGraphicsDropShadowEffect *applyDropShadow(QWidget *widget, const QColor &color = Qt::gray);

#endif // UISHADOW_H
