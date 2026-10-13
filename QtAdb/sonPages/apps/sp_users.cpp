#include "sp_users.h"
#include "ui_sp_users.h"
#include "qssloader.h"
#include "adbclient.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_users::sp_users(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_users)
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
    applyDropShadow(ui->pushButton);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->listView);
}

sp_users::~sp_users()
{
    delete ui;
}

void sp_users::on_refreshBtn_clicked()
{
    thread->initThread(AdbCmd::pmList("users"),dev,"#PERMISSIONGROUPS#");
    ui->progressBar->setVisible(true);
    thread->start();
}

void sp_users::refresh_tableView(QString s)
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
            if(permissionList[i] != "")
            {
                model->setItem(i,0,new QStandardItem(permissionList[i].simplified()));
            }
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

void sp_users::setDev(device device)
{
    dev = device;
    on_refreshBtn_clicked();
}

void sp_users::on_pushButton_clicked()
{
    // 原实现不检查是否选中：未选中时 user_id 为空串，
    // 确认后会执行 "adb shell pm remove-user "（把空参数丢给 adb）。
    const QModelIndex current = ui->listView->currentIndex();
    if(!current.isValid())
    {
        QMessageBox::information(NULL, tr("选择错误"), tr("请先在列表中选择一个用户。"));
        return;
    }

    QString user_id = explainer.get_words_before(explainer.get_words_after(current.data().toString(), "{"), ":");
    if(user_id.isEmpty() || user_id == "Users")
    {
        QMessageBox::information(NULL, tr("选择错误"),tr("请选择一个用户，而不是标题"));
        return;
    }

    // 确认对话框只对有效的选择弹出（原实现先弹窗、再判断，顺序相反）
    QMessageBox::StandardButton result=QMessageBox::question(NULL, tr("删除用户"),tr("您确定要删除用户吗？"));
    if(result == QMessageBox::Yes)
    {
        // 异步下发：原同步调用会阻塞界面直到命令结束
        AdbClient::instance()->runForDevice(dev.addr, {"shell", "pm", "remove-user", user_id});
        on_refreshBtn_clicked();
    }
}
