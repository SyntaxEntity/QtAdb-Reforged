#include "sp_permissions.h"
#include "ui_sp_permissions.h"
#include "qssloader.h"
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_permissions::sp_permissions(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_permissions)
{
    ui->setupUi(this);
    pa = parent;

    ui->progressBar->hide();
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    thread = new adbThread(this);
    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));


    connect(thread,SIGNAL(signal_output(QString)),this,SLOT(refresh_tableView(QString)));







    applyDropShadow(ui->back_to_basePage);
    applyDropShadow(ui->refreshBtn);
    applyDropShadow(ui->back_to_basePage_2, Qt::green);
    applyDropShadow(ui->open_another_window);

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->tableView);

}

sp_permissions::~sp_permissions()
{
    delete ui;
}

void sp_permissions::on_back_to_basePage_2_clicked()
{
    mode = Mode::Other;
    arg.clear();

    // 原实现每次点击都 new 一个 QDialog 并覆盖成员指针，旧对话框及其子控件永不释放
    // —— 每点一次"选项"就泄漏一个对话框。这里先释放上一个，并让新对话框挂在 this 之下。
    delete settings;
    settings = new QDialog(this);
    group = new QButtonGroup(settings);

    // 5 个单选项原来逐个写"创建 → 设文本 → 进按钮组 → 进布局 → 连 setArg()"（25 行）。
    // 现在只有"按钮 / 文案"这一张表；与 setArg() 里的表一一对应。
    struct Option
    {
        QRadioButton **button;
        const char *text;        // QT_TR_NOOP：运行时再 tr()，仍可被 lupdate 提取
    };
    const Option options[] = {
        { &btn_g, QT_TR_NOOP("【高级】按组进行整理") },
        { &btn_f, QT_TR_NOOP("【高级】输出所有信息") },
        { &btn_s, QT_TR_NOOP("简短摘要（推荐）") },
        { &btn_d, QT_TR_NOOP("仅列出危险权限") },
        { &btn_u, QT_TR_NOOP("仅列出用户将看到的权限") },
    };
    // 说明：原实现这里有 5 行 `btn_x->autoExclusive();` —— 那是取值调用、没有效果
    // （本意应是 setAutoExclusive(true)）。它们已加入同一个 QButtonGroup，互斥本就成立。

    QVBoxLayout *layout = new QVBoxLayout();
    settings->setLayout(layout);

    for(const Option &option : options)
    {
        QRadioButton *button = new QRadioButton(settings);
        button->setText(tr(option.text));
        *option.button = button;
        group->addButton(button);
        layout->addWidget(button);
        connect(button, SIGNAL(clicked()), this, SLOT(setArg()));
    }

    closeBtn = new QPushButton(settings);
    closeBtn->setText(tr("确定"));
    layout->addWidget(closeBtn);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(setArg()));
    connect(closeBtn, SIGNAL(clicked()), settings, SLOT(close()));
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(on_refreshBtn_clicked()));

    settings->setStyleSheet("background-color:white;border:0px solid white;border-radius:4px;");
    settings->setWindowTitle(tr("选项"));
    settings->move(ui->back_to_basePage_2->geometry().x(), ui->back_to_basePage_2->geometry().y());
    settings->exec();

    // 读取失败时给出警告并跳过（原实现不检查 QFile::open 的返回值）
    applyScrollBarStyle(ui->tableView);
}

void sp_permissions::setArg()
{
    // 六个分支原本各自"设 arg + 重置 5 个布尔量"（同一组赋值重复 6 遍，共 36 行）。
    // 现在只有"一个模式 + 一个参数串"，二者的对应关系就在下面这张表里。
    // 说明：原实现里的 arg_s / arg_d / arg_u 只被赋值、从未被读取（分派只看 arg_g / arg_f），
    // 属于死状态，这里随枚举一起消失。
    struct ModeEntry
    {
        QRadioButton *button;
        Mode mode;
        const char *argument;
    };
    const ModeEntry entries[] = {
        { btn_g, Mode::Grouped,   "-g" },
        { btn_f, Mode::Filtered,  "-f" },
        { btn_s, Mode::Other,     "-s" },   // 与 -d / -u 一样走"简短摘要"
        { btn_d, Mode::Other,     "-d" },
        { btn_u, Mode::Other,     "-u" },
    };

    mode = Mode::Other;
    arg.clear();
    for(const ModeEntry &entry : entries)
    {
        if(entry.button != nullptr && entry.button->isChecked())
        {
            mode = entry.mode;
            arg = QString::fromLatin1(entry.argument);
            break;
        }
    }
}

void sp_permissions::setDev(device device)
{
    dev = device;
    on_refreshBtn_clicked();
}

void sp_permissions::on_refreshBtn_clicked()
{
    // arg 是过滤开关（"-g" / "-f" / "-s" / "-d" / "-u"）：原实现拼接到命令串尾部，
    // 这里等价地直接并入参数表。
    QStringList cmd = AdbCmd::pmList("permissions");
    cmd += arg.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    thread->initThread(cmd, dev, "#PERMISSIONGROUPS#");
    ui->progressBar->setVisible(true);
    thread->start();
}

// ── 三种呈现方式各自独立成函数 ────────────────────────────────
// 原来三套填表逻辑都挤在 refresh_tableView() 里（150+ 行、共享一堆局部变量），
// 拆开后每个函数只关心自己那张表怎么填。分派见 refresh_tableView()。
void sp_permissions::fillGroupedTable(const QString &s)
{
    QStringList groupList = s.split("group:");
    QList<QStringList> groups;



    for(int i = 0; i < groupList.count(); i++)
    {
        groups.append(groupList[i].split("\n"));
    }


    QStandardItemModel* model = new QStandardItemModel(this);

    if(!groupList.isEmpty())
    {
        model->setColumnCount(groups.count());     // 列数只需设一次
        for(int i = 1; i < groups.count(); i++)
        {
            model->setHeaderData(i-1, Qt::Horizontal, groups[i][0]);

            for(int row = 1; row < groups[i].count(); row++)
            {
                model->setItem(row-1, i-1, new QStandardItem(groups[i][row].simplified().simplified()));
            }
        }

        ui->tableView->horizontalHeader()->setDefaultSectionSize(150);
        installTableModel(ui->tableView, model);

        this->update();
    }

    ui->progressBar->setVisible(false);
    // 数据为空时也要让视图有个（空）模型：走同一个助手，旧模型由它释放。
    if(ui->tableView->model() == nullptr)
    {
        installTableModel(ui->tableView, new QStandardItemModel(this));
    }

}

void sp_permissions::fillFilteredTable(const QString &s)
{
    QStringList groupList = s.split("+");
    QList<QStringList> groups;



    for(int i = 0; i < groupList.count(); i++)
    {
        groups.append(groupList[i].split("\n"));
    }


    QStandardItemModel* model = new QStandardItemModel(this);

    if(!groupList.isEmpty())
    {
        model->setColumnCount(5);
        model->setHeaderData(0,Qt::Horizontal, "permission");
        model->setHeaderData(1,Qt::Horizontal, "package");
        model->setHeaderData(2,Qt::Horizontal, "label");
        model->setHeaderData(3,Qt::Horizontal, "description");
        model->setHeaderData(4,Qt::Horizontal, "protectionLevel");

        for(int i = 1; i < groups.count();i++)
        {

            for(int j = 0; j< groups[i].count() && j <= 5;j++)
            {
                QStringList tempList;
                tempList = groups[i][j].simplified().simplified().split(":");

                if(tempList.count() >= 2)
                {
                    model->setItem(i-1,j,new QStandardItem(tempList[1]));
                }
            }
        }

        ui->tableView->horizontalHeader()->setDefaultSectionSize(150);
        installTableModel(ui->tableView, model);

        this->update();
    }

    ui->progressBar->setVisible(false);
    // 数据为空时也要让视图有个（空）模型：走同一个助手，旧模型由它释放。
    if(ui->tableView->model() == nullptr)
    {
        installTableModel(ui->tableView, new QStandardItemModel(this));
    }

}

void sp_permissions::refresh_tableView(QString s)
{
    // 两个特殊呈现方式 + 其余（-s / -d / -u / 默认）统一走"简短摘要"。
    // 原实现用 5 个布尔量表达同一件事，其中 3 个还从未被读取。
    if(mode == Mode::Grouped)
    {
        fillGroupedTable(s);
    }
    else if(mode == Mode::Filtered)
    {
        fillFilteredTable(s);
    }
    else
    {
        generalMode(s);
    }

// 数据为空时也要让视图有个（空）模型：走同一个助手，旧模型由它释放。
        if(ui->tableView->model() == nullptr)
        {
            installTableModel(ui->tableView, new QStandardItemModel(this));
        }
    }
void sp_permissions::on_open_another_window_clicked()
{
    // 列表刷新由 adbThread 异步完成；若用户在此之前点击“查看”，
    // current_model 仍为 nullptr，原实现直接交给 setModel() → 崩溃。
    if(installedTableModel() == nullptr)
    {
        QMessageBox::information(this, tr("查看"), tr("数据尚未加载完成，请先点击刷新。"));
        return;
    }

    QWidget *wgt = new QWidget();
    // 顶层窗口此前从不释放（每点击一次泄漏一个），改为关闭时自动删除
    wgt->setAttribute(Qt::WA_DeleteOnClose);
    wgt->setWindowTitle(tr("查看"));
    wgt->setStyleSheet("background-color:white;border:0px solid white;");
    QVBoxLayout *layout = new QVBoxLayout();
    QTableView *view = new QTableView();
    view->setModel(installedTableModel());
    view->setStyleSheet("background-color:white;border:0px;");
    wgt->setLayout(layout);
    layout->addWidget(view);
    wgt ->showMaximized();
}

void sp_permissions::generalMode(QString s)
{
    QStringList permissionList = s.split("\n");
    QStandardItemModel* model = new QStandardItemModel(this);

    if(!permissionList.isEmpty())
    {
        model->setHeaderData(0,Qt::Horizontal, permissionList[0]);
        // 从第 3 行开始（前两行是标题与分隔），空行跳过；
        // row 是"实际写入的目标行号"，因此不能直接用 i-2 代替。
        int row = 0;
        for(int i = 2; i < permissionList.count(); i++)
        {
            if(permissionList[i].isEmpty() || permissionList[i] == "\r")
            {
                continue;
            }
            model->setItem(row, 0, new QStandardItem(permissionList[i].simplified().simplified()));
            ++row;
        }
        // 表头默认列宽按"父页面宽度 - 200"估算。这里修两处健壮性问题：
        //   ① pa 可能为空（页面没有父对象时 pa==nullptr，直接 SEGV；
        //      用 sp_permissions(nullptr) 即可复现），改为回退到表格自身宽度；
        //   ② 窄窗口下 width()-200 会算出负数，列宽为负没有意义，夹到下限。
        const int parentWidth = pa != nullptr ? pa->geometry().width() : ui->tableView->width();
        ui->tableView->horizontalHeader()->setDefaultSectionSize(qMax(120, parentWidth - 200));
        installTableModel(ui->tableView, model);
        this->update();
    }

    ui->progressBar->setVisible(false);
}
