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
    json/                全局 JSON 对象分配器（JsonArena / JsonProxy）
    directory/           目录管理模块：数据根 / 游戏根下的全部路径
      Cores.cpp          共享游戏核心（cores/<id>）的建立与硬链接
      Cores.Scan.cpp     核心清单、refs/孤立核心、显式删除
      Migration.cpp      核心/入口分离迁移的执行（WAL + 回滚）
      Migration/         迁移计划、journal、rml 备份
      Versions.cpp       versions/ 扫描与 rml 事务恢复
      Directory.h        其他模块统一从这里取 versions/libraries/assets/servers 等路径
    launch/              启动模块
      Launch.h           组合：Version + Environment + Starter + Download
      Version/           版本探测与 json 合并
      Environment/       Java 发现与安装
      Starter/           命令行装配与进程启动
    server/              服务端模块
      Server.h           组合：Catalog + Runner（全局命名空间）
      Catalog/           服务端安装扫描 + 实例 manifest（servers/<dir>/rml.json）
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
  test/                  rml_check 装配自检 + rml_cores_test 迁移/引用测试（临时目录）
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
- 原版底座写 `role: "base"`，加载器版本写 `base: "<原版 id>"`；删除加载器版本后底座仍按 role 隐藏，不再靠 `user` 或 id 后缀推断
- 用户自己装的原版写 `role: "game"`，保持可见；外部版本没有 `rml.json`，重命名时补 `role: "external"`
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
- 迁移/引用/服务端 manifest 自检：`mingw32-make cores-test`（全部在 `build/` 临时目录，不碰真实 `.minecraft`）
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

## 7. 版本目录与 rml.json

- `cores/<core-id>/` 是隐藏的共享游戏核心：`<core-id>.json`、`<core-id>.jar`、`<core-id>-natives/`、`rml.json`（kind=core）。核心不进版本列表，用户不能直接选核心启动。
- `versions/<id>/` 是版本入口（A）：rml.json、加载器 profile、saves/mods/options/logs。核心文件必须物理归 cores；原版入口迁移后不再保留 json/jar/natives，加载器入口只保留自己的 profile 和私有数据。
- `servers/<dir>/` 是服务端实例，`rml.json`（kind=serverInstance）是唯一事实来源；服务端安装同样是 `versions/<id>` 入口，`serverJar`/`argFile` 留在入口，不搬进 cores。
- 入口有效性：有 rml，且 `versions/<id>/<id>.json` 或 `cores/<core>/<core>.json` 可解析；服务端安装没有版本描述也有效。
- `rml.json` 是版本管理的**唯一事实来源**，schema 2 字段：
  - `id` / `name`
  - `role`：`game` / `loader` / `base` / `server` / `external`
  - `loader` / `loaderVersion`
  - `minecraft`：这个版本实际对应的原版
  - `base`：加载器版本依赖的原版入口 id（父核心）
  - `core`：入口引用的隐藏核心 id（原版入口默认等于自己，加载器默认等于 base）
  - `complete` / `userInstalled` / `state` / `fresh`：同安装事务
- 没有该文件 = 外部版本；外部版本不迁移核心文件，加载器需要它的文件时只复制/硬链接到 cores，不改动外部目录。
- 旧字段 `managed` / `user` / `kind` 只在迁移时读取一次；新写入一律显式字段。
- rml.json 只能通过 `JsonArena::Open(path)` 借用代理读写；禁止直接 `ifstream` 或 `Json::Parse` 这个文件。
- Mod 仓库元数据集中在 `<game>/mods/rml.json`；旧的 `<jar>.rml.json` 首次读取时合并后删除。
- **核心不可变**：启动/修复/重装不得就地改写已存在的核心文件。内容不一致时生成新 id：`<core>-cov-<内容 sha1 前 8 位>`，入口 rml 指向新核心；旧核心继续保留给其它入口。
- **迁移纪律**：`Directory::MigrateCores` 先 dry-run，正式执行前备份入口 rml 并把每条文件操作写入 `cores/.migration/<时间戳>/journal.log`（WAL），再 rename/硬链接；失败自动按日志反向回滚，日志和备份保留现场；`RollbackLastMigration` 可手动回滚最近一次。迁移前必须确认游戏/服务端未运行，只允许搬 `<id>.json`/`<id>.jar`/`<id>-natives`，绝不碰 saves/mods/levels.dat。
- **引用与 GC**：`CoreEntry::refs` = 引用核心的 versions 入口（含服务端安装）；`Catalog::Installation::refs` = 服务端实例引用数。删除入口只删入口，不自动删核心；核心无引用时只作为 GC 候选，必须显式 `RemoveCore`/`RemoveOrphanCores`。有引用的核心删除请求必须回中文错误。
- 安装事务：开始写 `state: "installing"`、`complete: false`；成功提交 `committed`；失败由代理整目录删除（默认不保留 `failed`）；进程崩溃后由 `Directory::RecoverTransactions()` 清理所有非 `committed` 残留（只扫 versions，不碰 cores）。
- 安装新版本：先建事务并写 json → 再拉 client/libraries/assets → 最后解压 natives（老版本）→ 提交事务 → 调 `MigrateCores` 把核心文件搬进 cores。


## 8. 数据目录

- 所有运行数据在 `%APPDATA%\RML\`
  - `settings.json`：gameDir / selectedVersion / username / memoryMb / gc / jvmArgs
  - `sources.json`：软件源记录（mojang / assets / forge / neoforge / fabric），删除该文件即恢复默认
  - `servers.json`：旧服务端实例清单，只作为导入源/可选 pid 缓存；首次迁移前备份为 `servers.json.legacy.bak`，原文件不改写
  - `server-runstate.json`：服务端 pid 运行状态缓存，不写进实例 manifest
  - `background.png`：用户自定义背景（设置页选择图片后复制到这里）
  - `runtimes/`：RML 自己装的 Java
  - `curseforge.key`：CurseForge API key（没有则禁用该源）
  - `logs/rml-debug.log`：仅 debug 构建
- 服务端实例 manifest 在 `<game>/servers/<dir>/rml.json`，不是 APPDATA；pid 每秒只更新 `server-runstate.json`。
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
- 服务端支持原版、Fabric 与 Forge：原版下载 Mojang `server.jar`；Fabric 用 meta 的 `/server/json`
  装原版 server.jar + loader 依赖；Forge 用官方安装器的 `--installServer`，生成 `win_args.txt` 或老版本 universal jar。
- 服务器页的“服务端”按钮管理的是服务端**安装**（versions/<id>），不是服务器实例；仍有实例引用时会拒绝删除，只删安装入口、保留核心。
- 服务端实例以 `<game>/servers/<dir>/rml.json` 为准；旧 `servers.json` 首次读取时备份并导入，断链/非法记录留在备份里不删。
- 服务端安装也写 `core`（= minecraft）；启动服务端只用入口里的 `server.jar`/`win_args.txt`，不依赖客户端核心。
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

补充第 7 条：**机制和策略必须合并，对外只提供意图**。调用者只借用代理并表达编辑意图；分配器保证同一个资源指向同一个代理；原子性隐藏在代理内部。详见 §21。

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

## 21. 资源代理与原子性（机制与策略合一）

**纪律：机制和策略必须合并，对外只提供意图。**

- 调用者不直接打开、解析、写 JSON 文件，也不自己加锁、建临时文件、rename、校验或回滚；它只表达“打开哪个资源、改哪些键”的意图。
- `open(path)` 返回的是**借来的代理**，不是文件句柄，也不是可以长期持有的对象。离开作用域必须归还；不能把代理本身或 `operator[]` 返回的引用保存到作用域外。
- 分配器负责把路径规范化（分隔符、大小写、绝对路径），并保证**同一个 `rml.json` 的所有编辑指向同一个代理对象**。如果两个调用者拿到两个副本，锁、事务和原子提交就失效了。
- 原子性（独占写、变更日志、临时文件 + 原子替换、失败回滚、跨进程锁、崩溃恢复）全部隐藏在代理内部。调用方只看到 `operator[]` / `Get` / `create` / `remove`；提交/回滚是代理的职责，不是调用方的编排工作。
- `operator[]` 是写入口；只读请用 `Get` 或 `const JsonProxy&`，避免读取时在文档里创建 `null` 键。
- 领域 schema 校验（例如 rml 的 role/base/state 组合）由读取方在代理作用域内完成；代理负责的是文件级原子性，不是业务字段合法性。
- 如果接口提供 `commit()`，它也只是“现在提交”的意图，不是让调用方去管临时文件、锁或 rename；代理仍要自己保证提交的原子性和失败回滚。
- **不要**把机制和策略拆成需要调用方自己组合的多个对外类型。可以内部分层，但对外只暴露借来的代理和少量意图方法。
- 多文件操作必须由分配器/代理提供更高层事务；单个文件代理只保证这个文件的原子性，不能假装保证跨文件原子。
- 这一条同样约束 JSON 以外的共享资源：谁申请谁借用，分配器保证同一资源同一对象，机制不泄漏给调用方。

调用者看到的只有意图：

```cpp
auto j = allocator.Open("versions/1.21.1/rml.json");  // 借来的代理
j["state"] = "committed";
j.create("name");
j.remove("legacyField");
// 离开作用域：代理自己完成锁释放、提交或回滚
```

分配器内部可以分文档对象、租约、变更日志、校验器，但这些都不出现在调用方。
