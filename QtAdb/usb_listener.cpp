#include "usb_listener.h"
#include <QDebug>

bool usb_listener::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);

    MSG* msg = reinterpret_cast<MSG*>(message);
    const unsigned int msgType = msg->message;

    if(msgType == WM_DEVICECHANGE) {
        emit DeviceChangeCbk();
        emit DeviceChanged();

        if(msg->wParam == DBT_DEVICEARRIVAL)
        {
            emit DevicePlugIn();         //触发信号
        }
        if(msg->wParam == DBT_DEVICEREMOVECOMPLETE)
        {
            emit DevicePlugOut();        //触发信号
        }
    }

    // 事件过滤器约定：返回 true 表示事件已被处理并停止继续分发。
    // 原实现返回 QWidget::nativeEvent(...)，那是 widget 自己的事件处理入口，
    // 语义错误（过滤器不是事件接收者），此处明确返回 false。
    return false;
#else
    // Linux 上没有原生设备通知：本过滤器返回 false（未处理），热插拔由
    // MainWindow 里的定时器轮询兜底 —— m_deviceWatcher 定时调用 slot_refreshDevList()，
    // 走的是与 Windows 原生通知**完全相同**的下游路径（见 mainwindow.cpp 的说明）。
    // 若将来要换成 udev 或 `adb track-devices`，只需在这里发同样的信号。
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
#endif
}

void usb_listener::EmitMySignal()
{
    emit DevicePlugIn();
}
