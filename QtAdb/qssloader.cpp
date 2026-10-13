#include "qssloader.h"

#include <QAbstractScrollArea>
#include <QDebug>
#include <QFile>
#include <QScrollBar>

QString loadResourceStyleSheet(const QString &resourcePath)
{
    QFile file(resourcePath);
    if(!file.open(QFile::ReadOnly))
    {
        qWarning() << "无法读取样式资源：" << resourcePath << "，原因：" << file.errorString();
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

void applyScrollBarStyle(QAbstractScrollArea *area, const QString &resourcePath)
{
    if(area == nullptr)
    {
        return;
    }

    const QString styleSheet = loadResourceStyleSheet(resourcePath);
    if(styleSheet.isEmpty())
    {
        return;
    }

    area->verticalScrollBar()->setStyleSheet(styleSheet);
    area->horizontalScrollBar()->setStyleSheet(styleSheet);
}
