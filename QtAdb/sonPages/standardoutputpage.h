#ifndef STANDARDOUTPUTPAGE_H
#define STANDARDOUTPUTPAGE_H

#include <QWidget>
#include <QTimer>
#include <QClipboard>
#include <QGraphicsDropShadowEffect>

namespace Ui {
class standardOutputPage;
}

class standardOutputPage : public QWidget
{
    Q_OBJECT

public:
    explicit standardOutputPage(QWidget *parent = nullptr);
    ~standardOutputPage();
    QString text;

private:
    Ui::standardOutputPage *ui = nullptr;

public slots:
    void update(QString);
private slots:
    void on_refreshBtn_clicked();
    void on_clearBtn_clicked();
    void on_copyBtn_clicked();
};

#endif // STANDARDOUTPUTPAGE_H
