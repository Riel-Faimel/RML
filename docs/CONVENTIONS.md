# RML 编码纪律与注意事项

本文是给"下一个人"看的：接手这个仓库前先读一遍，能避开绝大多数返工。

---

## 1. 目录与分层

```
src/
  app/                   程序主体与各模块
    main.cpp             入口
    Bridge.*             应用层：把模块能力暴露给 QML
    tools/               工具模块（唯一允许自由函数的模块）
    launch/              启动模块
      Launch.h           组合：Version + Environment + Starter + Download
      Version/           版本探测与 json 合并
      Environment/       Java 发现与安装
      Starter/           命令行装配与进程启动
    download/            下载模块
      Download.h         组合：VersionSource + Installer
      VersionSource/     版本/内容源（Mojang、Fabric meta、Modrinth、CurseForge、Cache）
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
- Forge / NeoForge 安装未接入，菜单里选择后会明确报"尚未接入"
- QML 的 `clip` 只支持矩形裁剪；圆角裁剪需要自定义图元
- `dist` 体积约 97MB（Qt 运行库 + qml 模块）
