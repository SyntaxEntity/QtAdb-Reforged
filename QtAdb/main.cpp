#include "mainwindow.h"
#include "adbclient.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QCoreApplication>
#include <QStringList>
#include <QFontDatabase>
#include <QFont>
#include <QSplashScreen>
#include <QThread>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);


    // 用 addApplicationFont() 的返回值取真实字体族名。
    // 原实现忽略返回值、并把族名硬编码成 "LXGW WenKai Lite"：一旦资源改名或字体内部
    // 族名不同，程序就会去用一个不存在的字体族（静默回退到系统字体，且没有任何提示）。
    const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/LXGW/LXGWWenKaiLite-Regular.ttf"));
    const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);

    if(!fontFamilies.isEmpty())
    {
        a.setFont(QFont(fontFamilies.first()));
    }
    else
    {
        qWarning() << "内置字体加载失败，将使用系统默认字体";
    }
    a.setStyleSheet("QToolTip{border:1px solid #BDBDBD; background-color: #ffffff; color:rgba(117, 117, 117, 0.9);font-family: 'LXGW WenKai Lite'; font-size:12px;border-radius:4px;}");


    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "QtAdb_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QPixmap pix(":/ico/image/ico/logo.svg");
    QSplashScreen splash(pix);
    splash.setBaseSize(100,100);
    splash.setMaximumSize(100,100);
    splash.show();
    a.processEvents();


    MainWindow w;
    w.show();

    splash.finish(&w);

    const int exitCode = a.exec();

    // 退出前显式收口：清空请求队列与在途请求、终止子进程。
    // 不这样做的话，退出瞬间仍在途的请求会把它持有的回调与参数留到进程结束。
    AdbClient::instance()->shutdown();

    return exitCode;
}
