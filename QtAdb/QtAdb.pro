QT       += core gui
CONFIG += resources_big
# QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Qt 6 强制要求 C++17；此前的 c++11 会让 Qt6 头文件直接报
# qcompilerdetection.h: #error "Qt requires a C++17 compiler"
CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    adbclient.cpp \
    qssloader.cpp \
    uishadow.cpp \
    about.cpp \
    abstract/comboboxlistwidget.cpp \
    animationwidget.cpp \
    basepage.cpp \
    devicenames.cpp \
    devicenames.h \
    devitem.cpp \
    indexlistitem.cpp \
    main.cpp \
    mainwindow.cpp \
    mainwindow_dialogs.cpp \
    monitor.cpp \
    noticewidget.cpp \
    pagelistitem.cpp \
    pagemaker.cpp \
    sonPages/activator/sp_activator.cpp \
    sonPages/advanced/sp_accounts.cpp \
    sonPages/advanced/sp_captive_portal.cpp \
    sonPages/advanced/sp_customize_cmd.cpp \
    sonPages/advanced/sp_scales.cpp \
    sonPages/advanced/sp_status_bar.cpp \
    sonPages/advanced/sp_vibrate.cpp \
    sonPages/apps/sp_features.cpp \
    sonPages/apps/sp_installer.cpp \
    sonPages/apps/sp_libraries.cpp \
    sonPages/apps/sp_packages.cpp \
    sonPages/apps/sp_permissiongroups.cpp \
    sonPages/apps/sp_permissions.cpp \
    sonPages/apps/sp_users.cpp \
    sonPages/apps/tp_packageprofile.cpp \
    sonPages/devControl/btnemulator.cpp \
    sonPages/devControl/sp_devcontrol_power.cpp \
    sonPages/devInfo/sp_dpichanger.cpp \
    sonPages/devInfo/sp_wmsize.cpp \
    sonPages/links.cpp \
    sonPages/pay.cpp \
    sonPages/recovery/sp_recovery.cpp \
    sonPages/sonpagebase.cpp \
    sonPages/sonpagemanager.cpp \
    sonPages/standardoutputpage.cpp \
    textexplainer.cpp \
    threads/adbthread.cpp \
    threads/thread_monitor.cpp \
    usb_listener.cpp \
    welcomepage.cpp

HEADERS += \
    adbclient.h \
    adbcommands.h \
    qssloader.h \
    uishadow.h \
    uistyles.h \
    about.h \
    abstract/comboboxlistwidget.h \
    animationwidget.h \
    basepage.h \
    devitem.h \
    indexlistitem.h \
    mainwindow.h \
    monitor.h \
    noticewidget.h \
    pagelistitem.h \
    pagemaker.h \
    sonPages/activator/sp_activator.h \
    sonPages/advanced/sp_accounts.h \
    sonPages/advanced/sp_captive_portal.h \
    sonPages/advanced/sp_customize_cmd.h \
    sonPages/advanced/sp_scales.h \
    sonPages/advanced/sp_status_bar.h \
    sonPages/advanced/sp_vibrate.h \
    sonPages/apps/sp_features.h \
    sonPages/apps/sp_installer.h \
    sonPages/apps/sp_libraries.h \
    sonPages/apps/sp_packages.h \
    sonPages/apps/sp_permissiongroups.h \
    sonPages/apps/sp_permissions.h \
    sonPages/apps/sp_users.h \
    sonPages/apps/tp_packageprofile.h \
    sonPages/devControl/btnemulator.h \
    sonPages/devControl/sp_devcontrol_power.h \
    sonPages/devInfo/sp_dpichanger.h \
    sonPages/devInfo/sp_wmsize.h \
    sonPages/links.h \
    sonPages/pay.h \
    sonPages/recovery/sp_recovery.h \
    sonPages/sonpagebase.h \
    sonPages/sonpagemanager.h \
    sonPages/standardoutputpage.h \
    textexplainer.h \
    threads/adbthread.h \
    threads/thread_monitor.h \
    usb_listener.h \
    welcomepage.h

FORMS += \
    about.ui \
    basepage.ui \
    devitem.ui \
    indexlistitem.ui \
    mainwindow.ui \
    monitor.ui \
    noticewidget.ui \
    pagelistitem.ui \
    sonPages/activator/sp_activator.ui \
    sonPages/advanced/sp_accounts.ui \
    sonPages/advanced/sp_captive_portal.ui \
    sonPages/advanced/sp_customize_cmd.ui \
    sonPages/advanced/sp_scales.ui \
    sonPages/advanced/sp_status_bar.ui \
    sonPages/advanced/sp_vibrate.ui \
    sonPages/apps/sp_features.ui \
    sonPages/apps/sp_installer.ui \
    sonPages/apps/sp_libraries.ui \
    sonPages/apps/sp_packages.ui \
    sonPages/apps/sp_permissiongroups.ui \
    sonPages/apps/sp_permissions.ui \
    sonPages/apps/sp_users.ui \
    sonPages/apps/tp_packageprofile.ui \
    sonPages/devControl/btnemulator.ui \
    sonPages/devControl/sp_devcontrol_power.ui \
    sonPages/devInfo/sp_dpichanger.ui \
    sonPages/devInfo/sp_wmsize.ui \
    sonPages/links.ui \
    sonPages/pay.ui \
    sonPages/recovery/sp_recovery.ui \
    sonPages/sonpagebase.ui \
    sonPages/standardoutputpage.ui \
    welcomepage.ui

# 界面文案的源语言就是中文，因此不需要 zh_CN 翻译（原来这里指向的 QtAdb_zh_CN.ts 是空壳文件）。
# 需要的是英文翻译，而且必须与 CMakeLists.txt 的 QTADB_TS_FILES 指向同一个 .ts ——
# 这条约定由 .dsh/probe/check_build_sync.py 校验（两份构建描述的翻译列表必须一致）。
TRANSLATIONS += \
    translations/QtAdb_en.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    cfgs.qrc \
    fonts/fonts.qrc \
    ico.qrc \
    styles.qrc

# ⚠ 隐式约定：SOURCES/HEADERS 列表与 CMakeLists.txt 的源文件列表必须保持一致
#    （新增或删除源文件时两份都要改）。自测：校验脚本会比对两份列表与磁盘上的实际文件。
# Windows 资源图标（RC_FILE 仅对 Windows 目标有意义）
win32: RC_FILE += logo.rc
