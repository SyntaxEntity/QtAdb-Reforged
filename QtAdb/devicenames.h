#ifndef DEVICENAMES_H
#define DEVICENAMES_H

#include <QHash>
#include <QString>

// 设备别名（用户给设备起的名字）的持久化。
//
// 原实现的意图是把名字写回 ":/cfgs/cfg/name.cfg"，但那是编译进二进制的 Qt 资源，
// 运行时只读；这段代码被整体注释掉了，于是"命名"在三个层面都是坏的：
//   1. 输入框被 ui->lineEdit->hide() 隐藏，用户根本看不到入口；
//   2. 接收槽 comboboxListWidget::addName() 的函数体被注释，收到即丢弃；
//   3. 读取侧（setUpList 里的 nameList 解析）同样被注释，名字从未被应用。
//
// 这里改为写入用户可写位置（QStandardPaths::AppConfigLocation）下的 INI 文件，
// 并把"读/写/应用"三处接通。
namespace DeviceNames {

// 设备别名；未命名时返回空串
QString nameFor(const QString &addr);

// 保存设备别名；传入空串表示删除该记录。
// 返回是否真的写成功（false = 配置目录不可写、磁盘满等）。
// 原实现只调用 sync() 而不查 status()，写不进去也无从得知，
// 调用方还会照样打印"设备别名已保存"。
bool setName(const QString &addr, const QString &name);

// 全部别名（供调试与测试）
QHash<QString, QString> allNames();

// 存储文件路径（供提示与测试）
QString storagePath();

} // namespace DeviceNames

#endif // DEVICENAMES_H
