#include "sp_captive_portal.h"
#include "ui_sp_captive_portal.h"
#include "adbclient.h"
#include <QPointer>
#include "textexplainer.h"
#include "adbcommands.h"
#include "uishadow.h"

sp_captive_portal::sp_captive_portal(QWidget *parent) :
    animationWidget(parent),
    ui(new Ui::sp_captive_portal)
{
    ui->setupUi(this);
    setParent(parent);

    connect(this->ui->back_to_basePage,SIGNAL(clicked()),parent,SLOT(slot_destroySonPage()));

    setShadow(ui->label);
    setShadow(ui->label_3);
    setShadow(ui->label_5);
    setShadow(ui->android_version);
    setShadow(ui->back_to_basePage);
    setShadow(ui->current_server);
    setShadow(ui->restore);
    setShadow(ui->modify);
    setShadow(ui->pushButton);

    QStringList serverLists;
    serverLists.append("connect.rom.miui.com/generate_204");
    serverLists.append("connectivitycheck.platform.hicloud.com/generate_204");
    serverLists.append("wifi.vivo.com.cn/generate_204");
    serverLists.append("www.qualcomm.cn/generate_204");
    serverLists.append("edge.microsoft.com/captiveportal/generate_204");
    serverLists.append("cp.cloudflare.com/generate_204");
    serverLists.append("g.cn/generate_204");
    serverLists.append("google.cn/generate_204");
    serverLists.append("dl.google.com/generate_204");
    ui->comboBox->addItems(serverLists);
    ui->comboBox->setStyleSheet("QComboBox{color:black; border:0px; border-bottom:1px solid #BDBDBD; border-radius:0px; background-color:rgba(255,255,255,255);}"
                                "QComboBox::drop-down{border: 0px solid rgba(255,255,255,0);background-color:rgba(255,255,255,0);border-bottom-right-radius: 0px;}"
                                "QComboBox QAbstractItemView{border:1px solid #BDBDBD;border-radius:0px 0px 0px 0px;outline: 0px;}"
                                "QComboBox QAbstractItemView::item{height:30px;border:0px solid #BDBDBD;border-radius:0px 0px 0px 0px;}"
                                "QComboBox QAbstractItemView::item:hover{height:30px;border:0px solid #BDBDBD;border-radius:0px 0px 0px 0px;color:black}"
                                "QComboBox QAbstractItemView::item:selected{height:30px;border:1px solid #BDBDBD;border-radius:0px 0px 0px 0px;color:black}"
                                );



}

sp_captive_portal::~sp_captive_portal()
{
    delete ui;
}

void sp_captive_portal::setDev(device device)
{
    dev = device;
    refresh();
}

void sp_captive_portal::setShadow(QWidget* wgt)
{
    applyDropShadow(wgt);
}

void sp_captive_portal::refresh()
{
    // 两次读取存在依赖（先取版本号，再决定是否查询验证服务器），
    // 原实现是两次同步等待（合计最长 40s 界面无响应）→ 改为嵌套回调。
    QPointer<sp_captive_portal> self(this);

    AdbClient::instance()->runWithCallback(
        {"-s", dev.addr, "shell", "getprop", "ro.build.version.release"},
        [self](int, const QString &version, const QString &) {
            if(!self) {
                return;
            }
            self->ui->android_version->setText(version.simplified());

            // 取主版本号首位。设备未返回版本号时为空串，
            // 原实现对空串取 [0] 属越界访问（Qt 6 下为断言/未定义行为）。
            const QString ver = version.simplified().left(1);
            const bool needsServerQuery = (ver.toInt() < 7 && ver.toInt() >= 5);

            self->ui->current_server->setVisible(needsServerQuery);
            self->ui->label->setVisible(needsServerQuery);

            if(!needsServerQuery) {
                return;
            }

            //adb shell settings get global captive_portal_server
            QPointer<sp_captive_portal> nested = self;
            AdbClient::instance()->runWithCallback(
                {"-s", nested->dev.addr, "shell", "settings", "get", "global", "captive_portal_server"},
                [nested](int, const QString &server, const QString &) {
                    if(nested) {
                        nested->ui->current_server->setText(server.simplified());
                    }
                });
        });

    ui->comboBox->setPlaceholderText(tr("服务器未选择"));
    ui->comboBox->setCurrentIndex(-1);
}

void sp_captive_portal::on_modify_clicked()
{
    if(ui->comboBox->currentIndex() >= 0)
    {
        QMessageBox::StandardButton result=QMessageBox::question(NULL, tr("修改"),tr("您确定要将验证服务器修改为") + ui->comboBox->currentText() + tr("吗？"));
        if(result == QMessageBox::Yes)
        {
                // 必须与 Yes 显式比较：QMessageBox::No == 0x10000 为真值，
                // 原来的 if(result) 会把“否”当成“是”而执行修改。
                const QString verText = ui->android_version->text().simplified();
                const QString ver = verText.left(1);
                if(ver.toInt() < 7 && ver.toInt() >= 5)
                {
                    // 命令由 AdbCmd 具名构造，拼错参数位置会直接编译不过
                    AdbClient::instance()->runForDevice(
                        dev.addr, AdbCmd::settingsPut("global", "captive_portal_server",
                                                      "http://" + ui->comboBox->currentText()));
                }
                else
                {
                    AdbClient::instance()->runForDevice(
                        dev.addr, AdbCmd::settingsPut("global", "captive_portal_http_url",
                                                      "http://" + ui->comboBox->currentText()));
                    AdbClient::instance()->runForDevice(
                        dev.addr, AdbCmd::settingsPut("global", "captive_portal_https_url",
                                                      "https://" + ui->comboBox->currentText()));
                }
                QMessageBox::information(NULL, tr("修改"),tr("已修改"));


            refresh();
        }
    }
    else
    {
        ui->comboBox->setPlaceholderText(tr("服务器未选择"));
    }
}

void sp_captive_portal::on_restore_clicked()
{
    QMessageBox::StandardButton result=QMessageBox::question(NULL, tr("重置"),tr("您确定要重置吗？"));

    if(result == QMessageBox::Yes)
    {
        // 同上：No 为真值，必须显式判断 Yes
        const QString verText = ui->android_version->text().simplified();
        const QString ver = verText.left(1);
        if(ver.toInt() < 7 && ver.toInt() >= 5)
        {
            AdbClient::instance()->runForDevice(
                dev.addr, AdbCmd::settingsDelete("global", "captive_portal_server"));
        }
        else
        {
            AdbClient::instance()->runForDevice(
                dev.addr, AdbCmd::settingsDelete("global", "captive_portal_https_url"));
            AdbClient::instance()->runForDevice(
                dev.addr, AdbCmd::settingsDelete("global", "captive_portal_http_url"));
        }
        QMessageBox::information(NULL, tr("重置"),tr("已重置"));
        refresh();
    }
}



//屎山
void sp_captive_portal::on_delete_directly_clicked()
{

}
