#include "sp_packages.h"
#include "ui_sp_packages.h"
#include "qssloader.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_packages::sp_packages(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_packages)
{
    ui->setupUi(this);

    pa = parent;

    ui->progressBar->hide();
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    thread = new adbThread(this);
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));


    connect(thread,SIGNAL(signal_output(QString)),this,SLOT(refresh_tableView(QString)));
    connect(ui->checkBox_d,SIGNAL(clicked()),this,SLOT(setArgs()));
    connect(ui->checkBox_e,SIGNAL(clicked()),this,SLOT(setArgs()));
    connect(ui->checkBox_s,SIGNAL(clicked()),this,SLOT(setArgs()));
    connect(ui->checkBox_3,SIGNAL(clicked()),this,SLOT(setArgs()));




    applyDropShadow(ui->back_to_basePage);
    applyDropShadow(ui->refreshBtn);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->listView);

}

sp_packages::~sp_packages()
{
    delete ui;
}

void sp_packages::setDev(device device)
{
    dev = device;
    on_refreshBtn_clicked();
}

void sp_packages::on_refreshBtn_clicked()
{
    // arg 是过滤开关（" -e" / " -d" / " -s" / " -3"）：
    // 原实现把它拼到命令串尾部再按空格切分，这里等价地直接并入参数表。
    QStringList cmd = AdbCmd::pmList("packages");
    cmd += arg.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    thread->initThread(cmd, dev, "#PERMISSIONGROUPS#");
    ui->progressBar->setVisible(true);
    thread->start();
    arg.clear();
}

void sp_packages::refresh_tableView(QString s)
{
    QStringList packagesList = explainer.explainPermissionGroups(s);
    if(!packagesList.isEmpty())
    {
        // 模型只在实际有数据时创建：原实现在判断之前就 new，
        // 列表为空时既不安装也不释放，于是每次刷新泄漏一个模型。
        QStandardItemModel* model = new QStandardItemModel(this);
        for(int i = 0; i < packagesList.count(); i++)
        {
            QStringList tempList;
            tempList = packagesList[i].simplified().split(":");

            if(tempList.count() >= 2)
            {
                model->setItem(i,0,new QStandardItem(tempList[1]));
            }
        }
        ui->listView->setStyleSheet("QListView::item{height:24px;}");
        model->sort(Qt::AscendingOrder);
        installTableModel(ui->listView, model);
        this->update();
    }

    ui->progressBar->setVisible(false);


    // 数据为空时也要让视图有个（空）模型：同样走 installTableModel()，
    // 上一个模型由助手释放。每轮刷新只调用一次 setModel —— 次数越少，
    // 视图重建选择模型的次数越少（选择模型要等页面销毁才回收）。
    if(ui->listView->model() == nullptr)
    {
        installTableModel(ui->listView, new QStandardItemModel(this));
    }
}

void sp_packages::on_listView_clicked(const QModelIndex &index)
{
    QString name = ui->listView->model()->data(index).toString();
    tp_packageProfile * profilePage = new tp_packageProfile(NULL,name);
    profilePage->setDevice(dev);
    connect(profilePage,SIGNAL(packageChanged()),this,SLOT(on_refreshBtn_clicked()));
    profilePage->show();
}

void sp_packages::setArgs()
{
    if(ui->checkBox_e->isChecked())
    {
        arg.append(" -e");
    }

    if(ui->checkBox_d->isChecked())
    {
        arg.append(" -d");
    }

    if(ui->checkBox_s->isChecked())
    {
        arg.append(" -s");
    }

    if(ui->checkBox_3->isChecked())
    {
        arg.append(" -3");
    }

    on_refreshBtn_clicked();
}
