# RML 编码纪律与注意事项

本文是给"下一个人"看的：接手这个仓库前先读一遍，能避开绝大多数返工。

---

## 1. 目录与分层

```
src/
  app/                   程序主体与各模块
    main.cpp             入口
    Bridge.*             应用层：把模块能力暴露给 QML
    ServerBridge.*       应用层：服务器页专用桥
    tools/               工具模块（唯一允许自由函数的模块）
    directory/           目录管理模块：数据根 / 游戏根下的全部路径
      Directory.h        其他模块统一从这里取 versions/libraries/assets/servers 等路径
    launch/              启动模块
      Launch.h           组合：Version + Environment + Starter + Download
      Version/           版本探测与 json 合并
      Environment/       Java 发现与安装
      Starter/           命令行装配与进程启动
    server/              服务端模块
      Server.h           组合：Catalog + Runner（全局命名空间）
      Catalog/           服务端安装扫描 + 实例清单（servers.json）
      Runner/            服务端进程、stdin 命令、pid/停止
      Installer/         原版 / Fabric 服务端下载
      Instance/          实例数据结构
    download/            下载模块
      Download.h         组合：VersionSource + Installer
      VersionSource/     版本/内容源（Mojang、Fabric meta、Forge、NeoForge、Modrinth、CurseForge、Cache）
      VersionSource/Sources/  软件源记录（sources.json，设置页可改）
      Installer/         传输、落盘、版本文件安装
      Flow/              安装策略（见 §2）
  include/               纯虚接口（AccountProvider、ContentSource…）
  json/                  默认值 JSON（不是解析器）
  qml/                   Qt Quick 前端
  test/                  rml_check：不开窗口的装配自检
tools/                   deploy.bat
```

**硬规则**

- 模块 = `src/app/<模块>/` 下的一级目录，其类放在**全局命名空间**
- 组件 = 模块下的子目录，其类放在**模块命名空间**（如 `download::installer::Transfer`）
- 接口 = `src/include/` 里的纯虚类，不写实现
- **只有 `namespace tools` 允许有自由函数**，其它模块一律用类
- 受管对象用继承表达，管理器用组合表达
- 模块不允许直接放在 `src/` 下

## 2. 安装策略（CRTP）

不同加载器的安装流程用编译期多态表达，不写 `if (loader == ...)` 分支：

```cpp
template <typename Derived>
class Flow {
public:
    Outcome Run(const Request& request, const Progress& progress) {
        return static_cast<Derived*>(this)->RunImpl(request, progress);
    }
};
class Vanilla : public Flow<Vanilla> { ... };
class Fabric  : public Flow<Fabric>  { ... };
```

新增一个加载器 = 新增一个 `Flow` 子类，不改调用方。

## 3. 错误处理

- 通用签名：`bool DoSomething(..., std::string& error)`，返回 `false` 时 `error` 必须是**可读的中文**，带上关键路径/URL
- 不要用异常穿透模块边界
- 失败语义要区分：HTTP 失败、落盘失败、校验失败、解析失败，不能混成一个字符串
- 用户可见的状态集中在 `Bridge` 的 `status_` / `download_status_`

## 4. 文件系统

- 项目目录（数据根、游戏根、versions/libraries/assets/servers、RML 的 cache/tmp/logs 等）
  只从 `Directory` 模块取，其他模块不再自己拼路径
- 版本目录由 `Directory::RefreshVersions()` 扫描，`Versions()` / `VisibleVersions()` 返回容器引用；
  `launch::Version` 持有 `Directory`，读的是同一份容器，刷新后立即反映
- Forge/NeoForge 的底座默认标成依赖并隐藏；Vanilla 安装会把 `user=true`，用户自己装的原版仍然显示
- 全部用 `tools::ToPath(text)` 把 UTF-8 字符串转成 `std::filesystem::path`
- **不要直接用 `std::filesystem::u8path`**，它在 C++20 已弃用，GCC 15 会报 `-Wdeprecated-declarations`
- 写文件前先 `tools::EnsureDir(parent)`；Windows 上目录不存在时 `CreateFile` 会直接失败
- 路径统一用 `/` 拼接（`tools::JoinPath`），避免转义问题

## 5. 构建

- **只允许** `Makefile` + `mingw32-make`，项目里没有 CMake
- 交付路径唯一：`mingw32-make -j12 deploy`
  - 输出目录 `dist/`，产物 `dist/Riel Minecraft Launcher.exe`
  - `deploy` 时 `deploy.bat` 的输出重定向到 `build/deploy.log`，失败才提示
  - 交付前必须先关掉正在运行的 RML，否则 `dist` 被占用，deploy 会失败
- 依赖：`-MMD -MP` + `-include *.d`，**改了头文件必须重编**，否则会出现对象布局不一致导致的段错误
- 并行构建看 `CXX/LINK` 行确认真正重编了哪些文件

### 工具链

| 环境 | 编译器 |
| --- | --- |
| 用户交付 | `E:\develop_language\mingw64`（GCC 15） |
| 开发验证 | `D:\Qt\Tools\mingw1310_64`（GCC 13） |

代码必须在两套工具链下都能编译：

```cpp
#if defined(_WIN32) && __GNUC__ < 14
// GCC 13 的 MinGW 头没有 time64 声明，需要自己补 clock_gettime64 / nanosleep64
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
...
#pragma GCC diagnostic pop
#endif
```

**交付验证必须用用户那条 PATH 跑 deploy 并贴出退出码**，不能只用自己的工具链"看着过了"。

## 6. QML 约定

- `QQuickStyle::setStyle("Basic")` 必须设置，否则无法自定义
- 颜色/圆角/间距只从 `Theme.qml` 取，不写字面量
- 玻璃面板统一用 `components/GlassPanel.qml`；需要局部直角时用 `squareTopLeft/TopRight/BottomLeft/BottomRight`，不要另造蒙层
- 下拉/输入统一用 `SoftComboBox` / `SoftField`，不要用未定制的 `ComboBox` / `TextField`
- 菜单用 `SoftMenu` + `SoftMenuItem`

### 布局坑

- **同一个 item 不能同时设纵向 `anchors` 和 `y`**，`y` 会被忽略（表现为"完全不动"）
- 需要"从底部抽出"时优先用**裁剪框 + 内部 `y`**：
  ```qml
  Item {                       // 裁剪框：上下边界由锚点决定
      anchors.top: body.bottom
      anchors.bottom: bar.top
      clip: true
      Item { y: open ? 14 : parent.height; ... }
  }
  ```
- `clip` 只能裁到 item 自身矩形，没有独立的 clipRect；要"收紧裁剪区"就改这个 item 的几何
- 初始值不要依赖布局结果（初始 `height` 常为 0）；`Behavior` 在布局完成前应禁用，否则会补播一段初始动画
- 长路径/长串文本用 `Text.WrapAnywhere`，并给容器 `Layout.minimumWidth: 0`，否则会把窗口撑破

## 7. 版本目录

- 版本目录 = `versions/<id>/`，必须有可解析的 `<id>.json`（含 `id` 字段），否则不列出来
- `rml.json` 是托管标记：`{ "managed": true, "loader": "Fabric", "complete": true, "name": "显示名" }`
  - 没有该文件 = 外部版本
  - `name` 与目录名解耦，重命名只改这里
- 安装新版本：先落 json → 再拉 client/libraries/assets → 最后解压 natives（老版本）

## 8. 数据目录

- 所有运行数据在 `%APPDATA%\RML\`
  - `settings.json`：gameDir / selectedVersion / username / memoryMb / gc / jvmArgs
  - `sources.json`：软件源记录（mojang / assets / forge / neoforge / fabric），删除该文件即恢复默认
  - `servers.json`：服务端实例（名称、目录、安装 id、端口、内存、玩法参数、pid）
  - `background.png`：用户自定义背景（设置页选择图片后复制到这里）
  - `runtimes/`：RML 自己装的 Java
  - `curseforge.key`：CurseForge API key（没有则禁用该源）
  - `logs/rml-debug.log`：仅 debug 构建
- 不接受命令行参数传数据路径

## 9. Git

- 提交信息用中文，说清"改了什么、为什么"
- 同一处的反复试探不要留多条提交，交付前 squash 成一条
- `.gitignore` 已包含 `build/`、`dist/`、目标文件、临时文件
- `.gitattributes` 强制 `*.bat` 为 CRLF —— `deploy.bat` 用 LF 会被 cmd 解析错

## 10. 交付检查表

1. 关掉正在运行的 RML
2. `mingw32-make -j12 deploy`，退出码必须 0
3. 确认 `dist/Riel Minecraft Launcher.exe` 时间戳是新的
4. 需要自检时：`mingw32-make -j12`（debug）→ 运行 → 检查 `%APPDATA%\RML\logs\rml-debug.log` 无 `warn/error`
5. 不要在未验证的情况下声称"已修复"；截图类验证由用户完成，工具不要自作主张截屏

## 11. 已知限制

- CurseForge 需要 API key，未配置时 `Ready()` 返回原因
- Forge / NeoForge 走安装器里的 `install_profile.json`：要解包安装器、下依赖库、按 `data` / `processors`
  跑 java 处理器（`Processors`），所以安装时机器上必须有满足版本要求的 Java
- NeoForge 的 artifact 位置分两段：1.20.1 在 `net/neoforged/forge`（1.20.1-47.1.x），
  其余在 `net/neoforged/neoforge`（`21.1.x` / `26.1.2.x` 这类版本号）
- Forge / NeoForge 的安装器和依赖来自 `maven.minecraftforge.net`、`maven.neoforged.net`，
  可以在设置页改成自定义 maven 根；源不通时安装会明确报下载失败
- 服务端只支持原版与 Fabric：原版下载 Mojang `server.jar`；Fabric 用 meta 的 `/server/json`
  装原版 server.jar + loader 依赖，首次启动在实例目录生成 `.fabric/remappedJars`（需要可写）
- 新实例默认 `online-mode=false`（启动器目前用离线账号）；正版验证在服务器页“详细信息”里改。
  启动时会按实例配置重写受管键（端口/玩法/难度/人数/online-mode），其余键保留
- 服务端日志在实例目录 `logs/console.log`，控制台只读尾部；优雅停止靠 stdin `stop`，
  启动器重启后只能按记录的 pid 强制停止
- QML 的 `clip` 只支持矩形裁剪；圆角裁剪需要自定义图元
- `dist` 体积约 97MB（Qt 运行库 + qml 模块）

---

## 12. 六条纪律（原始表述）

1. 模块 = `src/app/` 下的一级文件夹，类放在**全局命名空间**
2. 组件 = 模块下的子文件夹，类放在**模块命名空间**（小写）
3. 接口 = `src/include/` 里的纯虚类，接口的分类树也放在同一目录
4. 只有 `namespace tools` 可以有自由函数，其余一律用类
5. 受管对象用继承表达
6. 管理器用组合表达

补充：`app/` 放主体与模块，`json/` 只放默认值文件（不是解析器），模块不允许直接挂在 `src/` 下。

## 13. UI 设计语言

- 主色蓝、白色面板、灰色文字；圆角统一走 `Theme.radius` / `Theme.radiusSmall`
- 顶栏**不透明蓝**，导航居中（启动 / 下载 / 服务器 / 设置），账号固定右上角
- 背景是一张图，所有面板用 `GlassPanel` 做毛玻璃；列表项是圆角块，不用圆点前缀
- 字号层级：页面标题 24、区块标题 13、正文 13、辅助 12、说明 11
- 按钮：主操作用实心蓝，次操作用半透明白 + 主色描边

## 14. Java 选择与 natives

- `Environment::Pick(required)`：先找主版本完全相等的；没有就取**不低于要求的最低版本**（Java 17 的版本用 21 跑没问题）
- 只有全部低于要求才报"Java 版本过低"
- 用户在启动页手动选的 Java 通过 `Options.java_path` 覆盖自动选择
- 1.19+ 的 natives 在 classpath 上，**没有 `natives` 声明就不要要求解压目录**
- 老版本：classifier jar 必须下载，装完用 `tar -xf` 解压到 `<id>-natives`，并把嵌套目录里的 dll 平铺到根
- 只有 classifier 的库**不进 classpath**（否则误报"缺少库文件"）

## 15. 下载与传输

- `Transfer` 默认 8 路并发；单个文件失败重试 3 次（间隔 300ms）
- 计数必须**累加**：安装器分"核心文件 → 资源"两段各调一次 `Run`，覆盖计数会让"跳过"数字变少
- 校验：先写 `.part`，比对 SHA1，通过后 rename；失败删除 `.part`
- 资产用 `resources.download.minecraft.net/<前两位>/<hash>`
- 下载进度通过 `Progress(label, done, total)` 回调，界面按 16/32 的步长节流

## 16. JSON 解析

- `Json::Parse` 容忍 UTF-8 BOM（`EF BB BF`）
- 解析失败要给出"哪个文件、哪一段"的上下文，不要只回 `parse error`
- 版本文件必须能解析出 `id` 才算有效版本

## 17. 调试构建与日志

- `RML_DEBUG` 只在 debug 配置定义；release 里不包含任何调试输出
- 两种配置都带 `-mwindows`，不弹控制台
- 日志：`%APPDATA%\RML\logs\rml-debug.log`，用 `qInstallMessageHandler` 写入
- 启动游戏的 stdout/stderr 重定向到 `<gameDir>/logs/rml-launch.log`（启动前先 `EnsureDir`）
- **不要自己截屏验证界面**，界面结论由用户给出

## 18. 启动流程

- `Build` 失败且 `missing` 非空 → **自动补齐**，补完重新 Build 再启动；界面不再提供"补齐文件"按钮
- 启动按钮在 `busy` 或 `installingVersion` 非空时置灰
- `Options`：`memory_mb`、`gc`（自动/G1/ZGC/Parallel → `-XX:+Use*GC`）、`jvm_args`（空格切分）、`java_path`
- 这些设置由 `Bridge::applyLaunchOptions` 写回 `settings.json`，编辑即时生效

## 19. 版本源

- 版本清单来自 Mojang `piston-meta` 官方接口；内容源为 Modrinth 与 CurseForge
- 分类：`type == release` → 正式版；命中愚人节 id 列表 → 愚人节版；其余 → 测试版
- **愚人节 id 列表要每年手动补一行**（官方清单里没有类型字段），现在写在 `Mojang.cpp`
- CurseForge 需要 `x-api-key`，放在 `%APPDATA%\RML\curseforge.key`，缺失时该源不可搜索

## 20. 协作约定

- 用户只看源码，不看长篇说明；不要主动写文档
- 没有明确要求时不要新增功能；不确定的业务规则先问，不要自己定
- 不要宣称"已修复"。没在用户环境验证过，就说明没验证
- 交付前必须跑 `mingw32-make -j12 deploy` 并给出真实退出码
- 遇到困难或做不下去，直接说，不要用半成品交差
