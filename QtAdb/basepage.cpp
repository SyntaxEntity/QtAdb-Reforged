#include "basepage.h"
#include "ui_basepage.h"
#include "qssloader.h"
#include "textexplainer.h"

basePage::basePage(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::basePage)
{
    ui->setupUi(this);
    SPManager = new sonPageManager;
    // 第三个参数是 parent：原实现省略了它，动画没有父对象 ——
    // 页面销毁时不会随之释放，每建一次页面就泄漏一个（LSan 的栈就指向这一行）。
    animation_listHide = new QPropertyAnimation(ui->listWidget, "geometry", this);

    sonPage = NULL;

/*******原来的
    ui->listWidget->setStyleSheet("QListWidget{background-color:rgba(255,255,255,0);border-width:1px;border-radius:4px;outline:0px;}"
                                  "QListView::item:hover {"
                                  "background-color: rgba(250, 250, 250, 0.9);"
                                  "padding: 0px;"
                                  "border: 1px solid #BDBDBD;"
                                  "}"
                                  "QListView::item:selected {"
                                  "background-color: rgba(245, 245, 245, 0.9);"
                                  "color: black;"
                                  "padding: 0px;"
                                  "border: 2px solid #BDBDBD;"
                                  "}"
                                  "QListView::item{height:70px;border: 1px solid #BDBDBD;border-radius:4px;margin:6px 0px 0px 6px;}"
                                  );*/
    ui->listWidget->setStyleSheet("QListWidget{background-color:rgba(255,255,255,0);border:0px;border-radius:4px;outline:0px;}"
                                  "QListView::item:hover {"
                                  "background-color: #EEEEEE;"
                                  "padding: 0px;"
                                  "border: 0px solid #E0E0E0;"
                                  "}"
                                  "QListView::item:selected {"
                                  "background-color: #EEEEEE;"
                                  "color: black;"
                                  "padding: 0px;"
                                  "border: 0px solid #BDBDBD;"
                                  "}"
                                  "QListView::item{height:70px;border: 0px solid #BDBDBD;border-radius:4px;margin:6px 0px 0px 6px;}"
                                  );
    ui->listWidget->setViewMode(QListView::ListMode);
    ui->listWidget->setFlow(QListView::TopToBottom);
    ui->listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->listWidget);


    connect(ui->listWidget,SIGNAL(currentRowChanged(int)),this,SLOT(slot_createSonPage(int)));

}

basePage::~basePage()
{

    delete SPManager;
    delete ui;
}
void basePage::addItemsToList(pageListItem *itemWidget)
{
    QListWidgetItem *tmpItem = new QListWidgetItem();
    ui->listWidget->addItem(tmpItem);
    ui->listWidget->setItemWidget(tmpItem, itemWidget);
}

void basePage::setFather(QWidget *parent)
{
}

void basePage::slot_createSonPage(int key)
{
    if(isEnable(key) && key >= 0)
    {
        this->setDisabled(true);

        listTimer = new QTimer(this);
        connect(listTimer, SIGNAL(timeout()), this, SLOT(slot_spgAnimationEnd()));
        listTimer->setSingleShot(true);
        listTimer->start(300);
        emit creatingSonPage();

        ui->listWidget->setCurrentRow(-1);
        sonPage = SPManager->selector(this,whoAmI(),key,dev);
        if(sonPage != NULL)
        {

            /**/


            animation_listHide->setDuration(300);
            animation_listHide->setEndValue(QRect(ui->listWidget->geometry().x(),ui->listWidget->geometry().y() + 50, ui->listWidget->width(),ui->listWidget->height()));
            animation_listHide->setStartValue(ui->listWidget->geometry());
            animation_listHide->setEasingCurve(QEasingCurve::OutQuart);
            animation_listHide->start();

            /**/
        }
    }
    ui->listWidget->setCurrentRow(-1);
}

void basePage::spg_return_pushed()
{
    sonPage->playExitAnimation_sp();
    QTimer *animationTimer = new QTimer(this);
    animationTimer->setSingleShot(true);
    animationTimer->start(300);
    connect(animationTimer, SIGNAL(timeout()), this, SLOT(slot_destroySonPage()));
}

void basePage::slot_destroySonPage()
{
    if(sonPage == NULL)
        throw "trying to destroy sonPage, but sonPage == NULL;";

    delete sonPage;
    sonPage = NULL;

    ui->listWidget->setVisible(true);
    playLoadAnimation_bp(this->height());
}

QString basePage::whoAmI()
{
    return myName;
}

void basePage::whoYouAre(QString name)
{
    myName = name;
}



void basePage::enableAllItems()
{
    QList<int> val;
    const int count = ui->listWidget->count();   // 已加入的条目数
    for(int i = 1; i <= count; ++i)
    {
        val.append(i);
    }
    setEnableValue(val);
}

void basePage::setEnableValue(const QList<int> &val)
{
    enableVal = val;
}

bool basePage::isEnable(int k) const
{
    // 原实现是"遍历 20 个槽位找 k+1"，空槽用 0 表示；
    // 改用 QList 后语义不变（等价于集合查找），也不再需要固定上限。
    return enableVal.contains(k + 1);
}

void basePage::setDev(device device)
{
    dev = device;
}

void basePage::self_castrate()
{
    delete ui->listWidget;
    delete ui->mainLayout;
    delete ui->verticalLayout;
}

void basePage::unlock()
{
    this->setDisabled(false);
}

void basePage::slot_spgAnimationEnd()
{
    ui->listWidget->setVisible(false);
    unlock();
    ui->mainLayout->addWidget(sonPage);
    sonPage->playLoadAnimation_sp(this->height());
}
