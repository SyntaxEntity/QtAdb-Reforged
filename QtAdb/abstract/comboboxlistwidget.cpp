#include "comboboxlistwidget.h"
#include "../devicenames.h"
#include "qssloader.h"
#include "textexplainer.h"



comboboxListWidget::comboboxListWidget(QWidget *parent)
    : QListWidget{parent}
{
    explainer = new textExplainer();

    this->setResizeMode(QListView::Adjust);
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(this);
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

comboboxListWidget::~comboboxListWidget()
{

    // explainer 由本类 new，必须由本类释放（否则每次页面重建都泄漏一个）
    delete explainer;
}

void comboboxListWidget::setUpList(QList<device> devList, QList<int> off)
{
    // 原实现这里还有一段读 :/cfgs/cfg/name.cfg 的代码（只读资源，永不成功），已删除。

    for (int i = 0 ; i < devList.size() ; i++ )
    {
        devItem *wgt = new devItem(this);
        wgt->setAddr(devList[i].addr);
        wgt->setDevice(devList[i].device_product);
        wgt->setDevModel(devList[i].model);
        wgt->setStat(devList[i].state);

        // 设备别名（用户命名，持久化在可写配置目录）。
        // 注意：这里【不能】再跟一段"否则显示型号名"的回退 —— devItem::setName("")
        // 本身就会把型号名设成占位提示；原实现残留的 nameList 分支因为 nameList
        // 永远是空的，会走到 else 把刚设好的别名覆盖成型号名。
        wgt->setName(DeviceNames::nameFor(devList[i].addr));

        QListWidgetItem *item = new QListWidgetItem(this);

        this->addItem(item);
        this->setItemWidget(item, wgt);

        connect(wgt,SIGNAL(addName(QString,QString)),this,SLOT(addName(QString,QString)));
    }
}

void comboboxListWidget::addName(QString addr,QString name)
{
    // 原实现整个函数体被注释掉（它想写回只读的 Qt 资源 :/cfgs/cfg/name.cfg，
    // 那套设计不可能成功），于是"命名"收到即丢弃。这里改为真正持久化。
    if(addr.isEmpty())
    {
        return;
    }

    // 如实报告写入结果：写入失败时不能显示"已保存"。
    // 成功时不需要输出（原实现这里有一条 qDebug，已随调试输出一并清理）。
    if(!DeviceNames::setName(addr, name))
    {
        qWarning() << tr("设备别名保存失败（配置目录不可写？）:") << addr
                   << tr("文件:") << DeviceNames::storagePath();
    }
}
