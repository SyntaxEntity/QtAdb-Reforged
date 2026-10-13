// 本文件由 .dsh/probe/apply_ui_styles.py 生成：把重复出现的样式串收成具名常量。
// 目的：同一条样式只写一遍 —— 改样式从"改 N 处"变成"改 1 处"。
#ifndef UISTYLES_H
#define UISTYLES_H

namespace UiStyles {

// 3 处使用
constexpr const char *kActivatorTile_bg = "border:0px;image:url(:/activatorApps/image/activatorApps/bg.png);";

// 2 处使用
constexpr const char *kActivatorTile_blackroom = "border:0px;image:url(:/activatorApps/image/activatorApps/blackroom.png);";

// 2 处使用
constexpr const char *kActivatorTile_icebox = "border:0px;image:url(:/activatorApps/image/activatorApps/icebox.png);";

// 2 处使用
constexpr const char *kFlatButton = "QPushButton{background-color:rgba(255,255,255,0.9);border-radius:4px;border:0px;}QPushButton:hover{background-color:rgba(255,255,255,0.7);}QPushButton:pressed{background-color:rgba(255,255,255,0.6);}";

// 3 处使用
constexpr const char *kPlainDialog = "background-color:rgba(255,255,255,1);border:0px; border-radius:0px;";

// 3 处使用
constexpr const char *kSemiWhitePanel = "background-color:rgba(255,255,255,0.9);border-radius:4px;border:0px;";

// 2 处使用
constexpr const char *kToolTip = "QToolTip{border:1px solid #BDBDBD; background-color: #ffffff; color:rgba(117, 117, 117, 0.9);font-family: MiSans Medium; font-size:12px;border-radius:4px;}";

// 无线调试表单里重复出现
constexpr const char *kFormPanel = "border-radius: 4px;border:1px solid #BDBDBD;";

// 无线调试表单里重复出现
constexpr const char *kFormStepLabel = "border-radius: 4px;border-bottom:1px solid #BDBDBD;";

// 无线调试表单里重复出现
constexpr const char *kFormLabel = "border-radius: 4px;border:0px solid #BDBDBD;";

}   // namespace UiStyles

#endif   // UISTYLES_H
