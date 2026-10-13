#ifndef SONPAGEBASE_H
#define SONPAGEBASE_H

#include <QWidget>

namespace Ui {
class sonPageBase;
}

class sonPageBase : public QWidget
{
    Q_OBJECT

public:
    explicit sonPageBase(QWidget *parent = nullptr);
    ~sonPageBase();


private:
    Ui::sonPageBase *ui = nullptr;
};

#endif // SONPAGEBASE_H
