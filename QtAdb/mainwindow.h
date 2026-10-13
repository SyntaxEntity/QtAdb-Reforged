#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QDir>
#include <QGraphicsEffect>
#include <QMessageBox>
#include <QSizePolicy>
#include <QScroller>
#include <QLineEdit>
#include <QMouseEvent>
#include "textexplainer.h"
#include "indexlistitem.h"
#include "basepage.h"
#include "pagemaker.h"
#include "about.h"
#include "usb_listener.h"
#include "threads/thread_monitor.h"
#include "devitem.h"
#include "abstract/comboboxlistwidget.h"

#include <QCoreApplication>
#include <QStringList>
#include <QFontDatabase>
#include <QFont>
#include <QMovie>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
signals:
    void adbDeviceChanged();

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    textExplainer *explainer = nullptr;
    pageMaker *maker = nullptr;

    QPointer<QWidget> getInfo = nullptr;   // 关闭即删，指针可能随时变空
    QPointer<QWidget> magiskSaver = nullptr;   // 关闭即删，指针可能随时变空
    QLineEdit * le_outIp = nullptr;
    QLineEdit * le_outPort = nullptr;
    QLineEdit * le_ipAddr = nullptr;
    QLineEdit * le_port = nullptr;
    QLineEdit * le_code = nullptr;

    int current_device;
    QList<device> devList;
    void refreshDevList();
    // 用 `adb devices -l` 的输出更新设备列表与界面（refreshDevList 的异步完成阶段）
    void applyDeviceList(const QString &output);
    // applyDeviceList() 拆出来的两段（原来都内联在里面）
    void rebuildDeviceCombo(const QList<int> &offline);        // 断连→清空→重填→重连
    void updateDeviceIndicators(const QList<int> &offline);    // 自动选中/置灰/顶部标签/监视器
    void addItemToIndex(indexListItem *itemWidget);
    void lock();
    void unlock();

private slots:
    void on_refreshButton_clicked();
    // 异步 ADB 请求完成（当前用于设备列表刷新）
    void onAdbRequestFinished(int requestId, int exitCode,
                              const QString &standardOutput, const QString &standardError);
    void setCurrentDevice(int index);
    void initBasePage(int key);
    void initSonPage(int key);
    void on_adbKillerBtn_clicked();

    void on_WIFIBtn_clicked();
    void connectWIFIDev();

    void on_WSABtn_clicked();
    void slot_taiChi();

    void on_cmdBtn_clicked();

    void on_pluginBtn_clicked();

    void slot_save_magisk();

private:
    Ui::MainWindow *ui = nullptr;

    void initEnvironmentPATH();
    void addIndexItems();
    void setStyles();

    // 无线调试窗口的两个表单区块（各自建一个面板返回）
    QWidget *buildWirelessOutPanel(QWidget *parent);
    QWidget *buildWirelessPairPanel(QWidget *parent);

    void resetMonitorThread(int);
    // 请求监视线程退出并断开其信号（线程自行结束后由 deleteLater 释放）
    void stopMonitorThread();

    bool taiChi = true;     //界面锁定定时器
    bool liangYi = false;   //判断刷新按钮槽函数是被谁调用：false：刷新按钮 click（）调用 ； true： 热插拔相关
    bool bagua = true;      //忘了是干啥的了

    //布尔 太极 为 真;

    bool firstBoot = true;
    QTimer *taiChiTimer = nullptr;

    basePage * currentPage = nullptr;
    usb_listener *listener = nullptr;

    thread_monitor *thread_mon = NULL;

    // 在途的设备列表请求号（0 表示没有）；用于避免重复排队并过滤异步结果
    int m_deviceListRequest = 0;
    // 设备热插拔轮询定时器（见构造函数中的说明）
    QTimer *m_deviceWatcher = nullptr;

    comboboxListWidget *view = nullptr;


public slots:
    void DevicePlugIn();
    void DevicePlugOut();
    void DeviceChanged();
    void slot_refreshDevList();     //如下
    void refreshDevListLater();     //与信号 usb_listener 连接的槽，在设备热插拔后启动定时器，超时1s后调用slot_refreshDevList();
    void slot_update_monitor(float,float);
    void reset_monitorBars();
};

#endif // MAINWINDOW_H
