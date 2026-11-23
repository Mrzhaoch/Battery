# BatteryHub（无线设备电量监控）

> 在 Windows 10/11 任务栏上直接显示无线鼠标、耳机、键盘、游戏手柄的电量，无需安装厂商软件。

**语言 / Language：** 简体中文（本文件）｜ [English（原英文 README）](README.en.md)

---

## 这是什么

BatteryHub 是一个极轻量（约 4MB 内存）的 Windows 任务栏小工具，直接与设备通信读取电量，不依赖罗技 G HUB、雷蛇 Synapse、赛睿 GG 等厂商软件（这些软件可关可开，互不冲突）。应用内部名称仍为 BatteryHub，主程序为 `BatteryHub.exe`。

## 仓库来源（血缘说明）

本仓库是**个人备份用途的 fork**，代码血缘如下：

1. 原始项目：`krisgfx9/Wireless-Device-Battery-Monitor`（又名 Wireless Battery / BatteryHub），其作者账号目前已在 GitHub 下线；
2. 社区延续分支：[`ahizechi/Battery`](https://github.com/ahizechi/Battery)（本仓库 fork 自这里，版本 v1.2.3）；
3. 当前个人备份：`Mrzhaoch/Battery`（本仓库）。

本仓库保留了完整的源代码、目录结构与提交历史，以后重装系统或换电脑时，直接拉取本仓库即可。

## 功能特性

- **任务栏电量**：每个设备显示百分比，下方有一根细水平条；充电时变绿，低电量（≤15%）变红。
- **无需厂商软件**：直接读取设备，支持罗技、雷蛇、赛睿、海盗船、HyperX、WLMouse、PS/Xbox 手柄、AirPods、Galaxy Buds 等。
- **低电量提醒**：可按设备单独设置提醒阈值（5%～40%）。
- **硬件读数（默认关闭）**：可选显示 CPU/GPU 占用与功耗、GPU/SSD 温度、内存/显存、磁盘占用、网络上下行速度。
- **小巧安静**：被关闭的功能完全不读取，屏幕关闭时暂停硬件采样。

> 与原版 BatteryHub 的差异：新增硬件读数与水平条；**移除了电池历史记录**（不向磁盘写记录）；更新只来自本仓库并需独立密钥签名。

## 支持的设备（节选）

- **鼠标/键盘**：罗技（Unifying/Bolt/Lightspeed，含 M575 轨迹球）、雷蛇、赛睿、海盗船、HyperX、Glorious、LAMZU、Keychron、MCHOSE、ROG、MSI、ZOWIE、WLMouse 等。
- **耳机/耳塞**：各品牌 2.4G/蓝牙耳机、Jabra、JBL、三星 Galaxy Buds、苹果 AirPods/Beats、Nothing 等。
- **游戏手柄**：索尼 PS4/PS5、Xbox 各代、Switch 兼容手柄、8BitDo、Flydigi、GameSir、北通等。
- **通用蓝牙**：凡在「设置 → 蓝牙和设备」中能看到电量的设备均支持。
- 完整清单见 [`docs/devices.md`](docs/devices.md)。

## 目录结构

```
Battery/
├── src/            # 主程序与各厂商驱动源码（C++/Win32）
├── docs/           # 详细指南、设备清单、协议说明
├── installer/      # Inno Setup 安装包构建脚本
├── lang/           # 多语言文件（含简体中文）
├── res/            # 图标等资源
├── linux/          # Linux（实验性）相关文件
├── tests/          # 测试
├── Makefile        # make 构建入口
├── build.bat       # Windows 构建脚本
└── BatteryHub.exe  # 构建产物（源码仓库默认不含，需自行构建）
```

---

## 重装系统 / 换电脑后如何使用本仓库

> 注意：GitHub 的 fork **只复制源代码，不复制上游 Releases 里的 exe/zip 安装包**。根据需要选择下面任一方式。

### 方式一：直接用安装包（最简单，推荐）

如果手头保留了编译好的 `BatteryHub-Setup-<版本>.exe`（本机已下载并校验过 v1.2.3），双击运行、一路下一步即可，无需管理员权限。

### 方式二：拉取本仓库源码

1. 安装 [Git for Windows](https://git-scm.com/download/win)。
2. 克隆本仓库：

```bash
git clone https://github.com/Mrzhaoch/Battery.git
cd Battery
```

3. 之后按下面「从源码构建」生成可执行程序。

## 从源码构建

1. 安装 [MinGW-w64](https://www.mingw-w64.org/)，推荐用 [MSYS2](https://www.msys2.org/)，在 MSYS2 终端执行：

```bash
pacman -S mingw-w64-x86_64-gcc make
```

2. 构建可执行文件（任选其一）：

```bash
make                      # 开发版构建
# 或在 Windows 命令行执行
build.bat
```

指定版本号构建：

```bash
make VERSION=1.2.3 CHANNEL=test
build.bat 1.2.3 test      # 等价的 cmd 写法
```

3. 生成 `BatteryHub.exe` 后可直接运行；若旁边没有 `headsetcontrol.exe`，仅会少一部分额外耳机支持，不影响主功能。

4. 如需打包成安装包和便携版 zip，再安装 [Inno Setup](https://jrsoftware.org/isinfo.php)，先完成上面的版本化构建，然后执行：

```powershell
powershell -File installer/build-installer.ps1 -Version 1.2.3
```

## 命令行参数

```bash
BatteryHub.exe --list     # 打印所有设备及电量后退出
BatteryHub.exe --json     # 以 JSON 数组输出设备及电量后退出
BatteryHub.exe --dump     # 列出所有 HID/USB/蓝牙设备，并存到 %TEMP%\BatteryHub-dump.txt
BatteryHub.exe --report   # 打开“请求支持某设备”窗口
BatteryHub.exe --demo     # 用各品牌模拟设备代替真实设备（截图/配色检查）
BatteryHub.exe --quit     # 关闭正在运行的实例
```

## 硬件读数（可选，默认关闭）

在菜单 **Hardware on the taskbar** 中逐项开启 CPU/GPU 占用、CPU/GPU 功耗、GPU/SSD 温度、RAM/VRAM、磁盘、网络速度等；刷新间隔可选 1/2/5 秒。数据均来自 Windows 性能计数器与系统驱动，无需额外安装或管理员权限。屏幕关闭时自动暂停。（Windows 下 CPU 温度需内核驱动，故不提供。）

## 卸载

设置 → 应用 → BatteryHub → 卸载；或运行安装目录下的 `unins000.exe`。

## 隐私说明

电量、设备别名等数据仅保存在本机注册表，本版本不向磁盘记录电量历史，也不发送任何设备或电量信息。检查更新时仅访问 GitHub 公开 API，可在菜单关闭。

## 开源协议

[GPL-3.0 or later](LICENSE)，与原项目一致。

## 相关链接

- 本仓库：https://github.com/Mrzhaoch/Battery
- 延续分支：https://github.com/ahizechi/Battery
- 原始项目（已下线，仅作记录）：https://github.com/krisgfx9/Wireless-Device-Battery-Monitor
