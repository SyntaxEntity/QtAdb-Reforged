#ifndef QSSLOADER_H
#define QSSLOADER_H

#include <QString>

class QAbstractScrollArea;

// 读取 Qt 资源中的样式表。打开失败时返回空串并输出警告，
// 而不是像原实现那样直接 readAll()（未检查 QFile::open 的返回值）。
QString loadResourceStyleSheet(const QString &resourcePath);

// 把滚动条样式（默认 :/qss/scrollbar.qss）应用到某个滚动区域。
// 原实现在 13 处重复了「QFile + open + readAll + setStyleSheet」这一整段，
// 既不检查 open() 返回值，又在需要同时设置两个方向的地方连续调用两次 readAll()
// （第二次读到的必然是空串）。这里统一处理。
void applyScrollBarStyle(QAbstractScrollArea *area,
                         const QString &resourcePath = QStringLiteral("://qss/scrollbar.qss"));

#endif // QSSLOADER_H
