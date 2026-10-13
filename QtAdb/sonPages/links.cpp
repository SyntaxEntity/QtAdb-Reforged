#include "links.h"
#include "qgraphicseffect.h"
#include "qicon.h"
#include "qscrollbar.h"
#include "ui_links.h"
#include "qssloader.h"
#include "uishadow.h"

links::links(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::links)
{
    ui->setupUi(this);


    this->setWindowTitle(tr("相关链接"));
    this->setWindowIcon(QIcon(":/ico/image/ico/link.svg"));

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->scrollArea);

    setShadow(ui->widget_11);
    setShadow(ui->widget_12);
    setShadow(ui->widget_14);
    setShadow(ui->widget_15);
}

links::~links()
{
    delete ui;
}

void links::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}
