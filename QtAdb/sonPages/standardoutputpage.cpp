#include "standardoutputpage.h"
#include "ui_standardoutputpage.h"
#include <QScrollBar>
#include <QTextCursor>
#include "uishadow.h"

standardOutputPage::standardOutputPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::standardOutputPage)
{
    ui->setupUi(this);

    // 原先这里建了一个 30Hz 定时器连到 SLOT(update())——那实际匹配的是
    // QWidget::update()（重绘），与 update(QString) 无关：既不会刷新文本，
    // 又白白触发每秒 30 次重绘。文本已由 update(QString) 增量刷新，定时器已删除。
    ui->textBrowser->setText(text);

    applyDropShadow(ui->textBrowser);

    applyDropShadow(ui->refreshBtn);

    applyDropShadow(ui->copyBtn);

    applyDropShadow(ui->clearBtn);
}

standardOutputPage::~standardOutputPage()
{
    delete ui;
}

void standardOutputPage::update(QString s)
{
    text.append(s);
    // 只追加新增内容：原实现每收到一块输出就 setText(全量文本)，
    // 长输出（安装/侧载日志）下是 O(n²)，会明显卡顿。
    ui->textBrowser->moveCursor(QTextCursor::End);
    ui->textBrowser->insertPlainText(s);
    // 自动滚动到底部，便于观察实时输出
    QScrollBar *bar = ui->textBrowser->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void standardOutputPage::on_refreshBtn_clicked()
{
    ui->textBrowser->setText(text);
}

void standardOutputPage::on_clearBtn_clicked()
{
    ui->textBrowser->clear();
    text.clear();
}

void standardOutputPage::on_copyBtn_clicked()
{
    QClipboard *clip = QApplication::clipboard();
    clip->setText(ui->textBrowser->toPlainText());
}
