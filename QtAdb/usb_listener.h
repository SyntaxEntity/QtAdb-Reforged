#ifndef USB_LISTENER_H
#define USB_LISTENER_H

#include <QWidget>
#include <QAbstractNativeEventFilter>

#ifdef Q_OS_WIN
// Win32 设备热插拔通知：WM_DEVICECHANGE / DBT_DEVICEARRIVAL / DBT_DEVICEREMOVECOMPLETE
#  include <windows.h>
#  include <dbt.h>
#endif

// 设备插拔监听器
//   Windows ：通过 QAbstractNativeEventFilter 接收 WM_DEVICECHANGE 消息。
//   Linux/macOS：暂无原生实现，nativeEventFilter() 返回 false（仅支持手动刷新）。
//                跨平台热插拔抽象（Linux 用 udev 或 `adb track-devices`）见 M3 计划。
class usb_listener:public QWidget, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:

    void EmitMySignal();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void DeviceChangeCbk();
    void DeviceChanged();
    void DevicePlugIn();
    void DevicePlugOut();
};

#endif // USB_LISTENER_H
