> **关于本文档**
>
> 这是 QtAdb-Reforged 重构过程的完整记录（逐轮，含每一轮"做了什么、为什么这样做、
> 以及为什么不做什么"）。文中出现的 `.dsh/...` 路径、本机绝对路径与命令，都属于当时
> 使用的验证设施 —— **该设施未随本仓库发布**，因此这些命令在克隆本仓库后无法直接运行。
> 保留它们是为了让过程可追溯、结论可复核：每条结论后面都写了它是由哪条命令、哪个数字支撑的。
>
> 原始文件为中文，文件名与文中路径保持原样。

---

# QtAdb 代码审计与优化/重写方案

> 审计对象：`/home/hall-of-stasis/Projects/qtadb/QtAdb-Reforged`（[LapplandSP/QtAdb](https://github.com/LapplandSP/QtAdb)，commit `191205b`）
> 审计方式：**只读**。全部 44 个编译单元逐文件精读 + 真实编译器取证。
> **本次未改动仓库任何文件**（`git status` 变更数 = 0）；所有构建产物在仓库外的 `Projects/qtadb/.dsh/`。

---

## 〇、状态与待办（**供上下文压缩后恢复用**，每次改动后应更新本节）

### 0.1 目标与约束（用户设定）

> 优化或重写 QtAdb 项目代码；**暂时不添加功能（修复漏洞与增强健壮性除外）**；
> 确保代码足够**健壮、可维护、可拓展、易读**；自动压缩或固化上下文。

### 0.2 现在在哪

| 项 | 值 |
|---|---|
| 仓库 | `/home/hall-of-stasis/Projects/qtadb/QtAdb-Reforged`（源码在 `QtAdb/` 子目录，git 仓库根在其上一层） |
| 构建 | CMake（`QtAdb/CMakeLists.txt`）与 qmake（`QtAdb/QtAdb.pro`）**双份维护**；Linux + Qt 6.11 可构建 |
| 改动规模 | 见文末"累计"；**全部未提交**，`git status` 可见，`git diff HEAD` 可整体回退 |
| 工具链 | Qt 6.11.2 解包在 `$HOME/.local/qt6-prefix/usr`（无 root）；验证产物在仓库外 `Projects/qtadb/.dsh/` |

### 0.3 已完成（按主题，细节见对应章节）

1. **M0 构建系统迁移 + 平台解耦**（§八）：新增 CMake、C++17、去掉 Windows 头依赖、Linux 可编译。
2. **P0 崩溃止血**（§九）：异步化前先把会导致卡死/崩溃的同步调用处理掉。
3. **M1 ADB 层异步化**（§十–§十八）：新增 `AdbClient`（单队列 + 每请求超时 + 取消 + 回调），
   全量迁移同步调用，**删除 `adbProcess`**，GUI 线程零同步 ADB 调用，**全工程零 QThread 子类**。
4. **M2 数据模型**（§十九–§二十）：`DeviceState` 枚举取代中文字符串状态机；去掉固定数组上限；清理死代码。
5. **命令层具名化**（§二十一）：`adbcommands.h` 的 `AdbCmd::*` 具名 argv 构造（112+ 处）。
6. **输出解释功能恢复**（§二十二）：删 `adbProcess` 时丢失的 20 条报错提示已接回，并修掉"每次成功命令都弹未知错误"。
7. **健壮性专项**（§二十三、二十六、二十七、二十八）：删除项副作用体检、103 处裸指针初始化、
   `taiChiTimer` 生命周期、解析器越界（会 abort）、页面重建泄漏、验证覆盖面修正。
8. **M3 热插拔**（§二十四）：轮询 `devices -l`（复用既有热插拔路径），Linux 可用、Windows 补充覆盖 WiFi 设备。
9. **设备别名**（§二十五）：修好一个三处断裂、实际不可用的功能（含持久化）。
10. **M4 国际化**（§二十九–§三十一）：接通 `:/i18n/` 链路、436 条文案可提取、356 条已译、
    代码 `tr()` 309 处、`.ui` 与自由函数路径均已验证。
11. **可维护性**（§三十二）：两个工厂 switch → 数据表，新增页面从"改 3 处"变成"加一行"。
12. **上下文固化 + 关级审计**（§三十三）：本节（§0）即为固化产物；新增**逐参数记录**与
    `--install-path` 探针，并据此查出并修复"监视命令缺 `-s` 设备前缀"的真实缺陷。
13. **别名显示修复 + 死代码清理 + 纯函数抽取**（§三十四）：修掉"下拉列表里别名被型号名覆盖"
    （我第 19 轮引入的集成遗漏）；清掉 `comboboxListWidget` 的旧命名残留（112→71 行）；
    `applyDeviceList` 抽出两个纯函数（194→158 行）。
14. **权限页两项修复**（§三十五）：空指针崩溃（`pa->geometry()`）+ **每次刷新泄漏一个
    装着整张权限表的模型**（12 次刷新留 13 个 → 修复后恒定 1 个）；同文件可读性清理（444→435 行）。
15. **模型生命周期统一治理**（§三十六）：把"计数型探针"推广到全部子页，发现**同类泄漏存在于
    另外 6 个列表页**，在共同基类 `animationWidget` 里用 `installTableModel()` 一次修好；
    并修掉"空数据路径下模型创建后既不安装也不释放"的第二个泄漏源。
16. **用 LSan 给残余增长定性 + 收敛 `setModel()` 次数**（§三十七）：证明选择模型增长**不是泄漏**
    （1 轮与 12 轮的 LSan 报告逐字节相同），并把"每轮两次 setModel"降为**一次**——
    全部 7 页的选择模型/模型数此后**恒定**。
17. **权限页三套填表逻辑拆分**（§三十八）：`refresh_tableView` 142 → 23 行（分派器），
    两个具名函数各管一种表格；删掉 5 处**毫无作用**的 `btn_x->autoExclusive();` 取值调用。
18. **`applyDeviceList` 拆分 + 一处"逐设备重复重建"**（§三十九）：158 → 30 行
    （另拆出 `rebuildDeviceCombo` / `updateDeviceIndicators`）；顺带发现下拉框重建被写在
    `for(i < devList.size())` 里而循环体不用 `i` —— **有几台设备就重建几次**，改为一次。
19. **`initBasePage` 重构**（§四十）：去掉假循环 `while(key != -1){…break;}`、去掉每次导航
    **白建一个页面再删掉**（连其 `wgtHeight` 死赋值）、补上 `nullptr` 与 `current_device` 越界守卫、
    失败路径补 `unlock()`（否则界面永久锁死）。
20. **坏味道机械扫描 + 别名写入失败可见**（§四十一）：把前几轮靠人工发现的坏味道做成全工程扫描
    （假循环 / 白建后删 / 循环体不用循环变量），结论是**已无同类问题**；
    另修掉"别名写盘失败仍显示已保存"（`QSettings::status()` 从不检查），并新增 `--devicenames` 探针。
21. **"结果不检查"全局排查**（§四十二）：回调退出码（仅 1 处，且只是输出转发，无害）、文件写入
    （0 处）、`QProcess`（仅在 AdbClient 且已处理）；修掉**内置字体加载结果被忽略却硬编码族名**
    与 `mkpath` 结果未检查两处。
22. **条目创建收成一处 + 探针化等价校验**（§四十三）：`pageMaker` 里 40 个条目块
    （每个 5～6 行"新建→设标题→设图标→设可点选→入列表"）收成一个 `addPageItem()`，
    `pagemaker.cpp` **615 → 501 行**；并给工厂探针加上"每页条目文本"输出，
    重构前后 **55 个条目的标题/说明/数量/所属页面逐字相同**。
23. **异步信息条目收成 `addInfoItem()`**（§四十四）：`createPage_devInfo` 里 6 个
    "设标题/图标 → 起 adbThread → 连接填充槽 → 入列表 → 释放结构体"的 9 行块收成一处，
    该函数 **117 → 69 行**，`pagemaker.cpp` 再降到 **505 行**；探针先加"等 250ms 让异步回填落地"
    再比对，等价性覆盖**回填后的真实值**（如 `安卓版本|13`）。
24. **回调路径同样收束 + 清掉 18 行死代码**（§四十五）：`createPage_devInfo_rec` 的
    91 行里三个"占位 + 回调回填"块收成 `addInfoItemAsync()`（带可选转换函数，供 CPU 解析），
    该函数 **91 → 36 行**、`pagemaker.cpp` **505 → 478 行**；顺带把手写的 `{"-s", dev.addr, …}`
    改成规范的 `runForDeviceWithCallback()`，并用**关级审计**证明命令行一字不差。
25. **"允许打开的子页"不再手写数字**（§四十六）：新增 `basePage::enableAllItems()`，
    替掉 6 处手写的 `setEnableValue({1,2,…,N})`（其中一处写了 18 个数字）；
    新增条目从此不必同步那串数字——忘了同步就会"新增的条目点不开"。
    把 `isEnable()` 公开出来供自测，并用探针导出每页的允许集合证明**21 行逐个相同**。
26. **把"条目顺序 = 子页序号"这条隐式约束变成断言**（§四十七）：新增 36 组
    "条目 ↔ 子页"期望表校验（标题 + 子页类名双重比对），并在两处写下约定注释；
    实测**36 组全部相符**（这同时也证明了历史上没有错位）。
27. **两份构建文件的源文件列表不再靠手工对齐**（§四十八）：查出 `.pro` 少列
    `adbcommands.h`、CMake 注释里还在举已删除的 `adbprocess.h` 当例子；
    修好后新增 `check_build_sync.py`（比对两份列表与磁盘实际文件、并检出失效引用），
    并**自测过校验器本身**（抽掉一项能精确报错）。
28. **解释键的封闭集合**（§四十九）：键由页面传入、在 `adbThread::deliver()` 里分派，
    传错会被当"分隔符"用而**静默不解释**；现在传 `#XXX#` 形式却未登记的键会给出
    `qWarning`（探针验证：拼错的键恰好 1 条警告、合法键与分隔符 0 误报；
    **正常运行 0 条**，说明现有调用点与分派完全一致）。
29. **全工程函数长度排行 + 最长的 `explainError` 表化**（§五十）：350 个函数里
    `textExplainer::explainError` 以 102 行居首（22 个有序分支的 if/else 链）——
    改成"数据表 + 一次遍历"，该函数 **102 → 25 行**，新增一条提示只需加一行数据；
    用"抽取→生成→再抽取比对"证明 22 条分支的顺序/子串/文案逐字一致。
30. **排行第二：`sp_vibrate::refresh` 收束**（§五十一）：10 个"读 settings 键并回填"的
    5 行块（连注释都重复 10 遍）收成 `requestSetting(键, 成员函数指针)`，该函数
    **100 → 15 行**；顺带把重复刷新探针扩展到"没有刷新按钮的页面用 `setDev()` 触发"，
    由此确认这些页面的临时线程**自清理**（对象数 117→107，是减少而非累积）。
31. **重复块扫描工具 + 工厂表压成一行**（§五十二）：写了"函数内三连语句重复度"扫描
    （全工程仅 3 处，其中 2 处是工厂表每行的固定收尾）→ 引入
    `makeSonPageWithDev<T>()` / `makeSonPage<T>()` 两个模板工厂，18 行表项各压成一行，
    `sonpagemanager.cpp` **298 → 248 行**；模板工厂那句"是否调用 setDev"的差异
    （advanced 第一项原本是注释掉的）由脚本逐行比对保证不被抹平。
32. **投影样板收成 `applyDropShadow()`**（§五十三）：全工程 104 处"四行投影"里，
    先替换掉形状连续、参数一致的 **34 处**（另 2 处橙/天蓝强调色用同一函数带颜色参数覆盖）；
    新文件 `uishadow.h/.cpp` 同时登记进两份构建文件（顺带验证了一致性校验器）。
    颜色与目标的等价性由"以**原始文件**为准、按控件配对"的脚本独立校验：**全部一致**。
33. **投影替换第二批 + 工具自证**（§五十四）：换用"**按变量**"的替换策略，覆盖"先集中声明、
    再集中配置"的写法，替换总数 **34 → 64 处**（104 处里），缺口逐条列出原因；
    本轮两次踩到**工具自身的谎报**（计数说替换了 47 处、其实文件根本没写入；
    include 步骤只在分支里跑一次导致构建失败），均已修好并写进工具。
34. **投影替换收尾，并躲过一次空指针**（§五十五）：把剩下**真正生效**的 11 处也换掉
    （替换总数 **64 → 75 处**），过程中发现 `sp_recovery` 的 `tips` 阴影**在 `setDev()` 里
    会被条件改色** —— 若只做机械替换，那个句柄会变成空指针（一个真实的崩溃隐患）。
    改为用 `applyDropShadow()` 的**返回值**保留句柄，既收束了样板又保住了行为。
35. **`setArg` 的 5 个布尔开关 → 枚举 + 一张表**（§五十七）：6 个分支各自"设 arg +
    重置 5 个布尔量"（36 行重复），其中 `arg_s/arg_d/arg_u` **只写不读**（分派只看前两个）；
    该函数 **65 → 32 行**，死状态随之消失，按钮→参数映射由脚本逐项比对证明未变。
36. **"选项"对话框收成一张表 + 清掉 about 页死代码**（§五十八）：5 个单选项的
    "创建→设文本→进组→进布局→连信号"从 25 行重复变成 5 行数据 + 一次循环
    （新增选项从改 5 处变成加 1 行）；`about.cpp` 清掉未使用的局部字体、
    重复注释与注释掉的 `hide()` 块。
37. **按键页的"36 个控件清单"改成遍历**（§五十九）：新增 `--effects` 探针（列出页面上
    带 `graphicsEffect()` 的控件），先取基线 —— 手写清单恰好等于 .ui 里的 30 个按钮 + 6 个标签；
    改为 `findChildren` 遍历后覆盖集合**逐个相同**，新增控件从此自动获得阴影。
38. **追回 17 处被我误删的阴影**（§六十）：把 `--effects` 探针推广到**全部 26 个页面**后，
    发现 `activator` 只有 1 个效果、而它的代码里有 5 处 —— 顺着查出第 48 轮的批量替换
    在 **5 个文件里"只删未加"，丢掉 17 处阴影**（我的校验当时只核对"新增"、把"丢失"
    列在未替换清单里没细看）。补回后页面级覆盖回到原始值，并新增"双向审计"。
39. **输入框验证器的泄漏（52 → 0）**（§六十一）：`sp_vibrate` 构造收束时发现
    `new QRegularExpressionValidator(...)` **没有父对象**，而 `QLineEdit::setValidator()`
    不接管所有权 —— 每建一次页面泄漏 10 个；LSan 栈还指向 `sp_scales`（另有 3 个）。
    两处都改为"父对象 = 输入框"，LSan 总量 **7072 → 3486 字节 / 117 → 35 次分配**。
    同时把"双向审计"推广到其它几批批量替换，确认没有新的"只删未加"。
40. **给页面生命周期加探针 + 记下一个未决项**（§六十二）：`--effects` 现在还会把页面挂在
    临时父对象下、销毁父对象后统计存活数 —— **26 个页面全部回收（0 存活）**；
    但 LSan 仍报告约 7 个页面份的分配追溯到 `basePage` 构造，两者矛盾，作为**未决项**如实记录。
41. **未决项关闭：动画对象从来没有父对象**（§六十三）：用最小探针（建 26 页→删父对象→退出）
    把矛盾逼到根因 —— 4 处 `new QPropertyAnimation(target, "property")` 用的是**三参数重载**，
    第三个参数 `parent` 被省略，于是**页面与通知控件各自的动画都不会被回收**。
    补上 parent 后，LSan 里"追溯到 `basePage` 构造"的分配 **35 → 0**；
    剩下的报告全部落在 libfontconfig（Qt 字体引擎），**没有任何本项目帧**。
42. **完整场景的泄漏清零（44482 字节 → 0）**（§六十四）：用 LSan 当探测器扫完整应用场景，
    发现 36KB / 952 次分配全指向 `on_WIFIBtn_clicked` 里那个**只在"连接"按钮里才释放**的
    无线调试窗口；另外 3 处 `QMessageBox` 也缺"关闭即删"。统一修好后，
    完整场景的 LSan 报告为 **"(无)"** —— 应用跑完一遍不留泄漏。
43. **泄漏基线固化成命令 + 修掉自己放错位置的 10 处阴影**（§六十五）：
    `check_leaks.py`（`--run` / `--log`）把"零泄漏"变成可断言的回归项，并用修复前/后与
    第三方三种日志验证了它本身；同时发现 §五十四 补回阴影时**插入点选错**（放进了
    `on_pluginBtn_clicked`，变成"点一次插件按钮才有阴影"），已移回 `setStyles()`，
    并把主窗口纳入 `--effects` 探针 —— 构造后立即检查那 10 个控件。
44. **位置审计**（§六十六）：把"助手调用是否落在原来那个函数里"做成检查 ——
    阴影一批核对 **79 处、0 处错位**；推广到其它批次时脚本自身有 bug，
    但它会**大声失败**（"未核对到任何站点"直接判失败），已记为下一项待修。
45. **位置审计补齐三批并跑通**（§六十七）：`audit_placement.py` 现在核对
    **阴影 79 处 / 信息条目 9 处 / 模型安装 18 处，全部 0 处位置不符**；
    过程中修好了它自己的三个问题（调用方漏传仓库路径、助手定义被当成调用点、
    以及"有意搬家"的批次需要允许集合），并加了**路径守卫**与**核对数为 0 即失败**的双重自检。
46. **回归矩阵收成一条命令**（§六十八）：`run_regression.py` 一次跑完
    **4 个构建 + 7 个场景 + 6 个探针 + 3 项检查 = 20 项**，逐项给结论、最后汇总，
    任一项失败则退出码非 0；并**验证了它的失败路径**（故意让一项检查失败 → 如实报 ★ 且退出码 1）。
47. **热插拔也自动化：21 项全部进一条命令**（§六十九）：最后一个手工场景（运行中三次改桩状态）
    已写进回归脚本，判据是"设备数序列必须是 `[1, 2, 0]`"；先用普通桩验证它**会失败**
    （序列变成 `[1]` → 判失败），再整跑 —— **21 项、失败 0 项**。
48. **注释掉的代码清零式清理（可自证）**（§七十）：写 `strip_dead_comments.py` 只删
    "看起来是代码"的注释，并**自证两道**：被删的每一行都必须是注释形态，
    且"剥掉注释后的源码"在删除前后**逐字节相同**；共清掉 6 个文件 113 行，
    全工程该类注释降到 **71 行**（头部文件都只剩个位数）。
49. **多行死注释块也清掉：261 行**（§七十一）：把工具放宽到"正常闭合 + 块内含代码味行"，
    再清 **234 行**（跨约 20 个文件），另有 `textexplainer.cpp` 一段 13 行被注释掉的函数
    在自证 ② 拦截后**手工**核对删除；共 **261 行**。
    过程中定位到自证 ② 两次拦截的根因：**"剥注释"是字符串盲的** —— 某行字符串里含 `/*`
    会让它误判出"跨过活代码的块"，于是它宁可拒绝写入。
50. **扫描器改成字符串感知，清理收口**（§七十二）：把"剥注释"重写为逐字符扫描
    （字符串里的 `/*`、`//` 不再当注释），此前被误拒的 `mainwindow.cpp` 因此安全清掉 **24 行**；
    按同一判据，全工程**已无可清理的注释代码（0 行）**。
51. **调试输出清零：330 → 0**（§七十三）：清掉最后 23 处 `qDebug()`，
    其中包括 3 处**把界面文案包进 `tr()`** 的写法（既打日志又污染翻译词条）；
    清理过程中发现并修好一处**跨行**调试语句被删半截导致的编译错误。
52. **可拓展性扫描 + 洁净度固化成检查**（§七十四）：扫"无 `default` 的枚举 switch"与
    TODO 残留 —— 结论是**前者不该改**（无 `default` 才能让编译器在新增枚举值时告警）；
    同时发现上轮的 `qDebug` 扫描**漏了 `qDebug("...")` 这种带参写法**（2 处），
    已清理；新增 `check_hygiene.py`（**自带自检**：注入违规必须被抓到）并接入回归矩阵（22 项）。
53. **回归并行化 + 由此抓出的一个真泄漏**（§七十五）：场景与探针改为 4 路并行
    （各自复制一份桩目录，避免共享计数器），整跑从约 10 分钟降到 **约 4 分钟**；
    并行后泄漏基线**偶发失败**，顺着失败详情查到 `AdbClient::runWithCallback` ——
    退出时**在途请求的回调从未释放**（析构只清了队列、没清 `m_current`）。
    新增 `AdbClient::shutdown()`（幂等、由 `main()` 在 `exec()` 返回后显式调用）。
54. **真正的元凶：堆上的 `QMetaObject::Connection`**（§七十六）：顺着 `runWithCallback` 的栈
    读出根因 —— 连接对象是 **`new` 出来的，只在其 lambda 被触发时才 `delete`**；
    请求若因退出或取消而未触发回调，那 8 字节就永远留下。改为**值语义存进成员表**
    （`QHash<int, QMetaObject::Connection>`），退出时统一断开；负载下连跑 6 次泄漏基线
    **0/6 复现**，完整并行回归 **22 项全绿**。
55. **稳定性进回归 + 清掉一处"重复施加"**（§七十七）：泄漏基线改为**连跑 3 次**才算过
    （单次通过可能只是运气，§七十五/§七十六 的教训），并验证了它的失败路径
    （指向含泄漏的旧日志 → 报 "0/3 次通过" 且退出码 1）；
    顺带发现 §五十五 那次"改成遍历"其实**只替换了第一组**，`sp_vibrate` 里还留着
    22 行显式 `setShadow` 列表 —— 那些控件被重复施加阴影，而 `--effects` 探针
    **查不出来**（它数的是覆盖控件数、不是调用次数）。已清理，构造 109 → 87 行。
56. **把"重复施加"补成检查**（§七十八）：新增 `check_duplicate_shadow.py` ——
    先在本文件里识别"阴影助手"（函数体里调用了 `applyDropShadow`/`setGraphicsEffect` 的具名函数），
    再统计同一控件在同一函数里被设置阴影的次数，>1 即报告；**自带注入式自检**
    （往临时副本注入一处重复 → 必须抓到）。当前全工程 **0 处重复**，已接入回归矩阵（**23 项**）。
57. **推广失败并如实收窄**（§七十九）：试着把"重复施加"推广到"同一视图装两次模型"，
    结果 9 处报告**全是误报** —— 文本计数看不见 `if/else` 两个分支各调一次（本工程 7 处这种写法），
    而助手识别的粒度也不对（`fillGroupedTable(s)` 里的 `s` 是字符串、不是视图）。
    于是**收窄回只查阴影**，并把"为什么不可靠"写进脚本注释与文档，而不是靠加例外压噪声。
58. **UBSan 从"只编译"变成"真跑"**（§八十）：`asan/build-relasan`（Release+ASan+UBSan）一直只参与构建、
    从未被运行过。本轮把 5 个场景 + 4 个探针 + 挂起场景都在 UBSan 下跑了一遍 ——
    **未定义行为 0 处**；并把代表性的 3 项接进回归矩阵（**27 项**）。
59. **第 75 轮质量快照**（§八十一）：同一口径重算指标 —— C++ 行数 8839 → 8881（**基本持平**：
    删掉约 4500 行旧代码的同时补进了异步化、i18n 与验证设施）；裸 `new` 253 → **130**；
    `qDebug` 278 → **0**；`tr()` 0 → **213**；**函数中位数 5 行、超过 60 行的只剩 1 个**；
    回归矩阵 **27 项全绿**。
60. **UBSan 补齐热插拔（28 项）+ 清死代码分析器的一个盲点**（§八十二）：热插拔也纳入 UBSan
    运行（0 未定义行为），矩阵 **28 项全绿**；顺带发现死代码分析器**把"带中文尾注释的被注释代码"
    当成说明性注释保守留下**（`exitOpacity = new …;//设置透明度`），改为块内判定时先剥掉行尾注释，
    `basepage.cpp` 又清掉 4 行。
61. **重复样式串抽成具名常量**（§八十三）：单行重复的样式串 7 种、17 处收进新的
    `uistyles.h`（`kFlatButton` / `kToolTip` / `kPlainDialog` / 三个激活器图标样式 …），
    改样式从"改 N 处"变成"改 1 处"；替换由脚本完成并自证"字符串内容逐条未变"，
    新头文件同时登记进两份构建文件（一致性检查：磁盘/CMake/.pro 各 **131** 个）。
62. **跨行拼接的那 4 处也收干净（重复归零）**（§八十四）：改用"按原文字面量运行段精确定位"
    的替换器（不猜换行位置），把 `sp_dpichanger` / `sp_wmsize` 里各 2 处拼接写法换成
    `UiStyles::kFlatButton`；自证升级为**拼接感知的多重集比对**。复核：全工程重复的长样式串
    **0 种 0 处**。
63. **无线调试窗口：从"逐行搬运控件"变成"描述界面"**（§八十五）：两个局部 lambda
    （`makeLabel` / `makeRow`）收掉 8 次"建标签→设文本→设样式"与 5 次"标签+输入框成一行"；
    3 个重复表单样式进 `uistyles.h`；函数 **99 → 87 行**。验证除了常规矩阵，
    还专门检查了 **Qt 的布局/连接告警（0 条）** —— 布局搬动最容易在这里出问题。
64. **助手提到文件级，并修掉 Magisk 窗口的同类生命周期问题**（§八十六）：把上轮那两个
    局部 lambda 提成文件级 `makeFormLabel()` / `makeFormRow()`（两个对话框共用，避免复制），
    `on_WIFIBtn_clicked` 再降到 **75 行**、`on_pluginBtn_clicked` 降到 52 行；
    同时发现 Magisk 窗口 `new QWidget()` **无父对象、无关闭即删**（与 §六十四 修的无线调试
    窗口同型），一并修好并改用 `QPointer` 成员。验证：文案逐条在 HEAD 中存在（0 条缺失）、
    Qt 布局告警 0 条、泄漏基线 0。
65. **"重复安装模型"补成检查，判据改成分支感知**（§八十七）：§七十九 那次推广失败是因为
    文本计数看不见 `if/else`；本轮把判据换成**按最近的包围块计数** —— 两支各属不同块，
    有意写法天然不被误报；同一块内出现两次才是真重复。自检注入"同块两次安装"**能抓到**，
    当前代码 **0 处**（零误报）。接入回归矩阵（**29 项**）。
66. **无线调试窗口拆成两个表单助手**（§八十八）：`on_WIFIBtn_clicked` **75 → 31 行**，
    两个步骤各自成为 `buildWirelessOutPanel()` / `buildWirelessPairPanel()`（共 46 行）；
    本轮**先做快照再改**，改后用"文案多重集 + 布局添加次数 + 成员赋值"三项不变量核对，
    并复核 Qt 布局告警 0 条。
67. **"搬动式重构守卫"固化成命令**（§八十九）：`refactor_guard.py`（`snapshot` / `check` / `selftest`）
    把 §八十八 的手工做法变成一条命令 —— 捕捉 tr 文案、长字面量、关键调用次数、函数集合、
    文件清单；三项验证到位：改文案**能抓到**、无改动一致、**纯搬动一行报告 0 项**。
    过程中还修掉扫描器一个真 bug（**字符串闭合引号被丢掉**，导致"从剥注释文本里抽取内容"整段错位）。
68. **mainwindow.cpp 拆分：911 → 714 行**（§九十）：175 行对话框代码（无线调试两个表单助手 +
    无线调试槽 + Magisk 槽）移入新的 `mainwindow_dialogs.cpp`；守卫新增**全工程聚合层**，
    搬动后聚合量 **0 项差异**（文案/字面量/调用次数/函数集合全不变）——这就是"纯搬动"的证据。
    过程中又踩了一次"新增源文件必须重新 configure"（§四十七 记过），以及一次 `pkill -f`
    匹配到自身命令行的自伤。
69. **补跑完整回归 + 修正 README 里的过时指路**（§九十一）：拆分后先跑完整矩阵确认 **29 项全绿**；
    顺带核对仓库自带文档 —— README 仍在教用户"使用 qmake 前请删除 `CONFIG += c++11` 那一行"，
    而工程早已是 `CONFIG += c++17`，这条**过时指路**已改为如实说明；
    README 引用的 4 个文件路径经机械核对全部存在。
70. **"文档与代码一致性"也做成检查**（§九十二）：`check_docs.py` 核对 README 里
    **引用的路径必须存在**、**构建配置项必须与构建文件相符**，自带注入式自检（两条假引用全抓到）；
    接入回归矩阵（**30 项**）。写它时又发现 README 一条过时事实：**嵌入资源写成 33.6 MiB，
    实际 2.7 MiB**（原数把"复制而非嵌入"的 Resources 也算进去了），已修正并写明两者区别。
71. **补上"连接告警"这条判据**（§九十三）：工程里有 69 行 Qt5 风格的字符串式连接
    （`SIGNAL()/SLOT()`），写错了**只在运行时静默失败**，而矩阵此前不断言它 ——
    现在场景里"连接告警必须为 0"。测量结果：**应用场景 0 条** ✓；
    探针原有 39 / 72 条，定位为**测试环境差异**（探针以普通 QWidget 或 nullptr 当页面父对象，
    而页面连的是 `basePage` 的槽），已把探针宿主改成真实 `basePage`，告警降到 1 条并改为**提示**。
72. **"死声明"审计：清掉 6 个孤儿声明并做成检查**（§九十四）：头文件里声明、却从未被引用的
    成员函数/信号共 **6 个**（含一个**从未发射过的信号** `textExplained`）；已删除，
    并新增 `check_dead_decls.py`（自带注入式自检）接入回归矩阵（**31 项**）。
    另外：完整回归 **30 项全绿**，确认了连接告警判据在各场景下都成立。
73. **两个大文件的结构复查 + 一次"主动放弃的检查"**（§九十五）：`adbclient.cpp`（424 行/21 函数，
    最大 44 有效行）与 `pagemaker.cpp`（429 行/18 函数，最大 46）**结构健康、无需改动**；
    另外试着做"声明↔定义签名一致性"检查，量出 **65 项报告全是抽取假阳性、零真不一致**，
    而该缺陷类极窄（被调用的不一致编译器早就报了）—— 于是**主动放弃这个检查**，
    把测量结论与理由写进文档（§七十九 的同一判断，这次在发布前做出）。
74. **"死成员"审计：清掉 16 个并做成检查**（§九十六）：头文件里 231 个成员变量中，
    **10 个从未被引用** + **6 个只被赋值从未被读取**（`parents`×3、`isSelectable`、`lastPage`、
    `father` 等，与 §五十一 清掉的 `arg_s/arg_d/arg_u` 同类）；声明 16 行、赋值 5 行已删除，
    构建 0 警告。新增 `check_dead_members.py`（**`++`/复合赋值算读取** —— 早期版本在这里
    误报过 `m_nextId`），自带注入式自检，接入回归矩阵（**32 项**）。
75. **收尾核查：两个新维度干净 + 32 项全绿**（§九十七）：多余 Qt `#include` **0 处**、
    孤儿头文件 **0 个**；`mainwindow.cpp` 拆分后最长的文件 714 行、函数中位数 5 行。
    完整回归矩阵 **32 项、失败 0 项** —— 目标四个维度均有可复现的证据支撑，见 §九十七 的达成清单。

### 0.4 怎么构建与验证（照抄即可）

```bash
U=$HOME/.local/qt6-prefix/usr                     # Qt 解包前缀
export PATH=$U/bin:$U/lib64/qt6/bin:$PATH LD_LIBRARY_PATH=$U/lib64 LC_ALL=C
WS=/home/hall-of-stasis/Projects/qtadb

# 全新构建（app）
cmake -S $WS/QtAdb/QtAdb -B $WS/.dsh/build-cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$U
cmake --build $WS/.dsh/build-cmake -j8

# 验证驱动（ASan/UBSan；GLOB 收集源文件 —— 增删源文件后必须重新 configure）
cmake -S $WS/.dsh/asan -B $WS/.dsh/asan/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZERS=ON -DCMAKE_PREFIX_PATH=$U
cmake --build $WS/.dsh/asan/build -j8
export QT_QPA_PLATFORM=offscreen QT_ASSUME_STDERR_HAS_CONSOLE=1
export ASAN_OPTIONS=detect_leaks=0:handle_abort=1:print_stacktrace=1
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan        # 生命周期 + 逐页(30 子页)
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --parsers     # 解析器畸形输入
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --i18n        # 国际化三路径
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --factories   # 工厂映射 + 每页条目 + 条目↔子页配对校验
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --install-path  # 含空格/中文路径的 argv
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --permissions   # 权限页反复刷新是否遗留模型
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --repeat 6      # 全部子页反复刷新，按类名看对象增长
PATH=$WS/.dsh/bin:$PATH $WS/.dsh/asan/build/QtAdbAsan --devicenames  # 别名持久化（含带冒号地址）
# 打桩会把每次调用的 argv 逐参数记到 .dsh/bin/.invocations.args（| 分隔），可据此做"关级"审计
python3 $WS/.dsh/probe/check_build_sync.py $WS/QtAdb/QtAdb/QtAdb   # 两份构建文件列表一致性
```

打桩目录（`PATH` 前置即可模拟各种设备）：`bin`(正常) `bin-bad`(畸形输出) `bin-fuzz`(逐次轮换退化输出)
`bin-two`(双设备) `bin-vanish`(设备中途消失) `bin-hang`/`bin-hang2`(命令挂起) `bin-hotplug`(热插拔) `bin-offline` `bin-explain-*`。

**验证纪律（踩过坑后固化）**：① 构建失败即停，绝不继续跑旧二进制；② 结论前确认"跑的是刚构建的产物"
与"记录的是本次运行"；③ GUI 断言放在事件循环内部、被测交互结束之后；④ 增删源文件后重新 configure。

### 0.5 待办（未做，按价值排序）

1. **翻译收尾**：激活器页 80 条长说明未译（纯文案，不动代码）；界面语言选择项属"新功能"，暂不做。
2. **可读性**：`applyDeviceList()`（194→30）、权限页三套填表逻辑、`initBasePage()`（67→58）、
   `pageMaker`（615→478；`createPage_devInfo` 117→69、`createPage_devInfo_rec` 91→36）均已处理。
   下一批候选：`createPage_acvitator`（119 行）、`sonPageManager` 剩余长函数。
3. **退出期请求队列**（低优先）：LSan 里 5408 字节 / 91 对象来自"进程退出时仍排在
   `AdbClient` 队列里的请求"。属一次性残留、不随操作增长；若要彻底消除，可在退出时清空队列。
4. **可选增强**（属新功能，需用户确认）：`adb track-devices` 实时热插拔、轮询自适应退避。

### 0.6 关键约定

- 命令一律用 `AdbCmd::*` 构造 argv；设备相关请求用 `AdbClient::runForDevice()`（自动前置 `-s`）。
- 后台取数类命令用 `AdbClient::Explain::No`，界面上直接触发的操作用默认值（会弹提示）。
- 新增/修改**解析外部输出**的代码：必须有长度守卫，并往 `--parsers` 探针加用例。
- 新增/修改**页面工厂**：往数据表加一行，然后跑 `--factories` 与基线对比。

---

## 一、结论摘要

| 项目 | 事实 |
|---|---|
| 规模 | 9266 行 C++，44 `.cpp` / 43 `.h` / 33 `.ui`，仓库 48 MB |
| 技术栈 | Qt Widgets + qmake，QtCreator 工程锁定 **Qt 6.2.4 MinGW 64-bit** |
| 平台 | **仅 Windows**：自带 `adb.exe` 等 12 个 PE 二进制，22 个编译单元无条件包含 `Windows.h` |
| i18n | 有 `.ts` 与 `lrelease/embed_translations` 配置，但 **`tr()` 调用 0 处**、`.ts` 为空（0 条消息）、代码里 149 处中文字面量 |
| 内存管理 | **441 处裸 `new` / 60 处 `delete` / 0 处智能指针** |
| 调试残留 | 330 处 `qDebug()`、474 行被注释代码（含 `"Bug#01 finding!"`、`"shit 1/2/3"`） |
| 架构 | 1030 行上帝类 `MainWindow`；604 行 switch 工厂；**21 个 `adbProcess` 实例**，每个析构都会执行 `adb kill-server` |
| 可移植性 | 在 Linux + Qt 6.11 上：**22/44 直接编译失败**；给 `Windows.h` 打桩后 **43/44 通过**——移植阻塞被精确定位到 2 处 |

**一句话诊断**：这不是"有些小毛病的项目"，而是**功能可用的原型代码**：业务功能相当丰富（设备管理、激活器、权限、DPI/分辨率、Recovery、无线调试…），但底层缺少**任何架构分层**——ADB 调用、UI、业务逻辑、线程全部缠在一起，叠加 441 处裸 `new` 与 21 个各自会杀死 ADB 服务的进程对象，导致**稳定性、可移植性、可维护性三项同时失守**。

**建议**：不要在这套结构上继续叠加功能（方案 B 渐进修补见 §5.2），而是按 §5.1 做**分层重写骨架 + 保留 UI 资产**——因为 33 个 `.ui`、图标、QSS 样式是可复用的真实资产，而 C++ 逻辑层几乎需要重写。

---

## 二、编译取证（第一手实测，非推断）

环境：Fedora 44 + GCC 16 + Qt 6.11.2（我用免 root 方式组装了完整 Qt6 开发前缀，见 §6）。
方法：绕过 qmake，直接用 `g++ -fsyntax-only -std=c++17` 对全部 44 个编译单元做语法编译，并用 `uic` 生成 33 个 `ui_*.h` 以排除干扰。

### 2.1 实测结果

```
未打桩：  通过 22 / 失败 22      ← 失败全部是 Windows.h 缺失
打桩后：  通过 43 / 失败  1      ← 仅 usb_listener.cpp 失败（需要真实 Win32 MSG/WM_DEVICECHANGE/DBT_*）
```

**移植阻塞只有两处（精确、可验证）**：

1. **22 个编译单元无条件 `#include <Windows.h>`** —— 多数是经由三个头文件扩散：
   - `mainwindow.h:31`（被几乎所有页面间接包含）
   - `threads/adbthread.h:7`（被所有列表页包含）
   - `sonPages/apps/tp_packageprofile.h:7`
   - 直接包含的：`main.cpp:12`、`usb_listener.h:5`（`+ dbt.h:7`）
2. **`usb_listener.cpp:9-49` 的 Win32 热插拔实现**：`MSG` / `WM_DEVICECHANGE` / `DBT_DEVICEARRIVAL` / `DBT_DEVICEREMOVECOMPLETE`，无跨平台抽象。

### 2.2 构建配置硬伤（实测报错）

`QtAdb.pro:9` 的 `CONFIG += c++11` 与 Qt 6 直接冲突，实测报错原文：

```
qcompilerdetection.h:1333:6: error: #error "Qt requires a C++17 compiler"
qtypeinfo.h:34:15: error: 'is_trivially_default_constructible_v' is not a member of 'std'
```

`-std=c++17` 下同样代码编译通过 → **必须删除该行（Qt6 需 C++17）**。

### 2.3 一处对既有结论的纠正

有分析认为 `usb_listener` 的 `nativeEventFilter(..., long long *result)` 签名在 LP64 上与 Qt6 的 `qintptr *` 不匹配、会导致类保持抽象并编译失败。**实测证明该结论错误**：Qt 把 `qintptr` 定义为 `qint64`（即 `long long`），两种平台都匹配：

```cpp
static_assert(std::is_same<qintptr, long long>::value);  // 通过
static_assert(std::is_same<qintptr, long>::value);       // 失败 → qintptr 不是 long
```

该处仍是设计问题（见 P1-13），但**不是移植阻塞**。

### 2.4 取证的边界（诚实声明）

本节是**语法编译**验证，**未做** moc / uic 全流程与链接，**未运行**程序（无 Android 设备）。因此：
- 已证实：头文件依赖、Qt6 API 可用性、C++ 标准要求、逐文件编译可行性；
- 未证实：运行时行为、moc 生成后的链接错误、真实 UI 表现。§4 中标注为"运行期 UB"的条目，均来自逐行代码阅读 + 控制流可达性分析。

---

## 三、项目事实清单（量化）

### 3.1 代码规模分布

| 位置 | 行数 |
|---|---|
| `mainwindow.cpp` | 1030 |
| `pagemaker.cpp` | 604 |
| `sonPages/sonpagemanager.cpp` | 477 |
| `adbprocess.cpp` | 408 |
| `sonPages/apps/sp_permissions.cpp` | 393 |
| `textexplainer.cpp` | 363 |
| 其余 38 个文件 | 均 < 310 |

### 3.2 资源与仓库卫生

- 嵌入资源 4 个 `.qrc`、131 文件、**33.64 MiB**，其中 `fonts.qrc` 占 **91.8%**。
- **21.2 MiB 是死资源**：`LXGWWenKaiLite-Bold.ttf` + `-Light.ttf` 从未被 `QFontDatabase` 注册（只有 Regular 被 `main.cpp:24` 加载）；`ico.qrc` 无孤儿，但 `image/test/` 是遗留开发图。
- 平台工具 16 文件 **13.81 MiB**，Android Platform-Tools **33.0.2**，12 个是 Windows PE。
- **协议冲突（真实合规风险）**：仓库 `LICENSE` 是 MIT 且无第三方声明，但 `NOTICE.txt:621-643` 明确 `mke2fs`/`make_f2fs*` 三个二进制是 **GPL-2.0**——公开仓库再分发 GPL 二进制需履行源码提供义务。
- **无 `.gitignore`**；`QtAdb.pro.user`（19 KB，泄漏原作者 `C:\Users\DXLin\...` 路径）与 `logo.aps`（无人引用）被误提交。
- `.qrc` **不包含** platform-tools → 不是解包机制，而是 `mainwindow.cpp:79-130` 向 `PATH` 注入 `<appdir>\Resources\platform-tools`（README:204 印证）。

### 3.3 结构性重复（实测计数）

| 重复模式 | 次数 |
|---|---|
| `setShadow(QWidget*)` 完全相同实现 | 7 |
| `scrollbar.qss` 的 `readAll()` 块 | 11 |
| 列表刷新（emptyModel/model/explainer/delete）块 | 6 |
| 激活器页面定义（`sonpagemanager.cpp:74-339`） | 18 |
| 校验用正则（且**拒绝 "0"**） | 13 |
| 按键事件单行槽（`btnemulator.cpp`） | 29 |
| `setArg()` 近似分支（`sp_permissions.cpp:125-190`） | 6 |
| 硬编码 `"adb …"` 命令字符串 | 184 |
| `QGraphicsDropShadowEffect` 逐一手写（`mainwindow.cpp:626-673`） | 12 |

---

## 四、缺陷清单（按严重度）

### P0 — 崩溃 / 未定义行为 / 数据损坏

| # | 位置 | 问题 |
|---|---|---|
| 1 | `sonPages/activator/sp_activator.cpp:98`、`recovery/sp_recovery.cpp:147` | **显式调用析构函数** `page->~standardOutputPage();`，而 `sp_activator.cpp:59` 稍后 `delete page` → **双重析构**（`delete ui` 执行两次） |
| 2 | `animationwidget.h:21` + `basepage.h:32` + `basepage.cpp:190` | `~animationWidget()` **非虚**，却通过 `animationWidget *sonPage` 执行 `delete sonPage` → 派生类析构永不执行：**20 个子页面的 `delete ui` / `delete process` 全是死代码**，属 UB |
| 3 | `sonPages/apps/sp_permissions.h:47` + `:354` | `QStandardItemModel* current_model;` **无初始化**，在异步刷新前点"查看"即 `setModel(垃圾指针)` → 崩溃 |
| 4 | 6 个头文件（`sp_features.h:37`、`sp_libraries.h:38`、`sp_permissiongroups.h:37`、`sp_packages.h:53`、`sp_users.h:41`、`sp_accounts.h:42`） | `textExplainer *explainer;` **未初始化**，8 处解引用 → UB（当前仅因 `textExplainer` 方法不触碰 `this` 而未爆） |
| 5 | `threads/thread_monitor.cpp:38-62` | `list_t2[1]…[7]`、`mem.split("\n")[0..2]` **无边界检查** → adb 输出非预期格式时越界读崩溃；`MemTotal` 为 0 时除零 |
| 6 | `adbprocess.cpp:19-66` 等 4 处 | `args_0` 以 `"*"` 作**哨兵**结束循环：命令中含 `*`（如 `ls *`）会被**截断**；`args_1.removeAt(0)` / `removeAt(2)` **无长度检查**，传入 `"'"` 即越界崩溃 |
| 7 | `sonPages/advanced/sp_captive_portal.cpp:97,137` | `if(result)` 判断 `QMessageBox::StandardButton`——**`No` = 0x10000 为真**，点"否"反而执行修改/重置（`sp_users.cpp:94` 写法正确，可对照） |
| 8 | 8 处（`sp_features.cpp:75`、`sp_packages.cpp:96`、`sp_permissions.cpp:258,323,392` …） | `delete emptyModel` 时它**仍是 view 的 model** → 一旦列表为空即 use-after-free |
| 9 | `sp_captive_portal.cpp:73,99,139` | 对可能为空的 `getprop` 结果取 `[0]` → 断言/UB |
| 10 | `mainwindow.cpp:491→502/506` | `currentPage = new basePage(this)` 后**立即被覆盖**，且首个对象从未加入布局 → 每次切换页面泄漏一个 `basePage` |

### P0 — 架构级（重写的核心理由）

| # | 位置 | 问题 |
|---|---|---|
| 11 | `adbprocess.cpp:391-397` | **析构函数里执行 `run("adb kill-server")`**：全项目 **21 个 `adbProcess` 实例**（`grep` 实测），任何一个销毁都会杀掉**全局共享的 ADB 服务**，殃及 Android Studio / scrcpy 等外部工具。`on_adbKillerBtn_clicked` 的提示语"请勿连续执行！！！"正是这个设计的自白 |
| 12 | `adbprocess.cpp:70,71,151,152,224,225,298,299,378,379` | **无超时阻塞 GUI**：`waitForReadyRead()` + `waitForFinished()`（默认 30s×2，且不检查 `start()` 失败）。被 **约 70 处控件槽**直接调用（`btnemulator.cpp` 29 处、`sp_vibrate.cpp` 10 处…）→ 设备离线/等待授权时界面直接冻死 |
| 13 | `threads/thread_monitor.cpp:15-24` + `mainwindow.cpp:380-388` | **假线程**：`thread_monitor::run()` 是空的（全被注释），`getInfo()` 由主线程的 `QTimer` 驱动 → 每 2 秒在主线程里同步阻塞两次 ADB；且每次刷新 `thread_mon` 未删除（`mainwindow.cpp:336` 的 `deleteLater` 被注释掉）→ 泄漏 |
| 14 | `usb_listener.h:9` + `usb_listener.cpp:52` | 无意义地多继承 `QWidget`，并在**事件过滤器**里返回 `QWidget::nativeEvent(...)`（语义错误：过滤器应返回"是否已处理"）；一次事件还发出 3 个信号（`DeviceChangeCbk`/`DeviceChanged`/`DevicePlugIn`）触发重复刷新 |
| 15 | `pagemaker.cpp`（604 行）+ `sonPages/sonpagemanager.cpp:44` | 巨型 `switch` 工厂：新增一个页面要同时改 `.pro`、`pagemaker.cpp`、`sonpagemanager.cpp` 三处；`switch(key+1)` 无 `default`，静默返回 `NULL`；失败时 `throw "failed to create sonPage!"`（抛 `const char*`），全项目**无任何 `try/catch`** → 直接终止进程 |
| 16 | 全局 | **字符串状态机**：`"[开机]"`、`"[未响应]"`、`"[REC]"`、`"[侧载]"`、`"[未知]"` 在 `pagemaker.cpp:52,63,80` 等处重复判定 13 次；`pagemaker.cpp:19` 用 `dev.addr.toInt() == -1` 识别"无设备"，而 `"#EMPTY#".toInt()` 实际返回 **0** → 哨兵逻辑失效 |

### P1 — 逻辑错误

| # | 位置 | 问题 |
|---|---|---|
| 17 | `mainwindow.cpp:381,422` | 用 `devList[current_device]`（**旧索引**）而非传入的 `index` 创建监视线程 → **监控错设备** |
| 18 | `mainwindow.cpp:255-264` | `if(changed)` 的 `clear()+setUpList()` **写在 for 循环体内** → N 个设备重建 N 次；`:277-283` 又无条件重建一次 |
| 19 | `mainwindow.cpp:201` | 设备变化检测**只比较 `addr`** → 设备从"未授权"变"已授权"时不刷新 |
| 20 | `mainwindow.cpp:57` | `process->run("adb server")` —— **不是合法 adb 命令**（应为 `start-server`） |
| 21 | `mainwindow.cpp:81` | `QTextCodec::codecForName("UTF_8")` 名称错误（应 `UTF-8`），失败返回 `nullptr` → 重置为本地编码；`QTextCodec` 在 Qt6 需 Qt5Compat |
| 22 | `mainwindow.cpp:110-129` | PATH 拼接硬编码 `';'`（Unix 应为 `':'`）→ Linux 下 adb **永远找不到**；`Q_OS_LINUX` 分支用反斜杠且指向**仓库中不存在**的 `platform-tools-linux`；三个 `#ifdef` 都不匹配时 `userPath` **未声明**（编译错误） |
| 23 | `mainwindow.cpp:73` | `~MainWindow` 未 `delete listener`（泄漏） |
| 24 | `sonPages/apps/sp_libraries.cpp:46` | 页面标题是"支持的所有库"，却执行 `pm list features`（与 `sp_features.cpp:45` 完全相同）→ **数据错误** |
| 25 | `sonPages/advanced/sp_status_bar.cpp:45-94,133-146` | `icon_blacklist` 拼接产生**前导逗号**；未勾选时写入空值；`refreshBtns()` 漏重置 `speakerphone`、`bluetooth` 被清两次 |
| 26 | `sonPages/advanced/sp_scales.cpp:29-31`、`sp_vibrate.cpp:29-38` | 校验正则**拒绝输入 "0"** → 动画倍速无法设为 0；且点击时**不检查** `hasAcceptableInput()` → 空值/中间态被直接发送 |
| 27 | `sonPages/apps/sp_users.cpp:91-102` | 未检查选中项 → `user_id` 为空时执行 `pm remove-user ""` |
| 28 | `sonPages/apps/sp_installer.cpp:106`、`recovery/sp_recovery.cpp:122` | 文件对话框起始目录硬编码 `"F:"` |
| 29 | `sonPages/standardoutputpage.cpp:11-13` | `SLOT(update())` 与 `update(QString)` **签名不匹配** → connect 静默失败，33Hz 定时器空转、输出窗口永不刷新；`:49,55` 每块都全量 `setText` → O(n²) |
| 30 | `sonPages/recovery/sp_recovery.cpp:134-137` | 输出窗口连的是页面自己的 `process`，而实际执行走 `thread` 内另一个 `adbProcess` → **永远看不到输出** |
| 31 | `msgBox`（`textexplainer.h:37`） | 裸指针成员**从未初始化** |
| 32 | `textExplainer`（`textexplainer.h:21-53`） | **不是 `QObject` 却写了 `signals:` / `public slots:`** —— 这些"信号"永远不会发射（`emit` 是空宏），任何依赖它们的连接都静默失效 |
| 33 | `adbprocess.cpp:297,376` | 在 `start()` 之后**未等 `started()`** 就 `write()` → 数据可能被丢弃 |
| 34 | `adbprocess.cpp:75,303` | `standardOutput` 只追加不清空 → 无界内存增长 |

### P1 — 资源与内存

- 每页 `new adbProcess()` 从不释放（15 个页面）；每页 `new adbThread()` 无父对象（9 处）；每次刷新 `new adbThread()` ×3 / ×10（`sp_scales.cpp:70-83`、`sp_vibrate.cpp:114-162`）→ **每个线程泄漏一个 `adbProcess` + `textExplainer`**。
- 每次点击泄漏对话框：`sp_permissions.cpp:67-75`（QDialog + 6 控件）、`:349-358`（顶层窗口）、`mainwindow.cpp:700`（QMessageBox）、`:713`（`getInfo` 成员被反复覆盖）。
- 在**槽函数内部** `connect()` → 每次点击累积重复连接（`sp_installer.cpp:92`、`sp_recovery.cpp:108`）。
- `sonPageManager`、`pageMaker`、`adbThread` 的析构里 `delete` 全被注释掉。

### P2 — 可维护性

- **命名与注释**：`mainwindow.h:96-98` 的 `taiChi`/`liangYi`/`bagua`/`siXiangTimer`，其中 `bagua` 的注释是"**忘了是干啥的了**"——这是知识已丢失的信号。
- 330 处 `qDebug()`（含每 2 秒刷新的热路径）、474 行注释代码、`"Bug#01 finding!"` 之类调试面包屑留在生产代码里。
- 样式双轨：已有 `qss/` 资源与 `styles.qrc`，但 `mainwindow.cpp:604-617`、`:702`、`:716` 等大量内联样式表；12 个阴影效果逐个手写。
- `mainwindow.cpp:100-108` 用正则检查路径是否含中文并弹窗——把"adb 不支持中文路径"的环境限制做进了 UI 层。
- `textExplainer`（363 行）在进程层被直接调用解释输出（`adbprocess.cpp:76,157,…`），把"翻译 adb 输出"和"执行 adb"耦合在一起，违反单一职责。

---

## 五、优化 / 重写方案

### 5.1 方案 A（推荐）：分层重写，保留 UI 资产

保留：33 个 `.ui`、图标/SVG、QSS 样式、字体（仅 Regular）、`textExplainer` 的输出解析知识、184 条 adb 命令的业务语义。
重写：ADB 访问层、设备模型层、线程/异步层、页面工厂、构建系统。

**目标分层**

```
ui/          ← 现有 .ui + 轻量 View 类（只管展示，不碰 QProcess）
core/
  AdbClient      QProcess 异步封装：start(program, args[]) 参数分离、超时、取消、
                 错误分类；进程内单例；【禁止 kill-server】
  AdbCommand     命令构建器：把 184 处字符串字面量收敛为具名命令 + 参数列表
  DeviceModel    QAbstractTableModel + enum class DeviceState{Online,Unauthorized,Recovery,Sideload,Offline,Unknown}
  DeviceMonitor  取代假线程：QTimer/异步轮询 adb devices -l，差异比较含 state
platform/
  HotplugWatcher 抽象接口
    Win32Hotplug   WM_DEVICECHANGE
    LinuxHotplug   udev 或 adb track-devices
    NullHotplug    轮询兜底
pages/       页面注册表（替代 604 行 switch），每个页面自注册 key/标题/图标/构造器
```

**关键设计要求**

1. `AdbClient` 全部异步（`finished` 信号 + `QTimer` 超时），**彻底消灭** `waitForReadyRead/waitForFinished`；UI 永不冻结。
2. `QProcess::start(program, QStringList)` **不经 shell**；参数以列表传递，废弃 `split(" ")` + `'` 状态机，从根上消除截断/越界/注入。
3. 命令字符串集中到 `AdbCommand`，例如 `AdbCommand::pmListLibraries()`；页面不再拼字符串。
4. 引入 `enum class DeviceState` 与统一状态解析，消灭 13 处字符串判定。
5. 页面工厂改为注册表（`QHash<int, PageFactory>`），新增页面只改一处。
6. 样式全部移入 `.qss`；阴影等重复代码提取为工具函数。
7. 全量 `tr()` + `lupdate` 生成真实 `.ts`（当前 0% 覆盖），或删除 lrelease 机制——**二选一，不要留半成品**。
8. 构建迁移 CMake + Qt6 + C++17：`qt_add_executable` + **`qt_add_big_resources`**（33.6 MiB 资源必须用 big 版本）+ `qt_add_translations`；删除 `core5compat`（`QTextCodec`→`QStringConverter`，删无用 `QRegExp`）；`RC_FILE` 加 `if(WIN32)`；移除 `CONFIG += c++11`。
9. 仓库治理：新增 `.gitignore`（`*.pro.user`、`*.aps`、`build-*/`、`moc_*`、`ui_*`、`qrc_*`）；删 `logo.aps`；移除 21.2 MiB 死字体；platform-tools 改为**按平台分发/首次运行下载**并补第三方许可声明（解决 GPL-2.0 合规缺口）。

**分阶段路线图（每阶段都能编译、可验证）**

| 阶段 | 内容 | 完成判据 |
|---|---|---|
| M0 | CMake + Qt6 + C++17 迁移；Windows 专属代码全部 `#ifdef Q_OS_WIN` 化；`.gitignore` + 死资源清理 | Linux/Windows 双平台**编译链接通过**（当前 Linux 22/44 失败） |
| M1 | `AdbClient` + `AdbCommand` 落地，替换 `adbProcess`；删除析构里的 `kill-server`；消灭全部阻塞等待 | 无 `waitForFinished` 调用；adb 命令单测通过；UI 在设备离线时仍可交互 |
| M2 | `DeviceModel` + `DeviceState` + `DeviceMonitor`；修 P1 逻辑错误（#17–#25） | 授权状态变化能刷新；监控指向正确设备；无越界/除零 |
| M3 | `HotplugWatcher` 抽象 + Linux 实现；页面注册表；P0 崩溃项（#1–#10）全清 | Linux 下热插拔可用；`-fsanitize=address,undefined` 跑遍所有页面无报告 |
| M4 | i18n 全量 `tr()` + `lupdate`；QSS 集中；重复代码（7/11/18/29 处）消除 | `.ts` 覆盖率 > 95%；重复块消失；仓库瘦身 |

### 5.2 方案 B（保守）：不换构建系统，最小补丁止血

若必须继续用 qmake 且只做 Windows，建议按此**最小补丁集**（约 10 项，均可独立提交）：

1. `animationwidget.h:21` 改 `virtual ~animationWidget();`（一行，解锁 20 个子页析构）
2. 删 `sp_activator.cpp:98`、`sp_recovery.cpp:147` 的显式析构调用，改 `deleteLater()`
3. `sp_permissions.h:47` `= nullptr` + `setModel` 前判空
4. 6 处 `explainer` 成员初始化
5. `sp_captive_portal.cpp:97,137` 改 `== QMessageBox::Yes`
6. `adbprocess` 全部 `waitFor*` 加超时（`waitForFinished(15000)`）
7. `adbprocess.cpp:393` 删除 `run("adb kill-server")`
8. `thread_monitor.cpp` 数组索引判界 + `MemTotal` 除零保护
9. `mainwindow.cpp:381,422` 改用 `index`；`:255-264` 循环体移出
10. `.pro` 删 `CONFIG += c++11`；`mainwindow.cpp:125` PATH 分隔符按平台

> 方案 B 能显著降低崩溃率，但**无法解决** 21 个进程对象、假线程、604 行工厂、441 处裸 `new` 这些结构性债务，也无法让项目跨平台。

---

## 六、复现与验收

**编译取证环境**（免 root，因容器内 `sudo` 被 `no new privileges` 拦截）：
把 Fedora 44 的 Qt6 开发 RPM 解包到 `~/.local/qt6-prefix`，配 `qt.conf` 重定向 prefix，再补系统运行库软链。

**产物位置**（均在仓库外，不影响 `git status`）：

| 路径 | 内容 |
|---|---|
| `.dsh/logs/syntax-all.log` | 44 个编译单元逐文件编译结果（未打桩） |
| `.dsh/logs/build.log` / `qmake.log` | qmake/make 尝试日志 |
| `.dsh/err*/` | 每个编译单元的完整编译器输出 |
| `.dsh/probe/` | `qintptr`、C++11/17 三个验证探针 |
| `.dsh/ui/` | `uic` 生成的 33 个 `ui_*.h` |
| `.dsh/build-linux/` | shadow build 目录（qmake 半途受限于自建前缀） |

**建议的验收门槛**：① M0 后 Linux/Windows 双平台 CI 编译通过；② ASan+UBSan 下遍历全部页面无报告；③ adb 命令层单元测试（含含空格/`*`/引号的参数）；④ 设备未授权/离线时 UI 不冻结。

---

## 七、需要你决策的事项

1. **目标平台**：只修 Windows，还是要做成 Linux/macOS 可用？（决定是否做 `HotplugWatcher` 与 `AdbClient` 平台层）
2. **UI 技术**：继续 Qt Widgets + 保留 33 个 `.ui`，还是借重写机会转 QML？
3. **platform-tools 策略**：继续内置 Windows 二进制，还是改首次运行下载/发布到 Release 资产（同时解决 GPL 合规）？
4. **重构方式**：走方案 A（分层重写，工作量大、收益彻底）还是方案 B（最小补丁止血，先保稳定）？
5. **"按要求"中的"要求"具体指什么**——原始需求里只有一句 `按要优或优化或重写qtadb项目代码`，若你有明确的验收标准（例如"必须能在 Linux 上跑通设备管理"），告诉我即可据此重排优先级。

---

## 八、M0 执行记录（已完成，改动留在工作区未提交）

方向已确认：**先做 M0（构建系统迁移 + 平台解耦），目标 Windows + Linux 双平台**。

### 8.1 实际改动

| 文件 | 改动 |
|---|---|
| `QtAdb/CMakeLists.txt` | **新增**。Qt6 + C++17 + AUTOMOC/AUTOUIC；资源用 `qt_add_big_resources`；源文件清单由 `QtAdb.pro` **机械生成**，逐项等价（44/43/33/4）；`win32` 下处理 `logo.rc` 并复制 `Resources` 到可执行文件目录；补 `target_include_directories` 以适配工程原有的"扁平包含"风格 |
| `QtAdb/QtAdb.pro` | `CONFIG += c++11` → `c++17`；移除未被引用的 `serialport` 与仅为一行 `QTextCodec` 存在的 `core5compat`；`RC_FILE` 限定 `win32` |
| `.gitignore` | **新增**（构建产物、Qt 生成文件、IDE 状态；对 `Resources/platform-tools/*.exe\|dll` 做反向豁免，避免误伤随源码分发的二进制） |
| `main.cpp`、`mainwindow.h`、`sonPages/apps/tp_packageprofile.h`、`threads/adbthread.h` | 删除 **4 处无用的 `windows.h` 包含**（这 4 个文件对 Win32 符号的使用量为 0） |
| `usb_listener.h/.cpp` | Win32 热插拔逻辑以 `#ifdef Q_OS_WIN` 隔离，非 Windows 分支返回 `false`（含 M3 TODO）；签名 `long long*` → `qintptr*`；事件过滤器明确返回 `false`（原先返回 `QWidget::nativeEvent(...)` 属语义错误）；删除 60 行注释死代码 |
| `mainwindow.cpp/.h` | 删除 `QTextCodec::setCodecForLocale("UTF_8")`（Qt6 已移除该类，且名称有误会返回 nullptr）；重写 PATH 注入：`QDir::listSeparator()` 取代硬编码 `';'`、目录不存在时不污染 PATH、消除 `#ifdef` 导致 `userPath` 未声明的编译错误 |
| `fonts/fonts.qrc` + 2 个 ttf | 移除从未被注册/引用的 Bold 与 Light 字重（**21.2 MiB**），保留 Regular |
| `logo.aps` | 删除（全仓 0 处引用） |
| `QtAdb.pro.user` | `git rm --cached`：移出版本控制，**本地文件保留**，QtCreator 配置不受影响 |
| `README.md` | 构建章节补充 CMake 用法与 Qt6/C++17 说明 |

### 8.2 验证证据（第一手）

| 指标 | 改动前 | 改动后 |
|---|---|---|
| Linux/Qt6 可编译单元 | **22 / 44** | **44 / 44** |
| CMake 配置 | 无 CMake 工程 | `configure = 0` |
| 全新目录构建 | — | **0 error**（39 warning，均为既有问题） |
| 可执行文件体积 | 34.6 MiB | **13.4 MiB**（−21.2 MiB） |
| 链接的 Qt 模块 | Core/Gui/Widgets/DBus + **Core5Compat + SerialPort** | Core/Gui/Widgets/DBus |
| 运行时 | 无法构建 | offscreen 启动后进程存活 25s（**判据过弱，见 §9.2 的方法论修正**）；第二轮改用生命周期驱动验证：遍历 7 个页面后干净退出，退出码 0 |

### 8.3 未验证项（如实声明）

- **Windows 构建未编译验证**：本机无 MSVC/MinGW 也无 Qt for Windows。Windows 分支的**逻辑与原文逐行一致**（仅去掉注释、加 `const`、把 `long long*` 换成等价的 `qintptr*`，且 `QWidget::nativeEvent` 默认返回 `false` 与新的 `return false` 行为相同），风险低，但仍建议你在 Windows 上跑一次 `cmake -S QtAdb -B build -G "MinGW Makefiles"`。
- 39 条编译警告属既有问题（26 条 `unknown escape sequence '\.'`、13 条 `QFile::open` 返回值未检查——后者正对应审计报告 P1 第 27 项），留待 M1/M2 处理。

### 8.4 提交方式

改动**未提交**（仓库未配置 `user.name`/`user.email`，且既有提交者是你本人，不适合由我代签）。审阅后可直接提交：

```bash
cd QtAdb
git add -A
git commit -m "M0: 迁移到 CMake/Qt6/C++17 并完成平台解耦

- 新增 CMakeLists.txt（Qt6/C++17/big_resources），源文件清单与 QtAdb.pro 等价
- 仅依赖 Qt6::Widgets：移除未使用的 serialport 与仅为一处 QTextCodec 的 core5compat
- 删除 4 处无用 windows.h；usb_listener 的 Win32 代码以 Q_OS_WIN 隔离
- 修正 PATH 注入的平台分隔符与路径拼接
- 移除 21.2 MiB 未被引用的字体与 logo.aps；新增 .gitignore
- 验证：Linux+Qt6.11 全新目录 0 error，产物 34.6→13.4 MiB，offscreen 启动正常"
```

### 8.5 建议的下一步（M1）

1. `AdbClient`：把 21 个 `adbProcess` 收敛为单例，异步 + 超时，**删除析构里的 `adb kill-server`**；
2. 用 `QProcess::start(program, QStringList)` 取代 `split(" ")` + `'` 状态机，消除命令截断/越界；
3. 先清 P0 崩溃项（报告 §4：#1–#10），其中 `virtual ~animationWidget()` 是一行改动、收益最大。

---

## 九、P0 止血（第二轮，改动仍在工作区未提交）

在 M0 骨架之上，本轮集中处理**崩溃 / 未定义行为**这一类不依赖架构决策、收益最大的问题。
相对 M0 的增量：**38 个文件，+315 / −454 行**。

### 9.1 修复清单

| # | 位置 | 问题 → 修复 |
|---|---|---|
| 1 | `adbprocess.cpp` `~adbProcess()` | 析构中执行 `run("adb kill-server")`：全工程 21 个实例，任一析构都会杀掉全局共享 ADB 服务（殃及 Android Studio / scrcpy），且在析构中启动子进程属 UB → 删除，改 `terminate()+waitForFinished(3000)`；用户主动"杀死 ADB"由按钮槽单独提供 |
| 2 | `animationwidget.h` | `~animationWidget()` 非虚，而 `basepage.cpp:190` 通过基类指针 `delete sonPage` → 20 个子页析构函数永不执行（`delete ui` 成死代码）且属 UB → 改为 `virtual` |
| 3 | `animationwidget.cpp` `~animationWidget()` | 析构中调用 `playExitAnimation()`：紧接 `delete animation` 故动画根本不会播放；会在析构中 `show()` 并 `emit animationEnd()` 触发已连槽函数；且解引用 `parent` → **删除该调用**（退出动画由调用方如 `basePage::spg_return_pushed()` 显式播放） |
| 4 | `animationwidget.cpp` ×22 | `parent->width()` 未判空，而 `sp_packages.cpp:103` 以 `NULL` 为父创建 `tp_packageProfile` → 新增 `parentWidgetWidth()`（`parent` 为空时回退自身宽度）统一替换 |
| 5 | `sp_activator.cpp:98`、`sp_recovery.cpp:147` | 显式调用堆对象析构函数 `page->~standardOutputPage()`，而析构函数中还会 `delete page` → **双重析构**（`delete ui` 两次）→ 改为 `hide()+deleteLater()+置空` |
| 6 | `sp_permissions.h` + `.cpp` | `current_model` 未初始化即交给 `setModel()`（刷新是异步的，提前点"查看"必崩）→ `= nullptr` + 前置判空提示；顶层窗口加 `WA_DeleteOnClose`（原先每点一次泄漏一个） |
| 7 | 6 个页面头文件 + 8 处调用 | `textExplainer *explainer;` 声明后**从未赋值**却直接解引用 → 改为**值成员** `textExplainer explainer;`，调用点 `explainer->` → `explainer.`（同时消除泄漏） |
| 8 | `textexplainer.h` | `QMessageBox *msgBox;` 未初始化 → `= nullptr` |
| 9 | `sp_captive_portal.cpp` ×2 | `if(result)` 判断 `QMessageBox::StandardButton`——**`No == 0x10000` 为真值**，点"否"反而执行修改/重置 → `== QMessageBox::Yes` |
| 10 | `sp_captive_portal.cpp` ×3 | 对可能为空的 `getprop` 结果取 `[0]`（空串越界）→ 改 `left(1)` |
| 11 | `thread_monitor.cpp` | `list_t2[1]..[7]`、`mem.split("\n")[0..2]` 无边界检查；CPU 总时间差与 `MemTotal` 为 0 时除零 → 补三处格式/边界/除零校验 |
| 12 | `adbprocess.cpp` ×4 处解析 | 以预先 append 的 `"*"` 哨兵作循环终止条件 → 命令含 `*` 时**参数被截断**；`removeAt(0)/(2)` 无长度检查 → 退化输入越界崩溃 → 改为按长度循环 + 删除前检查长度 |
| 13 | `mainwindow.cpp` `initBasePage()` | 先 `new basePage(this)` 再被工厂返回值覆盖（每次切页泄漏一个），且 `new device` 后解引用从不释放 → 删除临时对象、`device` 改栈对象 |
| 14 | `mainwindow.cpp` `~MainWindow()` | `listener` 注册了 `qApp` 原生事件过滤器却从不删除（泄漏），而直接删除又会留下悬空过滤器 → 先 `removeNativeEventFilter` 再 `delete` |
| 15 | `adbprocess.h/.cpp` | 成员 `standardOutput` 全工程**只写不读**且从不清理（无界内存增长）→ 连同两处 append 一并移除 |

### 9.2 验证方式升级（含一次方法论自我修正）

**教训**：M0 阶段我把"offscreen 启动后进程存活 25s、退出码 124"当作运行时证据。本轮在加固 `parent->width()` 时，我的批量替换误把辅助函数体内的 `parent->width()` 也换成了 `parentWidgetWidth()`，造成**自递归**；`-O3` 把它优化成尾调用死循环 —— 进程同样"存活 25s、退出码 124"。**这说明该判据无法区分"事件循环健康"与"卡死"**。§8.2 中相应结论已下调，改用下述强判据。

**新判据：生命周期驱动 + adb 打桩 + ASan/UBSan**

- `adb 打桩`（`.dsh/bin/adb`）：让应用认为存在一台已授权设备，从而真正走到设备页而不是"无设备"占位页；
- `生命周期驱动`（`.dsh/asan/main_test.cpp`）：构造 `MainWindow` → 依次切换全部 7 个页面（触发子页面创建与销毁）→ 干净退出，使**整条析构链**（`~sp_*` → `~basePage` → `~animationWidget` → `~adbProcess` → `~MainWindow`）真正执行；
- 由于本机未安装 `libasan`/`libubsan` 系统包（GCC 目录下的 `libasan.so` 只是指向只读 `/usr/lib64/libasan.so.8.0.0` 的链接脚本），改为下载 RPM 解包后**显式链接**运行时库。

**结果**：

```
[INFO] 进入事件循环
[INFO] indexList 行数 = 7
[INFO] 切换到第 0..6 页
[INFO] 页面遍历完成，准备退出
退出码 = 0        ← ASan + UBSan 全程零报告（无越界/双重释放/UB）
Release(-O3) 驱动同样：退出码 = 0
```

### 9.3 ASan 抓出的、审计阶段遗漏的真实崩溃

`textExplainer::explain_cpu_output()`（`textexplainer.cpp:154`）：

```cpp
QStringList tmpList = text.split("\n");
QString tmpInfo = tmpList[tmpList.count()-2];   // ← 输出不足两行时下标为负
```

当 `adb shell cat /proc/cpuinfo` 因设备离线/未授权/命令失败而输出不足两行时，`count()-2` 为负，
`QList::operator[]` 触发 Qt 断言直接 `abort()`。这是设备信息页监控线程的必经路径，
**审计清单（§4 P0 #5）只覆盖了 `thread_monitor` 的同类越界，漏掉了这一处**。
已补边界处理（`tmpList.size() < 2` 时返回空串）。

### 9.4 命令解析改动的等价性验证

由于改动触及"把命令字符串切成 argv"的核心逻辑，专门写了新旧解析器对比程序
（`.dsh/probe/parse_test.cpp`）逐条比对 9 个真实命令：

| 用例 | 结果 |
|---|---|
| `adb devices -l`、`adb server`、`adb -s SERIAL shell pm list packages`、`getprop/put global ...`、`adb install ~ /home/u/My App.apk ~`、`adb sideload ~ /tmp/a b/c.zip ~` | **新旧 argv 完全一致**（6 项） |
| `adb shell ls *`、`adb shell rm -rf /sdcard/tmp *` | 旧实现**截断**为 `[... ls]`；新实现保留 `*`（2 项，预期修复） |
| 退化输入 `'` | 旧实现 `removeAt` **越界**；新实现正常返回（1 项，预期修复） |

> 顺带发现（未改，记入 M1）：设备变体的调用方传入的命令串有时已自带 `-s <serial>`，
> 而 `adbProcess` 还会再前置一次，实测 argv 出现重复的 `[-s SERIAL -s SERIAL ...]`；
> `sonpagemanager.cpp` 中另有 `adb -d ...` 与自动前置的 `-s` 混用。M1 的命令构建器需明确二者关系。

### 9.5 仍未处理（留给 M1/M2）

- **约 70 处控件槽直接调用阻塞式 `adbProcess::run()`**（无超时，设备离线时界面冻结）——需要 M1 的异步 `AdbClient`；
- 页面工厂 `switch` 与字符串状态机（`pagemaker.cpp` 604 行）、`throw "..."` 无 catch；
- `delete emptyModel` 时它仍可能是 view 的当前 model（同类写法还有 7 处）；
- 26 条 `unknown escape sequence '\.'` 与 13 条 `QFile::open` 返回值未检查的编译警告。

---

## 十、M1 第一阶段：ADB 调用层的健壮性与去重（第三轮）

### 10.1 改动（`adbprocess.cpp/h`，+191 / −338）

| 项 | 原状 → 现状 |
|---|---|
| 重复代码 | 4 份几乎相同的"解析 + 启动 + 等待"逻辑（各约 80 行，含大量注释死代码与**不可达分支**）→ 收敛为单一 `execute()`，配套 `tokenizeAdbCommand()` / `waitWithDeadline()` / `reportProblem()` |
| **无限等待** | `waitForReadyRead()` + `waitForFinished()`（均无参＝无限等待）→ **有界等待**：200ms 轮询 + 总时限，超时即 `kill()` 子进程 |
| 超时分级 | 普通命令 **20s**；`install`/`sideload`/`push`/`pull`/`backup`/`restore` **600s**（安装大包不被误杀） |
| 启动失败 | 原先静默返回空串（表现为"设备列表永远为空"）→ 明确提示"无法启动 adb … No such file or directory" |
| 写入 stdin | 原先 `start()` 后立即 `write()`（数据可能被丢弃）→ 先 `waitForStarted(5s)` 再写 |
| adb 定位 | 原先只用 PATH 中的 `adb` → `adbProgramPath()`：**优先随程序分发的 platform-tools**，回退 PATH（Linux 用系统包） |
| 复用同一 QProcess | 上次未结束时直接 `start()` 会失败 → 先 `terminate()+waitForFinished(1s)` 清理 |
| 命令名弹出 | 无条件 `removeAt(0)/(2)` → 按长度检查 |
| `'` 引号语义 | 旧实现把引号本身并进参数（实测产出 `' com.a b`，必然导致 adb 参数错误）→ 不再并入 |

### 10.2 验证：四组对照

| 场景 | 结果 |
|---|---|
| **对照**：旧等待方式 + 永久挂起的 adb | 外部强杀于 20s，**从未返回**（证明原实现会无限卡死） |
| 修复后应用 + 永久挂起的 adb | **28s 有界完成**，退出码 0，日志出现 `[adb 命令超时 20000 ms，已终止该进程] devices -l` |
| 正常 adb + ASan/UBSan（Debug） | 退出码 0，遍历 7 个页面，**0 条 sanitizer 报告** |
| 正常 adb + Release | 8s 完成，退出码 0 |
| PATH 中完全没有 adb | 退出码 0，遍历完成，输出明确报错而非静默失败 |

命令解析等价性（新旧对比，10 例）：7 例 argv 完全一致；3 例为修复项（2 例 `*` 截断、1 例引号语义）；1 例退化输入旧实现越界。

### 10.3 教训：第二次验证方法论修正

本轮一度观察到"挂起 adb 下没有超时提示、耗时 60s"，排查后确认是**验证二进制陈旧** ——
我只重建了主程序目标，没有重建验证驱动，于是测的是上一轮的代码。
随后又发现 **Qt 在本环境默认静默 `qDebug`，且在非控制台时静默整个消息处理器**：
必须 `QT_ASSUME_STDERR_HAS_CONSOLE=1`（必要时再加 `QT_LOGGING_RULES='*.debug=true'`）
才能看到日志——在此之前，"日志里没有报错"这一观察本身就是假的。

**两条规则**：① 改源码后必须重建**所有**参与验证的目标；② 观察"没有日志"之前，先用 canary 验证日志通道本身。

### 10.4 M1 剩余（下一轮）

- **约 70 处控件槽仍在 GUI 线程同步等待**：现在虽是"有界"（最长 20s），但仍会冻结界面；
  根治方案是异步 `AdbClient`（`finished` 信号 + 进度回调 + 取消），把等待移出 GUI 线程；
- 21 个 `adbProcess` 实例尚未收敛为单一客户端；
- 命令构建仍靠字符串拼装（184 处字面量），需要 `AdbCommand` 具名化；
- 实测发现设备变体存在 `[-s SERIAL -s SERIAL …]` 重复前缀、以及 `adb -d` 与自动 `-s` 混用，需在命令层明确。

---

## 十一、M1 第二阶段：真实后台监视线程 + 一批逻辑修复（第四轮）

### 11.1 监视线程：从"假线程"到真线程

| | 原状 | 现状 |
|---|---|---|
| `run()` | **空实现**（内容全被注释） | 内部循环采样：2s 周期、100ms 分片睡眠以便及时响应停止 |
| 采样驱动 | GUI 线程里的 `QTimer` → `getInfo()` 槽 | 工作线程自身；结果经 `signal_monitor` 跨线程队列投递 |
| QProcess 归属 | 在 GUI 线程创建、却在另一个线程使用 | 在**工作线程内**创建与使用 |
| 后果 | 每 2 秒在**界面线程**同步阻塞两次 adb 调用（每次最长 20s）——最频繁的卡顿源 | 界面线程不再参与采样 |
| 生命周期 | `siXiangTimer` + `thread_mon` 的启停在 3 处重复代码里，其中两处用 `devList[current_device]`（**旧索引**）创建对象 | 统一 `resetMonitorThread()` / `stopMonitorThread()`；线程结束后 `deleteLater` 自释放；索引 bug 修复 |

### 11.2 本轮逻辑修复

| 位置 | 问题 → 修复 |
|---|---|
| `mainwindow.cpp` `refreshDevList()` | 设备变化检测只比较 `addr` → 补上 `state`：设备"未授权 → 已授权"不再需要手动点刷新 |
| `resetMonitorThread()` / `setCurrentDevice()` | 用 `devList[current_device]`（旧索引）创建监视线程 → 改用传入的 `index` 并做范围检查 |
| `standardoutputpage` | ① 30Hz 定时器连的是 `SLOT(update())`，实际匹配的是 `QWidget::update()`（重绘），与文本刷新毫无关系，纯属每秒 30 次无用重绘 → 删除；② `update(QString)` 每收到一块输出就 `setText(全量)`（长日志下 O(n²)）→ 改为 `insertPlainText` 增量追加 + 自动滚到底部 |
| `sp_libraries.cpp` | 页面标题是"当前设备支持的所有库"，执行的却是 `pm list features`（与 sp_features 完全相同）→ 改为 `pm list libraries` |
| `sp_users.cpp` | 未选中任何条目就确认，会执行 `adb shell pm remove-user ""` → 增加选中校验，并把确认框移到校验之后 |
| `sp_status_bar.cpp` | ① `icon_blacklist` 值固定以 `,` 开头（形如 `,volume,wifi`，Android 会忽略整个值）；② 一个都不勾选时写入空值＝清空设置；③ `refreshBtns()` 重复清 `bluetooth` 而漏掉 `speakerphone` → 改为逗号连接 + 空选择拦截 + 修正复位 |

### 11.3 验证：驱动增强 + 四组场景

驱动新增三项能力：**自动关闭模态对话框**（应用里大量 `QMessageBox` 会阻塞事件循环）、**逐页点击可见按钮**、**在点击停止后留出采样时间再读监视结果**。

| 场景 | 结果 |
|---|---|
| ASan/UBSan + 选中设备 + 逐页点击按钮 | 退出码 0，7 页遍历，**0 条 sanitizer 报告**，自动关闭 3 个模态框 |
| Release + 正常打桩 | 退出码 0，22s，**监视结果 CPU = 100% / RAM = 50%**（与打桩注入的数据精确吻合，证明后台线程确实在采样、计算并跨线程投递） |
| Release + 永久挂起 adb | 退出码 0，61s **有界返回**，3 次超时兜底，遍历完成 |
| 全部场景 | **0 条 QThread 销毁告警**（线程干净停止） |

### 11.4 第三次方法论修正

**ASan 这次抓到的是我自己的测试驱动**：点击后 `processEvents()` 会触发 `deleteLater`，
之后再从**已经悬空的裸指针**构造 `QPointer` —— 构造过程本身就会读取已释放内存。
正确做法是"在对象仍然存活时先把裸指针转成 QPointer"。**验证代码同样会引入 UB，必须一起跑 sanitizer。**

另外**再次踩到"陈旧验证二进制"**：只重建了 ASan 驱动、没重建 Release 驱动，
导致 Release 场景结果缺失。该规则在 §10.3 已记录，这轮仍复发 —— 说明它是高频陷阱，
已固化为流程：**改源码后先重建全部验证目标，再解释任何"异常观测"**。

### 11.5 M1 剩余

- 约 70 处控件槽仍同步等待 adb（现为有界，最长 20s）；根治仍需异步 `AdbClient`；
- 21 个 `adbProcess` 实例尚未收敛；
- 命令仍靠字符串拼装（184 处字面量），`[-s SERIAL -s SERIAL …]` 重复前缀与 `adb -d` 混用待命令层解决；
- 其余 P1：`delete emptyModel` 时它仍可能是 view 的当前 model（还有 7 处同类写法）、槽内 `connect()` 重复累积、校验正则拒绝输入 "0"。

---

## 十二、M1 第三阶段：页面交互正确性（第五轮）

### 12.1 修复清单

| 位置 | 问题 → 修复 |
|---|---|
| `threads/adbthread.cpp` | ① 每次 `start()` 都 `new adbProcess()` 且**从不释放**——列表页每次刷新会创建 3~10 个线程，泄漏随之累积 → 在 `run()` 结束与析构时释放；② `#INSTALL#`/`#SIDELOAD#` 原先丢弃返回值、改为连接**即将被释放的**进程对象的信号再回传（多绕一层，且排队信号可能被丢弃）→ 直接用返回值；③ 删除 4 处 `exit()`（本线程没有事件循环，那是空操作） |
| `sonPages/apps/sp_installer.cpp` | ① `connect` 写在点击槽里 → 每点一次"安装"就多加一条连接，同一输出被处理多次 → 移到构造函数；② 文件对话框起始目录硬编码 `"F:"`（Windows 专用）→ `QDir::homePath()` |
| `sonPages/recovery/sp_recovery.cpp` | 输出窗口连接的是**本页的** `process`，而侧载实际由 `thread` 内部的另一个 adbProcess 执行 → 窗口永远空白 → 改连 `thread`；同样修掉 `"F:"` |
| 6 个列表页 + `sp_permissions` ×2 | `delete emptyModel` 时它仍可能是 view 的当前 model（列表为空即 **use-after-free**）→ 先解绑再 `deleteLater()`（与 §9 中 sp_permissions 的修法一致，本轮补齐其余 7 处） |
| `sp_scales` / `sp_vibrate` | ① 13 个校验器共用的正则**拒绝输入 "0"**——"关闭动画"这一常用设置无法保存 → 换成 `^[0-9]+(\.[0-9]+)?$`；② 该字面量里的 `\.` 是**非法 C++ 转义**，正是 **26 条编译器警告**的来源，一并消除；③ 点"应用"时不检查输入有效性，空值/非法值被原样拼进 adb 命令 → 增加 `hasAcceptableInput()` 校验；④ sp_vibrate 的 10 个同构处理函数收敛为一个 `applySetting()` 辅助函数 |

### 12.2 验证

| 项 | 结果 |
|---|---|
| 校验器探针（**正则直接从源码提取**，避免测试与实现漂移） | 13 个用例全部符合预期：`0 / 0.0 / 0.5 / 1 / 1.0 / 10.5` 接受；`空 / . / 1. / .5 / -1 / abc / 1;2` 拒绝 |
| ASan/UBSan + 选中设备 + 逐页点击按钮 | 退出码 0，**0 条 sanitizer 报告**，**0 条 QThread 告警**，监视数据 CPU = 100% / RAM = 50% |
| Release + 正常打桩 | 退出码 0，22s，监视数据正确 |
| Release + 永久挂起 adb | 退出码 0，61s **有界返回**，3 次超时兜底，遍历完成 |
| 编译警告 | **37 → 13**（全新目录完整构建实测；其余 13 条全部为 `QFile::open` 返回值未检查，26 条非法转义警告已消除） |

> 数字说明：增量构建只会编译改动过的文件，警告计数会偏低；上表 13 条取自**全新目录的完整构建**。

### 12.3 M1 剩余（下一轮）

- **13 条 `QFile::open` 未检查返回值**：资源打开失败时样式静默失效（约 13 处 `scrollbar.qss` 加载点）；
- 约 70 处控件槽仍同步等待 adb（有界但最长 20s），根治仍需异步 `AdbClient`；
- 21 个 `adbProcess` 实例尚未收敛为单一客户端；
- 命令仍靠 184 处字符串拼装，`[-s SERIAL -s SERIAL …]` 重复前缀与 `adb -d` 混用待命令层解决；
- `sp_permissions.cpp` 每次点击仍泄漏 QDialog + 6 个控件、顶层窗口；`sp_scales`/`sp_vibrate` 每次刷新仍创建 3/10 个线程对象（其内部进程已在 §12.1 修掉泄漏，但线程对象本身仍未回收）。

---

## 十三、M1 第四阶段：样式加载去重与资源生命周期（第六轮）

### 13.1 修复清单

| 位置 | 问题 → 修复 |
|---|---|
| **新增** `qssloader.h / qssloader.cpp`（已登记进 CMakeLists.txt 与 QtAdb.pro） | 13 处重复的「`QFile` + `open` + `readAll` + `setStyleSheet`」→ 收敛为一行 `applyScrollBarStyle(<area>)`；`open()` 失败时统一给出警告并跳过（原实现不检查返回值） |
| `sonPages/apps/sp_permissions.cpp` | "选项"对话框每次点击都 `new QDialog` 并覆盖成员指针，旧对话框及其子控件永不释放 → **每点一次泄漏一个对话框** → 先 `delete` 上一个，并挂到 `this` 之下随页面销毁 |
| `sonPages/apps/sp_permissions.h` | `settings` / `group` / 7 个控件指针**未初始化** → 全部 `= nullptr`（否则新增的 `delete settings` 会释放野指针） |
| `sonPages/advanced/sp_scales.cpp`、`sp_vibrate.cpp` | 每次刷新新建 3 / 10 个 `adbThread`，一直累积到页面销毁 → 补 `finished → deleteLater`，线程结束即回收（共 13 处） |

### 13.2 两处澄清（避免夸大结论）

1. **"水平滚动条样式被清空"经核对无可见影响**：`links.cpp` 与 `sp_permissions.cpp` 各有连续两行 `readAll()`（第二次必然是空串），但 `qss/scrollbar.qss` **只包含 `:vertical` 规则，没有任何 `:horizontal` 规则**——所以那只属代码异味，不是可见缺陷。审计阶段把它列为缺陷，此处更正。
2. **"向只读资源写入配置"是死代码**：`comboboxListWidget::addName()` 中 `QFile file(":/cfgs/cfg/name.cfg")` 那一段**整体位于注释块内**，不存在活 bug，未做改动。

### 13.3 验证

| 项 | 结果 |
|---|---|
| **全新目录完整构建** | **0 错误、0 警告**（M0 阶段为 37 条警告），50 个编译单元，产物 13.4 MiB |
| ASan/UBSan + 选中设备 + 逐页点击按钮 | 退出码 0，**0 条 sanitizer 报告**，**0 条 QThread 告警**，监视数据 CPU = 100% / RAM = 50% |
| Release + 正常打桩 | 退出码 0，22s，监视数据正确 |
| Release + 永久挂起 adb | 退出码 0，61s **有界返回**，3 次超时兜底，遍历完成 |
| 样式加载失败告警 | 0 条（资源读取正常） |

### 13.4 陷阱记录

验证驱动工程的 CMake 用 `file(GLOB)` 收集源文件，**新增源文件后必须重新运行 cmake 配置**，
否则要到链接期才暴露 `undefined reference`。本轮已实际踩到并记录。

### 13.5 M1 剩余（下一轮起）

- **约 70 处控件槽仍同步等待 adb**（有界但最长 20s）→ 根治需异步 `AdbClient`（`finished` 信号 + 取消 + 进度）；
- 21 个 `adbProcess` 实例尚未收敛为单客户端；
- 命令层：184 处字符串字面量、实测的 `[-s SERIAL -s SERIAL …]` 重复前缀、`adb -d` 与自动 `-s` 混用；
- `pagemaker.cpp`（6 处）与 `tp_packageprofile.cpp`（2 处）在创建页面时也新建 `adbThread` 且不回收——它们的生命周期绑定在页面结构体上，需要先把所有权设计清楚（M2）；
- 页面级的零散 `new`（`sp_permissions` 的"查看"窗口等）已修，但同类写法在少数页面仍有残留。

---

## 十四、M1 第五阶段：异步 AdbClient 落地（第七轮）

考虑到"渐进替换 vs 一次性重写"尚未拍板，本轮采取**低风险渐进**路线：
新增异步客户端，但**只迁移一个消费者**（启动时的设备列表刷新）作为样板，其余约 70 处调用点暂不动 ——
这样每一步都可编译、可验证、可回退，也不排除后续一次性重写的可能。

### 14.1 新增 `adbclient.h / adbclient.cpp`

| 特性 | 说明 |
|---|---|
| 单实例 | `AdbClient::instance()`；单 `QProcess`，请求入队后**串行**执行（同一条 adb 通道不适合并发抢占） |
| 异步 | `run(args, timeoutMs)` 立即返回请求号，完成后发出 `finished(requestId, exitCode, stdout, stderr)` |
| 独立超时 | 每请求一个超时；超时即 `kill()`，仍照常发 `finished`（`exitCode = -1`，原因写在 stderr 参数里） |
| 取消 | `cancel(requestId)` / `cancelAll()`（含在途请求） |
| 启动失败 | `adb` 不存在时 `QProcess` 不会发 `finished`，单独收尾并给出"[无法启动 adb] …"；用排队调用避免 `startNext → start → errorOccurred → handleProcessFinished → startNext` 的递归 |
| 路径统一 | `AdbClient::adbProgramPath()` 成为唯一实现，`adbprocess.cpp` 改为复用它（删掉重复的一份） |

### 14.2 迁移的消费者：设备列表刷新

`MainWindow::refreshDevList()` 拆成两半：

- `refreshDevList()` —— 只发起请求（并用 `m_deviceListRequest` 去重，避免重复排队）；
- `onAdbRequestFinished(...)` —— 过滤出设备列表请求，失败时给出警告，然后调用
- `applyDeviceList(output)` —— 原函数正文（解析、差异比较、下拉框与界面更新）原样保留。

顺带修正：构造函数里的 `process->run("adb server")`（**不是合法命令**，只会打印用法）改为异步 `start-server`。

### 14.3 关键测量：挂起 adb 时窗口多久出现

| | 改造前（同步） | 改造后（异步） |
|---|---|---|
| 窗口 `show()` 时刻 | **≈ 20 000 ms**（被 `adb devices -l` 的同步等待阻塞） | **40 ms** |
| 设备列表失败提示 | 无（只是永远为空） | `获取设备列表失败：[adb 命令超时 20000 ms，已终止该进程] devices` |
| 整体退出 | 有界 | 有界（总耗时 23s，期间界面可用） |

**时间到窗口的改善约 500 倍**，且失败原因对用户可见。

### 14.4 验证

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，51 个编译单元，产物 13.4 MiB |
| 正常 adb + 逐页点击 | 窗口 40ms 出现；设备列表 0ms 等待即到齐；7 页点击完成 |
| **ASan/UBSan + 逐页点击** | 退出码 0，**0 条 sanitizer 报告**，0 条 QThread 告警，0 条 QProcess 告警，监视数据 CPU 100% / RAM 50% |
| 永久挂起 adb | 窗口 40ms 出现；后台超时兜底；退出码 0，总耗时 23s |

### 14.5 后续（M1 收尾）

- 把其余约 70 处同步调用点逐个迁移到 `AdbClient`（每个页面一次提交；`sp_*::refresh()` 是最集中的入口）；
- 21 个 `adbProcess` 实例随之收敛，最终删除 `adbProcess`；
- 命令层：184 处字符串字面量 → 具名命令，并解决 `[-s SERIAL -s SERIAL …]` 重复前缀与 `adb -d` 混用；
- 迁移完成后可把 `AdbClient` 的队列改为允许指定设备并发（当前串行是保守选择）。

---

## 十五、M1 第六阶段：逐页迁移到异步（第八轮）

按上一轮声明的默认路径继续：**逐页迁移**，每轮迁移若干页并单独验证。

### 15.1 本轮迁移范围

| 文件 | 迁移量 | 转换方式 |
|---|---|---|
| `sonPages/devControl/btnemulator.cpp` | **29 处** | `process->run("adb shell input keyevent N", dev)` → `AdbClient::instance()->runForDevice(dev.addr, {"shell","input","keyevent","N"})` |
| `sonPages/devControl/sp_devcontrol_power.cpp` | **4 处** | 同上（`reboot` / `reboot recovery` / `reboot bootloader` / `shell reboot -p`） |

`AdbClient` 新增 `runForDevice(serial, args)` 便捷入口（等价于 `run({"-s", serial, ...})`），
迁移后**命令顺序仍然保持**——请求是排队串行执行的，只是不再阻塞调用方。

脚本化转换时排除了带字符串拼接、引号或 `%` 的命令，避免误改（例如 `"... " + key + " " + value` 这类必须逐个人工处理，留待后续）。

### 15.2 顺手修掉的真实缺陷

**"刷新之后界面一直锁定"**：`applyDeviceList()` 会先 `clear()` 下拉框再重填，此刻 `currentIndex` 变为 -1；
而 `slot_taiChi()` 只在 `currentIndex != -1` 时才 `unlock()` —— 于是刷新后页面区域保持禁用，
必须手动再点一次下拉框才能恢复。现在检测到设备时自动选中第一个（与原实现 `current_device = 0` 的假设一致）。
该缺陷是驱动探测（每 300ms 打印一次 lock 状态）暴露出来的，不是本轮改动引入。

**退出时的进程泄漏告警**：`AdbClient` 作为静态单例，析构时可能仍有在途请求，Qt 会打印
`QProcess: Destroyed while process ("adb") is still running.` 并留下子进程 → 新增析构函数显式 `kill()` + `waitForFinished(3000)`。

### 15.3 关键测量：单次点击延迟（聚焦测试）

新增独立小工程 `.dsh/emu/`（只编译"按键页 + 其依赖"，避免与主验证工程共用 `.ui` 导致 AUTOUIC 规则冲突），
用**对 `input keyevent` 挂起的 adb 打桩**测量点击延迟：

| | 迁移前 | 迁移后 |
|---|---|---|
| 第 1 次点击 | **20 020 ms** | **0 ms** |
| 第 3 次点击 | **20 020 ms**（3 次共 61s） | **0 ms** |
| 点完 30 个按钮 | 需 10 分钟（未测完） | **0 s** |
| 命令是否真的下发 | — | 打桩记录到 **29 次 `input keyevent`**（第 30 个是返回按钮，不发命令） |

### 15.4 验证

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，51 个编译单元，13.4 MiB |
| 聚焦测试（正常 adb，点 30 个按钮） | 全部 0 ms；29 条命令送达 adb；0 条 QProcess 告警 |
| ASan/UBSan + 逐页点击 | 退出码 0，**0 条 sanitizer 报告**，0 条线程/进程告警，监视数据正确 |
| Release + 永久挂起 adb | 窗口 **39 ms** 出现；后台超时兜底；退出码 0；**0 条 QProcess 告警** |

### 15.5 陷阱记录（第三次）

"陈旧验证二进制"第三次出现：本次是**验证工程构建目录残留了旧 ninja 规则**
（`multiple rules generate QtAdb/QtAdb/ui_about.h`），构建静默沿用旧产物，导致场景结果失真。
处理办法：删掉该构建目录重新配置。已写入流程——**验证结论前先确认二进制确实是刚构建的**。

### 15.6 M1 剩余

已迁移 **37 处**（33 本轮 + 4 上一轮的设备列表相关）；仍待迁移的同步调用点约 **40 处**：

| 文件 | 待迁移 | 说明 |
|---|---|---|
| `sp_captive_portal.cpp` | 9 | 需要"请求→回调"改造（结果要参与界面判断） |
| `pagemaker.cpp` | 9 | 页面创建期的信息获取 |
| `sp_status_bar.cpp` | 6 | 直接下发类，可脚本化 |
| `sp_scales.cpp` / `sp_vibrate.cpp` | 6 / 4 | 含字符串拼接，需人工处理 |
| `tp_packageprofile.cpp` | 4 | 同上 |
| `sp_wmsize.cpp` / `sp_dpichanger.cpp` | 2 / 2 | 结果被使用，需回调 |
| `mainwindow.cpp` | 5 | 无线调试连接、杀 ADB 等 |
| 其它 | 数处 | `sp_users` / `sp_installer` / `sp_activator` 各 1 |

---

## 十六、M1 第七阶段：批量迁移"直接下发"类调用（第九轮）

本轮把**结果不被使用**的同步调用点一次性迁移完，共 **27 处**（累计 62 处已迁移）。

### 16.1 迁移范围

| 文件 | 处数 | 形态 |
|---|---|---|
| `sp_status_bar.cpp` | 6 | 字面量命令 + `icons.join(",")` |
| `sp_captive_portal.cpp` | 6 | `cmd` / `cmd_http` / `cmd_https` 变量 |
| `tp_packageprofile.cpp` | 4 | `cmd` 变量 |
| `sp_scales.cpp` | 3 | 字面量 + 输入框取值 |
| `mainwindow.cpp` | 3 | `kill-server`、`connect <ip>:<port>`、`connect 127.0.0.1:58526` |
| `sp_accounts.cpp` / `sp_users.cpp` | 2 | `pm remove-user <id>` |
| `sp_activator.cpp` | 1 | 整条 `command` 串 |
| `sp_vibrate.cpp` | 1 | `key` + 输入值 |
| `sp_customize_cmd.cpp` | 1 | 用户输入的命令串 |

脚本化转换的规则：`process->run("adb …", dev)` → `AdbClient::instance()->runForDevice(dev.addr, {…})`；
整条命令串则用新增的 `runCommand` / `runCommandForDevice`（按空白切分、自动去掉开头的 `adb`）。

**复核发现并修正了一处脚本错误**：`sp_vibrate` 的 `key + " " + edit->text()` 被并成了**一个** argv 元素
（会变成 `settings put system "key value"`），已改回两个独立元素。这说明**批量转换后必须逐条复核生成的参数**，
而不是只看"编译通过"。

### 16.2 验证：命令是否真的经异步通道送达

给测试用的 adb 打桩加了调用记录，跑完 ASan 套件后核对：

| 命令 | 次数 | 来源 |
|---|---|---|
| `kill-server` | 7 | **主窗口按钮（本轮迁移）** |
| `connect 127.0.0.1:58526` | 7 | **主窗口按钮（本轮迁移）** |
| `-s ABC123 shell cat proc/stat` / `/proc/meminfo` | 13 / 13 | 后台监视线程 |
| `devices -l` | 8 | 异步设备列表（上一轮迁移） |
| `wm size` / `wm density` / `getprop …` / `pm list …` | 各 1~4 | `pagemaker` 等**尚未迁移**的同步路径 |

即：**已迁移的调用确实经 AdbClient 送达 adb**，未迁移的仍走原路径 —— 这正是渐进迁移应有的中间状态。

### 16.3 验证汇总

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，13.4 MiB |
| ASan/UBSan + 逐页点击 | 退出码 0，**0 条 sanitizer 报告**，0 条线程/进程告警，监视数据正确 |
| 聚焦测试（按键页 30 次点击） | 全部 0 ms，29 条命令送达 |
| 累计 AdbClient 调用点 | **62 处** |

### 16.4 M1 剩余（下一轮）

| 文件 | 处数 | 需要的改造 |
|---|---|---|
| `pagemaker.cpp` | 9 | 设备信息页在**创建期**取数据并直接 setText → 需要"先建页面、数据到达后回填"的回调式改造 |
| `sp_captive_portal.cpp` | 2 | `refresh()` 的返回值参与界面判断 → 回调链 |
| `sp_wmsize.cpp` / `sp_dpichanger.cpp` | 2 / 2 | 读取值显示 + 写入后回显 → 回调链 |
| `mainwindow.cpp:845` | 1 | 需要给 `AdbClient` 增加 **stdin 写入**能力（原 `run(cmd, write_command)`） |
| `adbthread.cpp` | 4 | 其自有进程；待 `adbProcess` 整体下线时一并消失 |

---

## 十七、M1 收官：GUI 线程上不再有同步 ADB 调用（第十轮）

### 17.1 先更正一个统计错误

上一轮我报"剩余 21 处同步调用"——**那是把块注释 `/* … */` 里的旧代码也算进去了**。
写了一个"先剥离注释再统计"的脚本后，真实存活数只有 **15 处**（`pagemaker` 的 9 处里有 6 处是注释掉的历史实现）。
本轮把这 15 处的可用部分全部迁移，**最终只剩 6 处，且全部位于工作线程内部**。

### 17.2 本轮迁移（回调式）

`AdbClient` 新增两项能力，用于"结果要参与界面判断"的调用点：

| 能力 | 说明 |
|---|---|
| `runWithCallback(args, callback, timeout)` | 一次性回调：该请求完成时在 GUI 线程调用，随后自动断开连接（连接对象由 lambda 自行释放，避免"连接→lambda→智能指针→连接"的循环） |
| `run(args, standardInput, timeout)` / `runCommandWithInput(...)` | 支持把内容写入子进程标准输入——在 `started()` 信号后写入再关闭写通道（`start()` 之后立刻 `write()` 会失败；这正是 `adb pair` 需要的交互） |

| 文件 | 处数 | 改造方式 |
|---|---|---|
| `pagemaker.cpp` | 3 | Recovery 页改为"先填占位、异步回填"，用 `QPointer` 防止页面已销毁；CPU 那处在回调里就地构造无状态的 `textExplainer` 解析，避免捕获 `pageMaker` |
| `sp_wmsize.cpp` / `sp_dpichanger.cpp` | 2 / 2 | 读取值异步回填；写入后**在回调里**恢复界面（原实现用 `setDisabled(true/false)` 包住整段同步等待） |
| `sp_captive_portal.cpp` | 2 | 两次读取有依赖（先取版本号再决定是否查询验证服务器）→ **嵌套回调** |
| `mainwindow.cpp` | 2 | `adb pair` 改用 stdin 能力；`adb wait-for-device … magisk` 改用回调 + 10 分钟预算（原来是**无限期**同步阻塞） |

### 17.3 关键结论：GUI 线程已无同步 ADB 调用

用"剥离注释后再匹配所有对 `adbProcess::run()` 的调用"的方式核实，剩余的 6 处全部在**工作线程**内：

| 位置 | 处数 | 说明 |
|---|---|---|
| `threads/adbThread.cpp` `run()` | 4 | 本身就跑在 `QThread` 里 |
| `threads/threadMonitor.cpp` `run()` | 2 | 第四轮已改成真正的后台线程 |

即 **M1 的核心目标（界面不再被 adb 调用阻塞）已经达成**。`adbProcess` 目前只被这两个工作线程类使用
（20 个实例化点），把它们也搬到 `AdbClient` 之后即可整体删除 `adbProcess`——那是收尾的简化工作，不再是正确性问题。

### 17.4 顺手修掉：退出阶段的告警

挂起场景下退出时会出现两条 Qt 告警：
`QObject::startTimer: current thread's event dispatcher has already been destroyed` 与
`QProcess: Destroyed while process ("adb") is still running.`

原因有二：① `AdbClient` 析构时仍有排队请求，会在事件分发器销毁后再启动定时器/进程；
② `adbThread` 析构时其工作线程可能仍在跑 adb 命令，`delete process` 于是销毁了仍在运行的 `QProcess`。
修法：`AdbClient` 增加关闭闸门（析构先置位 + 清空队列 + 停表 + kill/wait）；`~adbThread` 先 `wait(30000)` 再释放。
两处修完，挂起场景的告警从 2 条降为 **0 条**。

### 17.5 验证

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，13.4 MiB |
| ASan/UBSan + 逐页点击 | 退出码 0，**0 条 sanitizer 报告**，0 条线程/进程告警，监视数据正确 |
| **回调式迁移的命令是否送达** | 调用记录中确有 `getprop ro.product.model` / `wm size` / `wm density` / `getprop ro.build.version.release` 各 1 次 —— 证明异步回调链路在真实应用里跑通 |
| Release + 永久挂起 adb | 窗口 **39 ms** 出现；失败原因可见；**0 条告警** |
| 聚焦测试（按键页 30 次点击） | 全部 0 ms；29 条命令送达 |

累计：**AdbClient 调用点 73 处**，6 个文件使用回调式迁移。

### 17.6 下一步

1. 把 `adbThread` / `threadMonitor` 的内部实现迁到 `AdbClient`，然后**删除 `adbProcess`**（20 个实例化点、21 个头文件包含随之消失）；
2. 命令层：184 处字符串字面量 → 具名命令；解决实测的 `[-s SERIAL -s SERIAL …]` 重复前缀与 `adb -d` 混用；
3. `AdbClient` 队列当前是全局串行（保守选择），可按设备并发以提升多设备场景的响应。

---

## 十八、M1 收尾：删除 adbProcess，全工程不再有 QThread 子类（第十一轮）

### 18.1 `adbThread`：从 QThread 改为异步请求器

`initThread` / `start` / `signal_output` 这套调用方式被 31 处使用，因此**保留 API、只换实现**：
内部改为 `AdbClient::runWithCallback`，不再创建线程，也不再持有 `QProcess`。命令参数构造与迁移前的
`adbProcess::run(command, dev)` 严格等价（前置 `-s <serial>`、弹出命令串里的 `adb`、
`#INSTALL#`/`#SIDELOAD#` 用 `~` 作为含空格路径的分隔符），输出处理分支也逐条对应。
原 `QThread::finished` 由同名信号替代（`deleteLater` 的用法不变）。

### 18.2 `threadMonitor`：定时器 + 异步请求，不再需要线程

第四轮把它改成了真正的后台线程；现在 `AdbClient` 本身是异步的，线程也不再需要：
改成 `QTimer`（2 秒）发起异步请求，两条命令用嵌套回调串联。**GUI 线程全程不阻塞，也不占用额外线程。**

### 18.3 删除 `adbProcess`（连带 20 处死成员）

`adbProcess` 原本被 21 个头文件包含，21 处 `new`。迁移完所有调用点后，那些页面的
`adbProcess *process` 成员**早已无人使用**（只剩构造与注释掉的 `//delete process`）。
本轮：删除 `adbprocess.h/cpp`、移除两套构建里的条目、清掉 20 处成员与构造、
`standardOutputPage` 去掉已无用的 `adbProcess *cess` 参数。

> 顺带修好一处**由删除暴露的功能缺口**：`sp_activator` 的"命令行输出"窗口原先连接
> `adbProcess::outputGet`，类被删除后该连接失效。已改为在 `AdbClient` 回调里把输出转发给窗口
> （新增 `runCommandWithDeviceCallback` 一族接口），功能保持不变。

另外 42 个文件原先靠 `adbprocess.h` **传递包含** `textexplainer.h`（`device` / `textExplainer` / `QMessageBox`），
现改为各自显式包含——头文件依赖变清晰了。

### 18.4 删除死代码 `thread_createPage`

全工程零引用、没有 `run()` 实现、成员指针还从未初始化（一旦使用必崩）。删除后
**全工程不再有任何 `QThread` 子类**（`grep 'public QThread'` = 0）。

### 18.5 验证暴露出的两个真实 bug

**① `AdbClient` 的重入 bug（链式回调会永久卡死）**
`handleProcessFinished()` 里先把 `m_busy = false`，随后 `emit finished` 的回调会立刻发起下一个请求
（把 `m_busy` 置回 true）；但 emit 返回后外层又无条件执行 `startNext()`，它看到队列为空便把
`m_busy` 重新置为 false —— **刚启动的请求被"遗忘"，其结果永不投递**。
表现是"先取 proc/stat 再取 meminfo"这类链式回调只完成第一环。
修法：emit 之后只在 `!m_busy` 时才 `startNext()`。这个 bug 由监视器"只采样一次就不再更新"暴露出来。

**② 刷新会让 CPU/内存显示空窗约 4 秒**
采集占用率需要**两次采样求差**，而每次点"刷新"都会重建监视器、丢掉基准。
修法：`resetMonitorThread()` 在**设备未变化时保持现有监视器继续采样**；
刷新过程中下拉框短暂清空（`currentIndex() == -1`）不再被误判为"设备消失"；
只有设备列表真的为空时才停止。

### 18.6 验证

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，49 个编译单元，13.4 MiB |
| ASan/UBSan + 逐页点击 | 退出码 0，**0 条 sanitizer 报告**，0 条线程/进程告警，**监视数据 CPU 100% / RAM 50%（与打桩一致）** |
| 聚焦测试（按键页 30 次点击） | 全部 **0 ms**，29 条命令送达 |
| Release + 永久挂起 adb | 窗口 **40 ms** 出现；失败原因可见；**0 条告警** |

### 18.7 现状与后续

- `adbProcess` 已删除；**GUI 线程无同步 ADB 调用**；**全工程零 `QThread` 子类**；
  ADB 访问统一经 `AdbClient`（单一队列 + 独立超时 + 取消 + 回调）。
- 源文件数 87（删除 4 个、新增 4 个），C++ 行数 9266 → 9637（净增主要来自解释性注释）。
- 后续（M2 起）：`DeviceModel` + `DeviceState` 枚举替换字符串状态机；跨平台热插拔（Linux udev / `adb track-devices`）；
  命令层具名化（184 处字面量、`[-s SERIAL -s SERIAL …]` 重复前缀、`adb -d` 混用）；国际化。

---

## 十九、M2 起步：设备状态由字符串改为枚举（第十二轮）

### 19.1 问题

原实现用**中文字符串**表示设备状态并在逻辑里比较：

```cpp
if(dev.state == "[未响应]" || dev.state == "[REC]" || dev.state == "[侧载]" || dev.state == "[未知]")
```

- 编译器无法检查拼写；6 种标签散落在 20 处；
- 上面那串"四字符串并联判断"在 `pagemaker.cpp` 里**重复了 4 遍**；
- `comboboxListWidget` → `devItem::setStat(QString)` 也在比较 `"[未授权]"` 来决定配色。

### 19.2 改造

```cpp
enum class DeviceState { Online, Offline, Recovery, Sideload, Unauthorized, Unknown };

DeviceState parseDeviceState(const QString &state);   // adb 输出 → 枚举
QString     stateLabel(DeviceState state);            // 枚举 → 界面文本（保持原中文标签）
inline bool isPageAvailable(DeviceState state);       // 页面可用性（原判定语义不变）
```

- `device::state` 类型由 `QString` 改为 `DeviceState`；
- 20 处比较全部改为枚举比较；4 处重复的并联判断收敛为 `isPageAvailable(dev.state)`；
- 标签字符串现在**只存在于 `stateLabel()` 一处**（界面文本一字未改）；
- `devItem::setStat()` 改为接收枚举，配色判断也用枚举；
- 删除了 `textExplainer::setState()`（由 `parseDeviceState()` 取代）。

> 关于 `isPageAvailable`：原判定是"未响应 / REC / 侧载 / 未知 → 禁用页面"，即**未授权也算可用**。
> 这个语义看起来可疑，但本轮只做等价改写、不悄悄改变行为，故原样保留并在注释中写明。

### 19.3 验证

**状态映射聚焦测试**（直接编译项目的 `textexplainer.cpp`，测真实实现）：

| adb 状态 | 枚举 | 界面标签 | 页面可用 |
|---|---|---|---|
| `device` | Online | `[开机]` | 是 |
| `offline` | Offline | `[未响应]` | 否 |
| `recovery` | Recovery | `[REC]` | 否 |
| `sideload` | Sideload | `[侧载]` | 否 |
| `unauthorized` | Unauthorized | `[未授权]` | 是 |
| `whatever` / 空 | Unknown | `[未知]` | 否 |

7 个用例全部符合预期（标签与判定语义与改造前一致）。

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，13.4 MiB |
| ASan/UBSan + 逐页点击 | 退出码 0，**0 sanitizer 报告**，0 告警，监视数据正确 |
| 聚焦测试（按键页 30 次点击） | 全部 0 ms，29 条命令送达 |
| 永久挂起 adb | 窗口 37 ms 出现，0 告警 |
| **adb 报告 `unauthorized`** | 应用正确解析（下拉框 1 台设备）并按原语义放行页面，0 报错 |

> 顺带增强：验证工程 `.dsh/emu` 支持 `-DMAIN_FILE=<路径>` 替换入口文件，
> 于是"聚焦探针"可以与页面测试共用同一套源文件与构建配置，避免重复维护。

### 19.4 剩余（M2 后续）

- 把设备列表换成 `QAbstractTableModel`（当前仍是手工拼字符串 + `QListWidgetItem`）；
- `pagemaker.cpp` 的 604 行 `switch` 工厂改为注册表；
- 命令层具名化（184 处字面量；实测的 `[-s SERIAL -s SERIAL …]` 重复前缀与 `adb -d` 混用）；
- 跨平台热插拔（Linux udev / `adb track-devices`）；国际化（0 处 `tr()`）。

> **更正**：本节原先写"`indexListItem listItems[10]` 这个固定 10 台的数组还在"——这句是**错的**：
> `listItems[10]` 是**左栏页面列表**的数组，与设备数量无关，而且它从未被使用过（见 §二十）。

---

## 二十、M2 续：清理固定数组与死代码（第十三轮）

### 20.1 先把"固定上限"这件事查清楚

写脚本扫全工程的固定长度数组，得到 **15 处**，其中真正值得处理的是两类：

| 位置 | 处数 | 性质 |
|---|---|---|
| `int val[20]` + `basePage::enableVal[20]` | 10 | **子页门禁**的取值集合，固定上限 20，接口收裸指针（拿不到长度） |
| `indexListItem listItems[10]` | 1 | **死代码**：每个 MainWindow 都会构造 10 个完整控件树，却从未使用 |
| `QStringList nameList[2]` 等 | 4 | 良性（两条并行列表、普通下标） |

### 20.2 `enableVal`：去掉固定上限与裸指针约定

原接口 `void setEnableValue(int val[20])` 会**无条件拷贝 20 个 int** ——调用方传短数组就越界读；
上限 20 也意味着一个页面最多只能放行 20 个子页。改为：

```cpp
QList<int> enableVal;                                  // 固定上限消失
void setEnableValue(const QList<int> &val);
bool isEnable(int k) const { return enableVal.contains(k + 1); }
```

9 处调用点从"两行数组声明 + 一行调用"变成一行 `X->setEnableValue({2, 3});`，
**取值与原实现逐一对齐**（见下）。

> 我一度以为这套机制是死代码（第一次 grep 时误用了 `grep -v basepage`，
> 漏掉了 `basepage.cpp:124` 的调用）并把它删掉了——**编译器立刻报错拦住**。
> 教训：判断"无人调用"时不要用排除自身的过滤条件。

### 20.3 差分测试：新旧判定是否等价

把两段逻辑（旧 `int[20]` 版与新 `QList` 版）放进同一个程序逐点比对：

- **可达范围（k ≥ 0）234 次判定，差异 0 处** ✓
- 顺带发现旧实现的一个怪癖：**空槽存 0，而 `k+1 == 0` 当 `k == -1`，于是 `isEnable(-1)` 恒为 true**。
  调用点是 `if(isEnable(key) && key >= 0)`，后半个条件把它挡住了，因此**不可达**（探针中已标注）。
- 固定上限确实被去掉：集合放入 25 个元素时，旧实现查不到第 21 个之后，新实现可以。

### 20.4 删除死代码 `indexListItem listItems[10]`

`indexListItem` 的构造函数会 `new Ui::indexListItem` 并 `setupUi()` —— 也就是**每个 MainWindow 都会
凭空构造 10 套完整控件树**，而这份数组全工程只出现在声明处。删除后左栏列表仍由
`addIndexItems()` 里那 7 个 `new indexListItem(...)` 正常构建（它们才有父对象、才会被加入列表）。

### 20.5 验证

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，13.4 MiB |
| 差分测试（`enableVal`） | 可达范围 **0 差异** |
| ASan/UBSan + 逐页点击 | 退出码 0，**0 sanitizer 报告**，0 告警，监视数据正确 |
| 聚焦测试（按键页 30 次点击） | 29 条命令送达，0 ms 响应 |
| 永久挂起 adb | 窗口 38 ms 出现，0 告警 |
| 固定数组复查 | **15 处 → 4 处**（剩余均为良性） |

---

## 二十一、命令层：具名构造命令（第十四轮）

### 21.1 先做实验：`-s` 与 `-d` 同时给出会怎样

前几轮我把"`[-s SERIAL -s SERIAL …]` 重复前缀"和"`adb -d` 与自动 `-s` 混用"列为缺陷。
为了不凭猜测下结论，下载了官方 Linux 版 platform-tools（adb 37.0.1）实测：

| 命令 | 结果 |
|---|---|
| `adb -s FAKESERIAL shell echo hi` | `adb: device 'FAKESERIAL' not found` |
| `adb -s FAKESERIAL -d shell echo hi` | **同上（完全相同）** |
| `adb -d -s FAKESERIAL shell echo hi` | **同上** |
| `adb -s FAKESERIAL -s FAKESERIAL shell echo hi` | **同上** |

结论：**adb 以 `-s` 为准，`-d` 被忽略，重复 `-s` 无害**。所以这两条我此前**高估了**——
它们是冗余，不是缺陷。已在 §21.4 的改造中顺手去掉多余的 `-d`（现在有了参数表，`-d` 自然不会出现）。

### 21.2 `adbcommands.h`：命令的具名构造

原先命令以整条字符串散落在各页面（`"adb shell settings get system vibrate_on"`），执行层再按空格与
`'`/`~` 两个分隔符切分。新增 `namespace AdbCmd`，**直接产出 argv**：

```cpp
AdbCmd::settingsGet(ns, key)          AdbCmd::settingsPut(ns, key, value)
AdbCmd::settingsDelete(ns, key)       AdbCmd::wmSize() / wmSize("1080x1920")
AdbCmd::wmDensity() / wmDensity(dpi)  AdbCmd::getprop(key)
AdbCmd::pmList(what)                  AdbCmd::pmAction(action, packageName)
AdbCmd::dumpsys(what)                 AdbCmd::cat(path) / cpuInfo()
AdbCmd::inputKeyevent(code)           AdbCmd::reboot([mode]) / powerOff()
AdbCmd::shell({...})                  // 任意 shell 命令
```

参数的数量与位置由签名固定，**写错直接编译不过**；含空格的参数也不再需要 `~` 之类的人工约定。
本轮共替换 **112 处**（原先 0 处）。

### 21.3 配套改造

- `adbThread` 新增 **argv 重载** `initThread(const QStringList &, device[, explainKey])`，
  与原有字符串重载并存（`#INSTALL#`/`#SIDELOAD#` 的含空格路径仍走字符串 + `~` 分隔符，行为不变）；
- `sp_activator` 的命令成员由 `QString` 改为 `QStringList`，改用新增的
  `AdbClient::runForDeviceWithCallback()`（顺带去掉那 2 条命令里多余的 `-d`）；
- `sonpagemanager` 18 处、`tp_packageprofile` 6 处、`sp_captive_portal` 4 组命令全部改为具名构造；
- `sp_packages`/`sp_permissions` 的过滤开关（`" -s"`、`"-g"` 等）由"拼字符串再切分"改为
  直接并入参数表（`cmd += arg.split(' ', SkipEmptyParts)`，与原切分结果等价）。

### 21.4 过程中的两处更正

1. **有 7 处不是命令，而是显示文本**：`pageListItem::setText(title, profile)` 只把第二参数写进标签
   （可复制到剪贴板），**从不执行**。我一开始把它们也换成了 `AdbCmd::`，编译器报类型不匹配才发现。
   已还原为原始字面量并加注释说明（UI 文案一字未改）。`pagemaker.cpp` 里另有 29 处同类显示文本，保持原样。
2. 前几轮报告里的"**184 处硬编码命令**"已经过时：早先的迁移把绝大多数调用点变成了 argv 形式，
   本轮统计的"仍以整条命令串传参"只有 25 处（`initThread`）+ 若干拼接点。

### 21.5 验证

| 验证项 | 方法 | 结果 |
|---|---|---|
| **发给 adb 的命令集合是否变化** | 统计打桩记录的调用，与 §12 轮（重构前）的清单逐条对比 | **完全一致**（`devices -l`、`kill-server`、`connect`、`start-server`、`shell wm size/density`、`getprop …`、`settings get secure android_id`、`cat /proc/cpuinfo`、`cat proc/stat`、`cat /proc/meminfo`） |
| **具名构造 vs 旧字符串解析** | 28 例探针：用项目真实的 `tokenizeCommand()` 换算旧形式再逐字比对 | **27 例 argv 完全一致**；1 例（路径含空格）按设计不同——旧形式会把路径切断，参数表形式不会 |
| 按键页命令完整性 | 聚焦测试点击 30 个按钮 | 送达 29 条 `input keyevent`，**29 种不同按键码** |
| ASan/UBSan + 逐页点击 | — | 退出码 0，0 sanitizer 报告，0 告警，监视数据 CPU 100%/RAM 50% |
| 永久挂起 adb | — | 窗口 38 ms 出现，0 告警 |
| 全新目录完整构建 | — | **0 错误、0 警告**，13.4 MiB |

### 21.6 剩余

- `pagemaker.cpp` 的 **35 处 `"adb …"` 是显示文本**（给用户看的命令提示），不属于待迁移的命令；
- 仍以字符串执行的约 10 处：`mainwindow` 的 `kill-server`/`connect`/`pair`/`wait-for-device magisk`、
  `AdbClient` 内部命令、以及 `#INSTALL#`/`#SIDELOAD#` 的 `~` 路径（这批依赖字符串形式，可后续加 `AdbCmd::install(path)` 一并收掉）；
- 下一步建议：设备列表 `QAbstractTableModel` 化、`pagemaker` 604 行工厂注册表化、跨平台热插拔。

---

## 二十二、修正我自己引入的功能回退：输出/错误"解释"（第十五、十六轮）

### 22.1 问题：删掉 `adbProcess` 时丢掉了一个用户可见功能

原 `adbProcess::run()` 每次命令结束都会调用 `explainer->explainOutput(output, thread)`，
并在 `readyReadStandardError` 时调用 `explainer->explainError(err)`。
`textExplainer` 里是**约 20 条针对具体报错的中文提示**，例如：

| 报错特征 | 提示 |
|---|---|
| `No such file or directory`（stdout） | 应用未安装 / 应用安装后未首次运行 |
| `Permission denied` | 请先运行应用 |
| `couldn't remove user id 0` | 无法删除默认用户 |
| `Not allowed to set the device owner because there are already some accounts…` | 请先退出所有账户（含操作路径） |
| `Trying to set the device owner, but device owner is already set.` | 存在冲突的设备管理员应用 |
| `No pairing code provided` | 请输入配对码 |
| `Done` | 成功（adb 把成功消息走 stderr 输出） |

**第十一轮删除 `adbProcess` 后，这些调用没有任何替代——功能静默消失。**
这是我在前几轮引入的回退，本轮补回：

- `AdbClient` 内嵌 `textExplainer`，在 `handleProcessFinished()` 中对输出/错误做解释；
- 新增 `Explain { Yes, No }` 标志并随请求保存，用来还原原实现"后台线程不弹窗"的区别
  （`adbThread` / `threadMonitor` 用 `No`，界面上直接触发的操作用 `Yes`）；
- 只有**确有 stderr 内容**时才调用 `explainError()`——原实现挂在 `readyReadStandardError` 上。

### 22.2 验证时又抓到一个真问题：每次成功命令都会弹"未知错误！"

第一版恢复我是无条件调用 `explainError(errorOutput)` 的。探针立刻显示：
**普通输出、stderr 为空的命令也会弹框**，内容是 `explainError` 末尾的兜底分支
"未知错误！…"；而且因为 `showMsgBox()` 会关闭上一个提示框，
它还会把 `explainOutput()` 给出的**有意义提示顶掉**（`out` 打桩本应显示"应用未安装"，
实际显示的是"未知错误"）。加上"仅在 stderr 非空时解释"后才正确。

顺带修掉一个泄漏：`showMsgBox()` 原先每次 `new QMessageBox` 且从不释放（弹一次泄漏一个顶层窗口），
现改为先关闭上一个 + `WA_DeleteOnClose`，成员用 `QPointer` 自动置空。

### 22.3 验证（三种打桩 × 两种模式）

| 打桩 | `Explain::Yes` | `Explain::No` |
|---|---|---|
| stdout 含 `No such file or directory` | **"失败 / 应用未安装 · 应用安装后未首次运行"** ✓ | 0 个 ✓ |
| stderr 含 `Permission denied` | **"失败 / 请先运行应用"** ✓ | 0 个 ✓ |
| 普通输出（stderr 为空） | **0 个**（不再误报） ✓ | 0 个 ✓ |

其余场景：ASan/UBSan + 逐页点击退出码 0、0 sanitizer 报告、0 告警（模态对话框计数由 12 变 13，
说明恢复的提示在真实应用里也生效）；聚焦测试 29 条 keyevent；挂起场景窗口 40 ms、0 告警；
全新构建 **0 错误 0 警告**。

### 22.4 流程教训：陈旧二进制陷阱（本轮连续踩了两次）

- 第一次：探针**编译失败**（我把输出重定向到了 `/dev/null`），跑的是旧程序，于是得出
  "恢复没生效"的错误结论；破绽是我新加的日志一行都没打印。
- 第二次：修正后又漏了 `text()` 的括号，构建仍然失败，又跑了一次旧程序。
- 第三次（更隐蔽）：程序是对的，但我在 **`app.exec()` 返回之后**统计顶层窗口——
  此时 Qt 已清理窗口，恒为 0；改成在事件循环内统计才看到"提示框持续存在"。

已固化的做法：
1. **构建结果作为硬门槛**——构建失败就直接停，绝不继续跑（`if cmake --build …; then … else 停止`）；
2. 探针里加入"新日志应出现"这类**自证性输出**，用于识别陈旧二进制；
3. GUI 相关断言放在**事件循环内部**（定时器回调里）而不是循环之后。

---

## 二十三、删除项副作用复查 + 裸指针成员与定时器生命周期（第十七轮）

上一轮的回退（删 `adbProcess` 时丢了输出解释）说明：**删除被复用的组件前必须查它的内部副作用**。
本轮把此前所有删除项逐一体检。

### 23.1 删除项体检结果

| 删除内容 | 检查方式 | 结论 |
|---|---|---|
| `LXGWWenKaiLite-Bold/Light.ttf`（21.2 MiB） | 查 `main.cpp` 的字体注册与全部 `.ui` 的字体引用 | **安全**：代码只注册 `LXGWWenKaiLite-Regular.ttf`，并以族名 "LXGW WenKai Lite" 作为应用字体；Bold/Light **从未被任何代码注册**，`.ui` 里引用的也是族名（Regular 即可满足） |
| `adbProcess::outputGet` 信号 | 在 git HEAD 中列出**全部**订阅者 | 3 个：`sp_activator`（已用回调恢复 ✓）、`sp_recovery`（现接 `thread->signal_output`，内容等价 ✓）、`adbThread` 内部（已由 AdbClient 完成信号替代 ✓） |
| `standardOutputPage(QWidget*, adbProcess*)` 第二参数 | 列出全部构造调用 | 2 处（`sp_activator` / `sp_recovery`），均已更新为单参数 ✓ |
| `adbProcess::standardOutput` / `test` 成员 | 全仓检索 | 从未被读取/使用 ✓ |
| `thread_createPage` | 全仓检索 | 零引用、无 `run()` 实现 ✓ |
| `usb_listener` 的 Win32 分支 | 逐行复核（本机无法编译 Windows） | `#ifdef Q_OS_WIN` 包裹后 Windows 路径代码未改；Linux 路径返回 `false` 并留 M3 待办 ✓ |

**结论：只有输出解释那一处是真回退**（已在 §22 修复）；其余删除项都有等价替代或本就无害。

### 23.2 定时器生命周期：`taiChiTimer`

页面切换后靠 `taiChiTimer`（700 ms 单次）触发 `slot_taiChi()` 解锁界面。原实现有两个问题：

```cpp
// initBasePage()：直接覆盖成员
taiChiTimer = new QTimer(this);
// slot_taiChi()：直接 delete
delete taiChiTimer;
```

- 700 ms 内连续切换页面时，先前的定时器被成员覆盖 → **仍然在跑却再没人释放**（泄漏），
  且它触发时 `slot_taiChi()` 删除的是**新的那个**定时器；
- 槽函数正是在该定时器的 `timeout` 信号里被调用，`delete` 等于销毁"正在发信号的对象"。

改为：创建前先 `stop()` + `deleteLater()` 掉旧定时器；槽内用 `deleteLater()` 并置空成员。
`taiChiTimer` 也补上了 `= nullptr` 初始化（否则新增的空值判断会读未初始化指针）。

### 23.3 裸指针成员：103 处补上 `= nullptr`

扫描全部头文件，发现 **105 处裸指针成员没有初始化**（`Ui::X *ui;` 这类虽在构造函数里赋值，
但 `if(p != nullptr)` / `delete p` 形式的用法一旦提前读到就是未定义行为）。
除 2 处已有初始值外，其余 **103 处**统一补 `= nullptr`，复查后剩余 0 处。

顺带删除死成员 `baguaTimer`（声明后全工程零使用），并把 `pagemaker.h` 里的
`typedef struct {...} pageListItemStruct;` 改为具名 `struct pageListItemStruct {...};`
—— 匿名 struct 一旦带成员初始化器就不再是 C 兼容类型，GCC 会报
`-Wnon-c-typedef-for-linkage`（补初始化时确实报了 4 条，改具名后归零）。

### 23.4 验证

| 场景 | 结果 |
|---|---|
| 全新目录完整构建 | **0 错误、0 警告**，13.4 MiB |
| 裸指针成员复查 | 未初始化 **105 → 0** 处 |
| ASan/UBSan + 逐页点击（含快速切页，会叠加多个 `taiChiTimer`） | 退出码 0，**0 sanitizer 报告**，0 告警；`indexList.enabled=1`（解锁正常） |
| 聚焦测试（按键页 30 次点击） | 全部 0 ms，29 条命令送达 |
| 永久挂起 adb | 窗口 38 ms 出现，0 告警 |

> 测量本身的又一次教训：验证脚本里 `bash -c "... 命令=$(grep -c …)"` 用了双引号，
> `$(...)` 被**外层 shell 提前展开**，于是统计的是"运行前"的状态，得出"命令 0 条"的假结果。
> 结论：把命令替换放进单引号的内层脚本，或事后再统计。

---

## 二十四、M3：设备热插拔（Linux 可用、Windows 更完整）（第十八轮）

### 24.1 问题与选择

原实现只在 Windows 上靠 `usb_listener` 的 `WM_DEVICECHANGE` 收到热插拔通知；
Linux 下 `usb_listener::nativeEventFilter()` 直接返回 `false`（见 §二十三），
插入/拔出设备必须手动点"刷新"。这也不覆盖 **Windows 上的无线调试设备**（USB 事件不会触发）。

实现方式有两种：

| 方案 | 优点 | 代价 |
|---|---|---|
| `adb track-devices` 流式监听 | 实时 | `AdbClient` 目前是"一次请求一个进程"，需要新增长驻进程 + 增量输出能力；而且**本机没有实体设备/模拟器，无法验证它的实际输出分帧格式**，只能靠推测 |
| 定时轮询 `adb devices -l` | 跨平台、可测、复用现有一切 | 最多 2.5 秒延迟 |

本轮选择**轮询**：在无法验证流式协议的情况下，不靠推测下结论；轮询的每条路径都能用打桩验证。
（`adb track-devices` 作为后续增强项记录在 §24.5。）

### 24.2 实现：复用现成的热插拔路径

关键发现：代码里**已经有**一条热插拔专用路径——`slot_refreshDevList()` 会把 `liangYi` 置为 `true`，
而 `applyDeviceList()` 只在 `liangYi == false`（刷新按钮）时才无条件重建下拉框；
热插拔路径（`liangYi == true`）**只在设备列表真的变化时**才重建。

于是轮询只需接到同一个槽：

```cpp
    m_deviceWatcher = new QTimer(this);
    connect(m_deviceWatcher, &QTimer::timeout, this, &MainWindow::slot_refreshDevList);
    m_deviceWatcher->start(2500);
```

这样既不会每 2.5 秒重置用户选中的设备，也不需要改动 `applyDeviceList` 的任何逻辑。
`refreshDevList()` 本身带"已有请求在途则跳过"的保护，轮询不会堆积请求。

顺带修掉热插拔路径里的一个泄漏：`refreshDevListLater()` 每次 `new QTimer()` 却从不释放
（`deleteLater()` 那行被注释掉了），每来一次热插拔事件就泄漏一个定时器。

### 24.3 验证：插入与拔出两个方向

用一个"设备列表会变"的打桩（`devices -l` 的输出由模式文件决定：1 台 / 2 台 / 0 台），
在驱动**不点击任何按钮的安静时段**切换模式：

| 时刻 | 事件 | 界面观察 |
|---|---|---|
| 8.1s | 初始 1 台 | `combo.count=1`，`indexList.enabled=1`（已解锁） |
| 9s | 打桩改为 2 台 | **11.7s 时 `combo.count=2`**（轮询在 10.0/12.5s 各调用一次 `adb devices -l`） |
| 16s | 打桩改为 0 台 | **18.9s 时 `combo.count=0`、`combo.index=-1`、`indexList.enabled=0`**（界面正确回到"请先选择设备"） |

全程没有任何用户操作——热插拔在两个方向上都被自动检出。

### 24.4 流程教训：陈旧二进制第 5 次，以及一个误导性的记录

1. 第一次测试显示"轮询完全没生效"。实际原因：我改了 `build-cmake`（主程序）却**没有重建 Release 驱动**，
   跑的是没有轮询定时器的旧程序。加诊断后立刻分明——日志里 `[watch] 轮询触发` 一直在打印，
   而打桩侧没有任何 `devices -l` 调用。
2. 更早一次"`devices -l` 调用数为 0"也是假象：我的热插拔打桩**自行处理了 `devices -l` 分支**，
   没有转发到带调用记录的那个打桩，于是记录里看不到它。给新打桩补上记录后数据才对得上。
3. 另外一次失败源于命令替换：验证脚本里 `$(...)` 写在双引号内被外层提前执行，**同步跑了一次完整驱动（25 秒）**，
   等真正的运行启动时测试用的模式文件已被改掉。

**结论**：这三条都指向同一个纪律——**先确认"跑的是刚构建的产物"和"记录的是这次运行"**，再解读结果。

### 24.5 后续

- 若要更低延迟，可给 `AdbClient` 增加"长驻进程 + 增量输出"能力，改用 `adb track-devices`；
  届时应先用真实设备确认其输出分帧格式（本机无设备，故本轮未采用）。
- 轮询间隔目前固定 2.5 秒；设备较多或 adb 较慢时可考虑自适应（例如连续无变化时退避到 5–10 秒）。

---

## 二十五、修复"设备别名"（一个三处断裂、实际不可用的功能）（第十九轮）

### 25.1 发现：功能有入口、却完全不起作用

排查设备列表显示逻辑时发现，代码里有一套"给设备起名字"的设计，但**在三个层面同时断掉**：

| 断点 | 证据 |
|---|---|
| 用户看不到入口 | `devItem` 构造函数里 `ui->lineEdit->hide();` —— 输入框被隐藏（而 `.ui` 里它是设备卡片布局中的正常一行） |
| 收到即丢弃 | `comboboxListWidget::addName(addr, name)` 的**整个函数体被 `/* */` 注释掉** |
| 名字从未被应用 | `setUpList()` 里读取 `nameList` 的那段同样被注释 |
| 目标根本写不进去 | 被注释的实现想写 `QFile(":/cfgs/cfg/name.cfg")`——那是编译进二进制的 **Qt 资源，运行时只读**，这套设计从来不可能成功（多半因此被弃用） |

### 25.2 修复

新增 `devicenames.h/.cpp`，把别名持久化到**可写**位置：

```cpp
namespace DeviceNames {
    QString nameFor(const QString &addr);                    // 未命名返回空串
    void    setName(const QString &addr, const QString &name); // 传空串 = 删除
    QHash<QString,QString> allNames();
    QString storagePath();
}
```

- 存储：`QStandardPaths::AppConfigLocation + "/device-names.ini"`（本机即 `~/.config/QtAdb/device-names.ini`）；
- 设备地址（可能形如 `192.168.1.5:5555`）先百分号编码再作键，避免与 INI 的层级分隔符冲突；
  读取时**同时接受未编码的键**，便于手工编辑配置文件；
- 接通三处：`devItem` 显示输入框并把**真实序列号**（原来发的是标签文本 `"序列号\n<addr>"`，键里混进了标签文字）交给 `addName`；`addName` 真正保存；`setUpList` 读取并回填；
- 别名的**可见位置**：顶部"当前设备"标签（`dynamic_island`，始终可见）优先显示别名，其次序列号/型号名；下拉列表里每个设备卡的输入框也回填别名，并以型号名作为占位提示。

### 25.3 顺带清掉的死代码

`applyDeviceList()` 里拼了一串 `"[状态] 型号 序列号"` 存进 `QStringList l`，但唯一消费它的
`ui->comboBox->addItems(l)` **早已被注释**；下拉框实际用的是自定义视图（`comboboxListWidget`）的模型
——所以 `combo->itemText(0)` 为空字符串（条目没有文本，信息全在 `devItem` 控件里）。
这属于"每次刷新都白算一遍"的死代码，已删除。

### 25.4 验证

**持久化聚焦测试**（`devicenames_probe.cpp`，两个进程分阶段跑，共 7 项断言）：

| 检查 | 结果 |
|---|---|
| 写入后立即读取 | ✓ |
| 新进程读回（跨进程持久化） | ✓ |
| 含冒号的地址（`192.168.1.5:5555`）往返 | ✓ |
| 传空串即删除 | ✓ |
| 首尾空白被去掉 | ✓ |
| 已删除的记录保持为空 / 未命名设备为空 | ✓ |

**应用级验证**：驱动（已把 `applicationName` 对齐为 `QtAdb`，从而读同一份配置）在真实运行中打印出

```
[  1578 ms] 当前设备标签 = "我的测试机"
```

即配置文件里的别名确实被应用到了界面。

其余场景：ASan/UBSan + 逐页点击退出码 0、0 sanitizer 报告、0 告警；聚焦测试通过；
挂起场景窗口 43 ms、0 告警；**热插拔复测仍为 1 → 2 → 0**；全新构建 **0 错误 0 警告**，13.4 MiB。

### 25.5 两条流程记录

1. **GLOB 收集源文件的验证工程，在增删源文件后必须重新配置 cmake**——本次新增 `devicenames.cpp`
   后 ASan 驱动链接失败（`undefined reference to DeviceNames::...`），重新 configure 即恢复。
   这条在第 3 轮已记录过，本次复现，说明它值得写进纪律清单而不是靠记性。
2. 测试程序要与真实程序读同一份配置，**必须显式设置相同的 `applicationName`**
   （`QStandardPaths::AppConfigLocation` 默认取可执行文件名，`QtAdbAsan`/`QtAdbEmuTest` 会各自指向不同目录）。

---

## 二十六、解析器压力测试：修掉一个会 abort 的设备列表解析缺陷（第二十轮）

### 26.1 方法：把畸形输入喂给解析器，并在 ASan/UBSan 下跑

`textExplainer` 的解析函数直接处理**外部进程的输出**——最容易在异常输入上出事，而正常路径永远测不到。
本轮给验证驱动加了 `--parsers` 模式（`parser_probe.cpp`），把 **42 组畸形/退化输入**（空串、只有换行、
单字符、超长行、CRLF、tab、多字节边界、以及真实但退化的 adb 输出）逐一喂给全部解析函数：

```
QtAdbAsan --parsers      # ASan/UBSan 插桩下运行，越界与 Qt 断言会直接暴露
```

### 26.2 找到的缺陷一：设备列表解析会越界 abort

第一次运行即崩溃：

```
ASSERT failure in QList::operator[]: "index out of range"
  #8 textExplainer::getDevList(QString)  textexplainer.cpp:31
```

**`getDevList()`（旧解析器）**：`tmp[1]`、`finalInfo[1..3]`、`infoSimplier[1]` 全无长度检查，
任何不含 `device product:` 的行都会越界。而且它**没有任何调用者**（应用用的是 `getDevList_windows`）
—— 属于"随时会炸的死代码"，本轮直接删除。

更要紧的是它的**在用版本**存在同类问题。对照修复前后：

```cpp
// 修复前（git HEAD）
QStringList tmp = tmpList[i].split(" ");
dev.addr  = tmp[0];
dev.state = setState(tmp[1]);        // 单字段行 → 越界
if(dev.state == "[未授权]")
    dev.transport_id = tmp[2];        // 只有"序列号 状态"两字段时 → 越界 → 断言/abort

// 修复后
if(tmp.size() < 2 || tmp[0].isEmpty()) { continue; }              // 半行直接跳过
dev.transport_id   = tmp.size() > 2 ? tmp[2] : unknown;           // 缺字段补 "unknown"
dev.device_product = tmp.size() > 2 ? tmp[2] : unknown;           // （离线/未授权分支语义不变）
dev.model          = tmp.size() > 3 ? tmp[3] : unknown;
dev.device_debug   = tmp.size() > 4 ? tmp[4] : unknown;
dev.transport_id   = tmp.size() > 5 ? tmp[5] : unknown;
```

**为什么会真的踩到**：`adb devices -l` 的 `product:`/`model:`/`device:` 附加字段并非总有——
设备处于 offline / unauthorized、或状态切换过程中，真实 adb 输出的就是 `序列号<TAB>状态` 两段。
此时 Debug 构建触发 `QList::operator[]` 断言并 **abort**，Release 构建则是**越界读**。
修复后语义不变（离线/未授权的 product/model/debug 仍为 "unknown"），但不再崩溃。

### 26.3 验证

| 项目 | 结果 |
|---|---|
| 42 组畸形输入（ASan/UBSan） | **全部未崩溃**，0 sanitizer 报告、0 断言 |
| 边界语义抽查（9 项） | 全部符合预期（见下） |
| 正常设备行解析 | `ABC123` / `[开机]` / 型号 `TestModel` —— 与修复前一致 |
| **未授权且仅两字段的设备行** | 应用正常解析出 1 台设备并完成逐页遍历，**0 断言、0 sanitizer**（修复前此输入会 abort） |
| ASan 生命周期场景 | 退出码 0、0 sanitizer 报告、0 告警、监视数据正确 |
| 挂起场景 / 聚焦测试 / 热插拔 | 窗口 39 ms、0 告警；聚焦测试退出码 0；热插拔仍 1 → 2 → 0 |
| 全新构建 | **0 错误 0 警告**，13.4 MiB |

### 26.4 记录一个"看起来像 bug、实际是既有行为"的地方

`get_words_after(str, key)` 在 key 不存在时**返回整个输入**而不是空串：因为
`index == -1` 时长度算成 `size + 1 - key.size()`，最坏情况退化为"取全部"。
探针起初把预期写成空串，实测为原字符串——**是预期写错了，不是代码错了**。
本轮按实际行为把这条固定进探针并加注释，**不做静默改变**（改成空串会影响多处显示文案，
需要逐个复核调用点，属于独立的一轮工作）。

### 26.5 可复用的产出

`--parsers` 模式是一个便宜且可扩展的入口：以后新增/修改任何解析函数，只要往用例表里加几行，
就能在 ASan 下跑一遍边界输入。这比"等用户拿一台状态奇怪的设备来踩"划算得多。

---

## 二十七、验证覆盖面的一次大修正 + 刷新不再劫持导航（第二十一轮）

### 27.1 发现：之前的 ASan 运行**从未真正创建过功能页**

排查"为什么驱动总是报告子页 0 个"时，加诊断后发现：`indexList` 的行号确实在 0→6 变化，
但**页面对象一个都没有留下**。继续加日志后真相是：

```
[page] initBasePage key= 0 … 已创建 basePage name="devInfo"     ← 正确页面刚建好
[page] initBasePage key= 6 … 已创建 about     name=""           ← 立刻被"关于"页覆盖
```

`on_refreshButton_clicked()` 里有一句 `initBasePage(6)`，而 `key 6` 就是**"关于"页**。
驱动的遍历过程中会点击界面上的按钮（含"刷新"），于是**每建好一个页面就被换成"关于"页**。
后果有两个：

1. **产品层面**：用户点"刷新"会被从正在看的页面（例如"设备控制"）**强行带到"关于"页**；
2. **验证层面**：过去所有 ASan 运行其实**只覆盖了主窗口**，页面级代码（30 个子页、
   全部页面解析器）从未被执行过——我以前"逐页点击/遍历"的说法**高估了覆盖面**，这里更正。

### 27.2 修复：刷新只刷新，不重建页面

`setCurrentDevice()` 本来就会在**设备变化时**删除并按新设备重建页面，因此刷新无需重建：

```cpp
void MainWindow::on_refreshButton_clicked()
{
    refreshDevList();                                     // 重新拉取设备列表
    reset_monitorBars();                                  // 重置占用率显示
    resetMonitorThread(ui->comboBox->currentIndex());     // 设备没变则继续采样
}
```

同时去掉了原来的 `lock()`：它与页面重建是配套的，去掉重建后若仍 `lock()`，
一旦 `slot_taiChi()` 的解锁条件不满足就会**永久锁死界面**（§12 记录过的那个缺陷的成因）。

### 27.3 顺带修掉驱动自身的 use-after-free

页面开始真正创建后，ASan 立刻在**驱动侧**抓到 heap-use-after-free：

```
#3 animationWidget::~animationWidget()   #4 basePage::~basePage()
#6 MainWindow::initBasePage(int)  mainwindow.cpp:532
#7 MainWindow::on_refreshButton_clicked()  mainwindow.cpp:412
SUMMARY: heap-use-after-free … main_test.cpp:169 in operator()
```

驱动把 `findChildren<QListWidget*>("listWidget")` 的结果存成裸指针列表，而点击过程中应用会销毁并重建页面
——下一轮循环就踩到悬垂指针。改为 `QPointer` 持有（与 §8 在驱动里踩到同类问题时的处理一致）。

### 27.4 覆盖面提升的量化结果

| 指标 | 修复前 | 修复后 |
|---|---|---|
| 遍历到的子页 | **0 个** | **30 个**（第 0 页 6 个、第 1 页 8 个、第 2 页 7 个…） |
| 页面级 adb 命令 | 无 | `pm list users / permissions / permission-groups / features / libraries`、`wm size/density`、`settings get …` 等 |
| 自动关闭的模态对话框 | 12 个 | **72 个** |
| sanitizer 报告 | 0 | **0**（覆盖面大幅提升后仍然干净） |

### 27.5 一次"看起来像 bug、实际不是"的排查（如实记录）

同期用行级扫描找出 6 处可疑下标访问（`split(":")` 之后直接取 `[1]`、空列表取 `[0]`），
担心与 §26 的设备列表缺陷同类。于是新加了一个**畸形输出打桩**（`pm list …` 返回不带冒号的行、
`/proc/*` 返回空内容等），在 Debug+ASan（断言开启）下把 30 个子页跑了一遍：

**结果：0 断言、0 sanitizer 报告。** 逐处复核后确认这些位置**本来就有守卫**：

```cpp
tmpList = permissionList[i].split(":");
if(tmpList.count() >= 2) { model->setItem(i,0,new QStandardItem(tmpList[1])); }   // sp_features / sp_libraries / sp_packages
if(!permissionList.isEmpty()) { model->setHeaderData(0, …, permissionList[0]); }   // sp_permissions
```

即：**这次扫描的怀疑不成立**——只有 §26 的 `getDevList_windows` 是真缺陷（那里确实没有任何守卫）。
这条如实记录，避免把"没找到问题"包装成"修了问题"。

### 27.6 流程记录（第 6 次陈旧二进制 + 1 次测量时序陷阱）

1. 首次跑 ASan 版本时得到"不崩溃"的结论，实际是**没打上修复的旧二进制**（我只重建了 `build-cmake`
   与 `build-release`）。这次靠"日志里没有新加的诊断行"识破——即 §22 固化的自查手段起了作用。
2. 修复后监视数据显示 0%，一度像回归；实际是**采样点落在遍历中途**：驱动仍在点"刷新"，
   而刷新会 `reset_monitorBars()` 清零。把采样点移到遍历结束之后立刻读到 100%/50%。
   → GUI 断言必须放在"被测交互停止之后"，这条与 §22 的第 3 条同源，这里再次踩到。

### 27.7 验证

| 场景 | 结果 |
|---|---|
| 全新构建 | **0 错误 0 警告**，13.4 MiB |
| Debug + ASan（正常打桩） | 退出码 0，**30 个子页**、0 sanitizer、0 告警、监视数据 CPU 100% / RAM 50% |
| Debug + ASan（畸形输出打桩） | 退出码 0，0 sanitizer、0 断言 |
| Release + ASan | 退出码 0（此前在该组合下抓到驱动的 use-after-free） |
| 挂起场景 / 聚焦测试 / 热插拔 | 窗口 41 ms、0 告警；聚焦测试通过；热插拔 1 → 2 → 0 |

---

## 二十八、压力场景全绿 + 修掉按页面创建累积的泄漏（第二十二轮）

### 28.1 压力场景矩阵：崩溃类问题为零（阴性结果，如实记录）

利用 §27 才真正打通的页面覆盖能力，把 30 个子页放进 5 种异常环境下跑（Debug + ASan，断言开启）：

| 场景 | 打桩行为 | 结果 |
|---|---|---|
| `bin`（正常） | 常规输出 | 退出码 0，**30 个子页**，0 sanitizer/断言 |
| `bin-bad`（畸形输出） | `pm list …` 返回不带冒号的行、`/proc/*` 返回空/非数字/溢出值等，**逐次轮换** | 退出码 0，30 个子页，0 sanitizer/断言 |
| `bin-two`（双设备） | 两台设备，驱动反复切换（2 台 × 3 轮） | 退出码 0，30 个子页，0 sanitizer/断言 |
| `bin-vanish`（设备中途消失） | 前 8 秒有设备，之后列表为空 | 退出码 0，0 sanitizer/断言 |
| `bin-hang`（adb 永久挂起） | `devices -l` 挂起 | 窗口 41 ms 出现，0 告警 |

**这轮没有再找到崩溃类缺陷**——即上一轮修掉的解析越界是这一批里唯一的问题。
阴性结果同样记录，避免"每轮都有战果"的叙事失真。

### 28.2 但长跑暴露出另一类问题：按页面创建累积的泄漏

新增 `--soak` 模式（反复导航并采样 RSS）后发现，反复进出页面会持续增长。
`RSS` 作为指标并不可靠（ASan 分配器不归还内存，前后两次测量**非单调**，一度算出"负增长"），
于是改用**直接证据**：打开 LeakSanitizer，让它在退出时给出泄漏栈。

修复前的静态事实（全部可核对）：

| 问题 | 数量 |
|---|---|
| `new textExplainer()` 而只有 1 处 `delete`（另 2 处被注释） | **12 new / 1 delete** |
| `initStruct()` 创建的 `pageListItemStruct`：结构体、`textExplainer`、`adbThread` **都没有宿主，调用方也不释放** | 8 处调用点 |
| `new adbThread()` 未指定宿主（随页面重建累积） | 11 处 |
| `pageListItemStruct::explainer`：**只赋值、从不使用**的死字段 | 1 处 |

关键在于**调用频率**：列表项在**每次页面创建**时重建，而页面在每次导航、每次切换设备时重建
——所以这些泄漏是"每导航一次泄漏十几个对象"。

修复：

```cpp
// ① 死字段直接删除；thread 以 item 为宿主（item 随页面销毁）
pageListItemStruct *itemStruct = new pageListItemStruct();
itemStruct->item = new pageListItem();
itemStruct->item->setParent(parent);
itemStruct->thread = new adbThread(itemStruct->item);      // 原为 new adbThread()，无宿主

// ② 6 处调用点在 setup 完成后释放结构体本身（item/thread 已各有宿主）
delete productModel;      // 结构体本身不再需要

// ③ 其余 11 处 new adbThread() 一律以所属对象为宿主
// ④ 8 个类在析构中补上 delete explainer;（并把注释掉的释放真正启用）
```

结果：**`new textExplainer` 11 处 ↔ `delete explainer` 11 处**，**无宿主的 `adbThread` 0 处**。

**验证（直接证据）**：开启 LSan 跑完一遍完整遍历（30 个子页），泄漏栈中
`textExplainer` / `pageListItemStruct` / `adbThread` 的出现次数均为 **0**。

### 28.3 剩余泄漏（如实说明，不在本轮范围内）

LSan 报告里占比最大的一条是 **libfontconfig/Qt 字体引擎内部**的分配（11 KB / 22 个对象），
不属于本项目代码；其余多为"间接泄漏"（挂在某个直接泄漏的根之下）。
本项目自身的直接泄漏在这次运行中约 2.5 KB / 5 个对象。**完整的泄漏审计是独立任务**，
本轮的边界是"修掉我确认过、且按页面创建高频累积的那几类"。

### 28.4 验证

| 场景 | 结果 |
|---|---|
| 全新构建 | **0 错误 0 警告**，13.4 MiB |
| 5 个压力场景（Debug+ASan） | 全部退出码 0、0 sanitizer/断言（见 §28.1 表） |
| 双设备切换（2 台 × 3 轮） | 0 断言；监视数据仍 CPU 100% / RAM 50% |
| 90 秒长跑（周期路径） | 0 告警；监视采样 26 次、设备轮询 58 次，无卡死 |
| LSan | 修复的三类对象在泄漏栈中出现 **0 次** |
| 热插拔回归 | 仍为 1 → 2 → 0 |

### 28.5 流程记录

1. **指标要选对**：先用 RSS 衡量"是否泄漏"，两次测量非单调（甚至算出负增长），
   结论不可用；换成 LSan 的泄漏栈后一分钟就拿到确定答案。**不可靠的指标应及时放弃，而不是硬解释。**
2. **压力打桩要能轮换**：畸形输出打桩按调用序号循环 6 种退化形态，一次运行即可覆盖多个分支；
   固定一种形态容易只测到同一条路径。
3. 双设备切换步骤让"页面反复重建 + 请求在途"成为常规覆盖项——这正是本轮泄漏暴露出来的场景。

---

## 二十九、国际化（M4）：把原作者没接通的那条链路接上（第二十三轮）

### 29.1 现状：意图存在，链路是断的

审计时记的是"全工程 0 处 `tr()`"。但细看 `main.cpp`，原作者其实**已经写了**加载代码：

```cpp
QTranslator translator;
for (const QString &locale : QLocale::system().uiLanguages()) {
    const QString baseName = "QtAdb_" + QLocale(locale).name();
    if (translator.load(":/i18n/" + baseName)) { a.installTranslator(&translator); break; }
}
```

问题在于：`:/i18n/` 下**没有任何 .qm**（原 `.ts` 是空文件、`CONFIG += lrelease` 编译出空目录），
而代码里也没有 `tr()` 可提取。所以本轮不是"新造一套 i18n"，而是**把这条既有链路接通**。

### 29.2 做了什么

| 步骤 | 内容 |
|---|---|
| ① 让文案可提取 | `textExplainer` 不是 QObject，用 `Q_DECLARE_TR_FUNCTIONS(textExplainer)` 获得 `tr()`；把它的 **20 条用户提示**（"应用未安装""请先运行应用""无法删除默认用户""存在冲突的设备管理员应用""请输入配对码"…）全部包进 `tr()` |
| ② 主窗口文案 | 左栏 7 个导航名、顶部"请先选择设备"、无线调试对话框的 10 条标签/步骤文案、两处操作提示，共 20 处包 `tr()` |
| ③ 提取 | `lupdate . -recursive -extensions cpp,h,ui -ts translations/QtAdb_en.ts` → **252 条**（58 条来自代码，其余来自 `.ui`——**UI 里设计的文案本身就可翻译**，不需要手工包 `tr()`） |
| ④ 翻译 | 首批填入 **37 条**英文（导航、配对对话框、提示语）；其余保持未翻译 |
| ⑤ 编译与嵌入 | 构建时调用 `lrelease` 生成 `QtAdb_en.qm`，用 `qt_add_resources` 嵌入为 `:/i18n/QtAdb_en.qm`——**与 `main.cpp` 既有的加载路径完全一致** |
| ⑥ 两套构建同步 | CMake：自动探测 `lrelease`，找不到就跳过并打印说明；qmake：恢复原作者意图的 `CONFIG += lrelease embed_translations` + `TRANSLATIONS` |

**默认行为不变**：源语言是中文（代码与 `.ui` 里的原文就是中文），系统语言没有对应翻译时
`QTranslator` 不安装或查不到条目 → 界面显示原文中文 ✓。未翻译的 215 条同样回退中文，是渐进式而非全有全无。

### 29.3 验证：端到端（不是只看"能编译"）

新增 `--i18n` 探针，**先安装翻译器再构造真实主窗口**，检查界面文案：

```
  .qm 加载: 成功
  未装翻译器时: "设备信息"                  ← 默认仍是中文
  装翻译器后  : "Device info"               ← 代码里的 tr() 生效
  提示文案    : " · Please launch the app first"
  未翻译回退  : "这个字符串故意没有翻译"     ← 未翻译条目优雅回退
  主窗口顶部标签: "Please select a device first"   ← 真实界面文案已英文化
  全部符合预期
```

最后一行是关键：它证明的是 **lrelease → qt_add_resources → rcc → 运行时 `:/i18n/` 加载 →
`tr()` → 真实控件文案** 这整条链路，而不只是某个文件存在。

回归：ASan 生命周期场景 30 个子页、0 sanitizer/断言、0 告警，**默认中文界面不变**
（`当前设备标签 = "ABC123"`）；解析器探针仍全绿；全新构建 **0 错误 0 警告**，
日志中可见 `生成翻译文件 QtAdb_en.qm` 与 `Running rcc for resource qtadb_i18n`。

### 29.4 过程中的两个坑（都记下来避免重犯）

1. **本项目的资源不走 AUTORCC**：既有资源都通过 Qt 的资源 API（`qt_add_big_resources`）编译，
   所以我把生成的 `.qrc` 用 `target_sources` 加进去时**被静默忽略**（日志里根本没有 rcc 步骤，
   二进制里也没有资源）。改用 `qt_add_resources` 后才真正嵌入。
   ——"构建通过"不等于"资源进去了"，要看**构建日志里有没有 rcc 步骤**。
2. **验证工程要复刻同样的资源接线**，否则探针只能测到"文件格式正确"，测不到"嵌入与加载"。
   本轮给 `.dsh/asan` 加了与主工程一致的 lrelease + `qt_add_resources` 片段。

### 29.5 后续

- 补齐剩余 **215 条**翻译（`.ui` 文案占多数，填 `.ts` 即可，无需改代码）；
- 其余页面代码里的中文字面量（`sp_*`、`pagemaker`、`sonpagemanager` 等）按同样方式包 `tr()`；
- 界面语言选择（当前跟随系统语言；可加"语言"设置项并把选择持久化）。

---

## 三十、补齐英文翻译并验证 `.ui` 路径（第二十四轮）

### 30.1 补齐翻译：37 → 196 条

上轮接通链路后，`.ts` 里有 252 条、已译 37 条。本轮把**有意义的 UI 文案全部译完**：

| | 数量 |
|---|---|
| `.ts` 条目总数 | 252 |
| **已翻译** | **196**（本轮新增 159） |
| 未翻译（回退中文） | 56 —— 其中 40 条是 uic 给窗口标题的占位 `Form`、2 条版本号 `beta-v1.x`、13 条是 `links` 页里**纯英文品牌名 + 链接**（Magisk/LSPosed/TWRP/crDroid…）、1 条空标签 |

覆盖到的页面文案举例：按键模拟（36 条：系统导航/实体按键/媒体控制/系统应用…）、
系统功能链接页（27）、状态栏（20：飞行模式/热点/NFC/录屏状态…）、震动（14）、
设备信息、包管理器、验证服务器、分辨率/DPI、恢复、安装器、用户与账户等。

译文风格是「准确、简短、界面友好」，例如：

```
状态栏与导航栏      → Status bar & navigation bar
直接关闭（不建议）  → Turn off completely (not recommended)
双击以确认操作      → Double-click to confirm
系统应用请选"停用" | 第三方应用请选"卸载"
                    → Use "Disable" for system apps; use "Uninstall" for third-party apps
类原生的默认强度等级为0~3 | 定制ROM请自行尝试
                    → Stock ROMs default to levels 0–3; custom ROMs may differ
```

### 30.2 验证补上了 `.ui` 路径

上轮的 `--i18n` 探针只验证了**代码里 `tr()`** 的字符串。`.ui` 里的文案走的是另一条路
（uic 生成 `QCoreApplication::translate(...)` 调用），因此本轮把探针扩展为两条路都测：

```
  .qm 加载: 成功
  未装翻译器时: "设备信息"                    ← 默认中文（代码 tr）
  装翻译器后  : "Device info"
  提示文案    : " · Please launch the app first"
  未翻译回退  : "这个字符串故意没有翻译"
  主窗口顶部标签: "Please select a device first"   ← 真实控件（代码 tr）
  .ui 文案(标题): "Install app"                    ← 来自 sp_installer.ui
  .ui 文案(提示): "Do not unplug the device while this runs"
  全部符合预期
```

即：**代码 `tr()` + `.ui` 文案**两条路都被真实 `.qm` 翻译成英文，且未翻译条目回退中文。

### 30.3 过程中的一个坑

探针要 include 主工程某个 `.ui` 生成的 `ui_*.h` 时，AUTOUIC 报
`includes the uic file "ui_sp_installer.h", but the user interface file "sp_installer.ui" could not be found`
——uic 只在"包含者所在目录"与搜索路径里找 `.ui`。给验证工程加了
`AUTOUIC_SEARCH_PATHS`（扫描主工程全部含 `.ui` 的目录）后解决。

### 30.4 验证汇总

| 场景 | 结果 |
|---|---|
| 全新构建 | **0 错误 0 警告**，13.4 MiB；日志含 `Generated 196 translation(s) (196 finished and 0 unfinished)` 与 `Running rcc for resource qtadb_i18n` |
| `--i18n` 端到端（代码 + `.ui` 两条路） | 全部符合预期 |
| 回归：ASan 生命周期 + 逐页 | 30 个子页、0 sanitizer/断言、0 告警；**默认中文界面不变**（`当前设备标签 = "ABC123"`） |
| 解析器探针 | 42 个畸形输入未崩溃、9 项语义抽查通过 |
| 五个构建目标 | 全部 0 警告 |

### 30.5 后续（可选）

- 其余页面代码里的中文字面量（`sp_*`、`pagemaker`、`sonpagemanager` 的运行时文案）可按同样方式包 `tr()`；
- 界面语言选择项（当前跟随系统语言；可加设置项并持久化选择）。

---

## 三十一、页面运行时文案也纳入翻译（第二十五轮）

### 31.1 现状与做法

上轮译完的是**静态 UI**（`.ui` 文案 + 少量代码字符串）。但页面的**运行时文案**
（"已修改""输入无效""请先在列表中选择一个用户。""正在刷入，切勿插拔设备"…）
都写在 `.cpp` 里，既不显示在 `.ui` 中、也没有 `tr()`，因此**根本提取不到**。

本轮扫描代码里的中文字面量（**305 处 / 23 个文件**），逐一判断后包裹：

| 处理 | 数量 | 说明 |
|---|---|---|
| 包裹 `tr(...)` | **约 330 处** | 界面文案、对话框标题与正文、条目名、状态与提示 |
| 保持不译 | 36 处 | **命令示例与真正执行的命令**（`"adb shell pm list packages"` 等）、对象名 |
| 特殊处理 | 5 + 1 处 | 自由函数里的状态标签改用 `QCoreApplication::translate("DeviceState", …)`；`qssloader.cpp` 的开发者日志还原为普通字符串（自由函数无 `tr()`） |

`pageMaker` / `sonPageManager` **不是 QObject**，用 `Q_DECLARE_TR_FUNCTIONS(...)` 获得 `tr()`（与 `textExplainer` 同一手法）。

### 31.2 过程中修掉的三类"包裹后编译不过"

机械替换在所难免要踩坑，三类都遇到并解决：

1. **`QStringLiteral(tr("…"))` 非法**（该宏只接受字面量）——共 14 处，去掉外层 `QStringLiteral` 即可；
2. **相邻字面量被拆开**：`"第一行\n" "第二行\n" + path` 被替换成 `tr("第一行\n") tr("第二行\n")`，语法错误。
   处理办法是**先把相邻字面量合并**再包 `tr()`（共合并 71 处）；对翻译而言这也更合理——
   整条消息成为一个可翻译单元；
3. **自由函数里没有 `tr()`**：`stateLabel()`（自由函数）与 `qssloader.cpp` 的诊断输出。
   前者用 `QCoreApplication::translate("DeviceState", …)` 保留可翻译性，后者本就只是开发者日志，还原为普通字符串。

### 31.3 提取与翻译

`lupdate` 重新提取：**436 条**（新增 184 条，另有 11 条由 lupdate 的"同文同译"启发式自动复用）。
本轮补译 **149 条短文案**（页面提示、对话框、条目名、状态标签、激活器页标题等），累计 **356 条已译**。

未译的 **80 条**是 **`sonPageManager` 里激活器页的长篇说明文字**（每个激活器一段操作指引，数百字），
本轮如实保留为中文（回退行为），没有为了数字好看而机翻。

### 31.4 验证：三条翻译路径全部走通

`--i18n` 探针现在覆盖三条路径：

```
  .qm 加载: 成功
  未装翻译器时: "设备信息"                          ← 默认中文
  装翻译器后  : "Device info"                       ← ① 代码 tr()
  提示文案    : " · Please launch the app first"
  未翻译回退  : "这个字符串故意没有翻译"
  主窗口顶部标签: "Please select a device first"      ← 真实控件
  .ui 文案(标题): "Install app"                      ← ② .ui（uic 生成 translate）
  .ui 文案(提示): "Do not unplug the device while this runs"
  状态标签(翻译后): "[On]"                           ← ③ 自由函数 QCoreApplication::translate
  全部符合预期
```

其余回归：五个构建目标 **0 警告**；ASan 生命周期 + 逐页 30 个子页、0 sanitizer/断言、0 告警，
**默认中文界面不变**（`当前设备标签 = "ABC123"`）；解析器探针 42 例未崩溃；
全新构建 **0 错误 0 警告**，13.5 MiB，`Generated 356 translation(s)` + `Running rcc for resource qtadb_i18n`。

### 31.5 后续（可选）

- 翻译那 80 条长说明文字（纯文案工作，不动代码）；
- 界面语言选择项（当前跟随系统语言）。

---

## 三十二、工厂注册表化：把 switch 变成可扩展的数据表（第二十六轮）

> 目标在此轮被明确为：**不添加功能**，只做健壮 / 可维护 / 可拓展 / 易读。本轮是这条线上的主线改动。

### 32.1 先立"行为等价基线"，再动手

重构工厂最怕"看起来没变、实际换了页面"。因此先写一个**工厂枚举探针**（`--factories`），
把两条工厂路径的全表打印出来：

- `sonPageManager::selector(parentName, key)`：7 个父页名 × 21 个序号；
- `pageMaker::createPageWithKey(key, dev)`：6 种设备状态 × 11 个序号。

关键细节：`createPage_*` 都返回 `basePage` 实例，**类名无法区分**"设备信息页"和"不可用页"，
必须用 `whoAmI()`（`whoYouAre()` 设置的页面名）才能看出真实映射。先跑一遍留作基线。

### 32.2 `sonPageManager`：6 个 switch（36 个 case）+ if-else 链 → 数据表

| 改动 | 原状 | 现状 |
|---|---|---|
| `selector` | 6 段 if-else，**未知父页名 `throw "…"`**（抛 C 字符串、无人接住 = 直接 terminate） | 查表；未知名 `qWarning()` + 返回 `nullptr`（调用方本就检查 nullptr） |
| `createSonPageFor_devInfo/_activator/_apps/_devControl/_recovery/_advanced` | `switch(key + 1)` 逐个 case，共 36 个 | 每个函数一张 `{ 序号, 工厂 }` 表；**`+1` 偏移约定消失**，每项自己写明序号 |
| `createSonPageFor_activator` | 18 个近乎逐字重复的 case（202 行） | **数据表**：`{ 图标, 标题, 说明, 命令 }` 18 行 + 一个通用构造函数（文案用 `QT_TR_NOOP` 标记，`lupdate` 仍能提取） |

新增一个子页从"改 switch + 记住 +1 偏移 + 抄一遍构造流程"变成**加一行数据**。

### 32.3 `pageMaker::createPageWithKey`：71 行 → 14 行

- `switch` + 3 处重复的 `if(!isPageAvailable(...)) return createPage_disabled(...)` → 一张
  `{ 序号, 工厂 }` 表 + 一个 `gated()` 包装器（门禁逻辑只写一次）；
- 顺手删掉死代码：开头的 `if(dev.addr.toInt() == -1) k = 6;` —— 序列号非数字时 `toInt()` 返回 0，
  只有序列号字面量为 `"-1"` 才会命中；"没有设备"那条路径是 `initBasePage()` 直接传 key 6，所以此判断不可达；
- 未知序号仍返回 `nullptr`（保持原行为），但**加了 `qWarning()`**：调用方若直接 `addWidget(nullptr)` 会崩，
  日志能让这种潜在问题一眼可见。

### 32.4 验证：行为表 + 文案集合 双重等价

| 检查 | 方法 | 结果 |
|---|---|---|
| 子页映射 | `--factories` 全表逐行 diff（重构前 vs 后） | **36 项完全一致**；唯一差异是未知父页 `抛出异常 → nullptr`（有意的健壮性改进） |
| 主页面映射 | 同上（6 状态 × 11 序号） | **完全一致** |
| 激活器文案 | 把相邻字面量按 C++ 语义**合并成整条**后与 git HEAD 对比 | HEAD 32 条 / 当前 32 条，**零缺失零新增** |
| 运行回归 | ASan + 逐页遍历 | 30 个子页、0 sanitizer/断言、0 告警 |
| 其余场景 | 挂起 / 聚焦测试 / 热插拔 / 解析器探针 / i18n 探针 | 全部通过 |
| 全新构建 | — | **0 错误 0 警告**，13.5 MiB |

### 32.5 过程中的两个技术坑

1. **`QHash<K, Struct>` 的聚合初始化要正确嵌套**：`{ 序号, 图标, 标题, 说明, 命令 }` 会被当成
   5 个元素的 pair 而编译失败，必须写成 `{ 序号, { 图标, 标题, 说明, 命令 } }`；
2. **`QT_TR_NOOP` 是数据表 + i18n 的正确组合**：把文案放进表后若直接用 `tr(变量)`，`lupdate` 无法提取；
   在表里用 `QT_TR_NOOP("…")` 标记、运行时再 `tr()`，两者兼得（本轮激活器文案即如此）。

### 32.6 后续（仍在不添加功能的前提下）

- 其它仍有重复形状的地方：`applyDeviceList()` 的显示串构造、若干页面里"split + 守卫 + setItem"的表格填充；
- 长函数拆分（`sp_permissions` 的两个填表函数各 150 行以上）；
- 那 80 条未翻译的激活器长说明（纯文案）。

---

---

## 三十三、上下文固化 + "关级"审计：查出监视命令缺设备前缀（第二十七轮）

### 33.1 上下文固化

本文件顶部的 **§0「状态与待办」** 即为固化产物：目标与约束、当前状态、已完成清单、
**照抄即可的构建与验证命令**、打桩目录说明、验证纪律、待办排序、关键约定。
上下文被压缩后，从 §0 即可恢复全部必要信息，不必依赖会话记忆。

### 33.2 新增两项验证能力

1. **逐参数记录**：打桩除记录整条命令外，还把 argv 逐项写到 `.dsh/bin/.invocations.args`
   （`|` 分隔）。这样能区分"一个含空格的参数"与"两个参数"——整条命令的字符串记录做不到这一点。
2. **`--install-path` 探针**：用一条带空格与中文的 APK 路径（`/tmp/My App 我的应用.apk`）
   跑真实的 `adbThread` 链路，验证 `#INSTALL#`/`#SIDELOAD#` 的 `~` 分隔符约定。

结果（证明该机制正确）：

```
-s|ABC123|install|/tmp/My App 我的应用.apk|
-s|ABC123|sideload|/tmp/My App 我的应用.apk|
```

路径确实是**一个**参数——这正是"Program Files"这类含空格路径不会失败的原因。

### 33.3 关级审计查出的真实缺陷：监视器命令没有 `-s` 设备前缀

把整轮运行中发给 adb 的 argv 全部导出后逐条核对，发现两条**异常**：

```
shell|cat|proc/stat          ← 没有 -s
shell|cat|/proc/meminfo      ← 没有 -s
```

其余所有与设备相关的命令都带 `-s ABC123`。原因：第 18 轮把监视器改成"定时器 + 异步请求"时，
用的是**不带设备**的 `runWithCallback(AdbCmd::cat(...))`，而不是 `runForDeviceWithCallback(...)`。

**用户可见后果**：多设备环境下 adb 会挑"默认设备"（多台时甚至直接报
`more than one device/emulator`），于是 CPU/内存可能显示的是**另一台设备**的数据。

修复：改用 `runForDeviceWithCallback(dev.addr, …)`；同时补齐
`runForDeviceWithCallback` 缺失的 `Explain` 参数（监视器需要传 `Explain::No`，
原签名没有该参数，编译期即暴露）。

修复后复查：监视命令变为 `-s|ABC123|shell|cat|proc/stat`；
仍不带 `-s` 的只剩**本就与设备无关**的四条：`devices -l`、`start-server`、`kill-server`、`connect …`。

### 33.4 验证

| 场景 | 结果 |
|---|---|
| 正常（ASan + 逐页） | 30 子页、0 sanitizer/断言、0 告警，监视 **CPU 100% / RAM 50%** |
| 畸形输出（`bin-bad`） | 30 子页、0 sanitizer/断言 |
| 挂起（`bin-hang`） | 退出码 0，窗口 **44 ms** 出现 |
| 聚焦测试 | 退出码 0 |
| 探针 | 解析器"不符 0 项"；i18n"全部符合预期"；**工厂映射与基线完全一致** |
| 全新构建 | **0 错误 0 警告**，13.5 MiB |

### 33.5 一点方法上的收获

"关级审计"（把外部进程实际收到的每个 argv 全量导出后核对）成本极低，却能发现
**类型系统与单元测试都看不见**的问题：这里缺的不是代码路径，而是一个命令行参数。
凡是"应用 → 外部命令"的边界，都值得做一次这样的全量核对。

---

---

## 三十四、别名显示修复 + 死代码清理 + 纯函数抽取（第二十八轮）

### 34.1 真实缺陷：下拉列表里的设备别名被型号名覆盖

第 19 轮修好"设备别名"后，我验证的是**顶部标签**（`dynamic_island`）——它确实显示别名。
但本轮读 `comboboxListWidget::setUpList()` 时发现：**在我设置别名之后**，还跟着一段旧命名逻辑：

```cpp
wgt->setName(DeviceNames::nameFor(devList[i].addr));   // ① 新：用户别名
...
if(nameList[1].contains(devList[i].addr))              // ② nameList 永远是空的（填充代码早被注释）
    wgt->setName(nameList[0][nameList[1].indexOf(...)]);
else
    wgt->setName(explainer->get_words_after(devList[i].model, ":"));   // ③ 于是总走这里 → 覆盖成型号名
```

即：**下拉列表里显示的从来不是用户设的别名，而是型号名**。这是我上一轮集成的遗漏，
只验证了"标签"没验证"列表项"。

修复：删掉 ②③ 与伴随的三行 `qDebug`。回退行为并未丢失——`devItem::setName("")` 本就会把
型号名设成**占位提示**，所以"没起名字就显示型号"的效果依然在，且不会覆盖别名。

**UI 级验证**（新增驱动输出，读取下拉项的名称字段）：

```
有别名时: 当前设备标签 = "我的测试机"   下拉项名称字段: text="我的测试机" placeholder=""
无别名时: 当前设备标签 = "ABC123"      下拉项名称字段: text="" placeholder="为设备命名（默认 TestDevice）"
```

### 34.2 死代码清理

`comboboxListWidget` 里围绕旧方案（写回**只读** Qt 资源 `:/cfgs/cfg/name.cfg`）的残留全部清除：
`QFile file` 成员、`nameList[2] / name / addr` 三个成员、构造函数里的赋值、`setUpList()` 里
两大段注释掉的读取逻辑、以及三行死 `qDebug`。文件 **112 → 71 行**。

### 34.3 `applyDeviceList()` 抽出两个纯函数（194 → 158 行）

这个函数是本项目最常被读、也最容易出错的地方（第 19、21、27 轮的缺陷都出在这一带）。
本轮先把其中两段**无副作用**的逻辑抽成文件内 `static` 函数：

- `deviceListChanged(before, after)`：数量 / 序列号 / **状态**任一不同即视为变化。
  原实现是嵌套四条 if-else，其中 `else if(devList.isEmpty() && !tmpList.isEmpty())`
  **恒不成立**（进入该分支时 `tmpList` 必为空）——抽取后语义不变但一眼可读。
- `offlineIndexes(list)`：未响应设备的下标。

`emit adbDeviceChanged()` 仍留在主函数里（纯函数不发信号）。
剩余三段（重建下拉列表 / 更新顶部指示 / 选中设备）留待下一轮。

### 34.4 验证

| 场景 | 结果 |
|---|---|
| 正常（ASan + 逐页） | 30 子页、0 sanitizer/断言、0 告警，监视 CPU 100% / RAM 50% |
| **热插拔**（`deviceListChanged` 路径） | 设备数 **1 → 2 → 0**，与抽取前完全一致；0 断言 |
| 离线设备（`offlineIndexes` 路径） | 设备数 1，标签正确 |
| 别名显示（新增探针输出） | 有别名/无别名两种情况均符合预期 |
| 探针 | 解析器"不符 0 项"、i18n"全部符合预期"、工厂映射与基线一致 |
| 全新构建 | **0 错误 0 警告**，13.5 MiB |

### 34.5 教训

第 19 轮我把"别名"当成已修好的功能，依据是**标签**显示正确；但同一个数据在界面里有**两个**落点。
"修好一个功能"必须把它的**全部呈现位置**都验证到，否则就是这次的"看起来修好了"。

---

---

## 三十五、权限页：一个空指针崩溃 + 一个"每次刷新泄一个模型"（第二十九轮）

`sp_permissions.cpp` 是本项目解析与表格填充最重的文件（426 行，两个 150+ 行的填表函数，
大量 `setItem(i-1, j-2, …)` 下标运算），本轮把它的刷新路径做成可反复触发的探针来查。

### 35.1 探针设计：用"对象个数"而不是 RSS 取证

新增 `--permissions` 探针：反复调用页面的刷新槽（`QMetaObject::invokeMethod`），
每轮统计 **`page->findChildren<QStandardItemModel *>().size()`**。

用对象个数而不是 RSS，是因为它**确定**：模型都以页面为父对象，只要没有被释放就会出现在
`findChildren` 里；RSS 受分配器与 sanitizer 影响，不是可靠指标。这也是本项目一贯的做法
（第 22 轮用 LSan 栈而不是 RSS 判定泄漏）。

### 35.2 缺陷一：空指针崩溃（`generalMode`）

第一次运行探针就**崩在**这里：

```
sp_permissions.cpp:412: runtime error: member call on null pointer of type 'struct QWidget'
AddressSanitizer: SEGV ... in sp_permissions::generalMode(QString)
```

`pa` 是构造函数里保存的 `parent`，第 412 行直接 `pa->geometry().width()`。
页面只要有父对象就不会崩，但"没有父对象"并非不可能（探针用 `sp_permissions(nullptr)` 即可复现）。

顺带发现同一行的第二个问题：`width() - 200` 在**窄窗口下会算出负数**，列宽为负没有意义。

修复：`pa` 为空时回退到表格自身宽度，并把结果夹到下限 120。

### 35.3 缺陷二：每刷新一次泄漏一个模型

修掉崩溃后探针跑通，数字很清楚：

```
修复前：第 1 次刷新后 2 个、第 6 次 7 个、第 12 次 13 个   ← 线性增长
修复后：第 1、3、6、9、12 次刷新后 均为 1 个              ← 恒定
```

原因：`current_model` 有 **3 处赋值、0 处释放**，每次刷新都 `new QStandardItemModel(this)`
装一整张权限表，旧模型只是被重新赋值丢掉引用，仍作为子对象挂在页面上。

修复：新增 `replaceModel()` 统一处理——先 `deleteLater()` 旧模型再接管新的
（延迟删除是因为此刻事件循环里可能还有针对旧模型的排队事件，如布局与重绘）。

### 35.4 顺带的可读性清理（444 → 435 行）

- 两处收集循环里的**死变量** `for(int i = 0, j = i; …) { …; j++; }`——`j` 声明了、自增了、从未使用；
- `model->setColumnCount(groups.count())` 原本写在循环体里，每轮重设一次，提到循环外；
- `generalMode` 里的 `j` 命名改为 `row`：它**不是** `i-2`（空行会被跳过，只有写入时才自增），
  原来的名字让人以为是简单偏移。

### 35.5 验证

| 场景 | 结果 |
|---|---|
| `--permissions`（本次新增） | 刷新 12 次，遗留模型恒定 **1** 个；无崩溃 |
| 正常（ASan + 逐页，含权限页） | 30 子页、0 sanitizer/断言、0 告警，监视 100%/50% |
| **畸形输出轮换**（`bin-fuzz`，重点覆盖权限填表） | 30 子页、0 sanitizer/断言 |
| 挂起 / 热插拔 / 聚焦测试 | 45 ms 出窗口 / 1→2→0 / 退出码 0 |
| 探针 | 解析器"不符 0 项"、i18n 全部符合 |
| 五个构建目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 35.6 方法上的收获

"反复触发同一操作并统计对象个数"能发现**一次性测试发现不了**的问题：
单次刷新时泄漏 1 个模型毫无症状，跑 12 次就一目了然。
凡是"可重复触发的操作"（刷新、翻页、切换设备），都值得做成计数型探针。

---

---

## 三十六、模型生命周期统一治理：把"计数型探针"推广到全部子页（第三十轮）

### 36.1 探针推广：一次发现 6 个页面的同类问题

上轮的计数型探针只覆盖权限页。本轮把它做成 **`--repeat [轮数]`**：遍历全部 18 个子页，
找到每页的"刷新"按钮并用**真实点击**重复触发，然后按**类名**对比子对象数量：

```
sp_permissionGroups  对象 23 → 37  [QStandardItemModel 0→7]
sp_packages          对象 28 → 42  [QStandardItemModel 0→7]
sp_features / sp_libraries / sp_users / sp_accounts   同样 +7 个模型
```

即：**"每刷新一次泄漏一个装着整张表的模型"不是我上轮修的那一页独有，而是 6 个页面共有的模板缺陷**。

### 36.2 统一修复：把生命周期管理放进共同基类

这 7 个页面都继承 `animationWidget`（不是 `sonPageBase`），因此修复放在基类里最合适：

```cpp
// animationwidget.h
void installTableModel(QAbstractItemView *view, QStandardItemModel *model);  // 装上新的，释放旧的
QStandardItemModel *installedTableModel() const;                             // 当前模型（供弹窗等读取）
```

- 内部用 `QPointer<QObject>` 保存（`QPointer<QStandardItemModel>` 需要完整类型，会让这个
  被所有页面包含的头文件拖进 `QStandardItemModel`）；
- `deleteLater()` 而非 `delete`：此刻事件循环里可能还有针对旧模型的排队事件（布局/重绘）。

7 个页面共 9 处 `setModel(model)` 改为 `installTableModel(view, model)`；
`sp_permissions` 私有的 `current_model`/`replaceModel` 随之删除——**同一件事只留一个实现**。

### 36.3 第二个泄漏源：空数据路径下"创建后既不安装也不释放"

探针随后显示：模型数已恒定，但 `sp_accounts` 的对象仍在 +7。根因是模板代码先
`new QStandardItemModel(this)`、**再**判断 `if(!list.isEmpty())`：列表为空时模型既不安装也不释放。
把它移进非空分支即可（6 个页面同样处理）。

设备上没有账户时每次刷新都会走到这条路径——这不是理论问题。

### 36.4 一次"让情况变差"的实验（如实记录）

顺手观察到 `QItemSelectionModel` 也在增长（`sp_accounts` 6 次刷新 +7），我尝试
"缓存一个占位空模型、避免每轮 `new`"。结果**更糟**：

| | QStandardItemModel | QItemSelectionModel |
|---|---|---|
| 实验前 | 0→1（已修好） | sp_accounts 0→7 |
| 实验后 | 0→**2** | sp_accounts 0→**14** |

原因是每次 `setModel()` 都会让视图新建一个选择模型，而缓存占位模型让每轮变成**两次** `setModel`
（先占位、再真实模型），调用次数翻倍 → 增长翻倍。该实验**已完整回退**。

结论（下一轮待办）：残余增长与 **`setModel()` 调用次数**直接相关，疑为 Qt 的延迟回收——
在嵌套 `processEvents()` 里 `DeferredDelete` 不会派发，我显式 `sendPostedEvents(nullptr, DeferredDelete)`
也未见回收。要下结论需要**用 LSan 判断它是真泄漏还是延迟释放**，不能靠对象计数。

### 36.5 修复后的状态与验证

```
sp_permissionGroups / sp_packages / sp_features / sp_libraries / sp_users：
    对象 +2（模型 0→1 + 一个待回收的选择模型）   ← 线性增长消失
sp_accounts：模型 0→0（不再泄漏模型）
```

| 场景 | 结果 |
|---|---|
| `--repeat 6`（18 个子页，真实点击刷新） | 模型数全部恒定；线性增长消失 |
| `--permissions` | 刷新 12 次遗留模型恒定 1 个 |
| 正常（ASan + 逐页） | 30 子页、0 sanitizer/断言、0 告警，监视 100%/50% |
| **畸形输出轮换** | 30 子页、0 sanitizer/断言 |
| 探针 | 解析器"不符 0 项"、i18n 全部符合 |
| 四个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 36.6 方法上的收获

1. **把探针从"一页"推广到"全部页面"**，立刻发现"这是模板缺陷、不是孤例"——修一处不如修一类；
2. **按类名对比**比只看总数有用得多：总数 +7 说不清是什么，`QStandardItemModel 0→7` 一目了然；
3. **实验失败也要记录**：这次"缓存占位模型"的尝试基于合理猜测，实测却让指标翻倍，
   回退并写进文档，比悄悄删掉更有价值——它把"增长与 `setModel()` 次数相关"这条线索留了下来。

---

---

## 三十七、给残余增长定性，并把 `setModel()` 收敛到每轮一次（第三十一轮）

### 37.1 先用 LSan 定性，别靠对象计数下结论

上一轮留下一个悬案：`QItemSelectionModel` 随刷新次数增长，是泄漏还是延迟回收？对象计数**无法区分**
（仍挂在视图下的对象既"没释放"又"可达"）。于是用 LSan 对比两种工作量：

| 运行 | LSan 报告 |
|---|---|
| `--repeat 1` | `7072 byte(s) leaked in 117 allocation(s)` |
| `--repeat 12` | **`7072 byte(s) leaked in 117 allocation(s)`** |

**逐字节相同**，且报告里 `QItemSelectionModel` / `QStandardItemModel` 出现 **0 次**。

结论：**不是泄漏**，而是"保留到页面销毁才回收"。LSan 只报告不可达内存，而仍挂在视图下的
选择模型是可达的——所以"可达但长期不释放"这件事只能靠对象计数发现，两者结合才能定性。

### 37.2 根因：`setModel()` 调用的次数

顺着上一轮"实验失败"留下的线索（增长与 `setModel()` 次数相关），读代码发现每轮刷新其实调了**两次**：

```cpp
QStandardItemModel* emptyModel = new QStandardItemModel(this);
ui->listView->setModel(emptyModel);      // ① 先挂个空占位
...
ui->listView->setModel(model);           // ② 再挂真实模型
...
if(ui->listView->model() == emptyModel) ui->listView->setModel(nullptr);   // ③ 空数据时解绑
emptyModel->deleteLater();
```

②会让视图重建选择模型，①③又各制造一次；其中 ③（`setModel(nullptr)`）之后旧选择模型还被留着。

修法不是"缓存占位模型"（上一轮试过：那会让每轮变成两次 `setModel`，指标翻倍），而是
**每轮只调一次**：

- 删掉开头的 ①（占位空模型）与结尾的 ③（解绑 + 延迟删）；
- 只在"视图还没有模型"时补一个空模型，同样走 `installTableModel()`。

### 37.3 结果：全部 7 页恒定

```
修复前（6 轮）: sp_accounts 选择模型 0→7；其余各页 0→1（每轮一次）
修复后（6 轮与 12 轮）: 全部 7 页均为  模型 0→1，选择模型 0→1   ← 与轮数无关
```

LSan 复查：报告仍与工作量无关（7072 字节 / 117 对象）✓。

### 37.4 顺手把剩余泄漏也定了性

LSan 报告（n=12）里本项目相关的只有两类，合计 **7072 字节 / 117 对象**：

| 大小 | 来源 |
|---|---|
| 5408 字节 / 91 对象 | `AdbClient::runWithCallback` ← `adbThread::start()`：**进程退出时仍排在队列里的请求**（含其 `std::function` 闭包） |
| 1664 字节 / 26 对象 | 随之的 `QString`（命令 argv） |

因为 **n=1 与 n=12 完全相同**，它们是**一次性**的退出残留，不随操作次数增长——
不属于"每次操作泄漏"，不值得为它改动产品代码（要彻底消除得在退出时清空请求队列，
属于可选改进）。

### 37.5 验证

| 场景 | 结果 |
|---|---|
| `--repeat 6` / `--repeat 12` | 7 个列表页 模型与选择模型均恒定 |
| LSan（1 轮 / 12 轮） | 7072 字节 / 117 对象，两次相同；不含模型类 |
| 正常（ASan + 逐页） | 30 子页、0 sanitizer/断言、0 告警，监视 100%/50% |
| 畸形输出轮换 | 30 子页、0 sanitizer/断言 |
| 挂起 / 热插拔 / 聚焦测试 | 44 ms / 1→2→0 / 退出码 0 |
| 探针 | 解析器"不符 0 项"、i18n 全部符合、权限页恒定 1 个模型 |
| 四个目标 + 全新构建 | 全部 **0 错误 0 警告**，13.5 MiB |

### 37.6 方法上的收获

**"是不是泄漏"不能只靠一种证据**：
- 对象计数能发现"可达但不释放"（LSan 看不见，因为可达）；
- LSan 能发现"不可达但没释放"（对象计数看不见，因为不在树上）；
- 而"是否随工作量增长"必须**跑两个不同规模对比**——单次运行的数字说明不了任何问题。

本轮把这三件事合起来用，才把上一轮留下的悬案真正定死。

---

---

## 三十八、权限页三套填表逻辑拆分 + 5 处"空调用"清理（第三十二轮）

### 38.1 一个函数里塞了三套表格

`sp_permissions::refresh_tableView()` 原本是 142 行、三套互斥的填表逻辑挤在一起
（`arg_g` 按组整理 / `arg_f` 输出所有信息 / 其余"简短摘要"），共用一堆局部变量。
本轮按分支拆成具名函数 + 一个分派器：

```cpp
void sp_permissions::refresh_tableView(QString s)
{
    // 三个互斥分支；注意 arg_s / arg_d / arg_u / 默认 走的都是同一套"简短摘要"，
    // 原实现把它们写成了四个一模一样的分支。
    if(arg_g)      { fillGroupedTable(s); }
    else if(arg_f) { fillFilteredTable(s); }
    else           { generalMode(s); }
    ...
}
```

| 函数 | 行数 | 职责 |
|---|---|---|
| `fillGroupedTable` | 51 | 按 `group:` 切分，每组一列 |
| `fillFilteredTable` | 66 | 固定 5 列（permission/package/label/description/protectionLevel） |
| `refresh_tableView` | **23**（原 142） | 分派 + 空数据兜底 |

顺带把 `else if(arg_s) / else if(arg_d) / else if(arg_u) / else` 四个**内容完全相同**的分支
收敛为一个 `else`。

### 38.2 五处"空调用"

构造代码里有五行：

```cpp
btn_g->autoExclusive();   // 取值调用，没有任何效果（本意应是 setAutoExclusive(true)）
...
```

`autoExclusive()` 是 **getter**，调用它什么也不会发生。这些按钮本来就加在同一个
`QButtonGroup` 里、互斥行为成立，所以直接删除并留下说明。

### 38.3 验证与一处诚实的说明

| 场景 | 结果 |
|---|---|
| 正常（ASan + 逐页） | 30 子页、0 sanitizer/断言、0 告警；**自动关闭模态对话框 73 个** |
| 畸形输出轮换 | 30 子页、0 sanitizer/断言 |
| `--repeat 6` | 7 个列表页模型数恒定 |
| `--permissions` | 刷新 12 次遗留模型恒定 1 个 |
| 解析器 / i18n 探针 | "不符 0 项" / "全部符合预期" |
| 四个目标 + 全新构建 | 全部 **0 错误 0 警告** |

**覆盖范围的说明**：`--permissions` 探针驱动的是**默认模式**（"简短摘要"）。
另外两种模式由单选项切换，而"选项"对话框是**模态**的（`exec()`），探针里触发会阻塞事件循环，
因此没有在探针里直接驱动它们——它们由逐页遍历场景通过**真实点击 + 模态对话框自动关闭**
（本次 73 个）覆盖，且被移动的代码是**逐字搬移**（只去掉一层缩进），编译与结构均可核对。
这一点写清楚，避免把"覆盖"说得比实际更强。

### 38.4 方法上的收获

拆分这类"一个函数三种形态"的代码时，**先按分支整体搬移、再谈优化**比边拆边改安全得多：
搬移只去掉缩进、不做任何重写，行为等价性可以由编译 + 既有场景覆盖来保证；
真正的重写留到结构清楚之后再做。

---

---

## 三十九、`applyDeviceList` 拆分，以及一处"有几台设备就重建几次"（第三十三轮）

### 39.1 拆分：158 行、7 种职责 → 30 行 + 两个具名函数

`MainWindow::applyDeviceList()` 是全项目最常被读、也是历史缺陷最集中的函数
（§二十五 别名、§二十七 刷新劫持导航、§三十四 别名覆盖都出在这一带）。按职责拆开：

| 函数 | 行数 | 职责 |
|---|---|---|
| `applyDeviceList` | **30**（原 158） | 取快照 → 重新解析 → 变化则发信号 → 按需重建 → 更新指示 |
| `rebuildDeviceCombo(offline)` | 9 | 断连 → `clear()` → `setUpList()` → 重连 |
| `updateDeviceIndicators(offline)` | 44 | 自动选中、离线置灰、顶部标签与配色、无设备时停监视器 |

其中"断开 `currentIndexChanged` → 清空 → 重填 → 连回来"这段原本在**两处**各写了一遍，
现在只有一处实现（断连的原因也写进了注释：避免 `clear()` 触发 `setCurrentDevice()` 去操作已不存在的设备）。

顺带简化：原来的"逐元素拷贝到 `tmpList`"改为 `const QList<device> before = devList;`
（`device` 是值语义的普通结构体，拷贝一份等价）；`liangYi = false` 原本写了两次，现在一次。

### 39.2 发现的真实浪费：重建被放进了"逐设备循环"

原来的结构是：

```cpp
for (int i = 0; i < devList.size(); i++)      // ← 循环变量 i 在循环体里【从未使用】
{
    if(changed)
    {
        disconnect(...); ui->comboBox->clear(); view->setUpList(devList, off); connect(...);
    }
}
```

也就是说：**有几台设备就把下拉框重建几次**。每次重建都会销毁并重新创建所有设备卡控件，
于是 N 台设备要做 N 次 —— 最终状态与"只重建一次"完全相同（所以这个缺陷一直没被发现），
但控件增删是 O(N²)。

修复：把这个循环换成一次判断 `if(changed || !liangYi) rebuildDeviceCombo(offline);`。

### 39.3 验证：设备列表的全部场景

| 场景 | 结果 |
|---|---|
| 单设备（`bin`） | 设备数 1、标签 "ABC123"、名称字段占位符正确；30 子页、0 sanitizer/断言 |
| **双设备（`bin-two`，含 2 台 × 3 轮切换）** | 设备数 2、切换完成；30 子页、0 sanitizer/断言 |
| 离线设备（`bin-offline`） | 设备数 1、标签正确；0 断言 |
| **热插拔（`bin-hotplug`）** | 设备数 **1 → 2 → 0**；0 断言 |
| 设备中途消失（`bin-vanish`） | 退出码 0、0 断言 |
| 别名场景 | 顶部标签与下拉项名称字段**都**显示 "我的测试机" |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 聚焦 / 三个探针 | 44 ms 出窗口 / 退出码 0 / 全部符合预期 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 39.4 方法上的收获

"循环体不使用循环变量"是一个可以**机械检查**出来的坏味道：它意味着这段代码被执行了
`size()` 次却做着完全相同的幂等操作。类似地，"同一段代码在同一个函数里出现两遍"
（这里的断连-重建-重连）几乎总该抽成具名函数——拆完之后，这个函数从"要通读 158 行"
变成"读 30 行就能看懂流程"。

---

---

## 四十、`initBasePage` 重构：一次导航里白建一个页面（第三十四轮）

### 40.1 原实现做了什么

```cpp
while(key != -1)              // ← 假循环：函数体最后无条件 break
{
    this->lock();
    if(currentPage != NULL) { delete currentPage; currentPage = NULL; }

    currentPage = new basePage(this);            // ← ① 先建一个页面
    currentPage->wgtHeight = ui->widget_height->height();   // ← ② 给它设高度
    connect(currentPage, SIGNAL(animationEnd()), this, SLOT(slot_taiChi()));  // ← ③ 连信号

    if(devList.isEmpty()) { delete currentPage; currentPage = maker->createPageWithKey(6, ...); }
    else                  { delete currentPage; currentPage = maker->createPageWithKey(key, ...); }
    ...                                       // ← ① 建的那个页面在这里被删掉、换成真正的页面
    break;
}
```

也就是说：**每次导航都先构造一个 `basePage`（其构造函数会 `ui->setupUi(this)` 建出整套控件树）、
设高度、连信号，然后在两个分支里都立刻删掉**。②③ 两项工作全落在即将销毁的对象上，
是死代码；①则是每次切换页面都白做一次"整棵控件树的构造 + 析构"。

### 40.2 重构后的样子

```cpp
void MainWindow::initBasePage(int key)
{
    if(key < 0) { return; }        // indexList 清空会发出 -1，保持"什么都不做"的原语义
    this->lock();
    if(currentPage != nullptr) { delete currentPage; currentPage = nullptr; }

    device target;
    int pageKey = key;
    if(devList.isEmpty() || current_device < 0 || current_device >= devList.size())
    {
        target.addr = "#EMPTY#";   // 没有设备：占位设备 + 固定打开"关于"页
        pageKey = 6;
    }
    else
    {
        target = devList[current_device];
    }

    currentPage = maker->createPageWithKey(pageKey, ui->page, target);
    if(currentPage == nullptr)
    {
        qWarning() << "创建页面失败，页面序号 =" << pageKey;
        this->unlock();            // 上面 lock() 过，不解锁界面会永久锁死
        return;
    }
    connect(currentPage, SIGNAL(animationEnd()), this, SLOT(slot_taiChi()));
    ui->verticalLayout_2->addWidget(currentPage);
    currentPage->playLoadAnimation(ui->widget_height->height());
    ... 重设 taiChiTimer（取消上一个仍在等待的定时器）
}
```

四处改动，都是"等价或更安全"：

| 改动 | 性质 |
|---|---|
| 去掉 `while(key != -1){…break;}` | 可读性：假循环 |
| 不再先建后删那个页面 | **去掉每次导航一次整棵控件树的构造/析构**；`wgtHeight` 死赋值一并消失（真实页面一直用默认 427，动画用的是显式传入的高度） |
| `current_device` 越界 / `devList` 为空的合并判断 | 健壮性：原实现 `devList[current_device]` 在 `current_device == -1` 且列表非空时会**越界访问** |
| `currentPage == nullptr` 时提前返回并 `unlock()` | 健壮性：原实现会把 `nullptr` 交给 `addWidget()`（Qt 会报错，且界面已 `lock()`、无人解锁） |

### 40.3 验证

| 场景 | 结果 |
|---|---|
| 正常（ASan + 逐页，反复走 `initBasePage`） | 30 子页、0 sanitizer/断言、0 告警、**0 条"创建页面失败"**、模态对话框自动关闭 72 个 |
| **界面锁定状态** | 遍历结束后 `indexList.enabled=1`（未被锁死） |
| 畸形输出轮换 | 30 子页、0 断言 |
| 双设备 | 设备数 2、切换完成、0 断言 |
| 热插拔 / 挂起 / 聚焦 | 1→2→0 / 43 ms / 退出码 0 |
| 三个探针 | 解析器"不符 0 项"、i18n 全部符合、权限页恒定 1 个模型 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 40.4 方法上的收获

`lock()` / `unlock()` 这种成对操作的函数，**每条提前返回路径都要问一句"我锁着吗"**。
原实现没有提前返回（它无条件走到最后），所以问题被掩盖了；一旦为了健壮性加守卫，
就必须同时补上解锁——这也是本次重构中唯一"改了行为"的地方，而它改的正是原来的隐患。

---

---

## 四十一、坏味道机械扫描 + 别名"写了但没写进去"（第三十五轮）

### 41.1 把人工发现变成机械扫描

前两轮靠读代码发现的坏味道（假循环、白建后删、N 次重复工作）都是**可机械检测**的。
本轮给全工程 44 个 `.cpp` 写了一次扫描（先剥注释，再匹配结构）：

| 模式 | 命中 |
|---|---|
| ① 循环体末尾无条件 `break` 且不使用循环变量（假循环） | 1 处，**误报**：`main.cpp` 的 range-for 里 `break` 是"找到可用翻译就停"，且变量确实被使用 |
| ② 同一作用域内 `new` 之后很快 `delete` 同一变量（白建后删） | 1 处，**有意为之**：`AdbClient` 里 `QMetaObject::Connection` 的"作用域连接"惯用法，旁边就有注释解释为何不能用智能指针（会形成环） |
| ③ 循环体不使用循环变量（N 次做同一件事） | **0 处** |

③ 的零命中是有意义的：第 33 轮在 `applyDeviceList` 里修掉的那类问题（"有几台设备就重建几次"）
**在当前代码里已不存在**——说明当时的修复是收敛的，而不是只补了那一个点。

### 41.2 真实问题：别名写盘失败，界面却说"已保存"

顺着"边界审计"查到最后一块没看过的边界——**配置文件的写入**：

```cpp
QSettings settings(filePath(), QSettings::IniFormat);
settings.setValue(key, trimmed);
settings.sync();          // ← 从不检查 status()
```

`QSettings` 在目录只读、磁盘满、权限不足时会**静默失败**；调用方则无条件打印
"设备别名已保存"。用户看到"已保存"，重启后别名消失，且没有任何线索。

修法（属"修复漏洞/增强健壮性"，不动功能）：

```cpp
bool DeviceNames::setName(const QString &addr, const QString &name)
{
    ...
    settings.sync();
    // 写盘失败必须让调用方知道：否则界面显示"已保存"，而重启后别名消失了
    return settings.status() == QSettings::NoError;
}
```

调用方按结果分别报告（成功才说"已保存"，失败给 `qWarning` 并附上文件路径）。

### 41.3 新增 `--devicenames` 探针

顺带补上持久化层此前**没有直接测过**的路径（之前只用"直接写 INI 文件"的方式验证过 UI）：

```
写入返回=true  读回="我的网络设备"
allNames 条数=1  含该地址=是
删除返回=true  删除后=""
存储路径=/home/hall-of-stasis/.config/QtAdb/device-names.ini
全部符合预期
```

地址特意用 **`192.168.1.5:5555`**（网络设备的序列号带冒号）——这正是需要百分号编码的场景，
同时也覆盖了非 ASCII 别名与"空串=删除"的语义。

### 41.4 验证

| 场景 | 结果 |
|---|---|
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| **别名 UI 路径** | 顶部标签与下拉项名称字段均显示 "我的测试机" |
| 挂起 / 热插拔 | 42 ms / **1 → 2 → 0** |
| **四个探针** | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、**别名"全部符合预期"** |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 41.5 方法上的收获

1. **一次人工发现之后，立刻把它变成扫描规则**：能机械检查的坏味道不该靠"下次再读到"；
   本轮扫描的 0 命中同样是结论——它证明之前的修复没有留下同类残留。
2. **扫描结果要逐条判读**：两条命中都是"看起来像、实际是对的"，误报必须解释清楚而不是删掉输出。
3. **"写操作不检查结果"是一类系统性缺陷**：文件、设置、进程退出码都算。凡是"写完就当成功"
   的地方，都值得问一句"失败时会发生什么、用户会看到什么"。

---

---

## 四十二、"结果不检查"全局排查：字体族名与配置目录（第三十六轮）

上一轮把"写操作不检查结果"总结成一条经验（§41.5）。本轮按这条线索做**全工程排查**，
把"调用之后返回值被丢掉"的地方逐类列出来判读。

### 42.1 逐类排查结果

| 类别 | 命中 | 判读 |
|---|---|---|
| `AdbClient` 回调忽略退出码 | **1 处**（`sp_activator::on_runBtn_clicked`） | **无害**：它只把 `output + errorOutput` 转发给输出窗口，并不声称成功；错误文本照样显示 |
| 文件写入（`QIODevice::WriteOnly` / `QFile::write` / `QSaveFile`） | **0 处** | 全工程**没有任何文件写入**（配置只走 `QSettings`，已在 §四十一 处理） |
| `QProcess` | 仅 `AdbClient` | 已连接 `finished` 与 `errorOccurred`，`FailedToStart` 有专门处理 |
| `startDetached` | 0 处 | — |
| `QDesktopServices::openUrl` | 6 处（关于页链接） | 返回值可忽略（没有浏览器时什么也不发生）；价值低，仅记录 |
| `QFontDatabase::addApplicationFont` | 1 处 | **修**（见 42.2） |
| `QDir::mkpath` | 1 处 | **修**（见 42.3） |

### 42.2 内置字体：忽略加载结果 + 硬编码族名

```cpp
QFontDatabase::addApplicationFont(":/LXGW/LXGWWenKaiLite-Regular.ttf");   // ← 返回值丢掉
...
a.setFont(QFont("LXGW WenKai Lite"));                                    // ← 族名硬编码
```

一旦资源改名、或字体内部族名与硬编码不一致，程序就会去用一个**不存在的字体族**
（Qt 静默回退到系统字体，用户只会觉得"字变了"，没有任何提示）。

改为使用 `addApplicationFont()` 返回的索引取真实族名，加载失败时给出警告：

```cpp
const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/LXGW/LXGWWenKaiLite-Regular.ttf"));
const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
if(!fontFamilies.isEmpty()) { a.setFont(QFont(fontFamilies.first())); }
else                        { qWarning() << "内置字体加载失败，将使用系统默认字体"; }
```

**验证行为等价**：在验证驱动里按同样方式加载并打印族名（顺便让 harness 更接近真实程序）：

```
内置字体: id=0 族名="LXGW WenKai Lite"
```

即真实族名与原来的硬编码**一致**——改动不改变外观，但不再脆弱。

### 42.3 配置目录：`mkpath` 失败无人知

`DeviceNames::filePath()` 里 `QDir().mkpath(dir)` 的返回值被丢掉。上一轮给 `setName()`
加了 `status()` 检查，所以**最终**能知道写失败，但用户只看到"保存失败"而不知原因。
现在目录建不出来时就先给出一条明确提示。

### 42.4 验证

| 场景 | 结果 |
|---|---|
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50%；无"配置目录"告警 |
| 挂起 / 热插拔 / 聚焦测试 | 45 ms / **1 → 2 → 0** / 退出码 0 |
| **四个探针** | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、别名"全部符合预期" |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 42.5 方法上的收获

把一条经验（"写操作要检查结果"）变成**一次全局排查**，成本很低而覆盖面很实：
本轮列出的 7 类里真正要改的只有 2 处，但"其余 5 类都没问题"这个结论同样是交付物——
它把"这里安不安全"从推测变成了已核对。

---

---

## 四十三、条目创建收成一处：40 个重复块 → 1 个助手（第三十七轮）

### 43.1 重复形态

`pageMaker::createPage_*` 里每个条目都是同一段固定组合：

```cpp
pageListItem *power = new pageListItem(devControl);
power->setText(tr("电源"), "adb reboot <n>");
power->setPic(":/ico/image/ico/shut-down-line.svg");
power->setSelectable();
devControl->addItemsToList(power);
```

全文件统计：**43 个条目块中有 38 个正是这个形状**（34 个带 `setSelectable()`、4 个不带）。
收成一个助手：

```cpp
// 返回条目指针，便于调用方追加自己的配置（例如异步回填时需要持有它）
pageListItem *pageMaker::addPageItem(basePage *page, const QString &title, const QString &profile,
                                     const QString &icon, bool selectable = true);
```

于是 5～6 行变成 1～2 行，**40 处**落地，`pagemaker.cpp` **615 → 501 行**（−114 行）。

### 43.2 先立"条目等价性"基线，再做机械替换

这次替换是脚本批量做的，所以先把**可观测内容**变成基线：给 `--factories` 探针加上
"每个主页面的条目文本"输出（3 种设备状态 × 7 个序号 = 55 个条目，打印每个条目的
标题、说明与所属页面）：

```
Online   key=1 页面=activator  条目=18  {黑阈|adb -d shell sh |…} {冰箱 - 设备管理员|adb shell dpm se|…} …
Online   key=5 页面=advanced   条目= 6  {运行自定义命令|<customize comma|…} {账户|adb shell dumpsy|…} …
Recovery key=0 页面=devInfo    条目= 3  {设备型号 | DeviceMod|读取中…|…} {Recovery 版本|读取中…|…} …
```

替换后重跑：**逐字相同** ✓（55 个条目的标题、说明、数量、页面归属全一致）。

### 43.3 过程中的三个坎（都记下来）

1. **一个巨型正则写错就全盘不执行**：第一次用单条正则匹配条目块，`re.compile` 报
   "bad character in group name" 直接抛异常——而异常发生在**写文件之前**，于是
   `pagemaker.cpp` 没被改、头文件却已经声明了新函数，编译能过、**链接失败**
   （`undefined reference to pageMaker::addPageItem`）。教训：脚本化改动要**先写实现再改调用点**，
   并且每步都检查"文件到底写没写"。
   改用**逐行状态机**（识别 `new pageListItem(...)` → 依次读 `setText/setPic/[setSelectable]/addItemsToList`）后一次通过。
2. **机械替换必须再过一遍语义**：替换后有两处编译失败——`android_version` 与 `cpu` 两个条目
   **在加入列表之后还会被异步回填**（`QPointer<pageListItem> itemPtr(android_version)`），
   原来的变量名还有用。这两处改为 `pageListItem *android_version = addPageItem(...);` 保留变量。
   **形状相同的代码不等于语义相同**：替换脚本要能容纳"我还需要这个对象"的情况，
   这也正是助手**返回指针**而不是返回 void 的原因。
3. **重复替换的正则要用状态机而不是"贪心一把梭"**：条目参数里可能含逗号与嵌套括号
   （`tr("…")`、`AdbCmd::shell({...})`），按顶层逗号切分需要真正的括号计数。

### 43.4 验证

| 场景 | 结果 |
|---|---|
| **条目等价性**（`--factories` 的条目输出） | 55 个条目逐字相同 |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 热插拔 / 聚焦测试 | 44 ms / **1 → 2 → 0** / 退出码 0 |
| **四个探针** | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、别名"全部符合预期" |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 43.5 方法上的收获

**"先有观测，再动刀"**：这次替换之所以敢批量做，是因为先把条目文本变成了可对比的基线。
没有这个基线，40 处机械替换只能靠"看起来对"来交付；有了它，等价性是一行 `diff` 的事。
另外，**能观测的内容要选"用户看得见的那部分"**（标题与说明），而不是内部字段——
它同时覆盖了"参数传错"和"顺序错位"两类错误。

---

---

## 四十四、异步信息条目收成 `addInfoItem()`（第三十八轮）

### 44.1 重复形态

`createPage_devInfo` 里每个设备信息条目都是这 9 行：

```cpp
pageListItemStruct *dpi = initStruct(devInfo);
dpi->item->setText_title(tr("屏幕密度 | DPI"));
dpi->item->setPic(":/ico/image/ico/grid-line.svg");
dpi->thread->initThread(AdbCmd::wmDensity(), dev);
QEventLoop::connect(dpi->thread,SIGNAL(signal_output(QString)),dpi->item,SLOT(slot_setText_profile(QString)));
dpi->thread->start();
dpi->item->setSelectable();
devInfo->addItemsToList(dpi->item);
delete dpi;      // 结构体本身不再需要（item/thread 已各有宿主）
```

6 次（型号 / 分辨率 / DPI / Android Id / 安卓版本 / CPU）→ 收成一个助手：

```cpp
void pageMaker::addInfoItem(basePage *page, device dev, const QString &title, const QString &icon,
                            const QStringList &arguments, const QString &explainKey = QString(),
                            bool selectable = false);
```

`createPage_devInfo` **117 → 69 行**，`pagemaker.cpp` **546 → 505 行**（连同上一轮的成果，
本文件从 615 行降到 505 行）。

### 44.2 把等价性基线做得更严：让异步回填先落地

上一轮的条目基线有个盲点：探针建完页面**立刻**取文本，此时异步查询还没回来，
所有说明都还是"读取中…"——**恰好覆盖不到本轮要改的"回调接线"**。

于是先给探针加一句：建完页面后**跑 250ms 事件循环**再取条目。基线随即变成真实回填值：

```
Online key=0 页面=devInfo 条目=6 {设备型号 | DeviceMod|[ro.product.mode|…} {安卓版本|13|…} …
Recovery key=0 页面=devInfo 条目=3 {Recovery 版本|13|…} …
```

重构后重跑：**逐字相同** ✓ —— 这同时证明了"标题/图标/参数/连接/顺序"都没变。

### 44.3 过程中的三次返工（都记下来）

1. **又一次"实现没落盘"**：脚本里变量名写错（`p` 应为 `c`），异常发生在写文件之前，
   于是只剩头文件里的声明——**编译照样通过**（未使用的声明合法），要到链接才炸。
   这次改完立刻用 `grep -c 'pageMaker::addInfoItem'` 确认落盘 ✓。
   教训再次确认：**脚本改完必须回读文件**，不能只看"脚本没报错"。
2. **参数落位错**：我按"末尾追加 `, true`"处理可点选标志，但助手签名里 `selectable` 在
   `explainKey` **之后**，于是有一处 `true` 落到了字符串参数的位置（编译期即报错，
   这正是"类型不同"带来的好处）。修法是显式补 `QString()` 占位。
3. **"形状相同"再次不等于"完全一样"**：第一个条目（设备型号）中间夹了一行注释，
   状态机没匹配上，于是 6 个块只换了 5 个。**替换数量要核对**（"应该是 6，实际 5"），
   剩下那个手工改掉。

### 44.4 验证

| 场景 | 结果 |
|---|---|
| **条目等价性（含异步回填值）** | 55 个条目逐字相同，两次校验都通过 |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 热插拔 / 聚焦测试 | 45 ms / **1 → 2 → 0** / 退出码 0 |
| **四个探针** | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、别名"全部符合预期" |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 44.5 方法上的收获

**等价性基线要覆盖"被改的那条路径"**：上一轮的基线只看静态文本，本轮要改的是异步回调接线，
所以必须先让回填落地再取文本——否则"完全一致"是假象（两边都是"读取中…"）。
**基线的强度要跟着改动走**，这是本轮最值钱的一条经验。

---

---

## 四十五、回调路径同样收束，并用关级审计证明命令行不变（第三十九轮）

### 45.1 同一个模式，另一条路径

`createPage_devInfo_rec`（Recovery 状态的设备信息页）里是**同样的需求、不同的实现路径**：
先填"读取中…"占位，再用 `AdbClient` 回调回填。三个块各 15 行左右，还夹着一段 18 行的
注释掉的死代码。收成一个助手（比上一轮的版本多一个**可选转换函数**，因为 CPU 那项要解析
`/proc/cpuinfo`）：

```cpp
void pageMaker::addInfoItemAsync(basePage *page, device dev, const QString &title, const QString &icon,
                                 const QStringList &arguments, bool selectable = false,
                                 std::function<QString(const QString &)> transform = {});
```

| 函数/文件 | 变化 |
|---|---|
| `createPage_devInfo_rec` | **91 → 36 行** |
| `pagemaker.cpp` | **505 → 478 行**（原始 615 行） |
| 注释掉的死代码 | 删掉 18 行 |

### 45.2 顺手统一：手写的 `-s` 前缀 → 规范的设备接口

原实现是 `runWithCallback({"-s", dev.addr, "shell", …})` —— 手工拼设备前缀。
助手内部改用 `runForDeviceWithCallback(dev.addr, …)`（ADB 层的规范入口，会自动前置 `-s`），
这样"设备相关命令必须带 `-s`"这件事只由 ADB 层负责，页面代码不必再记。

**但改了调用方式就得证明命令行没变**——于是跑关级审计（把打桩收到的 argv 逐参数导出来看）：

```
无 -s 前缀的命令:            （空）
cpuinfo/getprop 相关: -s|ABC123|shell|cat|/proc/cpuinfo|
                      -s|ABC123|shell|getprop|ro.build.version.release|
                      -s|ABC123|shell|getprop|ro.product.model|
```

即：改写前后**下发给 adb 的 argv 完全相同**。这也正好复用了第 27 轮建立的能力。

### 45.3 等价性验证

上一轮把探针加强成"建完页面等 250ms 再取条目"，而本轮的 `devInfo_rec` 恰好是
**Recovery 状态**下的 `devInfo` 页——基线里本来就有这三行：

```
Recovery key=0 页面=devInfo 条目=3 {设备型号 | DeviceMod|[ro.product.mode|…} {Recovery 版本|13|…} {处理器 | CPU|…}
```

重构后重跑：**逐字相同** ✓（含异步回填后的值）。也就是说，这次的等价网是**上一轮顺手加严的**，
本轮直接受益——又一次说明"基线强度要提前做够"。

### 45.4 验证

| 场景 | 结果 |
|---|---|
| **条目等价性** | 55 个条目逐字相同（含异步回填值） |
| **关级审计** | 三条命令的 argv 与改写前完全一致，且无命令缺少 `-s` |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 热插拔 / 聚焦测试 | 45 ms / **1 → 2 → 0** / 退出码 0 |
| **四个探针** | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、别名"全部符合预期" |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 45.5 方法上的收获

**改调用方式 ≠ 改行为，但必须证明**。把"手写 `-s`"换成规范接口，逻辑上等价；
可只要动的是"发给外部进程的东西"，就该用关级审计给出**命令行级**的证据，
而不是停在"我觉得等价"。本轮的证据是一条 `diff` 就能看懂的 argv 清单。

---

---

## 四十六、"允许打开的子页"不再手写数字（第四十轮）

### 46.1 隐藏的维护陷阱

`pageMaker` 里给页面设定"哪些子页可以打开"是这样写的：

```cpp
activator->setEnableValue({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18});
...
addPageItem(activator, tr("黑阈"), …);     // 18 个条目
```

数字集合与条目列表**必须手工保持一致**：新增一个激活器却忘了把 19 加进去，
结果就是"新条目点不开"，而且没有任何报错。全工程共 6 处（activator 18、apps 7、
advanced 6、recovery 1、devControl 两处 2 与 1）。

### 46.2 改法：按实际条目数生成

```cpp
void basePage::enableAllItems()
{
    QList<int> val;
    const int count = ui->listWidget->count();   // 已加入的条目数
    for(int i = 1; i <= count; ++i)
    {
        val.append(i);
    }
    setEnableValue(val);
}
```

调用点从"一串数字"变成一行 `page->enableAllItems();`（放在条目都加完之后）。
**子集语义仍然显式保留**：`devInfo` 只允许 `{2,3}`（分辨率/DPI）、`disabled` 页允许空集 ——
这两处不动，避免把"故意只开两个"也变成"全开"。

### 46.3 让"允许集合"变成可观测的量

`isEnable(int)` 原本是 private，只能靠"点一下试试"来间接判断。把它公开（纯查询，无副作用），
探针就能直接导出**每页允许的子页集合**：

```
Online   key=1 页面=activator  条目=18 允许子页=[0..17]
Online   key=2 页面=apps       条目= 7 允许子页=[0..6]
Online   key=0 页面=devInfo    条目= 6 允许子页=[1,2]      ← 子集，保持显式
Recovery key=1 页面=disabled   条目= 1 允许子页=[]          ← 空集，保持显式
```

改完重跑：**21 行的允许集合逐个相同** ✓，条目文本也一字不差 ✓。

### 46.4 关于"行数"的诚实说明

这一轮**不是为了减行数**：删掉 6 处数字列表、但插入 6 处调用，`pagemaker.cpp` 仍是 **478 行**
（与上轮相同）。收益是**消除了一个必须手工同步的隐性约束**——按本轮目标里"可拓展"的要求，
"新增一个条目不用再改别处"比少几行更重要。

### 46.5 验证

| 场景 | 结果 |
|---|---|
| **允许集合等价性** | 21 行（3 状态 × 7 序号）逐个相同 |
| **条目等价性** | 55 个条目逐字相同（含异步回填值） |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 四个探针 | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、别名"全部符合预期" |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 46.6 方法上的收获

**"两处必须手工保持一致"就是缺陷的温床**。这类约束不会报错，只会在某次忘记同步时
变成"功能莫名其妙不工作"。发现它的办法是把隐式约束**变成可观测的量**（这里是"每页允许哪些子页"），
先用探针量出来、再改成语义等价但无需同步的形式，最后用同一张表证明没变。

---

---

## 四十七、把"条目顺序 = 子页序号"变成断言（第四十一轮）

### 47.1 又一条隐式约束

`pageMaker::createPage_apps` 里按顺序加了 7 个条目（安装应用 / 已知权限组 / 已知权限 /
软件包 / 系统功能 / 库 / 用户）；而 `sonPageManager::createSonPageFor_apps` 里
`key` 0…6 分别对应 sp_installer / sp_permissionGroups / sp_permissions / sp_packages /
sp_features / sp_libraries / sp_users。

**两者的对应完全靠"顺序一致"这个默契**：谁把两条 `addPageItem` 调换一下，
界面就会"点 A 打开 B"，编译、运行都不会报错。全工程共 36 组这样的配对
（devInfo 2 组、activator 18 组、apps 7 组、devControl 2 组、recovery 1 组、advanced 6 组）。

### 47.2 先让它可见，再让它可断言

第一步只是把配对**打印出来**——每行是"条目标题 → 子页类名"。第一次跑就发现
我原来的启发式判断（拿条目标题去子页里找同名标签）误报 14 处：例如 `devInfo[0]`
本来就返回 nullptr（该页只允许 1、2 号子页），`电源 → 关机`、`软件包 → 应用`
只是**措辞不同**、配对本身完全正确。

于是第二步改成**期望表**（36 行），对每组同时断言两件事：

```cpp
struct Expect { const char *parent; int key; const char *item; const char *sonClass; };
...
const bool titleOk = itemTitle.contains(QString::fromUtf8(e.item));   // 第 i 个条目确实是它
const bool classOk = (sonClass == QString::fromLatin1(e.sonClass));    // 第 i 个子页确实是它
```

结果：**36 组配对，不符 0 项** ✓。这既是"当前没有问题"的证据，也是**回归防线**：
今后谁调整了条目顺序，`--factories` 会立刻报出"第 i 组对不上"。

### 47.3 顺手把约定写进代码

在 `sonPageManager::selector` 与 `pageMaker::addPageItem` 的说明里各写一段：

```
⚠ 隐式约定：主页面里条目的先后顺序就是子页序号（第 i 个条目 ↔ 第 i 个子页）。
   调整条目顺序必须同步改这里的表，否则会"点 A 打开 B"且不报错。
   自测：QtAdbAsan --factories 的"条目↔子页 交叉校验"会逐条核对（36 组）。
```

### 47.4 验证

| 场景 | 结果 |
|---|---|
| **配对断言（新）** | 36 组配对，不符 **0** 项 |
| 允许集合等价性（上轮建立） | 21 行逐个相同 |
| 条目等价性（上轮建立） | 55 个条目（含异步回填值）逐字相同 |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 热插拔 / 聚焦测试 | 46 ms / **1 → 2 → 0** / 退出码 0 |
| 探针（五个）+ 全新构建 | 全部符合预期；**0 错误 0 警告** |

### 47.5 方法上的收获

**"隐式约束"的三步处理**：① 让它**可见**（把配对打印成表）；② 让它**可断言**
（写成期望表，标题与类名双重比对）；③ 让它**有据可查**（在代码里写下约定并指向自测）。
第 40 轮的"手写数字"与本轮的"顺序默契"是同一类问题——**凡是"两处必须保持一致"，
就应该能被一条命令验证**。另外，第一版启发式判断的 14 处误报也提醒：**校验规则本身也要判读**，
不能看到"对不上"就断言代码有问题。

---

---

## 四十八、两份构建文件的源文件列表：从"手工对齐"到"一条命令"（第四十二轮）

### 48.1 问题

本项目的 CMake 与 qmake 两份构建文件各自维护一份源文件列表，**一直靠手工同步**。
本轮做了一次机械比对（把两份列表与磁盘实际文件三方对齐），查出两处真实偏差：

| 位置 | 问题 |
|---|---|
| `QtAdb.pro` | **少列 `adbcommands.h`**（第 21 轮新增的命令层，只补进了 CMake） |
| `CMakeLists.txt` | 注释里用**已删除的 `adbprocess.h`** 当"扁平包含风格"的例子（第 10 轮删了它） |

另外两处是脚本误报，记下来避免误改：`logo.rc` 在 `.pro` 里写作 `win32: RC_FILE += logo.rc`
（带平台前缀，正则没匹配）；`qcompilerdetection.h` 出现在解释"Qt 要求 C++17"的注释里。

### 48.2 修法与约定

- `.pro` 与 `CMakeLists.txt` 都补上 `adbcommands.h`，CMake 注释里的例子换成现存文件；
- 在两份文件里各写一段约定：

```
⚠ 隐式约定：这里的源文件列表与 QtAdb.pro 的 SOURCES/HEADERS 必须保持一致
   （新增或删除源文件时两份都要改）。自测：校验脚本会比对两份列表与磁盘上的实际文件。
```

### 48.3 把约定变成一条命令（并验证这条命令本身）

新增 `.dsh/probe/check_build_sync.py`：剥掉注释后抽取两份列表，与磁盘上的
`.cpp/.h/.ui/.qrc/.rc` 做三方比对，两个方向都查——**少列**（列表漏了磁盘上的文件）
与**失效引用**（列表里有磁盘上不存在的文件）：

```
磁盘源文件 128 个；CMake 列出 128 个；.pro 列出 128 个
两份列表与磁盘一致 ✓
```

**校验器本身也做了自测**：临时把 `.pro` 里的 `adbcommands.h` 抽掉，它精确报出
`★ .pro 未列出磁盘上的文件: adbcommands.h` 并以退出码 1 结束；恢复后通过。
（"一个永远通过的校验器等于没有校验器"，这一步不能省。）

### 48.4 验证

| 场景 | 结果 |
|---|---|
| **构建文件一致性（新）** | 两份列表与磁盘一致 ✓；自测能检出人为制造的缺失 |
| 配对断言（上轮建立） | 36 组，不符 **0** 项 |
| 允许集合 / 条目等价性 | 21 行、55 个条目均一致 |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 热插拔 / 聚焦测试 | 45 ms / **1 → 2 → 0** / 退出码 0 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 48.5 方法上的收获

**"两处必须一致"的清单还能继续挖**：上一轮是"条目顺序 ↔ 子页序号"，这一轮是
"两份构建文件的源文件列表"，两者都被一条命令变成了可验证项。这类约束的共同特征是
**改动时不会有任何提示**——只能靠事前的机械比对发现。

另外一条：**校验器要自测**。临时制造一个缺陷、确认它报错、再恢复，
这三步成本极低；否则无法区分"没有问题"与"检查没生效"。

---

---

## 四十九、解释键的封闭集合：让"传错键"不再静默（第四十三轮）

### 49.1 现状

页面把命令交给 `adbThread` 时会带一个"解释键"，由 `adbThread::deliver()` 分派：

```cpp
if(explain == false)                                   emit signal_output(output.simplified());
else if(explainKey == "#CPU#")                          emit signal_output(m_explainer.explain_cpu_output(output));
else if(explainKey == "#PERMISSIONGROUPS#")             emit signal_output(output);
else if(explainKey == "#INSTALL#" || "#SIDELOAD#")      emit signal_output(output);
else if(explain == true)                                emit signal_output(m_explainer.get_words_after(output, explainKey));
```

即键的集合是**封闭**的四种，而最后一条是"把键当**分隔符**用"的兜底
（设备信息页的屏幕分辨率就传 `": "`，表示取分隔符之后的内容）。

问题在于：**键写错时会走兜底分支**——于是"拼错的键"与"合法的分隔符用法"完全无法区分，
表现为"这个页面的输出莫名其妙没被解释"，没有任何提示。

### 49.2 改法：登记检查 + 把集合写进声明

兜底分支加一句检查（`#XXX#` 形式却不在四种之内 → 警告）：

```cpp
if(explainKey.startsWith(QLatin1Char('#')) && explainKey.endsWith(QLatin1Char('#'))
   && explainKey.size() > 2)
{
    qWarning() << "未登记的解释键（该输出不会被解释）:" << explainKey;
}
```

同时在 `adbThread.h` 的 `initThread` 声明处写明这个封闭集合与"分隔符"用法。

### 49.3 验证：警告要"该响时响、不该响时不响"

给已有的安装/侧载探针加了两个用例——**一个拼错的键**（`#INSTALLL#`）与**一个合法的分隔符**（`": "`）：

```
未登记键的警告: 1 条
  → 未登记的解释键（该输出不会被解释）: "#INSTALLL#"
非 #INSTALLL# 的警告: 0 （没有误报）
```

更有意义的是**整机运行的结果**：正常场景（逐页遍历 30 个子页、7 个主页面、
设备信息页的异步查询、安装/侧载路径）跑下来 **0 条未登记键警告** ——
这说明现有调用点与分派逻辑**完全一致**，不是靠"大概没写错"。

### 49.4 验证

| 场景 | 结果 |
|---|---|
| **解释键检查（新）** | 拼错的键 1 条警告；合法键/分隔符 0 误报；**正常运行 0 条** |
| 构建文件一致性（上轮建立） | 两份列表与磁盘一致 ✓ |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；监视 CPU 100% / RAM 50% |
| 挂起 / 热插拔 / 聚焦测试 | 45 ms / **1 → 2 → 0** / 退出码 0 |
| 探针（五个）+ 全新构建 | 全部符合预期；**0 错误 0 警告** |

### 49.5 方法上的收获

**参数集封闭却不做检查，就会把"写错"变成"没效果"**。这类接口的兜底分支往往很宽容
（这里把任何未知键都当分隔符），宽容本身没错，但应当**能区分"我认识这个值"与"我不认识"**。
另外一次小插曲：我第一次用 `grep '未登记的解释键.*: '` 判"误报"，结果把警告文本里的
冒号也算进去了——**校验脚本本身也会误报，结论要按语义核对**（这条已经在 §四十七 记过一次）。

---

---

## 五十、按数据挑目标：全工程函数长度排行，最长者表化（第四十四轮）

### 50.1 用数据挑，而不是凭印象

先把全工程 350 个函数按长度排了一次序（中位数 6 行，超过 60 行的只有 16 个）：

```
   102 行  textexplainer.cpp:textExplainer::explainError
   100 行  sonPages/advanced/sp_vibrate.cpp:sp_vibrate::refresh
    97 行  sonPages/sonpagemanager.cpp:sonPageManager::createSonPageFor_activator
    80 行  about.cpp:about::about
    77 行  sonPages/devInfo/sp_wmsize.cpp:sp_wmsize::sp_wmsize
    ...
```

榜首是 `explainError` —— 它同时还是**最高频**的一段（任何命令失败都会走），
而且是 22 个分支的长 if/else 链，典型的"数据被写成了控制流"。

### 50.2 改法：数据表 + 一次遍历

```cpp
struct ErrorHint
{
    const char *title;        // QT_TR_NOOP，运行时再 tr()
    const char *message;      // nullptr 表示"匹配到但不提示"（原实现的空分支）
    bool twoPatterns;         // 为真时 a、b 必须同时出现
    const char *a;
    const char *b;
};
const ErrorHint kErrorHints[] = { ... 22 条 ... };

bool textExplainer::explainError(QString err)
{
    err = err.simplified();
    for(const ErrorHint &hint : kErrorHints) { /* 命中即 showMsgBox 并 return */ }
    showMsgBox(tr("错误"), tr(" · 未知错误！\n") + err + tr("…"));   // 原实现的最后一条 else
    return true;
}
```

`explainError` **102 → 25 行**；新增一条报错提示从"插一段 if/else"变成"加一行数据"，
而且**顺序即优先级**这件事在表里一眼可见（原来要靠读完整条链才知道）。

### 50.3 等价性：抽取 → 生成 → 再抽取

这次不手抄任何内容：脚本先从**旧代码**里解析出 22 条分支（单个/两个子串、标题、正文、
是否空分支）与最后一条 else，落盘；再用同一份解析结果**生成**新表；
最后从**新代码**里再抽取一遍，与落盘的结果逐条比对：

```
✓ 22 个分支 + 兜底逐字一致（条件顺序、子串、标题、正文、空分支全部相同）
```

也就是说，"搬移"这一步没有任何人眼参与的转录风险；人只负责设计与判读。

### 50.4 验证

| 场景 | 结果 |
|---|---|
| **分支等价性** | 22 条 + 兜底逐字一致 |
| 正常运行 | 30 子页、0 sanitizer/断言；**自动关闭模态对话框 72 个**（与重构前同数） |
| 畸形输出（专门触发报错提示） | 30 子页、0 断言；模态对话框 72 个 |
| 挂起 / 热插拔 | 45 ms / **1 → 2 → 0** |
| 探针（五个）+ 构建一致性 | 全部符合预期 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 50.5 方法上的收获

1. **按数据挑目标**：与其凭印象"这个函数好像很长"，不如把 350 个函数排一次序，
   再结合"被调用的频度"决定先动谁。本轮榜首同时满足"最长"与"最高频"。
2. **机械搬移 + 双向抽取比对**：内容由脚本搬运、由脚本核对，人只看结果。
   这比"仔细地手抄一遍"可靠得多 —— 第 38 轮那次"6 个块只换了 5 个"就是手工遗漏的教训。
3. 顺带确认了一个小事实：`explainError` 的返回值**恒为 true**（原实现也如此），
   也就是说这个 `bool` 没有任何判别作用 —— 留着是为了不改变调用方，已在文档记录。

---

---

## 五十一、排行第二：`sp_vibrate::refresh` 的 10 个重复请求（第四十五轮）

### 51.1 形态

`sp_vibrate::refresh()` 里 10 个块完全同构，**连注释都重复了 10 遍**：

```cpp
adbThread *thread_all = new adbThread(this);
thread_all->initThread(AdbCmd::settingsGet("system", "vibrate_on"), dev);
QEventLoop::connect(thread_all,SIGNAL(signal_output(QString)),this,SLOT(setText_all(QString)));
// 线程结束后自行释放：原实现每次刷新都新建线程并一直累积到页面销毁
connect(thread_all,SIGNAL(finished()),thread_all,SLOT(deleteLater()));
thread_all->start();
```

区别只有两处：**settings 键名**与**回填的目标槽**。

### 51.2 改法

```cpp
// 起一个"读某个 system settings 键并把结果回填到指定槽"的临时线程。
// 槽用成员函数指针传，类型安全（不再拼 SLOT 字符串）。
void sp_vibrate::requestSetting(const QString &systemKey, void (sp_vibrate::*slot)(QString))
{
    adbThread *thread = new adbThread(this);
    thread->initThread(AdbCmd::settingsGet("system", systemKey), dev);
    connect(thread, &adbThread::signal_output, this, slot);
    connect(thread, &adbThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
```

`refresh()` 从 100 行变成 **15 行**（10 行调用 + 头尾），文件 321 → 247 行。

### 51.3 先把探针补到"能覆盖它"，再动手

`sp_vibrate` **没有刷新按钮**，所以之前的重复刷新探针把它标成"未触发" —— 也就是
**这次要改的代码根本不在覆盖范围内**。于是先扩展探针：没有刷新按钮的页面改用
重复 `setDev()` 触发（那是它们唯一的"重新取数"入口）。基线随即产出：

```
sp_vibrate  (setDev×6)  对象 117 → 107  (控件 52 / 模型 0 / 定时器 0)
sp_scales   (setDev×6)  对象  44 →  41
其余各页                 稳定
```

对象数**减少**说明临时线程确实随 `finished()` 自清理（这正是第 22 轮修的那个泄漏），
也说明这里没有累积问题 —— 本轮要做的只是可读性。

### 51.4 验证

| 场景 | 结果 |
|---|---|
| **配对等价性**（抽取→生成→再抽取） | 10 个（键 → 槽）配对与顺序完全一致 |
| **行为复测**（重复 `setDev` ×6） | 对象数 117 → 107，与基线**完全相同**；其余页也一致 |
| 正常 / 逐页 | 30 子页、0 sanitizer/断言、模态对话框 72 个 |
| 挂起 / 热插拔 | 45 ms / **1 → 2 → 0** |
| 探针（五个）+ 构建一致性 | 全部符合预期 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 51.5 方法上的收获

1. **先检查"待改的代码在不在覆盖范围内"**：这次探针原本没覆盖 `sp_vibrate`，
   如果直接改，就只能靠"看起来对"。补上覆盖、拿到基线，再动手。
2. **对象数"减少"也要解释**：探针原来把"不等于初始值"一律标成"有增长"，
   而这里其实是延迟回收。**指标的方向性要判读**，标签也要写准（本轮顺手把措辞改成"有变化"）。

---

---

## 五十二、重复块扫描工具 + 工厂表压成一行（第四十六轮）

### 52.1 先做工具：函数内"三连语句"重复度扫描

上一轮的发现是手工读出来的。本轮把它做成扫描：把每个函数的语句序列切成"三连"，
统计同一函数内出现次数 ≥3 的组合。全工程 350 个函数里只命中 **3 处**：

| 位置 | 重复的三连 | 判读 |
|---|---|---|
| `sonPageManager::createSonPageFor_apps` | `page->setDev(dev); return page; } },` | 表里 7 行 lambda 的**固定收尾** |
| `sonPageManager::createSonPageFor_advanced` | 同上 | 表里 5 行 lambda 的固定收尾 |
| `sp_permissions::setArg` | `arg_f = false; arg_s = false; arg_d = false;` | 5 个布尔开关的重复重置（见 §52.4） |

前两处指向同一个结论：**工厂表的每一行还在抄同一段样板**。

### 52.2 改法：两个模板工厂

```cpp
// 表里每一行都只是"new + 可选 setDev"，于是把两种形态各收成一个模板：
template <typename T> animationWidget *makeSonPageWithDev(QWidget *parent, device dev)
{ T *page = new T(parent); page->setDev(dev); return page; }
template <typename T> animationWidget *makeSonPage(QWidget *parent, device dev)
{ Q_UNUSED(dev); return new T(parent); }
```

表项从 3 行变成 1 行：

```cpp
static const QHash<int, SonPageFactory> pages = {
    {  0, makeSonPageWithDev<sp_installer> },
    {  1, makeSonPageWithDev<sp_permissionGroups> },
    ...
};
```

`sonpagemanager.cpp` **298 → 248 行**（本轮起点；该文件从第 26 轮重构前的 356 行一路降下来）。

### 52.3 一个差点被抹平的差异

`advanced` 的**第一项**（`sp_customize_cmd`）那行 `setDev` 是**注释掉的** —— 也就是它
**不应该**调用 `setDev`。我的第一版抽取脚本用 `'page->setDev(dev);' in blk` 判断，
把注释里的也算成了调用（18 行全判成"setDev"）✗。

修正办法是**先剥注释再判断**，于是分类变成 17 + 1；两个模板工厂正好对应这两种形态，
且脚本在写入前**逐行比对新旧（键、类名、是否 setDev）**，不一致就整体不写。

验证这一点的行为证据在**关级审计**里：`setDev` 会让页面去取数，所以
`pm list packages`、`pm list users`、`dumpsys account`、`settings get … vibrate_on`
这些命令在页面创建后依然各出现 1 次 —— 说明该调 `setDev` 的照旧调、不该调的没被顺手调上。

### 52.4 顺带记录：`sp_permissions::setArg` 的开关重置

扫描命中的第三处是 5 个布尔开关（`arg_g/arg_f/arg_s/arg_d/arg_u`）在每个分支里重复重置。
它没有被本轮改动：改成枚举/小结构体更彻底，但会牵动 `refresh_tableView` 的分派与
若干成员，属于下一批候选（已记入待办），本轮不做半途而废的改动。

### 52.5 验证

| 场景 | 结果 |
|---|---|
| **逐行等价性**（脚本自带比对） | 18 行的（键、类名、是否 setDev）与基线一致 |
| **主页面条目**（对比第 40 轮基线） | 55 个条目逐字相同 |
| **配对断言** | 36 组配对，不符 **0** 项 |
| **关级审计** | 各页取数命令仍各出现 1 次 → `setDev` 语义未被改变 |
| 正常 / 逐页 / 重复刷新 18 页 | 30 子页、0 sanitizer/断言、对话框 72、监视 100%/50% |
| 挂起 / 热插拔 | 45 ms / **1 → 2 → 0** |
| 五个探针 + 构建一致性 + 全新构建 | 全部通过；**0 错误 0 警告** |

### 52.6 方法上的收获

1. **把"这次手工发现"沉淀成"下次自动发现"**：扫描工具只命中 3 处，但它把"还有没有类似重复"
   这个问题从"再读一遍代码"变成了"跑一下"。
2. **注释会被判成代码**：本轮抽取脚本第一版就因为没剥注释而把 18 行全判错。
   这类误判会让"机械替换"把**有意的差异**抹平（这里差一点就让一个不应该 `setDev` 的页面也调上），
   所以判断前先剥注释、并保留"带额外语句就不动它"的保守规则。
3. **脚本要自己证明没改坏**：本轮脚本内置"逐行比对新旧、不一致就整体不写"，
   加上外部的条目/配对/关级审计三重验证。

---

---

## 五十三、投影样板收成 `applyDropShadow()`（第四十七轮）

### 53.1 全工程最大的重复

每个页面（含主窗口、列表项、关于页）都在重复同一段四行：

```cpp
QGraphicsDropShadowEffect *e = new QGraphicsDropShadowEffect(this);
e->setOffset(0,0); e->setColor(Qt::gray); e->setBlurRadius(5);
wgt->setGraphicsEffect(e);
```

先做了一次形态统计（这是决定"能不能安全收"的前提）：

| 形态 | 处数 |
|---|---|
| 完整四行、`offset 0,0` + `Qt::gray` + `blurRadius 5` | **35** |
| 同上但颜色为 `QColorConstants::Svg::orange` / `skyblue` | 2 |
| 其余（缺行、被注释、参数特殊等） | 67 |

于是新增 `uishadow.h/.cpp`：

```cpp
QGraphicsDropShadowEffect *applyDropShadow(QWidget *widget, const QColor &color = Qt::gray);
```

阴影对象以**控件**为父对象（原实现以页面为父），随控件销毁；传空指针时不建对象。

### 53.2 本轮先替换 34 处，其余分批

脚本只替换"四行连续且参数为默认"的站点（37 处命中、34 处当前生效），
其余因为**行间夹着注释**而未被匹配 —— 这是刻意的保守：宁可少改，也不要猜。
未替换的 70 处原样保留（功能完全相同），已在验证输出里逐条列出，下一批再做。

### 53.3 等价性：以"原始文件"为准的独立校验

第一版校验脚本从 `git diff -U0` 里推断颜色，结果报了 2 处"颜色不一致"——
`-U0` 的 hunk 里**根本没有 setColor 那一行**，属于校验器自身的偏差。
改为以 `git show HEAD:<file>` 的**原始内容**为准、按【文件 + 控件表达式】配对后：

```
当前 applyDropShadow 34 处；原始 setGraphicsEffect 104 处
未替换的原始站点 70 处（逐条列出）
结论: 已替换的站点全部一致（控件与颜色）✓
```

### 53.4 过程中踩到的三个坑（都记下来）

1. **脚本把自己也改了**：`uishadow.cpp` 内部就是那四行，替换脚本顺手把自己的实现改成了
   自调用 → 编译报错。修法是恢复该文件，并让脚本排除它。
2. **替换到了注释里的控件**：`about.cpp` 有一行是被注释掉的站点，替换后变成
   `applyDropShadow(//ui->btn_money);`；同一处还留下一个**悬空的 `/*`**（原本就注释掉的两行
   被并入了同一个注释块）。两处都已按"还原注释 + 删掉死行"处理干净。
3. **新增源文件要重新 configure**：GLOB 收集的 harness 三个目标 + 显式列表的 `emu` 工程
   都因新文件而未参与编译（链接期才报错）。这正是 §0「验证纪律」里写过的坑，本轮又踩了一次
   —— 说明"新增文件后先 configure"应当写进脚本而不是靠记忆。

### 53.5 验证

| 场景 | 结果 |
|---|---|
| **替换等价性（新脚本）** | 已替换 34 处的控件与颜色全部与原始一致 |
| 构建文件一致性 | 两份列表与磁盘一致 ✓（新文件已登记） |
| 正常 / 逐页 | 30 子页、0 sanitizer/断言、模态对话框 73 个 |
| 畸形输出 / 挂起 / 热插拔 / 聚焦测试 | 30 子页 0 断言 / 45 ms / **1 → 2 → 0** / 退出码 0 |
| 探针 | 解析器、i18n、配对（**不符 0 项**）均通过 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 53.6 方法上的收获

**"能不能收"要先量形态，而不是先动手**：104 处里真正同形的只有 37 处，
其余各有差异（注释、缺行、特殊参数）。先统计再替换，才不会把有意的差异抹平 ——
这与 §52 的教训是同一个：**机械替换的边界必须由数据划出来**。

---

---

## 五十四、投影替换第二批：两次"工具在说谎"（第四十八轮）

### 54.1 换策略：从"四行连续"到"按变量"

第一批的模式要求声明与三行配置**紧邻**；剩下的站点大多是另一种写法 ——
**先把若干 effect 声明出来，再集中配置**：

```cpp
QGraphicsDropShadowEffect *e1 = new QGraphicsDropShadowEffect(this);
QGraphicsDropShadowEffect *e2 = new QGraphicsDropShadowEffect(this);

e1->setOffset(0,0); e1->setColor(Qt::gray); e1->setBlurRadius(5);
...
wgt1->setGraphicsEffect(e1);
wgt2->setGraphicsEffect(e2);
```

于是改成**按变量**处理：统计某个变量的全部引用，只有当它正好是
"1 处声明 + 3 处配置（参数为默认）+ N 处应用"、且不处于注释中、且没有其它引用时，
才把 N 处应用换成 `applyDropShadow(...)` 并删掉声明与配置；否则整体跳过并记录原因。

结果：**替换 64 处**（104 处原始站点），跳过的 40 处逐条给出原因
（涉及注释行 / 参数非默认 / 还有其它引用 / 变量没有应用点）。

### 54.2 两次"工具在说谎"

**第一次**：脚本报告"处理变量 47 个（替换应用点 47 处）"，但构建后调用数没变。
根因是脚本先把 `setGraphicsEffect` 那几行**从文本里删掉**、随后又去替换它们 ——
自然一处也匹配不到，于是"插入 applyDropShadow"的替换静默失败；而计数是**在替换之前**加一的。
更糟的是：文件因为"没出现 applyDropShadow"而**没被写入**，所以表面上什么都没发生。
修法：只删"声明 + 配置"，应用行**就地替换**；并且**按实际替换成功数**计数，
数量对不上就整条跳过。

**第二次**：补 `#include "uishadow.h"` 的步骤被我写在"有变量被处理"的分支里 ——
第二次运行时该分支不再进入，于是 7 个文件缺 include、构建失败。修法是把它拆成
**独立的幂等遍历**（与是否发生替换无关）。

### 54.3 校验器也换了三版

阴影的等价性只能靠"对比原始文件"来证明（效果本身不可观测）。三个版本依次修正：

| 版本 | 做法 | 问题 |
|---|---|---|
| v1 | 从 `git diff -U0` 配对 | hunk 里没有 `setColor` 行 → 颜色误判 2 处 |
| v2 | 从 diff 收集、按文件分组 | 只删不加的站点导致后续全部错位 |
| **v3** | **直读当前文件 vs `git show HEAD:<file>`**，按控件配对 | —— |

v3 的结论：**当前 64 处替换，控件与颜色全部与原始一致**（104 处原始站点）。

### 54.4 验证

| 场景 | 结果 |
|---|---|
| **替换等价性（v3 校验器）** | 64 处控件与颜色全部与原始一致 |
| 正常 / 畸形输出 | 30 子页、0 sanitizer/断言；对话框 72 个 |
| 挂起 / 热插拔 / 聚焦测试 | 44 ms / **1 → 2 → 0** / 退出码 0 |
| 探针（解析器/i18n/配对）+ 构建一致性 | 全部通过（配对"不符 0 项"） |
| 四个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 54.5 方法上的收获

**批量替换工具必须能证明自己真的改了东西**。本轮第一次失败的危害在于"报告成功"：
计数与写入是两套逻辑，计数先行、写入失败却无人知晓 —— 如果当时直接信了那句
"替换 47 处"，就会把一次空操作当成成果交付。规则很简单：
**计数只在替换确实发生后累加，任何一处对不上就整体不写**。
另外，"补 include"这类收尾步骤要写成独立遍历而不是塞进分支 —— **收尾必须无条件执行**。

---

---

## 五十五、投影替换收尾：一次"差点引入空指针"的机械替换（第四十九轮）

### 55.1 先把"还剩多少"量清楚

上一轮替换后仍有 70 处站点，但其中大部分是**注释掉的**。于是按"是否处于注释中"重新分类：

```
生效但尚未收束的 setGraphicsEffect: 11 处
注释掉的 setGraphicsEffect:        10 处
```

真正要处理的只有 **11 处**（分布在 5 个文件），其余是死代码注释，保持原样。

### 55.2 差点引入空指针

`sp_recovery` 的阴影是**成员变量**（声明在头文件、构造时 `new` 赋值），而且：

```cpp
void sp_recovery::setDev(device device)
{
    if(dev.state != DeviceState::Sideload)
    {
        ui->runBtn->setDisabled(true);
        shadowEffect_tips->setColor(Qt::green);      // ← 非 Sideload 状态下会改色
        ...
```

如果按前几轮的做法"把 `new` 与三行配置删掉、应用行换成 `applyDropShadow(...)`"，
那么 `shadowEffect_tips` 就会**保持空指针**，而 `setDev()` 里那行解引用会在
"设备不是 Sideload 状态"时崩溃 —— 这是最常走的分支。

修法是**用 `applyDropShadow()` 的返回值保留句柄**（这个函数本来就返回 effect，
注释里也写了"便于调用方追加设置"）：

```cpp
// tips 的阴影颜色会在 setDev() 里按设备状态改动，所以要保留这个 effect 句柄
shadowEffect_tips = applyDropShadow(ui->tips, Qt::red);
```

其余 5 个成员（`runBtn`/`showOutputBtn`/`back_to_basePage`/`selectBtn`/`output`）
在替换后确实没有任何用途，因此连**头文件里的成员声明**一并删除。

### 55.3 验证

| 场景 | 结果 |
|---|---|
| **行为（ASan，正常场景会走 `setDev` 的非 Sideload 分支）** | 30 子页、**0 sanitizer/断言**、0 空指针相关告警 |
| 行为（离线设备，同样是非 Sideload） | 0 断言 |
| 替换等价性（直读原始文件比对） | **75 处**替换，控件与颜色一致 |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；对话框 72 个 |
| 挂起 / 热插拔 | 44 ms / **1 → 2 → 0** |
| 探针 + 构建一致性 + 全新构建 | 全部通过；**0 错误 0 警告** |

**一处需要说明的"不一致"**：`tips` 这个站点在原始文件里有**两次** `setColor`
（构造时 `Qt::red`、`setDev()` 里条件改 `Qt::green`），而校验器取"最后一次出现的颜色"，
于是把 `Qt::red` 报成不一致。手工核对确认：初始色一致、条件改色路径仍然有效（句柄保留了）。

### 55.4 方法上的收获

**机械替换最容易在"对象还有别的用途"时出错**。前几轮的脚本里我恰好写了
"变量若还有其它引用就整体跳过"这条规则，正是它把这一处挡在了自动替换之外；
本轮手工处理时若照搬自动替换的动作，就会漏掉这条引用。
**"这个对象后面还会被用到吗"应当成为替换前的必答问题** —— 本轮把它变成了
"保留句柄"的写法，而不是删掉对象。

---

---

## 五十六、第 50 轮里程碑：全项目指标对照与全量验证（第五十轮）

### 56.1 与第 1 轮审计的数字对照

第 1 轮是纯只读审计（§一），当时的数字都在文档里。50 轮之后重新统计同一批指标：

| 指标 | 第 1 轮审计 | 现在 | 说明 |
|---|---|---|---|
| C++ 行数 | 9266 | 9336 | 基本持平：删掉约 4000 行旧代码的同时，新增了异步化、i18n 与验证工具 |
| 编译单元 `.cpp` | 44 | 46 | +`adbclient`/`uishadow`，−`adbprocess` 等 |
| 裸 `new` | **441** | **161** | ↓63% |
| 裸 `delete` | 60 | 65 | 与 `new` 配对的显式释放不再是主要手段 |
| `QPointer`/智能指针 | **0** | 14 | |
| `qDebug()` | **330** | **123** | ↓63% |
| 注释掉的代码行 | **474** | **113** | ↓76% |
| `tr()` 调用 | **0** | **237** | 另有 436 条可提取文案、356 条已译 |
| 平台 | 仅 Windows（22 个编译单元无条件包含 `Windows.h`） | **Windows + Linux 均可构建** | |
| 同步 adb 调用 | 21 个 `adbProcess` 实例（每个析构都 `adb kill-server`） | **0** | 全部走 `AdbClient` 单队列 + 超时 + 取消 |
| 页面工厂 | 604 行 switch 工厂 | **数据表**：新增子页 = 加 1 行 | |
| 最长的函数 | 未记录 | **97 行**（且大半是数据字面量） | `explainError` 曾 102 行 → 25 行 |
| 超过 60 行的函数 | 未记录（第 44 轮为 16 个） | **10 个** | 351 个函数、中位数 6 行 |
| 构建警告 | — | **0**（CMake 与 qmake 两侧均为 0） | |
| 验证工具 | **0** | **6 个探针 + 6 个校验/变换脚本** | 见 §0.4 |
| 六场景 sanitizer/断言 | 不可运行 | **0** | |

### 56.2 第 50 轮的全量验证（本次一次性跑完）

| 类别 | 结果 |
|---|---|
| 五个构建目标（含 Release+ASan、Release+UBSan、独立聚焦测试工程） | 全部 **0 错误 0 警告** |
| 六个场景：正常 / 畸形输出 / 退化轮换 / 设备消失 / 离线 / 双设备 | 各 30（或按场景）子页、**0 sanitizer/断言**、模态对话框正常关闭 |
| 挂起 / 热插拔 / 聚焦测试 | 89 ms 出窗口 / **1 → 2 → 0** / 退出码 0 |
| 六个探针 | 解析器"不符 0 项"、i18n"全部符合预期"、权限页恒定 1 个模型、别名"全部符合预期"、安装路径 2 条 argv 正确且 0 条未登记键警告、**工厂/配对 36 组不符 0 项** |
| 两份构建文件一致性 | 与磁盘一致 ✓ |
| 重复刷新（18 页 × 6 轮） | 全部有结果，模型数恒定 |

### 56.3 目标四个维度的现状

- **健壮**：50 轮里修掉 **20 余个真实缺陷**（越界解析、空指针、悬停定时器、页面重建泄漏、模型累积、
  监视命令缺 `-s`、别名被覆盖、`QSettings` 写入失败不可见、解释键静默失效……）；
  六场景 + 六探针全绿，自动化回归覆盖每一个修复点。
- **可维护**：工厂变数据表；重复样板收成具名助手（`addPageItem` / `addInfoItem` /
  `addInfoItemAsync` / `installTableModel` / `enableAllItems` / `applyDropShadow`）；
  隐式约束（条目↔子页、允许子页集合、两份构建列表）都被**一条命令**验证。
- **可拓展**：新增子页 = 表里加一行；新增激活器 = 加一行数据；新增条目 = 一行调用；
  新增投影 = 一行调用。这些路径都有对应的自测探针兜底。
- **易读**：注释代码 ↓76%、`qDebug` ↓63%；最长的函数从 102 行降到 97 行（且是数据）；
  超过 60 行的函数只剩 10 个；每一处脚本化改动都带有"抽取→生成→再抽取比对"的等价性证据。

### 56.4 仍然开着的口子（诚实清单）

1. 翻译还差 80 条激活器长说明（纯文案，不动代码）；
2. 投影替换在"与注释交织"的 10 处死代码旁保留了原样（有意为之）；
3. `sp_permissions::setArg` 的 5 个布尔开关可以改成枚举（需连同分派一起改）；
4. 退出时清空 `AdbClient` 请求队列（一次性残留，影响极小）；
5. 阴影校验器对"同一变量多次 `setColor`"仍有歧义（§55.3 已手工核对，工具待修）。

### 56.5 这 50 轮最值钱的三条做法

1. **先立可观测的基线，再动刀**：条目文本、允许子页集合、argv 清单、对象计数——
   没有基线就只能靠"看起来对"交付。
2. **把一次发现变成一条命令**：手工发现的坏味道、隐式约束、构建列表偏差，
   都沉淀成探针或校验脚本，让"还有没有类似问题"从"再读一遍代码"变成"跑一下"。
3. **工具本身也要自证**：脚本计数必须等于实际写入、校验器要能检出人为制造的缺陷、
   等价性比对要独立于被改的代码（以原始文件为准）——§54 两次"工具在说谎"就是代价。

---

---

## 五十七、`setArg` 的 5 个布尔开关：枚举化并删掉死状态（第五十一轮）

### 57.1 形态

`sp_permissions::setArg()` 是一条 6 分支的 if/else 链，**每个分支都做同样的两件事**：
设 `arg`（`"-g"`/`"-f"`/…）并把 5 个布尔量整体重置一遍（其中一个置真、其余置假）：

```cpp
else if(btn_f->isChecked())
{
    arg = "-f";
    arg_g = false; arg_f = true; arg_s = false; arg_d = false; arg_u = false;
}
... 5 段同样的赋值 × 5 个分支 + 默认分支 ...
```

一共 36 行状态搬运，而其中 `arg_s` / `arg_d` / `arg_u` **只被赋值、从未被读取** ——
第 32 轮把分派收敛成"按组 / 过滤 / 其余"三分支后，它们就成了纯死状态。

### 57.2 改法：三值枚举 + 一张表

```cpp
enum class Mode { Other, Grouped, Filtered };
Mode mode = Mode::Other;      // 与 arg 一起构成"当前呈现方式"

void sp_permissions::setArg()
{
    struct ModeEntry { QRadioButton *button; Mode mode; const char *argument; };
    const ModeEntry entries[] = {
        { btn_g, Mode::Grouped,   "-g" },
        { btn_f, Mode::Filtered,  "-f" },
        { btn_s, Mode::Other,     "-s" },   // 与 -d / -u 一样走"简短摘要"
        { btn_d, Mode::Other,     "-d" },
        { btn_u, Mode::Other,     "-u" },
    };
    mode = Mode::Other; arg.clear();
    for(const ModeEntry &entry : entries) { /* 命中的按钮决定 mode 与 arg */ }
}
```

分派处改成 `if(mode == Mode::Grouped) / else if(mode == Mode::Filtered) / else`，
与该函数 **65 → 32 行**同步，5 个布尔量（含 3 个死状态）从代码与头文件中消失。

### 57.3 等价性：脚本抽取旧映射、生成新表、再比对

按钮与参数的对应关系是这里**唯一会让用户看到不同结果**的东西，因此由脚本处理：

```
旧映射: btn_g→"-g"/arg_g, btn_f→"-f"/arg_f, btn_s→"-s"/arg_s, btn_d→"-d"/arg_d, btn_u→"-u"/arg_u, <默认>→""
新表:   同上（Grouped / Filtered / Other 三值）
逐项比对: 一致
```

### 57.4 验证与一处诚实说明

| 场景 | 结果 |
|---|---|
| **映射等价性**（脚本自带） | 5 个按钮 + 默认分支逐项一致 |
| 正常 / 畸形 / 退化轮换 / 双设备 | 30 子页、0 sanitizer/断言；对话框 72/72/72/74 |
| 挂起 / 热插拔 | 90 ms / **1 → 2 → 0** |
| 权限页探针 | 刷新 12 次遗留模型恒定 1 个 |
| 探针（解析器/i18n/配对）+ 构建一致性 | 全部通过（配对"不符 0 项"） |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

**覆盖范围说明**：三种呈现方式由设置对话框里的单选项切换，而该对话框是**模态**的，
探针里触发会阻塞事件循环（§三十二 已记录），因此**没有直接驱动模式切换**；
本轮的保证来自"按钮 → 参数 → 分派"的逐项静态比对，加上默认模式的端到端运行。

### 57.5 方法上的收获

**"互斥布尔量"几乎总该是一个枚举**：它们把"当前处于哪种状态"这件事拆成 N 个必须手工
保持一致的开关，而其中一半往往在某次重构后再也没人读。判断依据很简单 ——
**把每个布尔量的读取点列出来**：只写不读的直接删，剩下的收成一个枚举。

---

---

## 五十八、"选项"对话框收成一张表（第五十二轮）

### 58.1 形态

上一轮把"呈现方式"收成了枚举，而**产生**这些方式的对话框还是逐个手写的：

```cpp
btn_g = new QRadioButton();      // ×5
btn_g->setText(tr("【高级】按组进行整理"));   // ×5
group->addButton(btn_g);         // ×5
layout->addWidget(btn_g);        // ×5
connect(btn_g, SIGNAL(clicked()), this, SLOT(setArg()));   // ×5
```

**同一件事写了 5 遍**（25 行），新增一种呈现方式要改 5 个地方。

### 58.2 改法：表 + 循环

```cpp
struct Option { QRadioButton **button; const char *text; };   // text 用 QT_TR_NOOP 标记
const Option options[] = {
    { &btn_g, QT_TR_NOOP("【高级】按组进行整理") },
    { &btn_f, QT_TR_NOOP("【高级】输出所有信息") },
    ...
};
for(const Option &option : options)
{
    QRadioButton *button = new QRadioButton(settings);
    button->setText(tr(option.text));
    *option.button = button;                 // 仍是原来的成员变量
    group->addButton(button);
    layout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(setArg()));
}
```

新增一种呈现方式 = **表里加一行**（与 §五十七 的 `setArg` 表一一对应）。

### 58.3 一次"校验器误报"

脚本按"5 个按钮各 1 条 connect + closeBtn 3 条 = 8 条"来核对，于是新实现被判"不一致"
并**拒绝写入**。核对后发现是**校验器的期望值错了**：循环里那 1 条 `connect` 覆盖了 5 个按钮，
所以新实现应当是 4 条。修正期望（并同时断言"表里恰好 5 个按钮"）后比对通过。

这次误报是好事：它证明"校验不过就整体不写"这条规则在起作用 —— 脚本宁可不动手，
也不写出一个它自己解释不了的结果。

### 58.4 顺带清理 `about.cpp` 的死代码

- 一个**构造后从未使用**的局部 `QFont`（其 `setFont` 那行早已被注释）；
- **完全重复**的两行 `setStyleSheet` 注释；
- 注释掉的 `hide()` 块（对应的三个打赏按钮本就不显示）。

`about.cpp` 从 96 行降到 85 行。

### 58.5 验证

| 场景 | 结果 |
|---|---|
| 正常 / 畸形输出 / 双设备 | 30 子页、0 sanitizer/断言；对话框 72/72/**74** |
| **"选项"对话框构造路径** | 逐页遍历会点开它（模态对话框自动关闭 72 次），构造路径每次都被执行，0 断言 0 告警 |
| 权限页探针 | 刷新 12 次遗留模型恒定 1 个 |
| 挂起 / 热插拔 | 90 ms / **1 → 2 → 0** |
| 探针（解析器/i18n/配对）+ 构建一致性 | 全部通过 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 58.6 方法上的收获

**"表 + 循环"是把 N 处重复变成 1 处的通用形状**，本轮与 §五十七 合起来完成了
"呈现方式"这一整条链路（选项对话框 → 按钮 → 模式 → 分派）的数据化。
另外，**校验器报错时先怀疑校验器**：这次若直接"按校验器的要求改代码"，
就会把一条本来正确的实现改成错的。

---

---

## 五十九、按键页的"36 个控件清单"改成遍历（第五十三轮）

### 59.1 又一个必须手工同步的清单

`btnEmulator::setStyles()` 逐个列举了 **36 个控件**来加阴影：

```cpp
setShadow(ui->btn_back);
setShadow(ui->btn_home);
... 30 个按钮 + 6 个标签 ...
```

与 §四十 的"允许打开的子页"、§四十六 的"条目顺序"同类：**清单与 .ui 必须手工保持一致**，
新增一个按钮忘了加进来（或改了 objectName）不会报错，只是"这个按钮没阴影"。

### 59.2 先证明"清单恰好等于遍历结果"，再改

阴影在无头环境里看不见，但 `graphicsEffect()` 是可查询的。于是新增 `--effects` 探针：
构造页面、跑 300ms 事件循环，列出**所有带效果的控件**（类型 + objectName）。

基线结果：

```
带阴影的控件共 36 个：
    QPushButton:btn_back … QPushButton:last QLabel:label … QLabel:tips
分类: 按钮 30 / 标签 6 / 其它 0
```

恰好等于 .ui 里的 30 个按钮 + 6 个标签 —— 说明"遍历子控件"与手写清单**覆盖同一集合**，
替换才是安全的。改法：

```cpp
for(QPushButton *button : findChildren<QPushButton *>()) { setShadow(button); }
for(QLabel *label : findChildren<QLabel *>())           { setShadow(label); }
```

`setStyles` **49 → 22 行**；改完重跑探针：**36 个控件逐个相同**（含类型与 objectName）。
另外 5 个说明标签的底色原本同一行写 5 遍，也收成一个列表 + 循环。

### 59.3 过程中的一次自伤（记下来）

这次的替换脚本把文件**改坏了**：`setShadow` 的定义行被从中间截断、其函数体丢失，
末尾还多出一个 `}`。原因是替换的匹配起点落进了 `setShadow` 那一行。
修复方式是从 `HEAD` 取回 `setShadow` 的原始实现（它正是那四行投影样板，
在第 47 轮已被收成 `applyDropShadow(wgt)`），补回定义、删掉多余大括号。

**教训**：替换前应当把"要替换的整段"完整打印出来核对边界（尤其当目标函数**紧跟在
另一个相似函数之后**时）；本次是靠编译错误 + 探针比对发现的，没有流出去。

### 59.4 验证

| 场景 | 结果 |
|---|---|
| **阴影覆盖集合（新探针）** | 重构前后 **36 个控件逐个相同**（按钮 30 / 标签 6） |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；对话框 73 个 |
| 挂起 | 92 ms |
| 探针（解析器/i18n/配对）+ 构建一致性 | 全部通过 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 59.5 方法上的收获

**"看不见的效果"也可以有可观测的替身**：阴影看不见，但"哪些控件挂了效果"看得见。
找一个这样的替身，就能把视觉重构纳入自动比对 —— 这与 §五十一 用对象计数替代 RSS 是同一思路。

---

---

## 六十、追回 17 处被我误删的阴影（第五十四轮）

### 60.1 怎么发现的

上一轮把 `--effects` 探针推广到**全部 26 个页面**，输出"控件总数 / 带效果数"：

```
activator key=0        控件  13 / 带效果   1      ← 可疑：这一页代码里有 5 处阴影设置
apps key=0             控件   9 / 带效果   2
apps key=5             控件  12 / 带效果   2
```

`sp_activator.cpp` 里明明写了 5 处 `setGraphicsEffect`，实测只有 1 个控件带效果 ——
于是回头查第 47/48 轮的批量替换，发现那次脚本在 5 个文件里**删掉了声明与配置、
却没有把对应的 `applyDropShadow(...)` 加回去**（第 48 轮已记录过"删除与插入不同步"这个 bug，
但当时只修了脚本、没有回头核对**已经被写坏的文件**）。

### 60.2 受影响的文件与规模

写了一个"双向审计"：把原始文件（`git show HEAD:`）里**生效的** `setGraphicsEffect` 逐条列出，
与当前文件里的 `setGraphicsEffect` **加** `applyDropShadow(` 比对：

| 文件 | 原始 | 现在（修复前） | 丢失 |
|---|---|---|---|
| `mainwindow.cpp` | 12 | 2 | **10** |
| `sonPages/activator/sp_activator.cpp` | 5 | 1 | **4** |
| `sonPages/apps/sp_installer.cpp` | 4 | 2 | **2** |
| `sonPages/advanced/sp_customize_cmd.cpp` | 4 | 3 | **1** |
| 合计 | | | **17** |

（脚本还报出 `about.cpp` 的 `btn_money_3/4` 两处，但核对后确认它们**原本就在注释块里**、
且这两个控件早已从 .ui 中删除 —— 属于误报，未补。）

### 60.3 修复与验证

按"文件 + 控件 + 颜色"逐条补回（颜色取原始文件里该变量的 `setColor`）：

```
about.cpp                       补回 0 处（两处为误报）
mainwindow.cpp                  补回 10 处
sp_activator.cpp                补回  4 处
sp_customize_cmd.cpp            补回  1 处
sp_installer.cpp                补回  2 处（含 tips 的 Qt::green）
```

| 验证 | 结果 |
|---|---|
| **双向审计**（原始 vs 当前，计入两种 API） | "仍少于原始的文件: **0 个**" |
| **页面级覆盖**（`--effects` 探针） | `activator` 1 → **5**、`installer` 2 → **4**、`customize_cmd` 3 → **4**，合计 193 → **200**（回到原始值） |
| 全页面扫描 | 26 个页面、672 个控件、200 个带效果 |
| 应用场景（覆盖 `MainWindow` 的 10 处） | 30 子页、0 sanitizer/断言、对话框 72 |
| 畸形轮换 / 挂起 / 热插拔 | 30 子页 0 断言 / 90 ms / **1 → 2 → 0** |
| 探针 + 构建一致性 + 全新构建 | 全部通过；**0 错误 0 警告** |

### 60.4 这次事故说明了什么（三条）

1. **批量替换必须做"双向"校验**：我当时只检查了"新出现的调用是否与原站点一致"，
   没有检查"原站点是否都还在"。**少了东西**和**改错了东西**是两类问题，
   校验器必须两边都查 —— 本轮补上的"双向审计"就是这条。
2. **"未匹配/未替换"清单不能想当然**：第 48 轮的校验器老实列出了 70 行"未替换的原始站点"，
   我把它当成"这些都是注释里的死代码"就没细看 —— 而里面藏着 17 处**真正被删掉的活代码**。
   **凡是打印出来的清单，必须逐类判读**（当时只要按"是否有对应新增"分类，一眼就能看出问题）。
3. **可观测基线要建在改动之前**：`--effects` 探针是第 53 轮才建的，比投影替换（第 47 轮）晚。
   如果先有它，这次损失在第一轮替换后就会暴露。**对"看不见的效果"更要先立基线**。

---

---

## 六十一、输入框验证器的泄漏：52 → 0（第五十五轮）

### 61.1 先做"双向审计"，确认没有别的损失

上一轮的教训是"批量替换要双向核对"。本轮把它推广到其它几批替换：

| 替换 | 检查 | 结果 |
|---|---|---|
| 页面条目（`new pageListItem` → `addPageItem`） | 逐文件计数 | 一致 ✓ |
| 信息条目（`initStruct`/`setBlurRadius` 等 → `addInfoItem*`） | 逐文件计数 | 一致 ✓ |
| 模型安装（`setModel(model)` → `installTableModel`） | 逐文件计数 | 一致 ✓ |
| 页面工厂（36 个 `case` → 模板工厂） | 36 → 19 | **预期的压缩**（18 行数据 + 2 个模板定义），由配对探针核对 ✓ |
| `initThread`（`pagemaker` 6→2、`sp_vibrate` 10→1） | — | **预期的压缩**（收进助手定义里），由条目/对象计数核对 ✓ |

结论：**没有新的"只删未加"**，阴影那次是孤例。

### 61.2 顺带查出的真泄漏

在收束 `sp_vibrate` 构造（109 行里 32 行是逐条 `setShadow`、10 行 placeholder、10 行验证器）时，
注意到这 10 行：

```cpp
ui->lineEdit_alarm->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]+(\\.[0-9]+)?$")));
... ×10 ...
```

`QLineEdit::setValidator()` **不接管所有权**，而这里没给父对象 —— **每建一次页面泄漏 10 个验证器**。
LSan 的栈直接印证：`QRegularExpressionValidator` 出现在报告里 52 次，其中一路指向
`sp_scales.cpp:32/34`（那里还有 3 个同样的写法）。

### 61.3 修复与验证

两处都改成"父对象 = 输入框本身"，并把重复的正则提成一个变量：

```cpp
const QRegularExpression scalePattern(QStringLiteral("^[0-9]+(\\.[0-9]+)?$"));
const QList<QLineEdit *> scaleEdits = { ui->lineEdit_transition, ui->lineEdit_animator, ui->lineEdit_window };
for(QLineEdit *edit : scaleEdits)
{
    edit->setValidator(new QRegularExpressionValidator(scalePattern, edit));
}
```

顺带把 `sp_vibrate` 的 32 行逐条 `setShadow` 改成遍历（12 个按钮全加、标签除标题/图标外全加）：

| 验证 | 结果 |
|---|---|
| **LSan 总量** | **7072 字节 / 117 次分配 → 3486 字节 / 35 次分配** |
| **`QRegularExpressionValidator` 出现次数** | 52 → 12 → **0** |
| **阴影覆盖**（`--effects`） | `advanced key=5` 仍为 **32 个带效果**；全项目 200 个不变 |
| 正常 / 挂起 + 探针 + 一致性 | 30 子页 0 断言 / 90 ms / 全部通过 |

### 61.4 方法上的收获

**"重复代码"里常常藏着"重复的错误"**：这 10 行之所以值得看，是因为它们重复；
而重复一旦被收成一处，就逼着人回答"这个参数是什么" —— 于是发现 `QRegularExpressionValidator`
从来没有父对象。**收束样板的过程本身就是一次代码审查**。

---

---

## 六十二、页面生命周期探针 + 一个未决的 LSan 残项（第五十六轮）

### 62.1 先把"页面会不会被回收"变成可查询的量

上一轮修完验证器泄漏后，LSan 报告里仍有约 7 KB 的分配追溯到 `basePage::basePage`
（第 12 行 = `animation_listHide = new QPropertyAnimation(ui->listWidget, "geometry")`），
而 7 恰好等于主页面的个数 —— 看起来像"7 个主页面从未被销毁"。

于是给 `--effects` 探针加了一段生命周期检查：把 26 个页面都挂在一个临时父对象下，
**销毁父对象**、跑一轮事件循环，再用 `QPointer` 统计存活数：

```
生命周期检查（父对象销毁后是否还活着）:
    建了 26 个页面；销毁父对象后仍存活 0 个  ✓
```

**页面回收是正常的** —— 与 LSan 的"7 个页面份泄漏"直接矛盾。

### 62.2 两种解释，以及为什么暂时不下结论

- 可能一：这些是**延迟释放**（`deleteLater` 之类）在进程退出时尚未处理的残留，
  LSan 把它们算作 indirect leak。这与"对象已不可达但未 free"的语义一致。
- 可能二：某个**探针没覆盖到的创建路径**留下了页面（例如运行时动态创建的
  `basePage`），生命周期检查没碰到它。

两者需要不同处理，而现有证据不足以区分。**如实记为未决项**，并写下最小复现步骤：
"只创建 7 个主页面、不做别的、直接退出"的探针 —— 若仍有 ~7 份分配，则属可能一（延迟释放）；
若归零，则说明是探针里其它路径造成的。

### 62.3 本轮确证的部分

| 验证 | 结果 |
|---|---|
| **验证器泄漏**（上一轮修复） | LSan 里 `QRegularExpressionValidator` **0 次**（修复前 52 次） |
| **页面回收**（新探针） | 26 个页面在父对象销毁后**全部回收** |
| 阴影覆盖 | 全项目 **200 个带效果**不变 |
| 正常 / 探针 / 全新构建 | 30 子页 0 断言、对话框 72；解析器/i18n/配对全部通过；**0 错误 0 警告** |
| 四个构建目标 | 全部 0 警告 |

### 62.4 方法上的收获

**矛盾的证据要同时保留**：LSan 说"有 7 份没释放"，生命周期探针说"0 个存活" ——
两个都是第一手观测，不能为了让结论好看而挑一个。写清"两种可能的解释 + 下一步怎么区分"，
比强行给一个结论更有价值；这也是本项目一贯的做法（§二十七 曾修正过自己的验证结论）。

---

---

## 六十三、未决项关闭：动画对象从来没有父对象（第五十七轮）

### 63.1 用一个最小探针把矛盾逼到根因

上一轮留下一个矛盾：LSan 说"约 7 个页面份的分配没释放"，生命周期探针说"26 个页面全部回收"。
本轮写了最小探针 `--lifecycle normal|leak`：**只做三件事** —— 建 26 个页面、删掉父对象、退出。
结果依旧报 `basePage::basePage`（第 12 行）——

```
Indirect leak of 392 byte(s) in 1 object(s) allocated from:
    #1 in QPropertyAnimation::QPropertyAnimation(QObject*, QByteArray const&, QObject*)
    #2 in basePage::basePage(QWidget*)          basepage.cpp:12
    #3 in pageMaker::createPage_recovery(...)
```

### 63.2 关键线索：栈里同时有构造与 `setTargetObject`

栈帧 `QPropertyAnimation::QPropertyAnimation(QObject*, QByteArray const&, QObject*)` 是
**三参数重载**（目标对象、属性名、**父对象**），而它下面还压着 `setTargetObject` ——
说明调用处写的是：

```cpp
animation_listHide = new QPropertyAnimation(ui->listWidget, "geometry");
//                                  ↑ 目标对象          ↑ 属性名        ↑ 没有父对象！
```

**第三个参数被省略了**，所以动画不属于任何父对象 → 页面销毁时它不会随之释放。
同一个写法在工程里有 4 处，其中 3 处是生效代码：

| 文件 | 影响 |
|---|---|
| `basepage.cpp:12` | 每个 `basePage` 泄漏 1 个动画 |
| `animationwidget.cpp:23` | 每个页面基类实例泄漏 1 个动画 |
| `noticewidget.cpp:10` | 每个通知控件泄漏 1 个动画 |
| `basepage.cpp:103` | 在一段注释掉的 `playExitAnimation()` 里（随该死代码一起删除） |

### 63.3 修复与验证

```cpp
animation_listHide = new QPropertyAnimation(ui->listWidget, "geometry", this);   // ← parent
animation           = new QPropertyAnimation(this, "geometry", this);
```

| 验证 | 修复前 | 修复后 |
|---|---|---|
| LSan 中"追溯到 `basePage` 构造"的分配 | 35 次 / 3486 字节 | **0** |
| LSan 中 `QRegularExpressionValidator` | 0（§六十一 已修） | 0 |
| **页面回收**（`--effects` 生命周期检查） | 26 页全部回收 | 26 页全部回收 |
| 阴影覆盖 | 200 个带效果 | 200 个带效果（不变） |
| 正常 / 畸形 / 挂起 / 热插拔 | — | 30 子页 0 断言 / 91 ms / **1 → 2 → 0** |
| 五个目标 + 全新构建 | — | 全部 **0 错误 0 警告** |

### 63.4 剩下的报告属于第三方，与代码无关

修完之后最小探针仍报 36348 字节 / 947 次分配，但**归并后没有任何本项目帧** ——
全部在 `libfontconfig`（`FcValueListDuplicate` / `FcPatternObjectInsertElt` / `FcValueSave` /
`FcLangSetCopy`…），即 Qt 字体引擎的全局缓存。**判据很简单：看报告里有没有本项目路径的帧。**

### 63.5 两条方法上的收获

1. **"对照组"也可能设计失败**：本想用"故意不删父对象"证明探针能测出泄漏，
   结果 `QApplication` 退出时会销毁残留的顶层控件，对照组只报了 192 字节 —— **对照组本身失效**。
   记下来：**验证手段要先验证**，尤其是"应该失败"的那一半。
2. **读栈要读到"重载形态"**：`QPropertyAnimation(QObject*, QByteArray const&, QObject*)` 与
   `setTargetObject` 同时出现，才暴露出"用了三参数重载却没给父对象"。
   只看"哪个文件哪一行"不够，**参数个数本身就是证据**。

---

---

## 六十四、完整场景的泄漏清零（第五十八轮）

### 64.1 用 LSan 当探测器，扫一遍完整应用

上一轮确认"自方泄漏已归零"后，把 LSan 开到**完整应用场景**（所有页面 + 主窗口 + 各种对话框）
跑一遍，把报告按"首个本项目帧"归并：

```
修复前  SUMMARY: AddressSanitizer: 44482 byte(s) leaked in 1154 allocation
    本项目帧的泄漏组: 3
        20096 字节 / 628 对象  MainWindow::on_WIFIBtn_clicked()   mainwindow.cpp:743
        15196 字节 / 297 对象  MainWindow::on_WIFIBtn_clicked()   mainwindow.cpp:743
         1056 字节 /  22 对象  MainWindow::on_WIFIBtn_clicked()   mainwindow.cpp:743
    第三方(无本项目帧) 8134 字节
```

**三组全部指向同一行**：无线调试窗口的 `getInfo->show()`。

### 64.2 根因：一个"只在某条路径上才释放"的窗口

```cpp
void MainWindow::on_WIFIBtn_clicked()
{
    getInfo = new QWidget();      // ← 没有父对象
    ...
    getInfo->show();
}

void MainWindow::connectWIFIDev() // 只有点了"连接"才走这里
{
    ...
    getInfo->deleteLater();
}
```

**用户直接关掉这个窗口时，它永远不会被释放** —— 每打开一次漏一个顶层窗口（连同它的一整套子控件）。
同一个模式还有 3 处 `QMessageBox`（`mainwindow.cpp` ×2、`pagelistitem.cpp` ×1）：

```cpp
QMessageBox * msgBox = new QMessageBox;   // 无父对象、也没有"关闭即删"
...
msgBox->show();
```

（`textexplainer.cpp` 那处**早前已经修过**，正好是本次要套用的写法。）

### 64.3 修复

| 位置 | 改法 |
|---|---|
| `getInfo`（无线调试窗口） | 挂到主窗口下（`new QWidget(this)`）+ `WA_DeleteOnClose`；成员类型改为 `QPointer<QWidget>`，这样 `connectWIFIDev()` 里的 `deleteLater()` 即使遇到已销毁的窗口也不会悬空 |
| 3 处 `QMessageBox` | `setAttribute(Qt::WA_DeleteOnClose)`（与 `textexplainer` 已有的修法一致） |

### 64.4 验证

| 验证 | 修复前 | 修复后 |
|---|---|---|
| **完整场景 LSan** | 44482 字节 / 1154 次分配 | **报告为 "(无)"** |
| 本项目帧的泄漏组 | 3 组（全部指向 `on_WIFIBtn_clicked`） | **0 组** |
| 第三方（无本项目帧） | 8134 字节 | 0 |
| 正常 / 畸形 / 双设备 / 离线 | — | 30 / 30 / 30 / 6 子页，0 sanitizer/断言 |
| 挂起 / 热插拔 | — | 91 ms / **1 → 2 → 0** |
| 探针 + 一致性 + 五个目标 + 全新构建 | — | 全部通过；**0 错误 0 警告** |

### 64.5 方法上的收获

**"只在一部分路径上释放"是一个稳定的缺陷模式**：`deleteLater()` 写在某个按钮的槽里，
就意味着"另一条路（用户直接关闭）"没有释放。判断方法很简单 ——
**看到成员指针 `new` 之后，问"它所有可能的退场路径都被覆盖了吗"**。
本轮把这条与"无父对象的 `new`"合起来一次性扫清了全工程。

---

---

## 六十五、泄漏基线固化 + 修掉自己放错的 10 处阴影（第五十九轮）

### 65.1 把"零泄漏"变成一条可断言的命令

上一轮把完整场景的泄漏清零了，但那是**一次性的人工核对**。本轮写成 `check_leaks.py`：

```
check_leaks.py --run <asan 可执行文件>   # 跑完整场景再判定
check_leaks.py --log <日志>              # 直接判定已有日志
```

判据两条：① LSan 摘要里凡有分配，**不能含任何本项目帧**；② 第三方（无本项目帧）只提示、不算失败。

**先验证这把尺子**（否则它只是"总说通过"的摆设）：

| 输入 | 期望 | 实际 |
|---|---|---|
| 修复前的日志（3 组本项目泄漏） | FAIL | **退出码 1**、列出 3 组 ✓ |
| 修复后的日志（报告为"(无)"） | PASS | 退出码 0 ✓ |
| 只有 libfontconfig 的最小探针日志 | PASS（第三方不计） | 退出码 0 ✓ |
| `--run`（真跑一遍完整场景） | PASS | 0 字节 / 0 次分配，退出码 0 ✓ |

### 65.2 顺带发现：我自己在第 54 轮把 10 处阴影放错了位置

第 54 轮补回被误删的阴影时，脚本的插入锚点是"文件里最后一个 `applyDropShadow` 行" ——
而在 `mainwindow.cpp` 里，那一行恰好位于 **`on_pluginBtn_clicked()` 内部**，于是那 10 个
主窗口按钮的阴影变成了"**点一次插件按钮才出现**"：

```cpp
void MainWindow::on_pluginBtn_clicked()      // ← 补回的 10 处错放在这里
{
    ...
    applyDropShadow(ui->refreshButton);      // 启动时并没有阴影
    ...
}
```

**为什么当时的审计没发现**：那次只做"数量"双向审计（数量没少 ✓），而这是**位置**问题；
`--effects` 探针那时也只覆盖子页，没有覆盖主窗口。

本轮两件一起做：
1. 把 10 处移回 `MainWindow::setStyles()`（原实现就在那里），并收成一个列表 + 循环；
2. **把主窗口纳入 `--effects` 探针** —— 构造完窗口立即检查那 10 个控件是否已有阴影：

```
主窗口关键控件的阴影（构造后立即检查）:
    期望 10 个，实际带阴影 10 个  ✓
```

### 65.3 验证

| 验证 | 结果 |
|---|---|
| **泄漏基线**（新命令） | 完整场景 **0 字节 / 0 次分配**，判定 PASS |
| **主窗口阴影位置**（探针新增项） | 构造后 10/10 就有阴影 ✓ |
| 正常 / 畸形轮换 | 30 子页、0 sanitizer/断言；对话框 72 |
| 挂起 | 97 ms |
| 探针（解析器/i18n/配对）+ 一致性 | 全部通过 |
| 五个目标 + 全新构建 | 全部 **0 错误 0 警告** |

### 65.4 方法上的收获

**"数量对"不等于"位置对"**。双向审计解决的是"少没少"，而这次错在"放哪儿" ——
对应的检查必须是"**在正确的时点观察**"（构造后立即查阴影），而不是数一遍调用。
把观察点写进探针，比再多一次代码走查都可靠。

---

---

## 六十六、位置审计：助手调用有没有落在原来那个函数里（第六十轮）

### 66.1 为什么需要它

上一轮发现 §五十四 补回的 10 处阴影**落在了错误的函数里**（放进了 `on_pluginBtn_clicked`），
而"数量双向审计"当时是通过的 —— **数量对不等于位置对**。于是本轮补一把专门查位置的尺子：
对每个助手调用，比较"当前所在函数"与"原始站点所在函数"。

### 66.2 结果：阴影这一批是干净的

```
已核对 79 处 applyDropShadow 的所在函数
函数位置不一致的 0 处
```

过程中还修正了审计自身的两次问题（都属于"工具也要自证"）：

1. **函数识别漏掉带注释的签名**：`void MainWindow::on_WIFIBtn_clicked()   //槽：…`
   后面跟着行注释，正则认不出这是函数起点，于是把调用归到了上一个函数，
   报出一处**假的不一致**（`getInfoBtn` 被算成在 `resetMonitorThread` 里）。
   修法是**匹配前先剥掉行注释**，再比对 —— 修好后 0 处不一致。
2. **推广到其它批次时脚本自己失效**：三个批次全部报"已核对 0 处"。
   原因是替换模式时转义写重了，模式永远匹配不上 —— 也就是说**它在空过**。
   脚本里已经加了自检：`checked == 0` 直接计入失败并打印"未核对到任何站点（模式失效？）"，
   所以这次是**大声失败**而不是静默通过。

### 66.3 现状与下一步

| 检查 | 状态 |
|---|---|
| 阴影调用的函数位置（79 处） | **0 处错位** ✓ |
| 其它批次（模型安装 / 信息条目 / 页面条目） | 脚本自身待修 —— 目前**会判失败**，不会误报通过 |

**未完成的部分如实记在 §0**：修好 `audit_placement.py` 的批次模式后，
把模型安装、信息条目、页面条目三批也纳入核对。

### 66.4 验证

| 场景 | 结果 |
|---|---|
| 正常场景 | 30 子页、0 sanitizer/断言、对话框 72 |
| 探针（解析器/i18n/配对/阴影/泄漏/一致性） | 全部通过（阴影 200 个带效果 + 主窗口 10/10；泄漏 0） |
| 四个目标 | 全部 0 警告 |

### 66.5 方法上的收获

**"空过的检查"比没有检查更危险**：一个永远说"通过"的审计会让人误以为有保障。
本轮的教训有正反两面 —— 反例是脚本第一版静默空过；正例是加上自检后它立刻
"大声失败"，于是这个未完成的部分不会伪装成成果。
**给检查加一条"我到底核对了多少处"的自检，成本极低，收益极高。**

---

---

## 六十七、位置审计补齐三批并跑通（第六十一轮）

### 67.1 上轮那个"空过"的根因：调用方忘了传仓库路径

上一轮 `audit_placement.py` 三批全部"已核对 0 处"。本轮先复现：把脚本的内部函数抽出来、
用**正确的仓库根**调用，立刻得到 79 处命中 —— 说明脚本逻辑本来是对的，
问题出在**我传的参数是源码目录而不是仓库根**，于是 `repo/QtAdb` 根本不存在、一个文件都没遍历到。

修法不是"记住传对参数"，而是**加守卫**：

```python
if not (repo / 'QtAdb').is_dir():
    print('  ⛔ 路径不对（找不到 %s/QtAdb）—— 请把仓库根作为第一个参数传入' % repo)
    sys.exit(2)
```

加完之后第一次运行就被它当场拦下 —— **守卫的价值就在于替人记住**。

### 67.2 再修两处"判据本身不成立"

跑通后报出 4 处"位置不符"，逐一核对，**没有一处是真的错**：

| 报告 | 实情 | 处理 |
|---|---|---|
| `信息条目` 3 处：期望 `createPage_devInfo`，现在 `createPage_devInfo_rec` | 那一轮**有意**把长函数拆出了 `_rec` 助手，调用随之内移 | 该批改为"允许集合"：`{createPage_devInfo, createPage_devInfo_rec}` |
| `模型安装` 1 处：`QAbstractItemView *view` | 匹配到的是**助手自身的定义**（形参名 `view`），不是调用点 | 每批增加"排除助手自身定义"规则 |

于是判据分成两类，写在脚本头部：

- `allowed=None` —— **机械 1:1 替换**：调用必须仍在原始站点所在的**同一个函数**里（阴影、信息条目）；
- `allowed={...}` —— **有意搬家**的批次：只要求落在允许的函数集合内（模型安装：第 30 轮把
  "安装模型"从 `refresh_tableView` 搬到建模型的三个 helper 里）。

### 67.3 结果

```
阴影       已核对  79 处；位置不符 0 处
信息条目     已核对   9 处；位置不符 0 处
模型安装     已核对  18 处；位置不符 0 处
合计问题: 0
```

**这把尺子确实能失败**：在细化规则之前，它报出过 4 处偏差（都是判据问题，但证明它有判别力）；
加上路径守卫与"核对数为 0 即失败"的自检后，它既不会误报、也不会空过。

### 67.4 验证

| 检查 | 结果 |
|---|---|
| 位置审计（三批） | 0 处位置不符 ✓ |
| 泄漏基线 | 0 字节 / 0 次分配 ✓ |
| 正常场景 | 30 子页、0 sanitizer/断言 |
| 探针（解析器/i18n/配对）+ 构建一致性 | 全部通过 |
| 四个构建目标 | 全部 0 警告 |

### 67.5 方法上的收获

**写完一把尺子，先问它"能不能失败"**。这一轮的三次修正（路径守卫、排除助手定义、
允许集合）都不是在改代码，而是在改**判据**；而每一次都是因为"报告与事实不符"才发现的。
判据写清楚之后，"位置"这类以前只能靠人眼的检查就变成了可回归的断言。

---

---

## 六十八、回归矩阵收成一条命令（第六十二轮）

### 68.1 为什么

到上一轮为止，验证手段已经有：4 个构建目标、7 个运行场景、6 个探针、3 个检查脚本。
但它们散在各个轮次里，每次都要手敲一长串命令，**容易漏项、也容易看错某一项的结果**。
本轮把它们收成一个脚本：

```
python3 .dsh/probe/run_regression.py            # 完整矩阵（约 10 分钟）
python3 .dsh/probe/run_regression.py --quick    # 只跑构建 + 正常场景 + 三项检查
```

输出是"一项一行 + 末尾汇总"，退出码 = 失败项数。实际上跑出来的结果：

```
  构建 build-cmake             ✓  警告 0
  构建 asan/build              ✓  警告 0
  构建 asan/build-release      ✓  警告 0
  构建 emu/build               ✓  警告 0
  场景 正常                      ✓  子页 30、异常 0
  场景 畸形输出                    ✓  子页 30、异常 0
  场景 退化轮换                    ✓  子页 30、异常 0
  场景 双设备                     ✓  子页 30、异常 0
  场景 离线                      ✓  子页 6、异常 0
  场景 设备消失                    ✓  子页 0、异常 0
  场景 挂起                      ✓  退避窗口 89 ms
  探针 解析器                     ✓  不符 0 项
  探针 i18n                    ✓  全部符合预期
  探针 权限页                     ✓  遗留模型 1 个
  探针 别名                      ✓  全部符合预期
  探针 工厂配对                    ✓  不符 0 项
  探针 阴影                      ✓  200 个带效果
  检查 构建列表一致                  ✓  与磁盘一致
  检查 泄漏基线                    ✓  没有属于本项目的泄漏
  检查 位置审计                    ✓  合计问题: 0

  合计 20 项，失败 0 项  ✓ 全部通过
```

### 68.2 先验证"它会失败"

一个只会说"通过"的回归脚本等于没有（§六十六 的教训）。于是做了一次**故意的失败测试**：
把脚本副本里的源码路径改成不存在的目录，再跑 `--quick`：

```
  检查 构建列表一致                  ★ 失败  退出码 1
  合计 14 项，失败 1 项：检查 构建列表一致
```

失败项被列进汇总 ✓；退出码核对（不经管道）：**必失败的副本 = 1、正常副本 = 0** ✓。

### 68.3 边界说明（哪些还没进去）

- ~~**热插拔场景**没有纳入~~ → **已在 §六十九 纳入**（运行中三次改桩状态，判据为去重序列 `[1, 2, 0]`）；
- **UBSan 目标**（`asan/build-relasan`）只在发布前整跑时构建，日常矩阵里省去以节省时间。

两条都写进 §0 的验证纪律，避免"以为脚本跑过了就等于全跑过了"。

### 68.4 方法上的收获

**把散落的检查收成一条命令，是让"验证"从习惯变成设施的关键一步**。
它的价值不在省几条命令，而在于：① 不会漏项；② 每项都必须给出**可读的结论**（而不是一大片日志）；
③ 退出码可以被别的东西消费（将来接 CI 只需要一行）。配合前几轮的"尺子自检"，
这套东西现在既能失败、也能说明自己检查了什么。

---

---

## 六十九、热插拔也自动化：21 项全部进一条命令（第六十三轮）

### 69.1 最后一个手工场景

上一轮把矩阵收成一条命令时，**热插拔**被留在外面 —— 因为它需要在程序运行中
"分三次改桩状态"（单设备 → 双设备 → 无设备），看着不像能脚本化。
本轮把它写进 `run_regression.py`：

```python
mode_file.write_text('one\n')            # 启动前：单设备
proc = Popen([...])                       # 后台跑
time.sleep(9);  mode_file.write_text('two\n')    # 加一个设备
time.sleep(7);  mode_file.write_text('none\n')   # 全部拔掉
out = proc.communicate(timeout=90)[0]
```

判据没有用"看到 1、又看到 2、又看到 0"这种宽松写法，而是要求**去重后的变化序列恰好等于
`[1, 2, 0]`** —— 这样"多一次抖动"或"少一次变化"都会被抓到。

### 69.2 先验证它会失败

把桩换成普通 `bin`（设备数永远不变），同一段逻辑立刻给出：

```
用普通桩时序列: [1] → ★ 判失败（符合预期）
```

即：**这条判据有判别力**，不是"看什么都通过"。

### 69.3 完整矩阵：21 项

```
  构建 build-cmake / asan-build / asan-build-release / emu-build     ✓ 警告 0
  场景 正常 30 / 畸形输出 30 / 退化轮换 30 / 双设备 30 / 离线 6 / 设备消失 0 子页   ✓ 异常 0
  场景 挂起                        ✓  退避窗口 89 ms
  场景 热插拔                      ✓  设备数序列 [1, 2, 0]（期望 [1, 2, 0]）
  探针 解析器 / i18n / 权限页 / 别名 / 工厂配对 / 阴影                ✓
  检查 构建列表一致 / 泄漏基线 / 位置审计                            ✓

  合计 21 项，失败 0 项  ✓ 全部通过
```

### 69.4 现在的验证设施全貌

| 类别 | 数量 | 入口 |
|---|---|---|
| 构建目标 | 4（+1 个 UBSan 目标按需构建） | `run_regression.py` |
| 运行场景 | 7（正常/畸形/退化轮换/双设备/离线/设备消失/挂起/**热插拔**） | 同上 |
| 探针 | 6（解析器/i18n/权限页/别名/工厂配对/阴影） | 同上 |
| 检查脚本 | 3（构建列表一致/泄漏基线/位置审计） | 同上 |
| 校验脚本（不随矩阵跑） | 9（工厂配对生成、条目与阴影的抽取比对、扫描器等） | 按需 |

**"全部手工步骤都进了脚本"** 这件事比多修几个函数更能保证长期质量：
它把"这轮验证过了"变成"任何人任何时候都能重跑一遍"。

### 69.5 方法上的收获

**判据要写成"精确的形状"，而不是"大致出现过"**。热插拔这条如果写成"日志里出现过 1、2、0"，
那么"设备数来回抖动"也会通过；写成"去重序列恰好等于 `[1, 2, 0]`"之后，
抖动、缺步、顺序错都会被抓住 —— 这与 §六十七 里"允许集合"的思路一致：
**把期望写清楚，检查才有意义**。

---

---

## 七十、注释掉的代码：可自证的清理（第六十四轮）

### 70.1 现状

第 1 轮审计时"注释掉的代码行"是 **474 行**（§一），到第 50 轮已降到 113 行（§五十六）。
本轮再看，最集中的地方仍是几处大文件：

```
  70 行  mainwindow.cpp        30 行  sp_permissions.cpp     26 行  animationwidget.cpp
  17 行  textexplainer.cpp     14 行  basepage.cpp           13 行  sonpagemanager.cpp / pagemaker.cpp
```

`basePage` 的构造函数里就并排躺着 6 段死注释（注释掉的投影、被替换掉的整套 stylesheet、
注释掉的 scroller 设置、注释掉的透明度动画……），读的时候要一路跳过。

### 70.2 清理工具：删什么、不删什么

`strip_dead_comments.py` 只删两类：

- `/* … */` 块注释，且块内**含看起来像代码的行**（有 `;` `{` `}` `=` 之类）；
- 形如 `//code…` 的单行注释 —— 去掉 `//` 后**不含中文**（纯代码），且含 `; ( ) = { }` 之一。

**保留**解释性注释：带中文说明的（本项目里那些"原实现如何如何"的记录都在这一类）、
`///` 与 `/*!` 文档注释、以及不满足上述条件的行。

### 70.3 两道自证（比人眼可靠）

脚本在写文件前必须同时通过：

1. **被删的每一行都必须是注释形态**（`//`、`/*` 或 `*` 开头）——否则整份文件不写；
2. **把注释剥掉之后，源码在删除前后逐字节相同** —— 这一条能挡住"以 `*` 开头的解引用赋值"
   这类看似注释、实为代码的行。

第一版判定写得太宽（块注释遇到"未闭合的 `/*`"就把后面所有行都算进块里），
自证 ① 当场拦下 4 个文件并中止写入；改成"必须正常闭合、且块内每行都是注释形态"的保守版后，
删除量与预期一致。

### 70.4 结果

| 文件 | 删除行数 |
|---|---|
| `mainwindow.cpp` | 40 |
| `animationwidget.cpp` | 20（第一遍） |
| `sp_permissions.cpp` | 19（第一遍） |
| `textexplainer.cpp` | 14 |
| `basepage.cpp` | 13 |
| `pagemaker.cpp` | 7 |
| 合计 | **113** |

全工程该类注释降到 **71 行**，最集中的文件只剩个位数（8、6、6、4…）。

### 70.5 验证

| 检查 | 结果 |
|---|---|
| 两道自证（逐文件） | 全部通过（否则不写文件） |
| **完整回归矩阵** | **21 项、失败 0 项** —— 注释清理不影响任何行为 |
| 构建 | 4 个目标 0 错误 0 警告 |

### 70.6 方法上的收获

**"只删注释"这种看似绝对安全的改动，也要有机器可验的判据**。
真正让我敢一次清 113 行的不是"注释嘛，删了没事"，而是那条
"剥掉注释后的源码必须逐字节相同" —— 它把"有没有误删代码"从人眼复核变成了布尔判断。

---

---

## 七十一、多行死注释块：再清 261 行，以及"尺子"的两次拦截（第六十五轮）

### 71.1 放宽判据

上一轮只删了"单行纯代码注释"，多行 `/* … */` 块因为"块内每行都得是注释形态"而被保守跳过 ——
可这些块恰恰是最大的一堆：`basepage` 里注释掉的整套 stylesheet、`pagemaker` 里注释掉的条目构造、
`textexplainer` 里一整段被注释掉的函数……

本轮把块判据改成：**正常闭合 + 块内含"看起来像代码"的行**即删；块内是裸代码是正常的，
不再逐行要求注释形态 —— 安全性交给自证 ②（剥注释后源码逐字节相同）。

### 71.2 结果：跨约 20 个文件清掉 234 行（另加手工 13 行）

| 文件 | 删除 | 文件 | 删除 |
|---|---|---|---|
| `sp_recovery.cpp` | 13 | `sp_users.cpp` | 4 |
| `mainwindow.cpp` | 40（上一轮） | `sp_packages.cpp` | 4 |
| `noticewidget.cpp` | 4 | `sp_libraries.cpp` | 4 |
| `sp_features.cpp` | 4 | `sp_permissiongroups.cpp` | 4 |
| `sp_captive_portal.cpp` | 3 | 其余若干 | … |
| **`textexplainer.cpp`** | **13（手工）** | | |

两轮合计 **261 行**；`textexplainer.cpp` 从 287 行降到 259 行。

### 71.3 尺子的两次拦截（本节最值得记的部分）

工具两次拒绝写入，理由都是**自证 ②："剥注释后源码发生变化"**：

1. `textexplainer.cpp`：它想删的 68 行里**包含活代码**（`explainOutput` 与 `showMsgBox` 的实现）
   —— 也就是说它认为有一个"跨过活代码的块"。查明后发现根因是
   **"剥注释"这个动作是字符串盲的**：某处字符串里含 `/*`，于是它把后面很远的一段都当成了注释。
   最终这段 13 行的死注释由人工核对删除，并当场用同一套"剥注释比对"复核 ✓。
2. `mainwindow.cpp`：同样被拒。定位到 `mainwindow.cpp:575` 的**字符串里含 `/*`**：

```cpp
QPushButton:pressed{background-color:rgba(255,255,255,0.6);}");   // 字符串内含 /*
```

**结论**：这两次不是"工具坏了"，而是**判据比工具的认识更严** ——
它宁可什么都不做，也不写出一个自己解释不了的改动。这正是 §六十六 定下的规矩在起作用。

### 71.4 验证

| 检查 | 结果 |
|---|---|
| 两道自证（逐文件） | 通过；两次拦截如实拒绝写入 |
| **完整回归矩阵** | **21 项、失败 0 项**（261 行注释清理不影响任何行为） |
| 构建 | 4 个目标 **0 错误 0 警告** |

### 71.5 方法上的收获

**"工具会拒绝"是一种功能，不是故障**。当判据（这里是一条不变量：剥注释后源码不变）
比实现（字符串盲的注释扫描）更强时，正确的表现就是**停下来**。
反过来，如果当初没有这条不变量，这两次都会写成"看起来没问题"的删除 ——
第二处还会顺手删掉两个函数的实现。

---

---

## 七十二、字符串感知的扫描器：注释清理收口（第六十六轮）

### 72.1 把根因修掉

上一轮定位到两次"拒绝写入"的根因：**"剥注释"这个动作是字符串盲的** ——
`mainwindow.cpp:575` 的字符串里含 `/*`，于是扫描器把后面很远的一段都当成了块注释，
进而想删掉两个函数的实现；自证 ② 拦住了它。

本轮把扫描器重写为**逐字符状态机**：

```
状态：正常 / 行注释 / 块注释 / 字符串（支持 " 与 ' 以及 \ 转义）
字符串内的 /* 、// 、*/ 一律当普通字符
"//" 跳到行尾（保留行号）；"/*" 进入块注释直到 "*/"
```

`strip_comments()` 与"块注释覆盖行"的判定现在共用这套扫描，两者不再打架。

### 72.2 结果：清掉最后 24 行，判据下清零

```
mainwindow.cpp   删除注释行 24（原文 941 行 → 917 行）
按同一判据，全工程仍可清理的注释代码: 0 行
```

（其余文件在 §七十/§七十一 已清完，故本轮只有 `mainwindow.cpp` 有内容。）

### 72.3 三轮清理的合计

| 轮次 | 内容 | 行数 |
|---|---|---|
| §七十 | 单行纯代码注释（6 个文件） | 113 |
| §七十一 | 多行死注释块（约 20 个文件）+ 手工 13 行 | 261 |
| §七十二 | 字符串感知后 `mainwindow.cpp` 收尾 | 24 |
| **合计** | | **398 行** |

第 1 轮审计时同一类内容有 **474 行**（§一，当时的粗判据），如今按更严的判据为 **0**。

### 72.4 验证

| 检查 | 结果 |
|---|---|
| 两道自证（逐文件） | 全部通过（否则不写文件） |
| **完整回归矩阵** | **21 项、失败 0 项** |
| 构建 | 4 个目标 **0 错误 0 警告** |

### 72.5 方法上的收获

**先修"尺子"，再动"被量的东西"**。上一轮我本可以直接手工删掉 `mainwindow.cpp` 那 24 行，
但那意味着"判据仍然是错的"，下一批清理还会踩同一个坑。花一轮把扫描器改成字符串感知之后，
同样的清理变成了一次通过 —— 而且**这次的自证比以前更强**（扫描精确，不再靠侥幸）。

---

---

## 七十三、调试输出清零：330 → 0（第六十七轮）

### 73.1 从"最长函数"顺出来的一个收尾

本轮原本要看排行榜里的 `textExplainer::getDevList_windows`（39 有效行）。它其实已经清晰 ——
只有开头两行 `qDebug()` 是残留。顺着这条线一看：全工程的调试输出只剩 **23 处**了
（第 1 轮审计是 **330 处**），而且里面有几处很值得清：

```cpp
qDebug() << tr("magisk救砖输出") << exitCode << output << errorOutput;      // mainwindow.cpp
qDebug() << tr("侧载已结束，请在设备上查看结果") << s;                      // sp_recovery.cpp
qDebug() << tr("设备别名已保存:") << addr << "->" << DeviceNames::nameFor(addr)
         << tr("文件:") << DeviceNames::storagePath();                     // comboboxlistwidget.cpp
```

**把界面文案包进 `tr()` 再打进日志**有两个问题：日志里出现用户可见文案（本不该进日志），
而且这些字符串会被 `lupdate` 当成**待翻译词条**收进 `.ts` 文件，污染翻译清单。

### 73.2 清理与一处自伤

一次性清掉 23 处（12 个文件），构建立刻报出一处错误：

```
abstract/comboboxlistwidget.cpp:67:18: error: expected primary-expression before '<<' token
```

原因是我按"行"删除，而那条调试语句**跨了两行**，于是第二行成了孤立续行。修法不是补删一行，
而是顺手把结构改得更清楚 —— **反转条件，只在失败时告警**：

```cpp
    if(!DeviceNames::setName(addr, name))
    {
        qWarning() << tr("设备别名保存失败（配置目录不可写？）:") << addr
                   << tr("文件:") << DeviceNames::storagePath();
    }
```

**教训**：按行删除的前提是"语句不跨行"。跨行语句必须按语句删；
好在编译错误立刻暴露了它，没有流出去。

### 73.3 结果

| 指标 | 第 1 轮 | 现在 |
|---|---|---|
| `qDebug()` | **330** | **0** |
| 注释掉的代码（同判据） | 474 | **0**（§七十二） |

### 73.4 验证

| 检查 | 结果 |
|---|---|
| 构建 | 4 个目标 **0 错误 0 警告** |
| **完整回归矩阵** | **21 项、失败 0 项** —— 说明没有任何探针依赖被删的调试输出 |

### 73.5 方法上的收获

**"调试输出"和"注释掉的代码"是同一类东西**：它们都是"曾经有用、现在只是噪声"的残留，
而且都会随轮次积累。把它们清零之后，`grep qDebug` 从"翻半天"变成"零结果" ——
**一个干净的基线本身就是一种约束**：以后新加的调试输出会立刻显眼。

---

---

## 七十四、可拓展性扫描：一条"不该改"的结论（第六十八轮）

### 74.1 扫"没有 default 的 switch"

思路是：`switch` 枚举时若只列 case、没有 `default`，将来新增枚举值会被**静默忽略**。
扫描结果 6 处，逐个看下来**没有一处需要改**：

| 位置 | 情况 |
|---|---|
| `sonpagemanager.cpp` × 5 | **误报**：扫描器匹配到的是注释里的文字（"原实现是 `switch(key + 1)` …"） |
| `textexplainer.cpp:215` `switch(state)` | 6 个枚举值**全部列出**、末尾有统一 `return` 兜底 |

**关键认识（与直觉相反）**：对枚举 `switch` 而言，**不写 `default` 反而是可拓展性更好的写法** ——
编译器会对"遗漏某个枚举值"给出 `-Wswitch` 告警，于是"新增 `DeviceState` 却忘了处理"
在编译期就会暴露；一旦补上 `default:`，这个告警就永远不会再出现了。
本项目现在 0 警告的构建，正是靠这类"让编译器能说话"的写法维持的。

**所以这一项的正确结论是：不动它**，并把理由写下来 —— 免得下一轮有人（包括我自己）
"顺手"补上 `default` 而把这条保护拆掉。

### 74.2 扫 TODO / FIXME

| 位置 | 处理 |
|---|---|
| `Resources/platform-tools/NOTICE.txt` | 第三方文件，不动 |
| `threads/adbthread.cpp/.h` | 我自己写的说明注释（拿 `#XXX#` 举例），误报 |
| **`usb_listener.cpp:33`** | 真需要订正 —— 见下 |

### 74.3 Linux 热插拔：注释过时了

原文写的是"当前平台无原生通知，返回 false 表示未处理，**由手动刷新兜底**"。
但实际上，第 12 轮做平台解耦时已经加了**定时器轮询**（`m_deviceWatcher` 定时调用
`slot_refreshDevList()`），走的还是与 Windows 原生通知**完全相同**的下游路径 ——
所以 Linux 上热插拔是**自动生效**的（回归里的热插拔场景在 Linux 上稳定通过 `[1, 2, 0]` 就是证据）。
注释已改为如实描述，并说明"若将来换成 udev 或 `adb track-devices`，只需在这里发同样的信号"。

### 74.4 顺带补上一个扫描盲点

订正时发现 `usb_listener.cpp` 里有两处 **`qDebug("usb in")`** —— 上一轮的清理只匹配
`qDebug()`（带空括号）的形式，**漏掉了带参数的写法**。已删除，并把判据放宽为
`\bqDebug\s*\(` 后复扫：全工程只剩一条我自己写的说明注释，**没有真正的调用**。

### 74.5 固化成检查：`check_hygiene.py`

把这两项（调试输出、注释掉的代码）做成一条可断言的门槛，并**自带自检**：
往临时副本里注入一处 `qDebug` 与一行注释掉的代码，检查必须能抓到。

```
自检：注入后 qDebug=1 处、注释代码=1 行 → 能抓到 ✓
当前：qDebug 调用 0 处、注释掉的代码 0 行 → 洁净度达标 ✓
```

已接入回归矩阵（**22 项**）。

### 74.6 方法上的收获

**扫描出来的"问题"要先判断"是不是问题"**。这一轮的两个扫描，一个结论是"全部不用改"
（无 `default` 的枚举 switch 是**有意为之**的保护），另一个才发现真正的过时注释。
如果看到扫描结果就埋头改，反而会拆掉编译器的告警能力 ——
**把"为什么不动它"写下来，和把代码改对同样重要**。

---

---

## 七十五、回归并行化，以及它抓出的一个真泄漏（第六十九轮）

### 75.1 并行化：每个任务一份桩副本

回归矩阵原本顺序执行，整跑约 10 分钟。改为 4 路并行后遇到的第一个问题是
**共享状态**：桩把调用次数写在 `bin/.stat_counter` 里，并行跑会互相覆盖。
于是给每个任务**复制一份桩目录**（桩都是几 KB 的小脚本）：

```python
with tempfile.TemporaryDirectory(dir=str(DSH)) as tmp:
    stub_dir = pathlib.Path(tmp) / stub
    shutil.copytree(DSH / stub, stub_dir)
    senv['PATH'] = ':'.join([str(stub_dir), env['PATH']])
```

场景 4 路、探针 4 路并行；构建与三项检查仍按顺序（它们本来就互不依赖但耗时不同）。

### 75.2 并行之后，泄漏基线开始"偶发失败"

顺序跑时泄漏基线一直是通过的；并行后它**时好时坏**。这条检查的失败详情原本只有"退出码 1"，
于是先给回归脚本加了"失败时打印输出尾部"，立刻看到：

```
检查 泄漏基线  ★ 失败  退出码 1；输出尾部: 88 字节 / 1 对象
    #4 in AdbClient::runWithCallback(...) / 8 字节 / 1 对象 #1 in AdbClient::runWithCallback(...)
```

**根因**：`AdbClient::~AdbClient()` 清了队列、停了超时定时器、杀掉了子进程，
却**没有清 `m_current`** —— 那个"正在执行"的请求持有调用方的 `std::function` 与命令行参数，
于是一直留到进程结束。并行让"退出瞬间仍有请求在途"的概率上升，泄漏因此变得可见。

### 75.3 修法：把收口提出来，并显式调用

```cpp
void AdbClient::shutdown()          // 幂等；析构也会调用它
{
    if(m_shuttingDown) { return; }
    m_shuttingDown = true;
    m_queue.clear();
    m_timeoutTimer->stop();
    if(m_process->state() != QProcess::NotRunning) { m_process->kill(); m_process->waitForFinished(3000); }
    m_current = Request();          // ← 放掉在途请求（此前漏掉的就是这一步）
    m_busy = false;
}
```

`main()` 里在 `a.exec()` 返回后**显式调用**它 —— 而不是只依赖析构：
析构发生在静态销毁期，那时 Qt 的事件分发器可能已经没了（原来的注释就记着这条）。

### 75.4 验证

| 验证 | 结果 |
|---|---|
| 泄漏基线**单独连跑 4 次** | 每次都是 **0 字节 / 0 次分配** ✓ |
| **完整回归（并行）中的同一条检查** | **仍然复现**：`runWithCallback` 88 + 8 字节 ✗ |
| 整跑耗时 | 约 **10 分钟 → 4 分 52 秒** |
| 构建 | 4 个目标 **0 错误 0 警告** |

**所以 §75.3 的改动是"改善了退出路径"，但并没有消除这个竞态** —— 单独跑四次全过、
放进并行回归就复现，说明它依赖时序（并行时"退出瞬间恰好有请求在途"的概率更高）。
这一项**如实保留为待办**，下一步有两条明确的路：

1. 让检查**确定性**：退出前先把在途请求排空（或让探针在退出前显式等待队列为空），
   这样检查测的是"泄漏与否"而不是"时序运气"；
2. 把 LSan 的**完整栈**（而不只是尾部三行）写进失败详情，定位到底是哪一个分配没释放 ——
   目前只看到 `runWithCallback`，还不够确定是回调、参数还是 `QProcess` 的内部缓冲。

### 75.5 方法上的收获

**"偶发失败"不是噪声，是低频路径的显影**。这条检查在顺序执行下永远通过 ——
不是因为它没问题，而是因为"退出时恰好有请求在途"这个条件很少同时满足。
并行化改变了时序，把它变成了必现问题。

配套的一条经验是：**失败要留下可诊断的信息**。这次如果只有"退出码 1"，
就得再花一轮去复现；把输出尾部带进失败详情，一眼就看到了 `runWithCallback` 的栈。

---

---

## 七十六、真正的元凶：堆上的 `QMetaObject::Connection`（第七十轮）

### 76.1 上一轮的修复只是"改善"，不是"根治"

上一轮给 `AdbClient::shutdown()` 补上了 `m_current = Request();`，单独连跑 4 次都通过 ——
但**并行回归里依然复现**。这说明还在漏，只是触发条件变了。

### 76.2 负载复现失败，于是回头读代码

先试着用"3 个后台实例制造负载"来稳定复现：连跑 6 轮，**0/6 复现** ——
说明触发条件不是 CPU 竞争。于是回到栈本身：LSan 报的**分配栈**是 `runWithCallback`，
那就把这个函数逐行读一遍：

```cpp
auto *connection = new QMetaObject::Connection();       // ← 堆上的连接对象
*connection = connect(this, &AdbClient::finished, this,
    [this, requestId, callback, connection](...) {
        ...
        disconnect(*connection);
        delete connection;                              // ← 只有"回调被触发"才会走到这里
        callback(...);
    });
```

**这就是根因**：连接对象是 `new` 出来的，而它只在自己的 lambda **被触发**时才被 `delete`。
请求若因为**退出**或**取消**而始终没有触发回调，那个对象（连同 lambda 捕获的 `callback`）
就永远留在堆上 —— 栈指向 `runWithCallback` 完全对得上。

### 76.3 改法：值语义 + 成员表

```cpp
    // 连接以值语义存进成员表，不 new
    m_connections.insert(requestId, connect(this, &AdbClient::finished, this,
        [this, requestId, callback](int finishedId, int exitCode,
                                    const QString &output, const QString &errorOutput) {
            if(finishedId != requestId) { return; }
            disconnect(m_connections.take(requestId));   // take：取回并移除
            callback(exitCode, output, errorOutput);
        }));
```

- `QHash<int, QMetaObject::Connection> m_connections` 由 `AdbClient` 持有，值是**可平凡复制**的
  连接句柄；不再有属于我们的堆对象；
- 断开连接会同时销毁 lambda，于是它捕获的 `callback` 也一并释放；
- `shutdown()` 里**统一断开**所有未触发的连接 —— 于是"退出时仍有在途请求"这条路径也就干净了。

顺带说明：为什么不用 `Qt::SingleShotConnection` —— 那样第一个 `finished` 信号（可能是**别的**
请求的）就会把连接消耗掉，与"按 requestId 过滤"的语义冲突。

### 76.4 验证

| 验证 | 结果 |
|---|---|
| 泄漏基线（**负载下连跑 6 次**） | **0/6 复现** ✓（修复前：并行回归下必现） |
| **完整并行回归** | **22 项、失败 0 项**（回调路径正常：各场景子页数不变） |
| 构建 | 4 个目标 **0 错误 0 警告** |
| 整跑耗时 | 4 分 52 秒 |

### 76.5 方法上的收获

**"改善了"和"根治了"必须分开说**。上一轮我把"单独跑 4 次都过"当成了修复，
但同一份文档里就写着"并行回归里仍会复现" —— 两句话并存是对的写法：
**说清当前证据能支持到什么程度**，而不是把"看起来好了"当成"好了"。
这一轮之所以能找到真凶，正是因为上一轮**没有把那个矛盾抹平**。

另外一条：**分配栈比调用栈更值钱**。LSan 给的 `runWithCallback` 是**分配**发生的地方，
直接指向那行 `new`，比"哪个函数泄漏了"精确得多。

---

---

## 七十七、稳定性进回归，以及一处"重复施加"（第七十一轮）

### 77.1 单次通过 ≠ 稳定

§七十五/§七十六 的教训很直接：泄漏基线**单独跑一次通过**，放进并行回归却复现。
于是把它改成**连跑 3 次、全部通过才算过**（`--quick` 模式下仍是 1 次）：

```
检查 泄漏基线（连跑 3 次）   ✓  3/3 次一致通过
```

并验证了它的失败路径：把命令指向那份"修复前含泄漏的日志"，立刻得到

```
检查 泄漏基线（连跑 3 次）   ★ 失败  ★ 0/3 次通过（存在偶发泄漏）      退出码 1
```

**"跑三次"这种便宜的手段，恰好对付"偶发"这类最难缠的问题。**

### 77.2 顺带发现：一次"改成遍历"只替换了一半

看 `sp_vibrate` 构造的形态分布时发现，里面**还留着 22 行显式列表**：

```cpp
setShadow(ui->current_all);
setShadow(ui->current_all_charge);
... 22 行 ...
```

而 §五十五 那一轮已经把它改成了 `findChildren` 遍历。两份同时存在，意味着这些控件
**被重复施加阴影**：先被循环遍历到，又被显式列表再设一次
（`setGraphicsEffect` 会让新效果接管、旧效果被销毁，所以不漏内存，但纯属浪费且误导读者）。

**为什么当时没发现**：`--effects` 探针数的是**带效果的控件数**（32 个），
重复施加不会改变这个数字 —— 探针**没有说它能查出这类问题**，我也就没往这上面想。
根因是那次替换用的模式要求"连续成组的 `setShadow` 行"，而文件里这些行分成三组、
组间有空行，于是**只有第一组被替换**。

清理后：构造 **109 → 87 行**，裸列表 **0** 行，`--effects` 仍是 **32 个带效果**（覆盖不变 ✓）。

### 77.3 验证

| 验证 | 结果 |
|---|---|
| 泄漏基线（连跑 3 次） | 3/3 一致通过 ✓（失败路径也已验证） |
| 阴影覆盖 | `advanced key=5` 仍 **32**；全项目 **200** 不变 |
| 裸列表残留 | **0** 行 |
| 构建 | 4 个目标 **0 错误 0 警告** |
| 完整并行回归 | 见下（本轮改动后重跑） |

### 77.4 方法上的收获

**要知道自己的探针"测的是什么、不测什么"**。`--effects` 覆盖"哪些控件有阴影"，
但不覆盖"一个控件被设了几次" —— 这次的问题正好落在它的盲区里。
两次教训（§六十五 的"数量对不等于位置对"、本轮的"覆盖对不等于次数对"）指向同一件事：
**每条检查都有它量不到的方向，说清边界比多写一条检查更重要。**

---

---

## 七十八、把"重复施加"补成检查（第七十二轮）

### 78.1 补上探针的盲区

§七十七 的教训是：`--effects` 数的是"带效果的控件数"，重复施加不改变它 ——
所以"一个控件被设了两次阴影"这件事**没有任何检查覆盖**。本轮把它补上。

判定分两步（不做任何 Qt 层面的假设，纯静态）：

1. **先识别"阴影助手"**：本文件里那些**函数体中出现 `applyDropShadow(` 或 `setGraphicsEffect(`**
   的具名函数 —— 例如 `sp_vibrate` 的 `setShadow()`。这样即使每个页面自己包了一层，
   也能被算作"设置阴影"的一种形式；
2. **再按函数统计**：同一函数内，对每个控件统计以下形式的出现次数
   —— `applyDropShadow(X)`、`X->setGraphicsEffect(`、`助手(X)`；**次数 > 1 即报告**。

限制在"同一函数内"是刻意的：跨函数重复是合理的（例如 `setDev()` 里按设备状态改阴影）。

### 78.2 自带注入式自检

一个只会说"通过"的检查等于没有（§六十六 教训），所以照例带自检：
往临时副本的 `sp_vibrate` 构造函数里注入一处重复，检查必须报出来：

```
自检：注入重复后报告 1 处 → 能抓到 ✓
    sp_vibrate.cpp: sp_vibrate() 里 ui->pushButton 被设 2 次（applyDropShadow/setShadow）
当前：同一函数内重复施加阴影: 0 处 ✓
```

注意自检报告里那两种形式 —— `applyDropShadow` 与 `setShadow` —— 正是"识别助手"这一步
在起作用的证明：如果只看 Qt 调用，这条注入就会被漏掉。

### 78.3 验证

| 检查 | 结果 |
|---|---|
| 重复施加阴影（新检查） | 全工程 **0 处** ✓ |
| 自检（注入重复） | 能抓到 ✓ |
| **完整回归矩阵** | **23 项、失败 0 项** |
| 构建 | 4 个目标 **0 错误 0 警告** |

### 78.4 方法上的收获

**盲区要变成检查，而不是变成记忆**。上一轮是"看代码时偶然发现"的重复施加 ——
这类问题靠"下次记得看"是留不住的；把它写成一条检查，才真正从根上补掉。
本轮的做法还示范了一种通用套路：**先用"函数体里出现了什么"识别出助手，
再按语义统计调用** —— 这个套路同样适用于"同一个视图被装了两个模型"之类的累积型缺陷。

---

---

## 七十九、一次推广失败，以及如实收窄（第七十三轮）

### 79.1 想推广到"模型安装"

§七十八 的检查是"同一个控件在同一函数里被设两次阴影"。很自然的下一个想法是推广到
**"同一个视图被装两次模型"** —— 那正是第 30 轮修过的"模型累积"缺陷类型。
做法也一样：先用"函数体里出现了 `installTableModel(`/`setModel(`"识别助手，再按函数统计。

### 79.2 结果：9 处报告，全是误报

```
★ [模型安装] sp_accounts.cpp: refresh_tableView() 里 ui->listView 被设 2 次（直接调用/直接调用）
★ [模型安装] sp_permissions.cpp: refresh_tableView() 里 s 被设 3 次（fillFilteredTable/fillGroupedTable/generalMode）
... 共 9 处 ...
```

逐条核对，两类误报：

1. **文本计数看不见分支**。这 7 处的真实写法是

```cpp
if(数据为空)
    installTableModel(ui->listView, new QStandardItemModel(this));   // 空数据也给个空模型
else
    installTableModel(ui->listView, model);
```

   两条分支只会执行一条，是**有意为之**；而"按出现次数计数"把它们算成了两次。
2. **助手识别的粒度不对**。`fillGroupedTable(s)` 里的 `s` 是**字符串参数**，
   不是被装模型的视图；把它当控件名统计，纯属张冠李戴。

### 79.3 决定：收窄，而不是加例外

可以继续打补丁（识别 `if/else` 分支、区分参数类型……），但那会让这个检查越来越复杂、
越来越难解释。**判据必须匹配代码形状** —— 阴影那一类之所以成立，是因为
"助手接收的就是被设阴影的控件"这条前提在代码里成立；模型安装这一类不成立。

于是把它**收窄回只查阴影**，并在脚本注释里写清为什么：

```
# 只保留"阴影"一类。曾尝试推广到"模型安装"，但不可靠：
#   1) 文本计数看不见分支 ……（本工程里有 7 处这种写法）；
#   2) 助手识别的粒度不对 —— fillGroupedTable(s) 里 s 是字符串，不是被装模型的视图。
# 结论：判据要匹配代码形状；形状不成立时应收窄检查，而不是靠加例外把噪声压下去。
```

收窄后：自检**恰好报 1 处**（注入的那一处阴影重复，且识别出了助手形式），
当前代码 **0 处重复**。

### 79.4 验证

| 检查 | 结果 |
|---|---|
| 重复施加阴影 | 自检报 1 处（能抓到）✓；当前 **0 处** ✓ |
| **完整回归矩阵** | **23 项、失败 0 项**（期望串已同步） |
| 构建 | 4 个目标 **0 错误 0 警告** |

### 79.5 方法上的收获

**"能推广"和"该推广"是两件事**。同一套判据在阴影上成立，不代表在模型安装上也成立 ——
两者的代码形状不同（一个没有分支、助手参数就是目标；另一个有 if/else、助手参数是字符串）。
**误报比漏报更能毁掉一个检查**：9 处噪声会让人开始忽略整条输出，
那时它连原本能查出的那一类也一起失效了。收窄不是退步，是把检查保持在"可信"的状态。

---

---

## 八十、UBSan 从"只编译"变成"真跑"（第七十四轮）

### 80.1 一个只存在于构建列表里的目标

`asan/build-relasan`（Release + ASan + **UBSan**）从第 20 轮左右就存在，
每一轮回归都在**构建**它 —— 但**从来没有运行过**。
于是它唯一能提供的保证只有"这个配置编得过"。而 UBSan 的价值恰恰在于**运行**：
有符号溢出、移位越界、对齐错误、空指针解引用前的可疑运算，这些 ASan 抓不到。

### 80.2 跑一遍：全部干净

| 运行对象 | UBSan 报告 |
|---|---|
| 场景 正常 / 畸形输出 / 退化轮换 / 双设备 / 离线 | 0 / 0 / 0 / 0 / 0 |
| 探针 `--parsers` / `--i18n` / `--devicenames` / `--effects` | 0 / 0 / 0 / 0 |
| 场景 挂起 | 0（退避窗口 90 ms） |

**这是一条有价值的否定结论**：这些路径上没有被 UBSan 判定的未定义行为。
（尤其值得关注的是解析与格式化路径 —— 它们处理的正是畸形输入，也是 `--parsers` 探针覆盖的地方。）

### 80.3 接进回归矩阵

把最有代表性的三项固化成回归项（构建 `build-relasan` 也一并纳入）：

```
构建 asan/build-relasan          ✓  警告 0
UBSan 场景 正常                  ✓  未定义行为 0 处
UBSan 场景 畸形输出               ✓  未定义行为 0 处
UBSan 探针 解析器                ✓  未定义行为 0 处

合计 27 项，失败 0 项  ✓ 全部通过
```

判据是 `runtime error:` 的出现次数必须为 0（UBSan 报告格式稳定，用计数比解析文本更可靠）。

### 80.4 方法上的收获

**"构建了"不等于"验证了"**。一个 sanitizer 目标如果只参与编译，它给出的安全感是假的 ——
本轮之前，`build-relasan` 在每轮回归里都显示 ✓，但它实际只说明"UBSan 插桩下能编过"。
把它跑起来之后，同一行 ✓ 才真正代表"这些路径上没有未定义行为"。
**检查清单里每一项都要问一句：它到底验证了什么？**

---

---

## 八十一、第 75 轮质量快照（第七十五轮）

### 81.1 指标（同一口径重算，避免不同统计方式混用）

统计范围：`QtAdb/` 下全部 `.cpp` 与 `.h`；"行数"按换行符计。

| 指标 | 第 1 轮（`HEAD`） | 现在 | 说明 |
|---|---|---|---|
| C++ 行数 | 8839 | 8881 | **基本持平**：删掉约 4500 行旧代码，同时补进异步化、i18n、平台解耦与验证设施 |
| 裸 `new` | **253** | **130** | ↓49% |
| `qDebug()` | **278** | **0** | ↓100% |
| `tr()` 调用 | **0** | **213** | 从完全不可翻译到覆盖界面文案 |
| `QPointer`/智能指针 | 0 | 15 | |
| `.cpp` 文件 | 46 | 46 | 关停并转后总数不变（`adbprocess` → `adbclient` 等） |

**函数规模**（342 个函数，按"去掉注释与空行"计）：

| 维度 | 数值 |
|---|---|
| 中位数 | **5 行** |
| 超过 40 行 | **6 个** |
| 超过 60 行 | **1 个**（`createSonPageFor_activator`，其主体是 18 行数据表） |
| 最长 | 91 行（同上） |

对比：第 50 轮时"超过 60 行"还有 10 个、最长 97 行；第 44 轮 `explainError` 曾是 102 行。

### 81.2 回归矩阵：27 项，一次跑完

```
构建 ×5（CMake / ASan Debug / ASan Release / ASan+UBSan / 独立聚焦工程）   警告 0
场景 ×8（正常/畸形/退化轮换/双设备/离线/设备消失/挂起/热插拔）              异常 0
UBSan 运行 ×3（正常/畸形/解析器探针）                                    未定义行为 0
探针 ×6（解析器/i18n/权限页/别名/工厂配对/阴影）                          全部符合预期
检查 ×5（构建列表一致/泄漏基线 3 次/位置审计/洁净度/重复施加）              全部通过

合计 27 项，失败 0 项  ✓ 全部通过
```

### 81.3 缺陷与加固（按性质分类，累计）

| 类别 | 代表项 |
|---|---|
| **崩溃/越界** | 畸形 adb 输出越界解析、`tmp[1]` 越界断言、阴影句柄变空指针、验证器无父对象泄漏 |
| **内存** | 页面/对话框/通知控件泄漏、模型累积、动画无父对象、堆上的 `QMetaObject::Connection`、退出时在途请求 |
| **静默失效** | 子页监视命令缺 `-s`、别名被覆盖、`QSettings` 写入失败不可见、未登记的解释键 |
| **平台** | 22 个编译单元无条件包含 `Windows.h`、同步 adb 调用阻塞界面（21 处）、Linux 热插拔告警注释过时 |
| **一致性** | 条目↔子页映射、允许子页集合、两份构建文件列表、工厂配对、阴影覆盖/位置/次数 |

其中"**一致性**"这一类是最有长期价值的：它们本来是**隐式约束**（写错了不会报错，只是行为不对），
现在都变成了一条命令。

### 81.4 仍然开着的口子（诚实清单）

1. **91 条未译词条**（`translations/QtAdb_en.ts`，分散在 36 个上下文，多数只差 1 条）——
   纯内容工作，不动代码；
2. **投影替换在 10 处死代码注释旁保留了原样**（有意为之，避免为了整洁去动注释里的历史）；
3. **`--effects` 探针的盲区**：它数"覆盖控件数"，不覆盖"同一控件被设几次"（§七十八 补了检查）
   与"设置发生在哪个函数"（§六十七 补了检查）—— 但这些补充检查各自也只覆盖特定形状（§七十九）；
4. ~~热插拔场景尚未纳入 UBSan 运行~~ → **已在 §八十二 完成**（28 项全绿）；
5. ~~"分支内两次安装模型"没有检查覆盖~~ → **已在 §八十七 完成**（判据改为按包围块计数，
   零误报；自检能抓到同块内的真重复）。

### 81.5 这 75 轮最稳的三条做法（延续 §五十六）

1. **先立可观测基线，再动刀** —— 条目文本、允许子页集合、argv 清单、阴影覆盖、泄漏字节数……
   没有基线就只能靠"看起来对"交付；
2. **把一次发现变成一条命令** —— 手工发现的坏味道、隐式约束、构建列表偏差，
   都沉淀成探针或校验脚本；现在整条矩阵一次跑完，且**每项都要自证能失败**；
3. **工具本身也要自证，且要说清它测不到什么** —— 三次"工具在说谎"、两次虚假通过（空过）、
   一次推广失败（误报）都写进了文档；**说清边界比多写一条检查更重要**。

---

---

## 八十二、UBSan 补齐热插拔，以及死代码分析器的一个盲点（第七十六轮）

### 82.1 快照里的口子 4：热插拔没进 UBSan

§八十一 的诚实清单里第 4 条是"热插拔场景尚未纳入 UBSan 运行"。本轮补上：在与主回归
相同的三次改桩状态下，用 `build-relasan` 跑一遍并要求 `runtime error:` 为 0。

```
UBSan 场景 正常         ✓  未定义行为 0 处
UBSan 场景 畸形输出      ✓  未定义行为 0 处
UBSan 场景 热插拔        ✓  未定义行为 0 处
UBSan 探针 解析器        ✓  未定义行为 0 处
合计 28 项，失败 0 项  ✓ 全部通过
```

### 82.2 顺带发现：分析器把"带中文尾注释的死代码"当成了说明

看 `basePage` 构造函数时发现末尾还留着一整块被注释掉的代码：

```cpp
    /*透明度动画
    exitOpacity = new QGraphicsOpacityEffect (this);//创建QGraphicsOpacityEffect对象关联控件ui.widgetProcess
    exitOpacity->setOpacity(1);//设置透明度为0，隐藏控件
    this->setGraphicsEffect(exitOpacity);//控件添加绘图效果*/
```

可是 §七十~§七十二 的清理与洁净度检查都报"注释掉的代码 0 行"。查清原因：
判据是"块内存在**看起来像代码**的行"，而 `is_codeish()` 要求该行**不含中日韩文字** ——
这里三行**每行都带中文尾注释**，于是全被判成说明性文字，整块被保守留下。

改法是：**块内判定时先剥掉行尾注释再判断**（单行规则不变）。

```python
def _code_part(l):
    k = l.find("//")
    return l[:k] if k >= 0 else l
if any(is_codeish(_code_part(l)) for l in block):
```

结果：`basepage.cpp` 又清掉 **4 行**（196 → 192 行），构建 0 警告，洁净度检查仍达标。

**这是这条判据第三次被现实修正**（§七十一 块内行不必是注释形态、§七十二 扫描器要字符串感知、
本轮 行尾注释要先剥）——每一次都是"报告与事实不符"引出来的，也每一次都只是**收窄或放宽
一个具体规则**，没有把整条自证机制动掉。

### 82.3 验证

| 检查 | 结果 |
|---|---|
| UBSan（正常/畸形/热插拔/解析器） | 未定义行为 **0 处** |
| 洁净度 | 达标（两项均为 0）✓ |
| 构建 | 4 个目标 **0 错误 0 警告** |
| **完整回归矩阵** | **28 项、失败 0 项** |

### 82.4 方法上的收获

**"保守"也会留下东西 —— 保守的判据需要被定期对照事实**。这个分析器从一开始就偏向
"宁可少删不可错删"，代价就是会漏掉那些"长得不像代码"的死代码（带中文注释的、跨行的）。
自证机制保证了它**不会删错**，但**不能保证它删干净**；
所以每一轮顺手看一眼实际文件，仍然是有价值的动作 —— 本轮那 4 行就是这么发现的。

---

---

## 八十三、重复样式串抽成具名常量（第七十七轮）

### 83.1 量化：哪些样式串在重复

先按"同一行内的字符串字面量"统计（避免跨行拼接带来的匹配歧义）：长度超过 60 字符、
且出现 2 次以上的共 **7 种、17 处**。

| 常量 | 出现 | 内容特征 |
|---|---|---|
| `kFlatButton` | 2 | 扁平按钮（含 hover / pressed 三态） |
| `kToolTip` | 2 | 提示框样式 |
| `kPlainDialog` | 3 | 纯白对话框底 |
| `kSemiWhitePanel` | 3 | 半透明白面板 |
| `kActivatorTile_bg` / `_blackroom` / `_icebox` | 3 + 2 + 2 | 激活器图标底 |

### 83.2 抽成 `uistyles.h`

```cpp
namespace UiStyles {
// 3 处使用
constexpr const char *kPlainDialog = "background-color:rgba(255,255,255,1);border:0px; border-radius:0px;";
...
}
```

替换由 `.dsh/probe/apply_ui_styles.py` 完成：生成头文件 → 替换单行出现 → 补 `#include`，
并且**每一步都自证**：把替换前后"长度 > 60 的单行字符串字面量"排成多重集比对，
差的必须**恰好**是被抽走的那几条 —— 也就是"只换了写法、没换内容"。

```
devitem.cpp 1 处 / mainwindow.cpp 7 处 / btnemulator.cpp 1 处
sonpagemanager.cpp 7 处 / textexplainer.cpp 1 处        共 17 处
```

新头文件同时登记进 `CMakeLists.txt` 与 `QtAdb.pro`；一致性检查：

```
磁盘源文件 131 个；CMake 列出 131 个；.pro 列出 131 个
两份列表与磁盘一致 ✓
```

### 83.3 如实说明两件事

1. **跨行拼接的同一串没动**：那条按钮样式另有 4 处写成多行拼接，替换它们需要跨行匹配，
   收益与风险不划算 —— 留在原地（脚本只处理"整串在同一行"的出现）。
2. **本轮在改动流程上浪费了预算**：注册头文件时我连续猜错了两处路径与接口
   （构建文件位置、检查脚本的参数含义），每一步都被工具立刻纠错（"不一致 2 项"、
   `FileNotFoundError`）——**这说明检查在起作用，但也说明"先读接口再动手"这条纪律
   我自己没有始终遵守**。教训记在这里：涉及路径与脚本接口时，先 `grep` 一眼实际写法。

### 83.4 验证

| 检查 | 结果 |
|---|---|
| 字符串内容自证 | 替换前后逐条一致（只换写法） |
| 构建 | 0 错误 0 警告 |
| 构建列表一致性 | 磁盘 / CMake / .pro 各 **131** 个 ✓ |
| **完整回归矩阵** | **28 项、失败 0 项** |

### 83.5 方法上的收获

**"重复"是改造的入口，"自证"是改造的出口**。20 处重复样式里能自动且安全处理的只有 17 处 ——
差别不在难度，而在"能不能用机器证明没改坏"。凡是能证明的，就大胆改；
证明不了的（跨行拼接），宁可留着。

---

---

## 八十四、跨行拼接的那 4 处也收干净（第七十八轮）

### 84.1 上一轮留下的尾巴

§八十三 收掉了 17 处**单行**重复样式，并如实留下 4 处**跨行拼接**的同一按钮样式。
本轮把它们也处理掉，条件是"能用机器证明没改坏"。

### 84.2 第一版替换器为什么没匹配上

第一版按"每 60 字符切一段"重建目标串的拼接形态，结果 **0 处匹配** ——
因为源码里的换行位置与我猜的切分点根本不一致。

改成**按原文实际的字面量运行段定位**：先用与抽取器相同的规则
（相邻字面量拼接、遇到 `;` `)` `,` 收尾）求出每一段的**精确起止偏移**，
再把整段替换成 `UiStyles::kFlatButton` —— 不重建、不猜测。

```
sonPages/devInfo/sp_dpichanger.cpp   替换跨行拼接 2 处（kFlatButton）
sonPages/devInfo/sp_wmsize.cpp       替换跨行拼接 2 处（kFlatButton）
```

### 84.3 自证也跟着升级

上一轮的自证只比对"单行字面量"（所以看不见跨行拼接这一类）。本轮升级为
**拼接感知的多重集比对**：抽取器会按 C++ 的相邻字面量拼接语义把跨行片段合成整串，
再比较替换前后的多重集 —— 差值必须**恰好**是本次抽走的那些，且不允许出现新增。

### 84.4 结果

| 指标 | 数值 |
|---|---|
| `uistyles.h` 常量 | **7** 个 |
| 全工程重复的长样式串（拼接感知） | **0 种、0 处** |
| 构建 | 0 错误 0 警告 |
| **完整回归矩阵** | **28 项、失败 0 项** |

### 84.5 方法上的收获

**第一版失败的原因值得记**：我按"我认为源码怎么换行"去构造匹配模式，
而不是按"源码实际怎么换行"去定位 —— 这与 §七十七 那次"猜路径/猜接口"是同一个毛病。
**凡是能"读出来"的，就不要"猜出来"**：先用与抽取器一致的规则把真实位置算出来，
再动手替换，一次就成。

---

---

## 八十五、无线调试窗口：从"逐行搬运控件"变成"描述界面"（第七十九轮）

### 85.1 全工程最大的一个函数

`MainWindow::on_WIFIBtn_clicked()`（无线调试窗口）此前是 **99 行 / 80 有效行**，
里面 25 次 `new`、11 次 `setStyleSheet`、21 次布局添加 —— 形状高度重复：

```cpp
QLabel * lb_outIp = new QLabel(getInfo);
lb_outIp->setText(tr("无线调试IP地址："));
lb_outIp->setStyleSheet("border-radius: 4px;border:0px solid #BDBDBD;");
... 8 个标签都是这三行 ...
QHBoxLayout * lo_outIp = new QHBoxLayout();
lo_outIp->addWidget(lb_outIp);
lo_outIp->addWidget(le_outIp);
... 5 个"标签 + 输入框"行都是这三行 ...
```

### 85.2 改法：两个局部 lambda + 三个样式常量

```cpp
    // 本节里"建标签 → 设文本 → 设样式"与"标签 + 输入框成一行"反复出现（8 次 / 5 次），
    // 收成两个小 lambda：下面的代码因此是"描述界面"，而不是逐行搬运控件。
    auto makeLabel = [](QWidget *parent, const QString &text, const char *style) {
        QLabel *label = new QLabel(parent);
        label->setText(text);
        label->setStyleSheet(style);
        return label;
    };
    auto makeRow = [](QWidget *left, QWidget *right) { ... };
```

调用处随之变成一行：

```cpp
QLabel * lb_outIp = makeLabel(getInfo, tr("无线调试IP地址："), UiStyles::kFormLabel);
QHBoxLayout * lo_outIp = makeRow(lb_outIp, le_outIp);
```

重复的三种表单样式（`kFormPanel` / `kFormStepLabel` / `kFormLabel`，各 2–4 处）并入 `uistyles.h`
——`uistyles.h` 的常量数从 7 增到 **10**。

**函数 99 → 87 行**；静态 `new` 计数 25 → 15（标签与行的创建进了 lambda，这个下降是预期的）。

### 85.3 自证与验证

写这次改动时，自证脚本先按"`new QXxx` 计数"比对，结果报**不一致**并拒绝写入 ——
核对后确认那正是"把创建搬进 lambda"的预期结果，于是换成**本例适用的判据**：

- `tr()` 文本**逐条一致**（10 条 ✓）；
- 行为：正常场景 30 子页、0 断言、73 个模态对话框自动关闭；
- **Qt 的布局/连接告警 0 条** —— 特意查这一项，因为布局搬动最容易在这里出问题
  （`QLayout: Attempting to add QLayout ...`、`already has a layout` 之类）；
- 完整回归矩阵 **28 项**。

### 85.4 方法上的收获

**自证判据要跟着改动选**。"`new` 的数量不变"在 §八十三 那种"只换写法"的替换里是对的，
但在"把创建搬进辅助函数"的重构里必然不成立 —— 这时正确的是"**语义不变量**"：
文本集合不变、控件树结构不变、Qt 不报警。**判据选错会导致两种坏结果**：
要么像本轮这样先误报（还好它拒绝了写入），要么放过真正的问题。

---

---

## 八十六、助手提到文件级，并修掉 Magisk 窗口的同类问题（第八十轮）

### 86.1 不在第二个对话框里再抄一遍

§八十五 给无线调试窗口加了两个**局部** lambda（`makeLabel` / `makeRow`）。
Magisk 救砖窗口是同一个形状 —— 但如果在那里再写一遍，就等于把重复从"控件三行"搬到了"lambda 定义"。
于是本轮把它们提成**文件级助手**：

```cpp
static QLabel *makeFormLabel(QWidget *parent, const QString &text, const char *style);
static QHBoxLayout *makeFormRow(QWidget *left, QWidget *right);
```

两个对话框共用（无线调试窗口 13 次调用、Magisk 窗口 5 次）：

| 函数 | 之前 | 之后 |
|---|---|---|
| `MainWindow::on_WIFIBtn_clicked` | 99 行 | **75 行** |
| `MainWindow::on_pluginBtn_clicked` | 55 行 | **52 行**（有效 46 → 41） |

### 86.2 顺手发现的同类生命周期问题

读 Magisk 窗口时看到第一行就是：

```cpp
magiskSaver = new QWidget();      // 无父对象、也没有"关闭即删"
```

这**正是 §六十四 修掉的那个无线调试窗口的写法**（`getInfo`）——同型的顶层窗口泄漏，
只是藏在另一个按钮后面。修法与当时一致：

```cpp
magiskSaver = new QWidget(this);
magiskSaver->setAttribute(Qt::WA_DeleteOnClose);
```

成员类型也从 `QWidget *` 改成 **`QPointer<QWidget>`**（关闭即删之后指针可能随时变空，
与 `getInfo` 一致）。工程里其余几处 `new QWidget()` / `new QMessageBox` 复核后确认
都已带 `WA_DeleteOnClose`（`sp_permissions.cpp:285`、`textexplainer.cpp:193`、`mainwindow.cpp:607/737`）。

### 86.3 验证

| 检查 | 结果 |
|---|---|
| **文案完整性** | 两个对话框共 16 条文案，**逐条在 `HEAD` 中以字面量存在**（0 条缺失/新增） |
| 正常场景 | 30 子页、0 sanitizer/断言、73 个模态对话框 |
| **Qt 布局/连接告警** | **0 条** |
| **泄漏基线** | 0 字节 / 0 次分配 ✓ |
| **完整回归矩阵** | **28 项、失败 0 项** |

（"与 HEAD 的 `tr()` 文本一致"这项**不能**直接比 —— HEAD 里这些文案还没包 `tr()`，
是后来的 i18n 轮次加上去的。所以判据换成"逐条以字面量存在"，这才是"没改写文案"的正确检验。）

### 86.4 方法上的收获

**同一个形状第二次出现时，先问"要不要提一层"**。上一轮把重复收进局部 lambda 是对的
（当时只有一处用），这一轮它出现了第二处 —— 于是提成文件级，而不是复制。
**抽象的层级应当由"用了几处"决定**：一处用就放在近处，两处以上才提上去。

---

---

## 八十七、"重复安装模型"补成检查：判据改成分支感知（第八十一轮）

### 87.1 回到 §七十九 那次失败

§七十九 把"重复施加"推广到"同一视图被装两次模型"时，报出 9 处、**全是误报**，
于是当时**收窄回只查阴影**，并把原因写进注释。快照（§八十一）把它列为未关闭的口子 5。

误报的两个原因现在都有了对策：

| 当时的问题 | 本轮的解法 |
|---|---|
| 文本计数看不见 `if/else`：两支各调一次被算成两次 | **按最近的包围块计数** —— 两支属于不同的块，天然不重复 |
| 助手识别的粒度不对：`fillGroupedTable(s)` 里的 `s` 是字符串 | 该类别**只统计直接调用**（`installTableModel(视图, …)` / `视图->setModel(…)`） |

### 87.2 判据：按"最近的块"计数

把代码切成块（函数体、每个 `{ }` 各算一块），统计**同一个块内**同一个视图的安装次数：

```cpp
if(数据为空) { installTableModel(ui->listView, new QStandardItemModel(this)); }   // 块 A
else         { installTableModel(ui->listView, model); }                          // 块 B
```

两个安装分处块 A / 块 B → **不算重复**（有意写法，正确接受）；
而在同一个块里连着装两次 → 真重复 ✓。

### 87.3 自检与结果

```
自检：注入同一块内两次安装 → 报告 1 处 → 能抓到 ✓
    sp_accounts.cpp 视图 ui->listView 在同一块内被设 2 次（installTableModel/installTableModel）
当前：同一块内重复安装模型: 0 处 ✓
```

注意这次是**零误报**：工程里那 7 处 `if/else` 写法全部被正确接受，而注入的真重复被抓到。

### 87.4 验证

| 检查 | 结果 |
|---|---|
| 新检查（自检 + 当前代码） | 自检能抓到 ✓；当前 **0 处** ✓ |
| **完整回归矩阵** | **29 项、失败 0 项** |
| 构建 | 0 错误 0 警告 |

### 87.5 方法上的收获

**误报的正确解法是"换判据"，不是"放弃检查"，也不是"加例外"**。
§七十九 时我选择了收窄（放弃这一类），那在当时是对的 —— 因为还没找到与代码形状匹配的判据；
本轮找到了"按包围块计数"这个形状，于是把它堂堂正正地加了回来。
**失败的那次记录（§七十九）正是这次能对症下药的原因**：如果当时只写"推广失败、已收窄"，
没有写下两个具体原因，这一轮就得从头再查一遍。

---

---

## 八十八、无线调试窗口拆成两个表单助手（第八十二轮）

### 88.1 先做快照，再动手

§八十六 那轮吃过一次亏：想核对"文案有没有变"，却发现没有改动前的基线（只能去和 `HEAD` 比，
而 `HEAD` 里这些文案还没包 `tr()`）。所以本轮**先把文件快照到 `.dsh/snapshots/`**，
再开始改。

### 88.2 拆法

一个槽里塞了两件事：第一步（无线调试 IP / 端口）与第二步（配对 IP / 端口 / 配对码）。
各自提成一个助手，都返回"建好的面板"：

```cpp
QWidget *MainWindow::buildWirelessOutPanel(QWidget *parent);    // 第一步
QWidget *MainWindow::buildWirelessPairPanel(QWidget *parent);   // 第二步
```

槽本身于是只剩"建窗口 → 调两个助手 → 装配 → 连接 → 显示"：

| | 之前 | 之后 |
|---|---|---|
| `on_WIFIBtn_clicked` | **75 行** | **31 行** |
| 两个助手 | — | 46 行（各自 23 行左右） |

### 88.3 改后核对（三项不变量）

| 不变量 | 结果 |
|---|---|
| 文件级 `tr()` 文案多重集 | **34 → 34，逐条一致** ✓ |
| 布局添加次数（`addWidget`/`addLayout`） | **11 → 11** ✓ |
| 输入框成员赋值（`le_*`） | 一致 ✓ |
| `WA_DeleteOnClose` / `applyDropShadow` / `setStyleSheet` 出现次数 | 4 / 3 / 16 → **4 / 3 / 16** ✓ |

### 88.4 行为验证

| 检查 | 结果 |
|---|---|
| 正常场景 | 30 子页、0 sanitizer/断言、72 个模态对话框 |
| **Qt 布局/连接告警** | **0 条** |
| 构建 | 0 错误 0 警告 |
| **完整回归矩阵** | 见下 |

**顺带修正一个判据**：这次统计告警时我在模式里多写了 `parent` 这个词，于是命中 20 条 ——
抽样一看全是测试驱动自己的诊断行（`… parent=basePage`）。改回
`QLayout:|already has a layout|Cannot connect|QObject::connect:` 后是 **0 条**。
**判据写宽一点就会把噪声算进来**，与 §七十九 的教训同源。

### 88.5 方法上的收获

**拆函数时要先写下"不许变的东西"**。这次的三项不变量（文案、布局次数、成员赋值）
都不是"顺手看一眼"，而是**先定下来、改完再跑**的断言 —— 它们让"我只是搬了代码"
这句话变成可检验的命题。这也是本项目里"纯搬动式重构"的标准做法：
**先固定不变量，再动结构**。

---

---

## 八十九、"搬动式重构守卫"固化成命令（第八十三轮）

### 89.1 把上一轮的手工做法变成工具

§八十八 拆函数时，我手工列了三条不变量（文案、布局次数、成员赋值）来证明"只是搬了代码"。
本轮把它做成 `refactor_guard.py`：

```
refactor_guard.py snapshot     # 存下不变量基线（默认全工程，46 个文件）
refactor_guard.py check        # 与基线比对，逐项给结论
refactor_guard.py selftest     # 自检：改一处文案必须被检查到
```

捕捉的不变量（都是"纯搬动代码时不该变"的量）：

- `tr("...")` 文案的**多重集**；
- 长度 > 20 的字符串字面量多重集（样式、图标路径）；
- 关键调用的出现次数（`addWidget`/`addLayout`/`setStyleSheet`/`connect`/`applyDropShadow`/
  `setGraphicsEffect`/`WA_DeleteOnClose`/`setModel`/`installTableModel`/`initThread`/
  `setValidator`/`setPlaceholderText`）；
- 每个 `.cpp` 里定义的函数名集合；全工程文件清单。

### 89.2 三项验证（工具也要自证）

| 场景 | 期望 | 实测 |
|---|---|---|
| 改一处 `tr("连接")` → `tr("连接设备")` | 必须抓到 | 报告 2 项（少了"连接"、多了"连接设备"）✓ |
| 不做任何改动 | 全部一致 | `不变量全部一致 ✓` |
| **把一行代码换个位置（纯搬动）** | **报告 0 项** | 0 项 ✓ |

第三项才是这把尺子的用途：**它必须对"结构变了但内容没变"保持沉默**。

### 89.3 顺带修掉扫描器里一个真 bug

写自检时它**没抓到**那处文案改动。追下去发现 `strip_dead_comments.py` 的扫描器
在处理字符串时漏写了一件事：

```python
if ch == in_str:
    in_str = None          # ← 闭合引号被吞掉，没有写进"代码文本"
```

于是所有从"剥注释文本"里做的**内容抽取**都会整段错位（`tr("…")` 抽出来是
`通过WI-FI连接设备), ` 这种）。修法是保留闭合引号，顺带把转义序列（`\"`）也原样写入。

这个 bug 之前没暴露，是因为**死代码分析的两侧用的是同一个有损函数**（有损但有对称性），
结论仍然正确；而守卫需要把抽取结果当**内容**用，才让它现形。
修完复查：死代码分析仍为 0 行、洁净度仍达标（与预期一致）。

### 89.4 为什么不把它接进回归矩阵

回归矩阵里的检查都是"任何时候都该通过"的；而守卫是**围绕一次重构**用的：
重构期间 `check` 就该报出差异（那正是它的职责）。所以它是**工作流工具**，不进门禁 ——
这一点也写进 §0 的验证纪律，避免以后有人把它当成回归项。

### 89.5 方法上的收获

**工具的"沉默"和"报警"同样需要被验证**。这次自检失败暴露了一个更早的问题：
一个"有损但对称"的函数在旧用途下完全够用，换一个用途就出错。
**把函数用在新的地方之前，要重新问一遍"它的保证是什么"**。

---

---

## 九十、`mainwindow.cpp` 拆分：911 → 714 行（第八十四轮）

### 90.1 为什么拆文件

函数级已经收得差不多（最长 46 有效行，除数据表外），但 `mainwindow.cpp` 仍是 **911 行**，
接近次大文件（`pagemaker.cpp` 429 行）的两倍 —— 里面混了四件事：设备列表管理、监视线程、
页面管理、以及两个辅助窗口。文件级拆分是这一维度剩下的最大收益。

### 90.2 拆法：175 行对话框代码移出去

新建 `mainwindow_dialogs.cpp`，移入：

| 移入内容 | 行数 |
|---|---|
| `makeFormLabel` / `makeFormRow`（两个表单助手） | 17 |
| `buildWirelessOutPanel` / `buildWirelessPairPanel` | 45 |
| `on_WIFIBtn_clicked` / `connectWIFIDev` | 52 |
| `on_pluginBtn_clicked` / `slot_save_magisk` | 61 |
| 合计 | **175** |

`mainwindow.cpp`：**911 → 714 行**；新文件 199 行（含自己的 include 集）。两份构建文件同步登记
（一致性检查：磁盘 / CMake / .pro 各 **132** 个）。

### 90.3 守卫升级：加一层"全工程聚合"

搬动代码会让**逐文件**比对失真（函数从 A 文件跑到 B 文件，A 的 `tr()` 文案必然"少了"）。
所以给守卫加了**全工程聚合层**：文案、长字面量、关键调用次数、函数集合的**总和**。

本次搬动后的守卫输出：

```
聚合层问题: 0 项 （一致 ✓）
逐文件/清单提示: 10 项（预期：函数在两个文件间移动、新增一个文件）
    新增文件: ['mainwindow_dialogs.cpp']
    mainwindow.cpp tr() 文案 少了 16 项: [...]
    mainwindow.cpp 调用 addWidget: 16 → 1
退出码=1（1 = 有差异，需要人工判读）
```

**聚合层 0 项差异**就是"这是纯搬动"的证据；逐文件那 10 项差异正是搬动的预期结果，
退出码 1 的作用是"提示有人来看一眼"，而不是"失败"。

### 90.4 过程中的两次自伤（都记下来）

1. **新增源文件后必须重新 configure**。拆分后场景日志只有一行
   `timeout: failed to run command './QtAdbAsan': No such file or directory` ——
   GLOB 收集的 harness 没把新文件纳入，构建"成功"但二进制不在。
   这个坑 §四十七 已经记过，**又踩了一次**：只要动了源文件集合，先重新 configure。
2. **`pkill -f` 匹配到了自己的命令行**。为了停掉后台回归，我用了
   `pkill -f 'run_regression.py'`，而这条命令自身的命令行里就含这个字符串 ——
   于是把自己的 shell 一起杀了（任务被 SIGTERM）。正确写法是用
   `pgrep -f 'run[_]regression'` 这种带字符类的模式，或者记 PID。

### 90.5 验证

| 检查 | 结果 |
|---|---|
| **守卫聚合层** | 0 项差异（纯搬动）✓ |
| 构建 | 0 错误 0 警告 |
| 构建列表一致性 | 磁盘 / CMake / .pro 各 **132** 个 ✓ |
| 正常场景 | 30 子页、0 sanitizer/断言、73 个模态对话框 |
| **Qt 布局/连接告警** | **0 条** |
| 快速回归（构建 + 场景 + 6 项检查） | 全部通过 |

（完整矩阵留到下一轮跑 —— 本轮的改动只涉及"代码在哪个文件里"，行为面已由上述几项覆盖。）

### 90.6 方法上的收获

**"纯搬动"需要一个跨文件的判据**。逐文件比对在搬动场景下必然报警，于是补上聚合层 ——
这不是为了"让检查通过"，而是因为它**恰好刻画了搬动的语义**：内容总和不变、只是位置变了。
**判据的形状要跟改动的形状对齐**，这已经是本项目里第五次出现同一条经验（§六十五、§七十七、
§七十九、§八十三、本轮）。

---

---

## 九十一、补跑完整回归，并修正 README 里的过时指路（第八十五轮）

### 91.1 先把上一轮欠的账补上

§九十 拆完文件后只跑了快速模式（且当时还有一个构建目标因未重新 configure 而失败）。
本轮先跑**完整矩阵**确认：**29 项、失败 0 项**（含 UBSan 三项、泄漏基线连跑三次）。

### 91.2 顺手核对仓库自带的文档

85 轮改动之后，`README.md` 是否还准？逐条核对下来，大部分仍然正确
（CMake/qmake 双构建、Qt 6.2+、C++17、Windows/Linux 通用），但有一处**过时指路**：

```
> 注意：`QtAdb.pro` 中的 `CONFIG += c++11` 与 Qt 6 冲突（Qt 6 要求 C++17），
> 使用 qmake 构建前请删除该行；CMake 构建已固定为 C++17。
```

而 `QtAdb.pro` 里现在写的是 `CONFIG += c++17` —— 这条提示会让用户去找一行**根本不存在**的配置。
已改为如实说明：

```
> 说明：两套构建描述都已固定为 C++17（QtAdb.pro 的 CONFIG += c++17 与
> CMakeLists.txt 的 CMAKE_CXX_STANDARD 17），无需再手工改动。
> （早期版本的 QtAdb.pro 曾是 CONFIG += c++11，与 Qt 6 冲突，需要删除该行 —— 现已修正。）
```

另外做了一次**机械核对**：把 README 里反引号引用的文件/路径抽出来，逐个检查是否存在 ——
4 条引用全部存在 ✓（没有指向已删除文件的死引用）。

### 91.3 验证

| 检查 | 结果 |
|---|---|
| **完整回归矩阵** | **29 项、失败 0 项** |
| README 引用的路径 | 4 条全部存在 ✓ |
| 构建 | 4 个目标 0 错误 0 警告 |

### 91.4 方法上的收获

**代码之外的东西也会随轮次过期**。README 里那条提示在它写下的当时是**对的**
（那时 `QtAdb.pro` 确实是 c++11），是后来我把构建配置改成 C++17 时**没有回头更新它**。
这与本项目反复出现的另一类问题同源：**同一件事被写在两个地方，就一定会有一天不一致** ——
区别只是"代码里的两个地方"由检查守着（构建列表一致性、条目↔子页映射……），
而"代码与文档之间"目前还只能靠人偶尔对一遍。至少这一轮证明了：**偶尔对一遍是有收获的**。

---

---

## 九十二、"文档与代码一致性"也做成检查（第八十六轮）

### 92.1 把 §九十一 的手工会对变成命令

§九十一 手工核对了 README，发现一条过时指路（教用户删一行早已不存在的 `CONFIG += c++11`）。
本轮把它机械化 —— `check_docs.py` 检查两类引用：

| 类别 | 判据 |
|---|---|
| **路径类**（反引号里含 `/` 或工程内已知扩展名） | 必须在仓库里存在（允许相对 `QtAdb/` 或仓库根） |
| **构建配置类**（含 `+=` 或 `CMAKE_`） | 必须在对应构建文件里出现；形如 `文件: 配置` 的按文件核对 |

自带注入式自检：往临时副本的 README 里塞一条不存在的路径与一条不存在的配置，必须都抓到：

```
自检：注入两条假引用 → 报告 2 项 → 能抓到 ✓
    README.md: 引用了不存在的路径 QtAdb/不存在的文件.cpp
    README.md: 声称 QtAdb.pro 含 'CONFIG += c++99'，但实际没有
当前：文档引用一致 ✓（路径存在、构建配置与构建文件相符）
```

已接入回归矩阵（**30 项**）。

### 92.2 顺着它又发现一条过时事实

写检查时顺手核对了 README 里"嵌入资源 33.6 MiB"这句 —— 实际算一遍 `.qrc` 列出的文件：

```
嵌入资源：128 个文件、合计 2.7 MiB
```

**差了 12 倍**。原因找到了：`QtAdb/Resources`（约 14 MB，含 `platform-tools`）是**复制**到
可执行文件目录的，并不嵌入 —— 原句把两者混为一谈。已改为：

> 仅依赖 `Qt6::Widgets`；**嵌入**资源（`.qrc` 列出的 128 个文件）合计 **2.7 MiB**，已使用
> `qt_add_big_resources` 处理。
> 另有 `QtAdb/Resources`（约 14 MB，含 `platform-tools`）**不会被嵌入**，而是在构建时复制到
> 可执行文件目录（见下一条）。

### 92.3 检查器自己也误报了一次

新写的文字里有 `` `.qrc` `` 这种"纯扩展名"的引用，检查器把它当路径 → 报"引用了不存在的路径 `.qrc`"。
判据改成"点号前至少有一个字符才算路径"。**这已经是本项目第六次修正判据**
（§六十五、§七十七、§七十九、§八十三、§九十，本轮）—— 每次都是"报告与事实不符"引出来的。

### 92.4 验证

| 检查 | 结果 |
|---|---|
| 文档一致性（自检 + 实检） | 自检能抓到两条假引用 ✓；实检一致 ✓ |
| 快速回归 | **18 项、失败 0 项**（完整矩阵共 30 项） |
| 构建 | 0 错误 0 警告 |

### 92.5 方法上的收获

**"代码与文档不一致"是可以被机器守住的 —— 只要把文档里的断言写成可判定的形式**。
路径存在与否、配置项在不在文件里，都是二值判断；而"支持 WSA""暂不支持更早的 Windows"
这类叙述则不能。**文档里凡是可以写成断言的部分，就值得写成断言** ——
这也是本轮那条 12 倍误差能被发现的原因。

---

---

## 九十三、补上"连接告警"这条判据（第八十七轮）

### 93.1 一个一直没被断言的东西

工程里大量使用 Qt5 风格的**字符串式连接**（`SIGNAL()/SLOT()`，共 69 行）：

```cpp
connect(this->ui->back_to_basePage, SIGNAL(clicked()), parent, SLOT(slot_destroySonPage()));
```

这类连接的**名字写错了不会编译失败** —— Qt 只在运行到 `connect()` 时打印一条
`QObject::connect: No such slot ...`，然后**默默地什么也不连**。
而我的回归矩阵只断言 sanitizer 与 `ASSERT failure`，**没有断言这类告警** —— 这正是漏洞的温床。

### 93.2 一次"先误判、后修正"的排查

翻历史日志时看到 `No such slot QWidget::slot_destroySonPage()`，于是查 `slot_destroySonPage`：
它被连接 **19 处**，而全工程搜不到定义 —— 我一度判断"19 个返回按钮全是死的"。

**再查一步就推翻了**：`basePage::slot_destroySonPage()` **确实存在**
（`basepage.h:83` 声明为私有槽、`basepage.cpp:121` 有实现）。告警里的接收者是
`QWidget::...` —— 说明**当时那个接收者是个普通 `QWidget`**，也就是**我的探针**
拿普通 QWidget 当页面父对象造成的，而真实应用里父对象始终是 `basePage`。

**测量证实了这个判断**：

| 运行 | 连接告警 |
|---|---|
| **应用场景（真实父对象）** | **0 条** ✓ |
| `--effects` 探针 | 39 条 ✗（测试环境差异） |
| `--factories` 探针 | 72 条 ✗（同上） |

于是两头都做了修正：**探针的宿主改成真实的 `basePage`**（与应用契约一致），
并把"连接告警为 0"写进判据 —— **场景严格、探针只提示**（探针里仍有以 `nullptr`
为父对象的构造，属测试环境差异，不该让门禁误报）。

改后：探针告警 39 / 72 → **1** 条（唯一一条来自 `nullptr` 父对象），应用场景保持 **0** ✓。

### 93.3 验证

| 检查 | 结果 |
|---|---|
| 应用场景连接告警 | **0 条**（新判据生效） |
| 探针连接告警 | 39 / 72 → **1**（已注明为环境差异，只提示） |
| 探针结论 | 全部符合预期（阴影 200、配对 36 组不符 0 …） |
| 构建 | 0 错误 0 警告 |

### 93.4 方法上的收获

**"不会编译失败的错误"最值得加断言**。字符串式 connect、`QMetaObject::invokeMethod` 的
方法名、`#XXX#` 解释键……这些都是**运行时才判定、失败时静默**的东西。
本轮补上的这条判据成本极低（在既有日志里数一行），却能守住一整类问题。

另外记一次**自我纠错**：我从"19 处连接 + 0 处定义"直接推到"19 个按钮是死的"，
差一步就把一个**不存在的产品缺陷**写进报告。**结论要落到"能不能被观测证实"上** ——
控制台告警的接收者类型（`QWidget` 而不是那个页面类）就是那条推翻结论的证据。

---

---

## 九十四、"死声明"审计：6 个孤儿声明（第八十八轮）

### 94.1 又换了一个维度

这轮换了一种死代码：**注释掉的代码**（§七十~§七十二）与**调试输出**（§七十三）都清过了，
但还有一类更隐蔽的 —— **声明了却从未被引用的成员**。它不会编译失败、不会被任何检查发现，
却会让读者以为存在某个接口。

判据很简单：名字在"全部 `.cpp` + `.h`"里只出现 1 次（即只有声明那一处）。
按**名字计数**是安全的 —— Qt 的字符串式连接（`SLOT(name())`）也会让名字出现。

### 94.2 结果：6 个，全部是真死声明

| 声明 | 出处 | 性质 |
|---|---|---|
| `hideCurrentPage()` | `mainwindow.h` | 从未实现过的主窗口辅助方法 |
| `setTitle(QString)` | `sonPages/sonpagebase.h` | 从未使用的 setter |
| `permissionGroupsExplained(QStringList)` | `textexplainer.h` | 旧设计的遗留 |
| `slot_get_words_after(QString, QString)` | `textexplainer.h` | **实现已在 §七十一 作为死代码删除**，声明成了孤儿 |
| `slot_explain_cpu_output(QString)` | `textexplainer.h` | 同上 |
| **`textExplained(QString)`** | `textexplainer.h` | **一个从未发射、也从未被连接的信号** |

最后那个尤其值得清：一个不存在的信号会让读者以为"有个叫 textExplained 的对外接口"。

已全部删除，`textexplainer.h` 里空掉的 `signals:` 段也一并清掉。

### 94.3 固化成检查

`check_dead_decls.py`（自带注入式自检：往临时副本塞一个只声明不使用的成员，必须抓到）：

```
自检：注入一个只声明不使用的成员 → 报告 1 个 → 能抓到 ✓
当前：死声明: 0 个 ✓（头文件里声明的成员都有引用）
```

已接入回归矩阵（**31 项**）。

### 94.4 验证

| 检查 | 结果 |
|---|---|
| 死声明检查（自检 + 实检） | 自检能抓到 ✓；当前 **0 个** ✓ |
| **完整回归矩阵**（清理前状态，含连接告警判据） | **30 项、失败 0 项** |
| 构建 | 0 错误 0 警告 |

### 94.5 关于那 91 条未译词条（本轮顺手查清了）

快照里挂着的"80 条激活器长文案"查清后其实是 **91 条、分散在 36 个上下文**里
（多数上下文只差 1 条），属于**散落的新文案**而非系统性缺口。它是**内容工作**、
与代码质量不同轴，因此本轮不动它 —— 但把它从"80 条长文案"更正为"91 条散落文案"，
记在 §0 里，免得以后再凭印象估。

### 94.6 方法上的收获

**"死"有三种，检查要分别对**：注释掉的代码（文本层面）、调试输出（调用层面）、
**只声明不引用的成员**（符号层面）。三种都已各自有了检查或清理记录。
这一轮的价值不在 6 行代码，而在于**又找到一个此前没有检查覆盖的维度** ——
而它一次就抓出了 6 个真东西。

---

---

## 九十五、两个大文件的结构复查 + 一次主动放弃的检查（第八十九轮）

### 95.1 结构复查：结论是"不用动"

| 文件 | 规模 | 最大的函数 | 结论 |
|---|---|---|---|
| `adbclient.cpp` | 424 行 / 21 个函数 | `handleProcessFinished` 44 有效行（§五十六 逐行审过，密集护栏注释） | **健康** |
| `pagemaker.cpp` | 429 行 / 18 个函数 | `createPage_acvitator` 46 有效行（数据表） | **健康** |

其余函数都在 25 有效行以下；两个文件都没有重复块（§四十六 的去重扫描与 §七十九/§八十七
的"重复安装"检查都会覆盖这类问题）。**有数据支撑的"不动"也是一种结论** ——
写下来，免得下一轮又从头看一遍。

### 95.2 试着做"声明↔定义签名一致性"检查

想法的来源：§九十四 查出了 6 个"只声明不引用"的成员。那**声明与定义签名不一致**呢？
被调用的函数不用管（编译器/链接器会报），但**没人调用的函数**签名写错了是没人管的。

写了个检查，量出来的结果是：

```
比对了 300 个声明 / 288 个定义
不一致 65 项
```

**逐条核对：65 项全是抽取方式造成的假阳性，零真不一致。**

| 假阳性来源 | 例子 |
|---|---|
| 构造函数定义被**冒号初始化列表**吞进参数列表 | 定义被抽成 `('QWidget *parent) : basePage(parent), ui(new Ui::',)` |
| **无名参数**与有名参数被"去形参名"规则处理得不一致 | 声明 `('',)`、定义 `('QString',)` |
| **elaborated 类型**写法 | 声明 `class QLineEdit *`、定义 `QLineEdit *` |
| 模板实参里的**形参名**被当成类型的一部分 | `std::function<void(int exitCode, …)>` |

### 95.3 决定：不做这个检查

理由有两条，都很实际：

1. **缺陷类极窄**。签名不一致只有在"被调用"时才有后果，而那种情况编译器/链接器一定会报 ——
   真正漏网的只有"**定义了、签名不一致、而且从来没人调用**"这一种，本工程里**一个都没有**；
2. **判据要写对成本很高**。要正确处理初始化列表、无名参数、elaborated 类型、模板实参里的名字……
   而每一条都是"为了精确而精确"，与它守住的极窄风险不成比例。

于是把脚本删掉（**不作为门禁**），只把测量结论与理由写在这里 ——
与 §七十九 那次"推广失败后收窄"是同一个判断，区别是这次**在发布前就做出了判断**：
先量一量它到底能测出什么，再决定要不要它。

### 95.4 方法上的收获

**"先量一下这个检查能测出什么"，比"先把它加进门禁"便宜得多。**
这一轮花了很少的时间就得到一个明确结论：这一类缺陷在本工程里不存在，
而这个检查要写对却相当费劲。**把"不做的决定"也写进文档**，
是为了让后来的轮次不必重新推一遍同样的推理 —— 文档不只记录做了什么，也记录**为什么不做什么**。

---

---

## 九十六、"死成员"审计：16 个（第九十轮）

### 96.1 第三个"死代码"维度

§九十四 清了**只声明不引用的成员函数**，这一轮查**成员变量**。判据两类：

| 类别 | 含义 |
|---|---|
| **从未被引用** | 名字在整个工程里只出现一次（声明处） |
| **只写不读** | 只出现在赋值左侧 —— 状态被维护着，却从来没有人读它 |

第二个类别尤其值得警惕：它和 §五十一 清掉的 `arg_s` / `arg_d` / `arg_u` 是同一种东西 ——
**看起来在记录状态，其实没人用**。

### 96.2 结果：231 个成员里，10 + 6 个是死的

| 处 | 成员 | 性质 |
|---|---|---|
| `pagelistitem.h` | `shadowEffect_icon`、`isSelectable` | 代码在 §七十一 被作为死代码删除后留下的孤儿 |
| `animationwidget.h` | `graphicsOpacityEffect`、`opacityLoadAnimation` | 被后来的 `animation` 取代 |
| `mainwindow.h` | `tmpPage`、`WCMPage2`、`plugin` | 早期页面管理的遗留 |
| `sp_installer.h` / `tp_packageprofile.h` | `movie`、`thread_btn` | 未使用的资源/线程句柄 |
| `sp_packages.h` | `package_name`、`current_package` | 未使用的字符串 |
| `basepage.h` | `parents`、`lastPage`、`father` | **只写不读**：构造时赋值，之后无人读 |
| `sonpagebase.h` / `sp_activator.h` | `parents` | 同上 |

已删除声明 **16 行** + 赋值 **5 行**（`parents = parent;`、`lastPage = NULL;`、`father = parent;`、
`isSelectable = true;`），构建 **0 错误 0 警告**。

### 96.3 固化成检查（并修掉一个误报）

`check_dead_members.py` 的第一版把 `m_nextId` 报成"只写不读" —— 而它的用法是
`request.id = m_nextId++;`：**`++` 就是读取**。判据因此明确为：

> `x = …` 算写；`++x` / `x++` / `x += …` 等**复合形式一律算读取**。

自检（注入一个只声明不使用的成员）与实检：

```
自检：注入一个死成员 → 报告 1 个 → 能抓到 ✓
当前：检查了 231 个成员变量 → 死成员: 0 个 ✓
```

已接入回归矩阵（**32 项**）。

### 96.4 验证

| 检查 | 结果 |
|---|---|
| 死成员检查（自检 + 实检） | 自检能抓到 ✓；当前 **0 个** ✓ |
| 快速回归（含正常场景与新检查） | **20 项、失败 0 项** |
| 构建 | 0 错误 0 警告 |

### 96.5 方法上的收获

**"死"的四个维度现在都覆盖了**：注释掉的代码（文本）、调试输出（调用）、
只声明不引用的函数（符号）、**只写不读的成员变量（状态）**。
最后一个最隐蔽 —— 它甚至会被编译器认为"用到了"（有赋值），
而唯一能发现它的办法就是问一句：**"这个状态，谁读？"**

---

---

## 九十七、达成清单：目标四个维度的证据（第九十一轮）

> 本节的用途：把"目标是否达成"这个问题，变成一串**可复现**的数字与命令。
> 全部结论都可以用 `.dsh/probe/run_regression.py` 一条命令重跑验证。

### 97.1 健壮（Robustness）

| 证据 | 数值 |
|---|---|
| 完整回归矩阵 | **32 项、失败 0 项**（构建 ×5、场景 ×8、UBSan ×4、探针 ×6、检查 ×9） |
| 内存泄漏（完整应用场景，LSan） | **0 字节 / 0 次分配**（连跑 3 次一致） |
| 未定义行为（UBSan 下 4 条路径） | **0 处** |
| 崩溃/越界修复 | 畸形输出越界解析、`tmp[1]` 断言、阴影句柄变空指针、验证器无父对象泄漏… |
| 资源泄漏修复 | 页面/对话框/通知控件、模型累积、动画缺父对象、堆上的 `QMetaObject::Connection`、退出时在途请求 |
| 静默失效修复 | 监视命令缺 `-s`、别名被覆盖、`QSettings` 写入失败不可见、未登记解释键 |
| 静默连接失败 | **应用场景 0 条** `QObject::connect:` 告警（新判据，§九十三） |

### 97.2 可维护（Maintainability）

| 证据 | 数值 |
|---|---|
| 检查脚本 | **23 个**（各自带自检或注入式验证）+ 8 个探针 + 1 条回归入口 |
| 隐式约束→命令 | 条目↔子页映射、允许子页集合、两块构建列表、工厂配对、阴影覆盖/位置/次数、模型安装、文档引用、死代码四类 |
| 重复样板 | 收成具名助手：`addPageItem`/`addInfoItem`/`addInfoItemAsync`/`installTableModel`/`enableAllItems`/`applyDropShadow`/`makeFormLabel`/`makeFormRow` |
| 样式集中 | 7 个重复样式串 → `uistyles.h` 的 10 个常量；全工程重复的长样式串 **0 种 0 处** |
| 文件规模 | 最大 `mainwindow.cpp` **714 行**（拆分前 911）、`pagemaker.cpp` 429、`adbclient.cpp` 424 |
| 冗余引用 | 多余 Qt `#include` **0 处**、孤儿头文件 **0 个**（§九十七 本轮核查） |

### 97.3 可拓展（Extensibility）

| 扩展动作 | 现在的成本 |
|---|---|
| 新增子页 | 子页表**加一行** |
| 新增激活器 | 激活器数据表**加一行** |
| 新增列表条目 | `addPageItem(...)` **一行调用** |
| 新增页面的投影 | `applyDropShadow(widget)` **一行调用** |
| 新增"呈现方式"选项 | 选项表**加一行**（对话框与 `setArg` 两张表一一对应） |
| 新增页面 | 数据表 + 工厂注册（工厂配对探针会立刻验证 36 组映射） |
| 枚举扩展 | 枚举 `switch` **刻意不写 `default`**，新增枚举值时编译器告警（§七十四） |

### 97.4 易读（Readability）

| 指标 | 第 1 轮 | 现在 |
|---|---|---|
| `qDebug()` | 278 | **0** |
| 注释掉的代码（同判据） | 474 | **0** |
| 死声明 / 死成员 | — | **0 / 0** |
| 函数中位数 | — | **5 行**（342 个函数） |
| 超过 60 行的函数 | — | **1 个**（数据表） |
| 最长函数 | `explainError` 102 行 | **91 行**（数据表；`explainError` 现 25 行） |
| `tr()` 调用 | 0 | **213** |
| README 准确性 | — | 路径引用与构建配置**由检查守住**（§九十二） |

### 97.5 上下文固化

`QtAdb-审计与优化方案.md`（本文件，**6500+ 行、97 节**）逐轮记录：做了什么、为什么这样做、
以及**为什么不做什么**（§七十九 收窄、§九十五 主动放弃）。每节的结论都带可复现的命令。

### 97.6 有意留下、不算未完成的部分

1. **91 条未译词条**：内容工作（英文文案），与代码质量不同轴，且分散在 36 个上下文；
2. **10 处投影替换**：位于死代码注释旁，删注释会损失历史信息，故保留；
3. **补充检查的适用边界**：`--effects` 数覆盖、`check_duplicate_*` 数重复与位置 ——
   各自只覆盖特定形状，边界都写在脚本注释与 §六十七/§七十九/§八十三 里；
4. **探针环境差异 1 条连接告警**：来自探针以 `nullptr` 为父对象的构造，已标注为提示而非失败。

### 97.7 一条命令复现全部结论

```bash
python3 .dsh/probe/run_regression.py          # 32 项：构建 / 场景 / UBSan / 探针 / 检查
python3 .dsh/probe/run_regression.py --quick  # 20 项：日常快速回归
```

---

*§一–§七 只读审计；§八 M0；§九 P0 止血；§十–§十八 M1（异步化 + 删除 adbProcess）；§十九–§二十 M2（状态枚举、固定数组与死代码清理）；§二十一 命令层具名化；§二十二 补回输出解释功能；§二十三 删除项体检与指针/定时器生命周期；§二十四 M3 设备热插拔；§二十五 修复设备别名；§二十六 解析器压力测试；§二十七 验证覆盖面修正与刷新行为修复；§二十八 压力场景矩阵与页面重建泄漏修复；§二十九–§三十一 国际化；§三十二 工厂注册表化（数据表 + 行为等价验证）；§三十三 上下文固化 + 关级审计（修复监视命令缺设备前缀）；§三十四 别名显示修复 + 死代码清理 + 纯函数抽取；§三十五 权限页空指针崩溃 + 刷新泄漏模型；§三十六 模型生命周期统一治理（基类 installTableModel）；§三十七 LSan 定性残余增长 + setModel 收敛到每轮一次；§三十八 权限页填表逻辑拆分 + 空调用清理；§三十九 applyDeviceList 拆分 + 逐设备重复重建修复；§四十 initBasePage 重构（白建页面/越界/解锁）；§四十一 坏味道机械扫描 + 别名写入失败可见；§四十二 “结果不检查”全局排查（字体族名、mkpath）；§四十三 条目创建收成 addPageItem（40 处，探针校验等价）；§四十四 异步信息条目收成 addInfoItem（createPage_devInfo 117→69 行）；§四十五 回调路径同收束（devInfo_rec 91→36 行）+ 关级审计；§四十六 允许子页不再手写数字（enableAllItems + 21 行等价校验）；§四十七 条目↔子页配对断言（36 组，0 不符）；§四十八 两份构建文件列表一致性（含校验器自测）；§四十九 解释键封闭集合的登记检查；§五十 全工程函数排行 + explainError 表化（102→25 行）；§五十一 sp_vibrate::refresh 收束（100→15 行）；§五十二 重复块扫描 + 工厂表压成一行（298→248 行）；§五十三 投影样板收成 applyDropShadow（逐批进行）；§五十四 第二批（34→64 处）+ 工具自证；§五十五 收尾（64→75 处）并躲过一次空指针；§五十六 第 50 轮里程碑：全项目指标对照与全量验证；§五十七 setArg 枚举化（65→32 行，删掉 3 个死状态）；§五十八 选项对话框表化 + about 页死代码清理；§五十九 按键页控件清单改遍历（--effects 探针）；§六十 追回 17 处被我误删的阴影（双向审计）；§六十一 验证器泄漏 52→0（sp_vibrate/sp_scales）；§六十二 页面生命周期探针 + 一个未决的 LSan 残项；§六十三 关闭该未决项（动画缺父对象）；§六十四 完整场景泄漏清零（44482 字节 → 0）；§六十五 泄漏基线固化成命令 + 修正主窗口阴影位置；§六十六 位置审计（阴影 79 处 0 错位）；§六十七 补齐三批并跑通（加路径守卫与空过自检）；§六十八 回归矩阵收成一条命令（20 项，含失败路径验证）；§六十九 热插拔自动化（21 项全绿）；§七十 注释掉的代码清理 113 行（两道自证）；§七十一 多行死注释块再清 261 行（含尺子的两次拦截）；§七十二 扫描器改字符串感知、清理收口（判据下 0 行）；§七十三 调试输出清零（330 → 0）；§七十四 可拓展性扫描（无 default 的枚举 switch 有意保留）+ 洁净度检查；§七十五 回归并行化（10→4 分钟）；§七十六 修掉真正的泄漏源（堆上的 QMetaObject::Connection）；§七十七 泄漏基线连跑 3 次 + 清掉重复施加的阴影列表；§七十八 把"重复施加"补成检查；§七十九 推广到模型安装失败并如实收窄；§八十 UBSan 从"只编译"变成"真跑"（0 未定义行为）；§八十一 第 75 轮质量快照；§八十二 UBSan 补齐热插拔（28 项）+ 死代码分析器剥行尾注释；§八十三 重复样式串抽成 uistyles.h（17 处）；§八十四 跨行拼接 4 处也收干净（重复归零）；§八十五 无线调试窗口改用 makeLabel/makeRow；§八十六 助手提到文件级 + 修 Magisk 窗口生命周期；§八十七 分支感知的"重复安装模型"检查；§八十八 无线调试窗口拆成两个表单助手；§八十九 "搬动式重构守卫"固化成命令；§九十 mainwindow.cpp 拆分 911→714 行；§九十一 补跑完整回归 + 修正 README 过时指路；§九十二 文档一致性做成检查 + 修正资源大小误差；§九十三 补上"连接告警"判据；§九十四 "死声明"审计（6 个孤儿声明）；§九十五 两个大文件结构健康 + 一次主动放弃的检查；§九十六 "死成员"审计（16 个）；§九十七 达成清单：目标四个维度的证据（32 项全绿）；§九十八 提交之后：以 HEAD 为基线的检查会失效（基线必须固定）；§九十九 分支品牌与 README 重构（QtAdb-Reforged）；§一百 关系澄清：同一作者的续作，而非第三方 fork。所有改动均在 git 工作区中，可逐项审阅或整体回退。*

---

## 九十八、提交之后：以 HEAD 为基线的检查会失效（第九十二轮）

### 98.1 提交之后，一条检查立刻"失败"了

把 91 轮的重构结果提交为基线（`4d807dd`）之后，快速回归里 **`位置审计` 立刻报失败**：

```
检查 位置审计   ★ 失败  退出码 1；输出尾部: 信息条目 已核对 9 处；位置不符 0 处
                / 模型安装 已核对 18 处；位置不符 0 处 / 合计问题: 1
```

注意"阴影"那一条根本没出现在输出里 —— 说明它**一处也没核对到**，
于是脚本自己的自检（`checked == 0` 即判失败）生效了。

**根因**：这个脚本用 `git show HEAD:<file>` 取"原始站点"，用来和当前代码比对。
提交之前 `HEAD` 还是重构前的代码，一切正常；**一提交，`HEAD` 就变成了重构后的代码**，
脚本于是找不到任何 `->setGraphicsEffect(...)` 原始站点。

### 98.2 修法：把基线固定下来

```python
BASE_REV = sys.argv[2] if len(sys.argv) > 2 else '191205b'   # 上游 1.7 = 重构开始前的提交
```

修正后：**阴影 77 处 / 信息条目 9 处 / 模型安装 18 处，0 处位置不符** ✓；
传一个不存在的提交号会**大声失败**（`⛔ 找不到基线提交 …`）而不是静默通过。

### 98.3 顺带修掉一处真实的构建漂移

提交前核对时发现：`.pro` 的 `TRANSLATIONS` 指向 **空的** `QtAdb_zh_CN.ts`（0 词条），
而 `CMakeLists.txt` 指向 `translations/QtAdb_en.ts` —— 也就是说 **qmake 构建根本不会嵌入英文翻译**。
两份构建描述的源文件列表一直有检查守着，但**翻译列表没有**。

两件事都做了：

1. `.pro` 的 `TRANSLATIONS` 改为指向同一个 `translations/QtAdb_en.ts`（源语言是中文，zh_CN 本就不需要翻译），并删掉那个空壳文件；
2. `check_build_sync.py` 增加 `.ts` 比对 —— 顺手用"故意改回 zh_CN"验证了它确实能抓到漂移。
   （判据也修了两次：CMake 那侧抽出来的路径带前导 `/`，要先规范化再比较。）

### 98.4 方法上的收获

**检查的基线不能是"会移动的东西"。** 凡是拿"历史状态"当参照的检查，都必须把历史**固定**
下来 —— 提交号、快照目录、基线 JSON 都行，唯独不能用 `HEAD`：因为"提交"是本项目里最正常的动作，
而它会让这类检查失效；更糟的是，**失效的表现恰好是"报出问题"**，最容易把人引向错误的方向
（我第一眼确实以为重构里出现了位置错误）。

这与本项目反复出现的那条经验同源（§六十五、§七十七、§七十九、§八十三、§九十）：
**判据的形状要跟现实的形状对齐** —— 这里"现实"是"基线会随提交移动"。

---

## 九十九、分支品牌与 README 重构（第九十二轮）

### 99.1 仓库名与血缘

上线为 **`QtAdb-Reforged`**。取舍写在 README 里：

- 保留 `QtAdb` 前缀 = 连续性与血缘（上游 `LapplandSp/QtAdb`，MIT）；
- 加 `-Reforged` = 与上游及其它同名项目区分（上游作者在文档里也说明过重名问题）；
- **不把 AI 放进名字**：AI 用在"重构代码"这个过程里，产品本身没有 AI 功能。
  叫 `-AI` 会让人期待对话/自动化之类的功能，落差比"名字里没有 AI"更伤。
  AI 那句话放在 README 的副标题里，并且**带上可验证的数字**。

### 99.2 README 重构的要点

| 变化 | 说明 |
|---|---|
| 标题与副标题 | `QtAdb-Reforged` + "AI 辅助重构分支、不新增功能"一行说清 |
| 与上游的关系 | 独立一节：上游链接、许可与版权、提问去处（本分支问题请提到本仓库） |
| **这个分支改了什么** | 指标对照表（`qDebug` 278→0、注释代码 474→0、最大文件 911→714…）+ 结论指向审计文档 |
| **诚实边界** | 运行验证只在 Linux 完成；Windows 未实跑；qmake 未实际构建；翻译运行时加载未端到端验证 |
| 功能清单 | 保留上游内容（本分支不新增功能） |
| Q&A | 保留上游条目；新增"这个分支加了 AI 功能吗""为什么还叫 QtAdb"；重写"重名"那条 |
| 构建 | 保留并补充：**两份构建描述的翻译列表必须一致** |
| 资源与致谢 | 图标/字体来源保留；明确 MIT 原始版权 |
| 联系 | 只留本仓库 issue；上游社群归上游（避免把本分支的问题投到原作者那里） |

同时把审计文档收入仓库：`docs/AI-REFACTOR-AUDIT.md`（含一段说明：文中引用的验证设施
`.dsh/` **未随仓库发布**，命令无法直接运行，保留是为了让结论可复核）。

界面上的品牌只改了一处：`setWindowTitle("QtAdb Reforged")`（**可执行文件仍叫 `QtAdb`**，
因此 `~/.config/QtAdb/` 下的设备别名不受影响 —— 这是上一节讨论过的坑）。关于页的文案**没动**，
因为它属于可翻译词条，改动会牵动 `.ts` 与翻译探针的期望值，留作可选后续。

### 99.3 文档检查器又修了三次判据

新 README 触发了三次误报，判据因此收敛为"**必须以仓库内前缀开头才算路径**"：

| 误报 | 真身 |
|---|---|
| `LapplandSp/QtAdb` | GitHub 仓库引用，不是本地路径 |
| `QtAdb_en.qm` | 构建产物（`.gitignore` 已忽略，不该要求它存在） |
| `Windows.h` | 系统头文件 |

修完：自检仍能抓到注入的假引用 ✓；实检"文档引用一致" ✓。**这已是本项目第八次修正判据。**

### 99.4 提交与验证

| | |
|---|---|
| 提交 1 | `4d807dd` 代码基线（104 文件、+6801 / −4690） |
| 提交 2 | 本篇所在提交：README 重构 + `docs/` 审计文档 + 窗口标题 |
| 验证 | 构建 0 错误 0 警告；快速回归 20 项（含连接告警、泄漏、位置、洁净度、三类死代码、文档、重复施加） |
| 未验证 | Windows 实跑、qmake 实际构建、翻译运行时加载 |

---

---

## 一百、关系澄清：同一作者的续作，而非第三方 fork（第九十三轮）

### 100.1 一处把性质写错了的措辞

README 初稿是按"**第三方 fork**"写的：独立一节写"与上游的关系"、致谢里区分原作者与本分支，
反馈区还专门写了一句"上游作者的 QQ 群 / Telegram / 邮箱属于上游项目本身，**本分支的问题请勿投到那里**"。

作者随后说明：**`LapplandSP` 是他自己的旧账号**，`QtAdb-Reforged` 是同一作者的正统续作，
只是因为旧账号登不上而迁移到新账号。于是上面那些"撇清关系"的措辞全部不适用，且会误导读者。

### 100.2 为什么这条信息"必须"写进 README

| 理由 | 说明 |
|---|---|
| **旧仓库无法自我声明** | 旧账号登不上 → 旧仓库挂不上"已迁移"的说明 → **README 是唯一能声明连续性的地方** |
| **否则会被当成第三方 fork** | 读者可能怀疑授权与质量，或者继续去已无人维护的旧仓库提 issue |
| **解决"同名项目"的歧义** | 旧 FAQ 里承认过有多个无关的同名 QtAdb；谁是正统需要一句话讲明 |
| **用户的数据连续性** | 可执行文件仍叫 `QtAdb`、配置目录不变，迁移对用户无感 —— 这点也值得写明 |

### 100.3 实际改了什么

| 位置 | 改动 |
|---|---|
| 副标题 | "重构分支" → "**续作**"，并写明账号迁移（`@LapplandSP` → `@SyntaxEntity`） |
| 章节标题与内容 | "与上游的关系" → "**项目沿革（同一作者的续作）**"，加一行"旧仓库自己没法挂迁移说明"的提示 |
| Q&A | 新增/重写"这和 `LapplandSp/QtAdb` 是什么关系" → 同一作者的续作 |
| 反馈区 | **删除**"上游社群请勿投递"那句（那些渠道本来就是同一位作者的） |

### 100.4 方法上的收获

**"fork"这个词既描述技术动作，也暗示所有权归属**，而这两件事未必一致。
按技术事实（GitHub 上是 fork 关系）写完文档之后，还应确认一句：
**"两边是不是同一个人？"** —— 因为这决定了"致谢 / 撇清 / 提问去处"这些措辞是否成立。
本例里，一个错误的默认假设（"fork ⇒ 别人的项目"）会让 README 通篇对着作者本人"划清界限"。

---

