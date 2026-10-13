#include "uishadow.h"

#include <QGraphicsDropShadowEffect>
#include <QWidget>

QGraphicsDropShadowEffect *applyDropShadow(QWidget *widget, const QColor &color)
{
    if(widget == nullptr)
    {
        return nullptr;      // 防御：调用方传空指针时不要建一个没有宿主的 effect
    }

    // 父对象设为控件：随控件一起销毁（原实现传的是 this，即页面本身）
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(widget);
    effect->setOffset(0, 0);
    effect->setColor(color);
    effect->setBlurRadius(5);
    widget->setGraphicsEffect(effect);
    return effect;
}
