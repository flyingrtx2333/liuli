# Explicit UI v0.5：字素编辑、条件语言与 Linux 无障碍实验

目标是轻量原生桌面框架，不是把 Chromium 换个名字。当前仍未达到完整 Electron 替代标准；本轮 43 项 Python 测试、3 项原生 CTest 和合并后的实窗/AT-SPI 流程已通过。验收路线见 docs/electron-replacement.md。

## 这一阶段的路径

模型声明 + 布局声明 + 动作/持久化声明 → 编译为 C++ → SDL 窗口 → Cairo / Pango 原生绘制。

- 行列流式布局按窗口尺寸重新排版、重绘文字，并同步点击区域；不是 CSS，也不是完整自动布局系统。
- 编译后的命名动作可以追加/移除文本项，所有表达式先计算，再作为一个事务提交。
- 不变字符串和数组由模型候选共享，不再为计数变化复制万条列表。过滤仍扫描和复制结果。
- 文本框支持 Pango 字素簇导航/删除、鼠标拖选、Shift-click 和有界撤销/重做；中英文、组合 emoji、双向文字已覆盖测试。完整系统输入法、多行和部分光标语义仍未完成。
- 新增严格 bool 类型、比较和惰性条件表达式；不与整数隐式转换。未选择的分支不会执行，但两边都必须通过类型检查。
- 可选 Linux ATK 桥接已通过独立 AT-SPI 客户端和实窗测试；不是完整 Orca 或有声读屏验收。默认关闭，依赖版本和线程约束见 docs/linux-accessibility-spike.md。
- 持久化字段显式声明，类型/字段不匹配整批拒绝。文件大小、条目数量和数组长度有上限。
- 运行时保存/加载在独立有界工作线程执行。迟到的加载不能覆盖较新的编辑；请求保存后马上关闭，会等待保存结果。坏文件启动时拒绝加载，也禁止静默覆盖它。

## 构建与启动

需要 Python 3.9+、C++17、SDL2、Cairo 和 Pango。CMake 构建入口见 docs/building.md。

```sh
sh scripts/check.sh
sh scripts/build-window.sh
./build/window/window-host --state-file /absolute/path/to/demo-state.eui
```

没有 --state-file 时不读写用户状态文件。Ctrl+S 保存，Ctrl+O 加载，Ctrl+Esc 请求取消；窗口标题显示操作状态。启动加载在窗口创建前执行。取消不能中断所有系统调用，也不能撤销已经提交的保存；当前没有通用“未保存修改”确认对话框。

当前云端 X11 有特定 MIT-SHM 问题，需要任务内构建的非共享内存 SDL。不能把 dummy/offscreen 当作实窗。复现和启动方式见 docs/cloud-x11.md；该方案不改系统库或 X Server 设置。

## 运行任务列表示例

```sh
cmake -S . -B build/tasks -DCMAKE_BUILD_TYPE=Release \
  -DEUI_MODEL="$PWD/examples/task_list.eui.json" \
  -DEUI_LAYOUT="$PWD/examples/task_list.layout.json"
cmake --build build/tasks --target eui-window
./build/tasks/eui-window --state-file /absolute/path/to/tasks.eui
```

输入任务并点击 Add task；Remove first 删除第一项。列表项选择、多文档和原生文件对话框尚未实现。这是验证编译动作和保存/重开的真实小应用，不是完整待办产品。

## 已知边界

- 只有 Linux 原生窗口有本轮实际交互证据。Windows 验证正在独立准备，macOS/iOS 未验收。
- 绘制仍为 CPU 路径。静态玻璃背景只作视觉近似，不是苹果 Liquid Glass 原版或动态 GPU 材质。
- 原生输入使用字素簇，便携 TextEdit 的旧码点接口仍保留。实际中文输入法候选窗口、完整读屏任务、多行输入和高 DPI 策略尚未验收。
- POSIX 原子替换不等于断电耐久性：未 fsync 父目录，无并发写冲突协议；Windows 原子保存后端尚未实现。
- 单场景内存、短时间空闲 CPU 与微基准不能证明完整应用低功耗或优于 Electron。每项数据对应自己的二进制和环境，见 evidence/v0.4/。
- Dockerfile 是依赖配方，未构建验证。源码包和本地工作不等于远端私有仓库或可复用 Codex 配置已经建好。

后续重点为输入/无障碍完整性、跨平台运行与分发，以及具名应用的同功能对照。不会以一个截图或一组微基准宣布“彻底取代”。

最新已知限制见 docs/known-limitations.md。本项目已有本地 Git 历史，但远端私有仓库尚未就绪；不会把源码附件等同于已经推送。
