cmake_minimum_required(VERSION 3.21)

project(QtAdb VERSION 2.0.0 LANGUAGES CXX)

# Qt 6 强制要求 C++17。原 qmake 工程的 `CONFIG += c++11` 会让 Qt6 直接报
# qcompilerdetection.h: #error "Qt requires a C++17 compiler"。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 只依赖 Widgets：
#   * serialport  —— 全工程 0 处引用，已移除
#   * core5compat —— 仅为 QTextCodec 一行而存在，该行已删除，已移除
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

# 打开 AUTOMOC / AUTOUIC / AUTORCC 并设定 C++ 标准
qt_standard_project_setup()

set(QTADB_SOURCES
    adbclient.cpp
    qssloader.cpp
    uishadow.cpp
    about.cpp
    abstract/comboboxlistwidget.cpp
    animationwidget.cpp
    basepage.cpp
    devicenames.cpp
    devicenames.h
    devitem.cpp
    indexlistitem.cpp
    main.cpp
    mainwindow.cpp
    mainwindow_dialogs.cpp
    monitor.cpp
    noticewidget.cpp
    pagelistitem.cpp
    pagemaker.cpp
    sonPages/activator/sp_activator.cpp
    sonPages/advanced/sp_accounts.cpp
    sonPages/advanced/sp_captive_portal.cpp
    sonPages/advanced/sp_customize_cmd.cpp
    sonPages/advanced/sp_scales.cpp
    sonPages/advanced/sp_status_bar.cpp
    sonPages/advanced/sp_vibrate.cpp
    sonPages/apps/sp_features.cpp
    sonPages/apps/sp_installer.cpp
    sonPages/apps/sp_libraries.cpp
    sonPages/apps/sp_packages.cpp
    sonPages/apps/sp_permissiongroups.cpp
    sonPages/apps/sp_permissions.cpp
    sonPages/apps/sp_users.cpp
    sonPages/apps/tp_packageprofile.cpp
    sonPages/devControl/btnemulator.cpp
    sonPages/devControl/sp_devcontrol_power.cpp
    sonPages/devInfo/sp_dpichanger.cpp
    sonPages/devInfo/sp_wmsize.cpp
    sonPages/links.cpp
    sonPages/pay.cpp
    sonPages/recovery/sp_recovery.cpp
    sonPages/sonpagebase.cpp
    sonPages/sonpagemanager.cpp
    sonPages/standardoutputpage.cpp
    textexplainer.cpp
    threads/adbthread.cpp
    threads/thread_monitor.cpp
    usb_listener.cpp
    welcomepage.cpp
)

set(QTADB_HEADERS
    adbclient.h
    adbcommands.h
    qssloader.h
    uishadow.h
    uistyles.h
    about.h
    abstract/comboboxlistwidget.h
    animationwidget.h
    basepage.h
    devitem.h
    indexlistitem.h
    mainwindow.h
    monitor.h
    noticewidget.h
    pagelistitem.h
    pagemaker.h
    sonPages/activator/sp_activator.h
    sonPages/advanced/sp_accounts.h
    sonPages/advanced/sp_captive_portal.h
    sonPages/advanced/sp_customize_cmd.h
    sonPages/advanced/sp_scales.h
    sonPages/advanced/sp_status_bar.h
    sonPages/advanced/sp_vibrate.h
    sonPages/apps/sp_features.h
    sonPages/apps/sp_installer.h
    sonPages/apps/sp_libraries.h
    sonPages/apps/sp_packages.h
    sonPages/apps/sp_permissiongroups.h
    sonPages/apps/sp_permissions.h
    sonPages/apps/sp_users.h
    sonPages/apps/tp_packageprofile.h
    sonPages/devControl/btnemulator.h
    sonPages/devControl/sp_devcontrol_power.h
    sonPages/devInfo/sp_dpichanger.h
    sonPages/devInfo/sp_wmsize.h
    sonPages/links.h
    sonPages/pay.h
    sonPages/recovery/sp_recovery.h
    sonPages/sonpagebase.h
    sonPages/sonpagemanager.h
    sonPages/standardoutputpage.h
    textexplainer.h
    threads/adbthread.h
    threads/thread_monitor.h
    usb_listener.h
    welcomepage.h
)

set(QTADB_FORMS
    about.ui
    basepage.ui
    devitem.ui
    indexlistitem.ui
    mainwindow.ui
    monitor.ui
    noticewidget.ui
    pagelistitem.ui
    sonPages/activator/sp_activator.ui
    sonPages/advanced/sp_accounts.ui
    sonPages/advanced/sp_captive_portal.ui
    sonPages/advanced/sp_customize_cmd.ui
    sonPages/advanced/sp_scales.ui
    sonPages/advanced/sp_status_bar.ui
    sonPages/advanced/sp_vibrate.ui
    sonPages/apps/sp_features.ui
    sonPages/apps/sp_installer.ui
    sonPages/apps/sp_libraries.ui
    sonPages/apps/sp_packages.ui
    sonPages/apps/sp_permissiongroups.ui
    sonPages/apps/sp_permissions.ui
    sonPages/apps/sp_users.ui
    sonPages/apps/tp_packageprofile.ui
    sonPages/devControl/btnemulator.ui
    sonPages/devControl/sp_devcontrol_power.ui
    sonPages/devInfo/sp_dpichanger.ui
    sonPages/devInfo/sp_wmsize.ui
    sonPages/links.ui
    sonPages/pay.ui
    sonPages/recovery/sp_recovery.ui
    sonPages/sonpagebase.ui
    sonPages/standardoutputpage.ui
    welcomepage.ui
)

# 嵌入资源共 33.6 MiB（其中字体 32.4 MiB），必须使用 big resources，
# 否则 rcc 会生成一个巨大的 .cpp 拖垮编译。
qt_add_big_resources(QTADB_RESOURCES
    cfgs.qrc
    fonts/fonts.qrc
    ico.qrc
    styles.qrc
)

qt_add_executable(QtAdb
    ${QTADB_SOURCES}
    ${QTADB_HEADERS}
    ${QTADB_FORMS}
    ${QTADB_RESOURCES}
)

target_link_libraries(QtAdb PRIVATE Qt6::Widgets)

# ⚠ 隐式约定：这里的源文件列表与 QtAdb.pro 的 SOURCES/HEADERS 必须保持一致
#    （新增或删除源文件时两份都要改）。自测：校验脚本会比对两份列表与磁盘上的实际文件。
#
# 工程内采用“扁平”包含风格（子目录的头部直接 #include "adbcommands.h"，
# textexplainer.h 里 #include <noticewidget.h>）。qmake 默认会把项目根目录加入
# 包含路径，CMake 需要显式声明，否则子目录中的编译单元找不到根目录下的头文件。
target_include_directories(QtAdb PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    enable_language(RC)
    # 原 QtAdb.pro 的 RC_FILE += logo.rc（仅 Windows 有意义）
    target_sources(QtAdb PRIVATE logo.rc)
    # MainWindow::initEnvironmentPATH() 约定在可执行文件旁查找 Resources/platform-tools
    add_custom_command(TARGET QtAdb POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
                "$<TARGET_FILE_DIR:QtAdb>/Resources"
        COMMENT "复制 Resources（含 platform-tools）到可执行文件目录")
endif()

install(TARGETS QtAdb RUNTIME DESTINATION bin)

# ── 国际化（M4）──────────────────────────────────────────────
# 约定与 main.cpp 中的加载代码一致：翻译文件嵌入到 Qt 资源 :/i18n/ 下，
# 名字形如 QtAdb_<locale>.qm（main.cpp 用 "QtAdb_" + QLocale(...).name() 加载）。
# 源语言是中文（代码与 .ui 里的原文），未翻译的条目自动回退中文，
# 因此没有对应语言时界面行为与之前完全一致。
set(QTADB_TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/QtAdb_en.ts")
find_program(QTADB_LRELEASE NAMES lrelease-qt6 lrelease lrelease-pro
             HINTS "${Qt6_DIR}/../../../lib64/qt6/bin" "${Qt6_DIR}/../../../bin")

if(QTADB_LRELEASE AND EXISTS "${QTADB_TS_FILES}")
    set(QTADB_QM_DIR "${CMAKE_CURRENT_BINARY_DIR}/i18n")
    set(QTADB_QM_FILE "${QTADB_QM_DIR}/QtAdb_en.qm")

    add_custom_command(
        OUTPUT "${QTADB_QM_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${QTADB_QM_DIR}"
        COMMAND "${QTADB_LRELEASE}" "${QTADB_TS_FILES}" -qm "${QTADB_QM_FILE}"
        DEPENDS "${QTADB_TS_FILES}"
        COMMENT "生成翻译文件 QtAdb_en.qm")
    add_custom_target(QtAdbTranslations DEPENDS "${QTADB_QM_FILE}")
    add_dependencies(QtAdb QtAdbTranslations)

    # 本项目资源统一走 Qt 的资源 API（未启用 AUTORCC），因此这里也用 qt_add_resources
    qt_add_resources(QtAdb "qtadb_i18n"
        PREFIX "/i18n"
        BASE "${QTADB_QM_DIR}"
        FILES "${QTADB_QM_FILE}")

    message(STATUS "翻译：启用，lrelease = ${QTADB_LRELEASE}")
else()
    message(STATUS "翻译：跳过（未找到 lrelease 或缺少 ${QTADB_TS_FILES}）")
endif()

install(TARGETS QtAdb RUNTIME DESTINATION bin)
