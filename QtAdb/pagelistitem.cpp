#include "pagelistitem.h"
#include "ui_pagelistitem.h"
#include "uishadow.h"

pageListItem::pageListItem(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::pageListItem)
{
    ui->setupUi(this);
    ui->enableLight->setVisible(false);

    ui->widget->setStyleSheet("QWidget{background-color:transparent; border-radius:4px; border-color:gray;border:0px;}");
    ui->pic->setStyleSheet("QPushButton{background-color:rgba(255,255,255,255);border-radius:4px;border:1px solid #BDBDBD;}");


    setStyles();
}

pageListItem::~pageListItem()
{
    delete ui;
}

void pageListItem::setText(QString title, QString profile)
{
    ui->title->setText(title);
    ui->profile->setText(profile);
}

void pageListItem::setText_title(QString title)
{
    ui->title->setText(title);
}

void pageListItem::setText_profile(QString profile)
{
    ui->profile->setText(profile);
}

void pageListItem::setPic(QString ico)
{
    ui->pic->setIcon(QIcon(ico));
}

void pageListItem::on_copyButton_clicked()
{
    QClipboard *clip = QApplication::clipboard();
    clip->setText(ui->profile->text());

    noticeWidget *notWgt = new noticeWidget(nullptr);
    notWgt->popUp(tr("已将文本\n") + ui->profile->text() + tr("\n复制到剪贴板"));
}

void pageListItem::setSelectable()
{
    ui->enableLight->setVisible(true);
}

void pageListItem::slot_setText(QString t, QString p)
{
    setText(t,p);
}

void pageListItem::slot_setText_title(QString t)
{
    setText_title(t);
}

void pageListItem::slot_setText_profile(QString p)
{
    setText_profile(p);
}

void pageListItem::setStyles()
{

    applyDropShadow(ui->enableLight);
}

void pageListItem::slot_setStyles()
{
    setStyles();
}
