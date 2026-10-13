#include "sp_permissiongroups.h"
#include "ui_sp_permissiongroups.h"
#include "qssloader.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_permissionGroups::sp_permissionGroups(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_permissionGroups)
{
    ui->setupUi(this);
    setParent(parent);

    ui->progressBar->hide();
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    thread = new adbThread(this);
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    connect(thread,SIGNAL(signal_output(QString)),this,SLOT(refresh_tableView(QString)));




    applyDropShadow(ui->back_to_basePage);
    applyDropShadow(ui->refreshBtn);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->listView);
}

sp_permissionGroups::~sp_permissionGroups()
{
    delete ui;
}

void sp_permissionGroups::on_refreshBtn_clicked()
{
    thread->initThread(AdbCmd::pmList("permission-groups"),dev,"#PERMISSIONGROUPS#");
    ui->progressBar->setVisible(true);
    thread->start();
}

void sp_permissionGroups::refresh_tableView(QString s)
{
    QStringList permissionList = explainer.explainPermissionGroups(s);
    if(!permissionList.isEmpty())
    {
        // 模型只在实际有数据时创建：原实现在判断之前就 new，
        // 列表为空时既不安装也不释放，于是每次刷新泄漏一个模型
        // （--repeat 探针在 sp_accounts 上实测到 6 次刷新漏 6 个）。
        QStandardItemModel* model = new QStandardItemModel(this);
        for(int i = 0; i < permissionList.count(); i++)
        {
            model->setItem(i,0,new QStandardItem(permissionList[i]));
        }
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

void sp_permissionGroups::setDev(device device)
{
    dev = device;
    on_refreshBtn_clicked();
}
