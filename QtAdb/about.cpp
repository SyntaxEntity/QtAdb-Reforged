#include "about.h"
#include "ui_about.h"
#include "uishadow.h"

about::about(QWidget *parent) :
    basePage(parent),
    ui(new Ui::about)
{
    self_castrate();
    ui->setupUi(this);

    applyDropShadow(ui->btn_website);

    applyDropShadow(ui->btn_coolapk);

    applyDropShadow(ui->btn_github);

    // 关于页只保留 GitHub 一个入口（指向本仓库）。其余入口（官网、酷安、QA、QQ 频道/群）
    // 属于旧账号时期的分发渠道，先隐藏 —— 需要恢复时删掉这一段即可。
    for(QPushButton *b : {ui->btn_website, ui->btn_links, ui->btn_coolapk,
                          ui->btn_qq_dis, ui->btn_qgroup, ui->btn_money_2})
    {
        if(b != nullptr)
        {
            b->hide();
        }
    }

    // 这一项（打赏按钮）原实现里就是注释掉的，保持不启用

    applyDropShadow(ui->btn_money_2, QColorConstants::Svg::orange);

    applyDropShadow(ui->btn_qq_dis);

    applyDropShadow(ui->btn_qgroup);

    applyDropShadow(ui->btn_version);

    applyDropShadow(ui->btn_links, QColorConstants::Svg::skyblue);

    // 原实现这里构造了一个局部 QFont 但从未使用（setFont 那行是注释），已删除

    // 三个打赏按钮原实现里隐藏了，但相关代码已被注释，界面本就不显示
}

about::~about()
{
    delete ui;
}

void about::on_btn_website_clicked()
{
    QDesktopServices::openUrl(QUrl("https://lapplandsp.github.io/QtAdb-Pages/", QUrl::TolerantMode));
}

void about::on_btn_coolapk_clicked()
{
    QDesktopServices::openUrl(QUrl("http://www.coolapk.com/u/692559", QUrl::TolerantMode));
}

void about::on_btn_github_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/SyntaxEntity/QtAdb-Reforged", QUrl::TolerantMode));
}

void about::on_btn_money_2_clicked()
{
    pay * payPage = new pay();
    payPage->setStyleSheet("background-color:rgba(255,255,255,1);");
    payPage->setWindowIcon(QIcon(":/ico/image/ico/about/cola.svg"));
    payPage->setBaseSize(300,500);
    payPage->setWindowTitle(tr("请我喝可乐"));
    payPage->show();
}

void about::on_btn_links_clicked()
{
    links *linksPage = new links();
    linksPage->show();
}

void about::on_btn_version_clicked()
{
    QDesktopServices::openUrl(QUrl("https://lapplandsp.github.io/QtAdb-Pages/qa/", QUrl::TolerantMode));
}

void about::on_btn_qq_dis_clicked()
{
    QDesktopServices::openUrl(QUrl("https://pd.qq.com/s/k96ej1", QUrl::TolerantMode));
}

void about::on_btn_qgroup_clicked()
{
    QDesktopServices::openUrl(QUrl("http://qm.qq.com/cgi-bin/qm/qr?_wv=1027&k=5s7RHXEhHg54Zng2B9NgOJ1f1LT1j-JU&authKey=Sxp0Mgvc0F8Esh6YuANcWe4n1abjXvZ%2BPPMfw8pIf85FEdRuRxGgbwpxcBGyaGR4&noverify=0&group_code=860105291", QUrl::TolerantMode));
}

