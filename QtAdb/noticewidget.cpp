#include "noticewidget.h"
#include "qscreen.h"
#include "ui_noticewidget.h"

noticeWidget::noticeWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::noticeWidget)
{
    ui->setupUi(this);
    // 第三个参数是 parent：原实现省略了它，动画没有父对象 —— 每个通知控件都会漏。
    animation = new QPropertyAnimation(this, "geometry", this);
    setParent(parent);

    this->setWindowFlags(Qt::FramelessWindowHint|Qt::Tool|Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    //设置窗体为圆角

    QBitmap bmp(this->size());

    bmp.fill();

    QPainter p(&bmp);

    p.setPen(Qt::NoPen);

    p.setBrush(Qt::black);

    p.drawRoundedRect(bmp.rect(),8,8);

    setMask(bmp);


    int x = QCursor().pos().x();
    int y = QCursor().pos().y();
    animation->setDuration(750);
    animation->setEndValue(QRect(x,y, 120,70));
    animation->setStartValue(QRect(x-30,y, 120,70));

    animation->setEasingCurve(QEasingCurve::OutQuart);
}

void noticeWidget::popUp(QString message)
{
    ui->label->setText(message);
    animation->start();
    this->show();

    timer = new QTimer(this);

    connect(timer,SIGNAL(timeout()),this,SLOT(timeUp()));
    timer->setSingleShot(true);
    timer->start(1500);

}

void noticeWidget::timeUp()
{
    this->close();
}


noticeWidget::~noticeWidget()
{
    delete ui;
}
