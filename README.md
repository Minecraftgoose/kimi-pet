# kimi小圆球

一个会自己东张西望的小Kimi，住在你的桌面上。

> 想先看看它长什么样、能干什么？打开 [docs/index.html](docs/index.html)。页面里那只球用的是**桌面版的 Canvas 网页原型**——球面投影的眼睛、果冻形变、眨眼、生气都在，下面还能点色点换衣服；功能卡的图标是内嵌的 Microsoft Fluent Emoji 3D。另有一个 [历代下载页](docs/downloads/index.html)（`https://minecraftgoose.github.io/kimi-pet/downloads/`），把每个版本的附件直链列成表格。两页的浏览器标签图标用的都是软件本体的 `KimiPet.ico`。

---
<div align="center"><img width="137" height="137" alt="图片" src="https://github.com/user-attachments/assets/bdbb817d-cf9e-4abf-8092-ee0d0b525b55" /></div>


## 源码结构

按职责拆成了 6 个文件，共用的类型、常量和全局变量都在 `pet.h` 里：

所有 C 源码都在 **`src/`** 目录下，仓库根目录只放文档和 CI：

```
README.md  LICENSE  AGENTS.md  .gitignore
.github/
  ├─ workflows/release.yml
  └─ scripts/gen_downloads.py   # 生成 /downloads 表格（CI 每次 push 调）
docs/
  ├─ favicon.ico                # 软件图标（与 src/KimiPet.ico 同一份），两页共用
  ├─ index.html                 # 介绍页
  └─ downloads/index.html       # 历代下载页（表格由 CI 注入）
src/
  ├─ pet.h       公共声明：类型、常量、全局变量 extern、函数原型
  ├─ util.c      数学工具、UTF-8 ↔ 宽字符转换、文件读写、文本/颜色解析
  ├─ config.c    KimiPet.ini：解析、写回、缺键自动升级、路径定位
  ├─ mod.c       锚点、图元（SDF 绘制）、mod\ 目录里的模组
  ├─ render.c    球体状态、位图缓冲、运动与绘制
  ├─ update.c    检查更新：查 GitHub Release、下载、写覆盖脚本
  ├─ winhttpdl.c WinHTTP 下载兜底（独立翻译单元，避开 wininet.h 类型冲突）
  ├─ main.c      窗口、右键菜单、鼠标钩子、开机自启、入口
  ├─ icon.rc  KimiPet.ico
  └─ build.sh  build.bat
```

编译：

```bash
cd src && sh build.sh        # 产物写到 ../../exe（仓库外）
```

改 `build.sh` 里的 `SRCS` 就能加减文件；`build.bat` 同理。

## 下载

到 [Releases](https://github.com/Minecraftgoose/kimi-pet/releases) 直接取对应架构的 exe —— 附件是平铺的，不打包 zip：

| 文件 | 说明 |
|---|---|
| `KimiPet_x64.exe` | 64 位系统用这个 |
| `KimiPet_x86.exe` | 32 位系统用这个 |

另有 `SHA256SUMS.txt` 可校验，以及 `KimiPetUpdater.bat`（手动更新器，见下方「检查更新」）。

### 历代下载页

`docs/downloads/index.html` → Pages 上的 **`/downloads/`**。一张表格列出从 v1.0 到当前每个版本的**附件直链**（`browser_download_url` 原值，点开即下，旁边有「复制」按钮），外加顶部「当前最新」卡片。早期只有 zip 的版本会如实标注，不会伪造 exe 链接。

表格**不要手工改**：`.github/scripts/gen_downloads.py` 在 CI 每次 push（分支 push 和 tag push 都跑）时从 GitHub Releases API 重新拉一遍全部正式发行版，替换页面里 `<!-- DOWNLOADS_ROWS_BEGIN/END -->` 等标记区间，再用 contents API 写回默认分支的 `docs/downloads/index.html`。内容没变化就不产生空 commit；用 `GITHUB_TOKEN` 创建的 commit 不会触发新 workflow，消息里再带 `[skip ci]` 双保险。写回时**优先拿默认分支上现有的页面当模板**（只替换标记区间），所以 tag 打在旧 commit 上也不会把 main 上更新的样式覆盖掉。

这不违反「分支 push 不打包」：这一步只同步一页静态文档，不生成 `dist/`、不传 Artifacts、不发 Release。tag push 时它排在「发布到 Release」之后，刚发出来的版本会立刻出现在表里。

两页的样式和脚本都内嵌在 HTML 里，不依赖第三方 CDN；唯一的同站资源是 `docs/favicon.ico`（浏览器标签图标 + 页脚品牌小图），也就是 exe 用的那个 `src/KimiPet.ico`。注意 Pages 是项目页、挂在 `/kimi-pet/` 下，所以图标必须用**相对路径**：介绍页写 `favicon.ico`，下载页写 `../favicon.ico`，不能写 `/favicon.ico`（那会指到 github.io 根域名）。

> 仓库里所有 `.bat`（含 `build.bat`）都是**纯 ASCII**、不带 `chcp`：部分 cmd 代码页会把 bat 按 ANSI 读，一个中文或全角标点就可能让整段脚本失效，所以提示一律英文。


## 怎么玩

| 操作 | 效果 |
|---|---|
| 左键拖动 | 挪位置 |
| 左键单击 | 逗它，它会鼓一下 |
| 连点 5 次 | 它生气了，眯着眼瞪你几秒 |
| 鼠标 3 分钟不动 | 它会闭上眼睡觉，留下一条缝，还会轻轻呼吸（想马上试就把 `sleep_delay` 改成 2） |
| 睡着时连点 3 下 | 把它叫醒（1.2 秒内点满 3 下，中间停太久要重数） |
| 右键 → 模组 | 勾选即可戴上/摘下 `mod\` 里的模组，改完立刻生效 |
| 右键 → 呼吸训练 | 球放大 = 吸气、缩小 = 呼气、不动 = 屏息，眼睛跟着睁合，一直循环（见下方专节） |
| 右键 → 呼吸节奏 | 等长 / 长呼气 / 慢一点，点一下自动写好吸气、呼气秒数 |
| 右键 | 菜单：颜色 / 模组 / 始终置顶 / 跟随鼠标 / 开机自启 / 呼吸训练 / 呼吸节奏 / 重新居中 / 检查更新… / 重新加载配置 / 打开配置文件… / 关于 kimi 小圆球 / 退出 |

### 换颜色

右键菜单 → **颜色**：

淡蓝（默认）· 天蓝 · 靛蓝 · 紫 · 青 · 绿 · 粉 · 橙

不够用就自己加，见下面的配置文件。选中的颜色、几个开关全部记在 `KimiPet.ini`，不再写注册表。

### 检查更新

右键 → **检查更新…**，会去 GitHub 查本仓库的最新 Release。有新版本就下载对应架构的 exe、覆盖旧文件并自动重启，配置文件和模组都不动。

检查全程在后台线程进行，球照常动、不会卡住：

- **API 请求**（小 JSON）15 秒超时，另有 30 秒硬超时兜底，超时就明确提示并放弃
- **下载 exe 也有看门狗，但阈值完全独立**：连接 20 秒（单独算，别让「连不上」陪着等满下载时长）、接收间隔 5 分钟（只要持续来数据，再慢也不掐）、总时长 330 秒硬兜底。1.3 那次的错是把下载塞进**20 秒的 API 看门狗**，不是「下载不该被监控」——现在下载若永久阻塞，看门狗会关掉正在阻塞的请求句柄、给明确提示，并释放「检查更新」让你可以重试，不会留下一个永不返回的线程
- **4xx/5xx 的错误页会读回来**：状态码仍判失败，但不会丢掉响应体。GitHub 403 的 JSON 里有 `message` / `documentation_url`，那才是排查最需要的东西。v1.2 不查状态码并不是它更容易成功的原因：旧版只是把限流错误页当 JSON 解析，最后报「版本号看不懂」
- 响应体超过 **16MB** 一律判为失败。以前达到上限只是 `break`，截断的 PE 仍会通过 MZ 校验并覆盖自己，能把安装写坏
- 连不上、超时、下载到的不是 exe，都只显示诊断信息、不改动旧文件

### 出错时直接看原始响应

**任何错误都不写「可能是没网 / 代理拦截」「稍后再试即可」这类猜测性话术**，弹框只给事实，然后原样贴上服务器的响应：

```
[阶段] API
[HTTP] 403
[URL] https://api.github.com/repos/Minecraftgoose/kimi-pet/releases/latest
[响应 271 字节]
{"message":"API rate limit exceeded ...","documentation_url":"..."}
[完整响应] C:\Users\...\Temp\KimiPet_update_error.txt
```

- 阶段、HTTP 状态、Win32 错误码、URL、WinINet 扩展错误文本都是事实字段；没有响应体时显示 `[响应] (无响应体)` 和 `[Win32] 0`（系统层没报错，是连上了却一个字节都没来）
- **403/429/500 的错误页、200 但读不到 `tag_name` 的异常 JSON、Release 缺附件时的 `assets` 列表、下载拿到的 HTML 403 页，全都原样贴出** —— 不用猜，看原文就知道是限流、token、附件漏传还是代理改写
- 响应按 UTF-8 解码，非法字节按字节显示、控制字符替换成 `.`；弹框最多显示 2000 字符，**完整原始字节**写到 `%TEMP%\KimiPet_update_error.txt`，路径也在框里给出。每轮检查开始会先清掉上一轮的这个文件，不会留旧内容误导
- 「已经是最新版了」和更新确认框不是错误，保持普通文案

**下载主路径是 WinINet 直下** —— 和 v1.2 能顺利升级到 1.3 时用的是同一套链路。旧版成功的关键是「不设接收超时、不被短看门狗掐」，新版保留了这份宽容（接收间隔 5 分钟），只是补上了连接超时独立化和防永久阻塞的硬兜底。

主路径失败后依次兜底：

1. **WinHTTP（纯 C，Win7 原生系统组件）** —— 显式请求 TLS 1.2，不依赖任何外部 exe 或第三方库，也不会被火绒当作可疑命令行下载拦掉。`winhttpdl.c` 是独立翻译单元：`wininet.h` 与 `winhttp.h` 在 mingw-w64 下 typedef 冲突，不能放在同一个 `.c` 里。链接参数是 **`-lwinhttp`**，不是 `-lws2_32`（本程序不用裸 Winsock，WinHTTP 已封装 socket、HTTP 头、Content-Length 和 302 重定向）。
2. 系统目录里的 `curl.exe`（含 32 位进程用的 `Sysnative` 路径）→ PATH 里的 `curl.exe` → **`certutil.exe`**（Win7+ 自带，走系统 HTTP 栈）→ PowerShell WebClient。

> 编译相关：`build.sh` / `build.bat` 的 `LIBS` 同时列了 `-lwininet -lwinhttp`。代码里设了 `_WIN32_WINNT 0x0601`，产物是普通 Win32 PE，不依赖新版 SDK 运行时。Win7 SP1 想真正握上 TLS 1.2 仍需 KB3140245；`WinHttpSetOption` 失败时程序会忽略并继续用系统默认，不会因为一个 option 就放弃下载。

**老系统特别注意**：PowerShell 2.0 / .NET 3.5 的 `SecurityProtocolType` 只有 `Ssl3`、`Tls` 两个成员，写 `[Net.SecurityProtocolType]::Tls12` 会直接抛 `PropertyAssignmentException`，下载还没开始就失败。所以 TLS 1.2 用数值 `3072` 经 `[Enum]::ToObject` 设置，并整段包在 `try/catch` 里 —— 赋值失败也继续尝试。手动 `KimiPetUpdater.bat` 同样按 curl → certutil → PowerShell 顺序走。

覆盖用的临时 bat 用**系统 OEM 代码页**写、不带 `chcp`：以前写 UTF-8 再靠 `chcp 65001` 撑着，中文 Windows 按 GBK 预读批处理行，中文路径会变成乱码（如「张三」→「寮犱笁」），`copy` 找不到源文件，覆盖就悄悄失败了。

下载完会校验 PE 头（`MZ`）和体积，失败只提示、**不改动旧文件也不退出程序**。

**主路径也下载不下来怎么办**：

- **老系统最省事的一条（推荐）**：直接用浏览器打开 Releases 页面下载 `KimiPet_x64.exe`，下载完**再运行一次** `KimiPetUpdater.bat`。它会扫描 Downloads / 桌面 / 脚本同目录 / TEMP，找到刚下载的 exe、显示大小与日期让你确认、校验 PE 头后自动替换并重启 —— **全程不走命令行下载**。Win7 没有 `curl.exe`、系统未启用 TLS 1.2、火绒拦 `certutil` / PowerShell，都不影响这条路径。若火绒弹了下载提示，点「允许」后**重新运行一次**再试（弹窗到达时对应工具往往已经放弃）。
- 设环境变量 `KIMIPET_MIRROR` 为镜像前缀（例如 `https://ghproxy.net/`），程序会自动拼在直链前面；
- 或者从 Release 附件下载 **`KimiPetUpdater.bat`**，放到 exe 同目录，退出 KimiPet 后双击运行 —— 它用 `latest/download/` 稳定直链自动下载覆盖并重启。这也是 **v1.3 ~ v1.3.3 老版本用户**的升级办法：那些已经发出去的 exe 内建下载方式改不了，手动跑一次更新器即可跨过去，之后新版就能自己更新了。更新器依次试 curl / certutil / PowerShell，三条都不通会自动打开 Releases 页面并提示用上面的「浏览器下载 + 拾取」办法。

发版时 Release 附件里必须有**独立的 `KimiPet_x64.exe` / `KimiPet_x86.exe`**（程序按架构名直接找这两个文件下载），否则检查更新会提示找不到安装包。附件里也要放上**最新版**的 `KimiPetUpdater.bat`，别沿用旧 Release 上传过的那份。

**别把 Release 存成草案（draft）**：`/releases/latest` 只返回最新的**非草案、非预发布** Release，而 `KimiPetUpdater.bat` 用的是 `latest/download/` 稳定直链；草案状态下它会指向上一个正式版，本地若是更新的版本反而会被覆盖成旧版。草案附件的匿名下载还会返回 404。CI 里已固定 `draft: false` / `prerelease: false`，如果网页上看到 `Draft` 标记，进 Edit → **Publish release** 即可。

### 关于菜单与介绍页参数

右键 → **关于 kimi 小圆球** 不再弹信息框，而是用默认浏览器打开介绍页，hash 里带上本机版本和配置文件实际路径：

```
https://minecraftgoose.github.io/kimi-pet/#v1.4.6&C%3A%5CUsers%5C...%5CKimiPet.ini
```

- 路径按 UTF-8 percent-encoding（只保留 `A-Z a-z 0-9 - . _ ~`），中文、空格、反斜杠、`&`、`#` 都能安全带上；网页用 `decodeURIComponent` 还原
- 页面顶部出现一条「来自本机的 kimi 小圆球」，显示版本与配置文件路径，可点 × 关掉；本机版本与网页版本不一致时会额外提示
- 直接打开网页（不带 hash）时这条完全隐藏，不打扰普通访客
- 没有默认浏览器、打开失败时，程序会显示 URL 本身和 Win32 错误码，不写猜测性话术

### 调大小

**只能在 `KimiPet.ini` 里改 `size`** —— 右键菜单和滚轮都不动它了。出厂是 **60px**，这也是下限；想更大就往上写，想更小就顺便把 `size_min` 一起改小。改完右键 → **重新加载配置**（或重启）生效。


## 配置文件

第一次启动，exe 旁边会生成 `KimiPet.ini`（UTF-8）。exe 放在没写权限的目录时（比如 Program Files），改放到 `%APPDATA%\KimiPet\KimiPet.ini`。右键 → **关于 kimi 小圆球** 会直接打开介绍页，页面顶部显示本机版本和这个**实际路径**；把文件删掉，下次启动会按出厂设置重新生成一份。

改完保存 → 右键菜单点 **重新加载配置** 立刻生效，不用重启；点 **打开配置文件…** 直接用记事本打开它。

```ini
[general]
topmost      = 1      ; 窗口始终置顶：1 开 / 0 关
follow       = 0      ; 眼神跟随鼠标：1 开 / 0 关
autorun      = 0      ; 开机自启动：1 开 / 0 关
sleep        = 1      ; 允许睡觉：1 允许 / 0 永远不睡（只在 ini 里，菜单没有这项）
sleep_delay  = 180    ; 鼠标静止多少秒后入睡，180 = 3 分钟
sleep_wake_on_move = 1 ; 睡着后鼠标一动就醒？0 = 只能点 3 下叫醒
breathe          = 0    ; 呼吸训练：1 开 / 0 关（菜单里也能勾）
breathe_in       = 5    ; 吸气秒数：球放大、眼睛睁开
breathe_hold_in  = 0    ; 吸末屏息秒数：球保持最大；0 = 不屏息
breathe_out      = 5    ; 呼气秒数：球缩小、眼睛闭上
breathe_hold_out = 0    ; 呼末屏息秒数：球保持最小；0 = 不屏息
size         = 60     ; 球直径（px）—— 唯一的调法，菜单和滚轮都改不了
size_min     = 60     ; 直径下限，出厂 = 60
size_max     = 420    ; 直径上限 —— 想玩更离谱的球就往上加
color        = 淡蓝   ; 当前配色，名字要对得上 [colors] 里的某一行

[colors]
; 想加颜色就自己加一行，右键菜单的「颜色」里会自动多一项，最多 32 个。三种写法：
淡蓝 = #4a86d8,#5a95e0,#6ca4e8,#8cb9f0,#a9cdf6   ; 5 层渐变完全自己定，从暗到亮
天蓝 = 201,65,57                                 ; 色相,饱和度,亮度（HSL）
朱红 = #ff0000                                   ; 只给基色，5 层明暗自动推
```

行首写 `#` 或 `;` 就是注释。软件写回时只动那一行，注释、空行和文件里原有的顺序都原样保留。

**老配置文件会自动升级**：新版加了新选项（比如 `sleep` 那几个），启动时发现你的 ini 里没有，会自己补一行带注释的进去，你原有的值和注释一个都不动。不用手动补，也不用删文件重生成。

### 睡觉

鼠标静止 `sleep_delay` 秒（默认 180，即 3 分钟）后，它会慢慢合上眼 —— 上下眼睑往中间收，中间留一条缝，同时有很轻的呼吸起伏。这段时间鼠标一动就重新计时。

睡着后**连点 3 下**叫醒（要在 1.2 秒内点满，中间停太久会重数）。这几下不算惹它生气。

`sleep_delay` 想写多小都行（最小 0.5 秒），写 `2` 就能马上看到效果。改完记得右键 → **重新加载配置**，或者重启一次。

默认情况下鼠标一晃也会把它吵醒，但鼠标要累计移动 14 像素以上才算数，手抖一下不至于。想让它睡踏实点，把 `sleep_wake_on_move` 改成 `0`，那样鼠标怎么动都没用，只能点 3 下叫醒。

想让它永远精神，把 `sleep` 改成 `0`。这两项都只在配置文件里，右键菜单没有对应条目 —— 改完用右键 → **重新加载配置** 生效。

### 呼吸训练

右键菜单勾上 **呼吸训练**，或在 ini 里写 `breathe = 1`，球就开始做四相循环：

| 阶段 | 球 | 眼睛 |
|---|---|---|
| 吸气 | 缓慢放大 | 逐渐睁开 |
| 吸气末屏息 | **不放大也不缩小**（保持最大） | 保持睁开 |
| 呼气 | 缓慢缩小 | 逐渐闭上 |
| 呼气末屏息 | **不放大也不缩小**（保持最小） | 保持闭合，留一条缝 |

**「球不动」就是屏息。** 两段屏息默认都是 `0`（出厂不启用），写 0 就自动跳过那一相，退化成纯「放大—缩小」两相。相与相之间用缓入缓出衔接，切换时不会突然跳一下。

**没有轮次、总时长、完成页或统计** —— 一直循环到你把开关关掉。训练开着时不会自动睡着，也不眨眼；关掉后睡觉计时从关掉那刻重新算。模组锚点跟着球一起缩放，兔耳、猫耳不会掉队。

#### 三个节奏预设

右键 → **呼吸节奏**，点一下就把 `breathe_in` / `breathe_out` 写回 ini（当前命中的那个会打勾）：

| 预设 | 写入 | 一轮 | 说明 |
|---|---|---:|---|
| 等长 5-5 | `in=5` `out=5` | 10 秒 | 默认，约每分钟 6 次 |
| 长呼气 4-6 | `in=4` `out=6` | 10 秒 | 吸呼比 2:3，呼气稍长 |
| 慢一点 6-6 | `in=6` `out=6` | 12 秒 | 整体放慢，约每分钟 5 次 |

**预设只改这两个数**，`breathe_hold_in` / `breathe_hold_out` 保持你自己写的值。所以如果你在 ini 里写 `breathe_hold_in = 4`、`breathe_hold_out = 4`，再点「等长」得到的就是 5-4-5-4；想要纯两相就把两个 hold 都写 0。想写别的秒数也随便，1~30 秒有效，菜单三个都不勾就是「自定义」。

点完预设会从「呼尽」重新起步。**预设不会自动打开训练**，开关是旁边那个「呼吸训练」勾选项。

#### 关于「有争议的不做」

菜单里**不主动提供**箱式 4-4-4-4、4-7-8 这类含屏息的具体方案，也不提供疗效、HRV、放松度之类的说法 —— 屏息时长由你在 ini 里自己定，软件只负责把节奏表达出来。本功能只做节律引导，不诊断、不治疗、不监测任何指标，也不替代医疗建议。觉得不舒服就直接关掉，恢复自己习惯的呼吸即可。

<div align="center"><img width="748" height="420" alt="图片" src="https://github.com/user-attachments/assets/696fa47d-9d35-444a-888f-e4a960e07638" /></div>


## 大屏幕 / 高 DPI

已适配，可用于希沃白板

## 模组

exe 同级会有一个 `mod\` 文件夹，里面一个 `.ini` 就是一个模组。首次运行会自动生成三个示例（默认全关）：`兔耳.ini`、`猫耳（白）.ini`、`锚点调试.ini`。

升级到新版时，新增的内置模组会**自动补进已有的 `mod\` 文件夹**，已经存在的文件一律不覆盖 —— 你改过参数的、自己加的模组都原样保留。

想戴上，两种方式都行：

- 右键菜单 → **模组** → 勾上名字（会写回 `KimiPet.ini` 的 `[mods]` 段）
- 直接把对应 ini 里的 `enabled = 0` 改成 `1`

改完用右键 → **重新加载配置** 立刻生效，不用重启，也不用手写进主配置。


### 锚点

装饰不是写死坐标，而是挂在锚点上，所以会跟着球一起缩放、鼓起、歪头、眼神转动。

| 锚点 | 位置 |
|---|---|
| `body` | 球心 |
| `eye_l` `eye_r` | 左右眼中心，**跟着眼神动** |
| `eyes` | 双眼中点（画鼻梁、眉毛用这个） |
| `mouth` | 嘴 |
| `head` | 头顶 |
| `ear_l` `ear_r` | 左右耳根 |
| `foot_l` `foot_r` | 左右脚 |

顺手的别名：`center`→body、`head_top`/`top`→head、`eye`→eyes、`ear`→ear_l、`foot`→foot_l。

坐标单位统一是**球半径 = 12**，球心为原点，y 向下为正。想确认锚点在哪，把「锚点调试」模组打开，每个锚点会点一个红点。


### 一个模组文件长什么样

```ini
[mod]
name    = 兔耳
enabled = 0

; layer = back 表示画在球体之前，耳根会被球挡住，像是从头后面长出来的
[part]
at    = ear_l
shape = ellipse
rx    = 1.9
ry    = 5.2
rot   = -13
dy    = -4.2
color = #fdf3f6
layer = back

[part]
at    = ear_r
shape = ellipse
rx    = 1.9
ry    = 5.2
rot   = 13
dy    = -4.2
color = #fdf3f6
layer = back
```

想画两眼之间连线的东西（鼻梁、眉毛），用 `at` + `to` 两个锚点：

```ini
[part]
at    = eye_l        ; 起点锚点
to    = eye_r        ; 终点锚点 —— 两眼一动它就跟着动
shape = line
thick = .4
dy    = -1.6
color = #262626
```

`[part]` 可以写任意多个，最多 256 个图元、32 个模组。

### 图元 `shape`

| shape | 需要的参数 | 效果 |
|---|---|---|
| `circle` | `r` | 实心圆 |
| `ring` | `r` `thick` | 圆环（镜片、描边） |
| `ellipse` | `rx` `ry` | 椭圆（耳朵、腮红） |
| `rect` | `w` `h` `corner` | 矩形，`corner` 是圆角 |
| `line` | `thick` + 终点 | 圆头线段（鼻梁、胡须、腿） |
| `arc` | `r` `thick` `a0` `a1` | 圆弧，角度用度，0 = 正右方，顺时针（因为 y 向下） |
| `triangle` / `tri` | `w` `h` `lean` `corner` | 等腰三角形，尖朝上。`w` 底宽、`h` 高、`lean` 让尖端左右偏、`corner` 圆角（做猫耳用） |

### 每个图元都能调

| 参数 | 作用 |
|---|---|
| `at` / `to` | 起点 / 终点锚点 |
| `dx` `dy` | 起点偏移（`dy` 为负 = 往上） |
| `dx2` `dy2` | 终点偏移（`to` 给了锚点时，是相对那个锚点再挪一点） |
| `rot` | 旋转，单位度 |
| `scale` | 缩放，1 = 原样 |
| `color` | `#RRGGBB` 或 `r,g,b` |
| `alpha` | 0~1 透明度 |
| `layer` | `back` = 画在球体之前（耳朵、脚这类该被挡住的）；默认 `front` = 盖在眼睛之后 |

`w` 在 `rect` / `triangle` 里是宽度，在其他图元里当线宽用；想明确点就写 `thick`。`triangle` 的 `lean` 是顶点相对底边中点的水平偏移，`corner` 把尖角磨圆，配合 `rot` 能做出向外撇的耳朵。

## 自己编译

```bash
# Linux （需 mingw-w64）
sudo apt install mingw-w64      
sh build.sh

# Windows（需 MinGW-w64：MSYS2 的 mingw64 shell 或 w64devkit）
build.bat
```


## 想改外观？

所有参数集中在 [`pet.c`](pet.c) 上方的配置区，改完重新编译即可：

| 参数 | 作用 |
|---|---|
| `bodySize` `baseW` `eyeAspect` `eyeGap` `eyeY` | 眼睛形状、大小与间距 |
| `eyeMove` `turnK` `roll` | 眼球转动幅度与球面倾斜感 |
| `pressPop` `jellyK` `jellyMax` | 点击时的鼓起与果冻形变 |
| `angryLid` `lidSlant` | 生气时的眯眼程度与斜切角度 |
| `blink` `jitter` `speed` | 眨眼频率、眼神抖动、追随速度 |
| `g_skin[]` / `[colors]` | 球体五层配色与预设色（默认 8 色写在 `KimiPet.ini` 里） |
| `HUE_S` `HUE_L` | 预设色的默认饱和度与明度 |
| `size_min` `size_max` | 球直径的上下限（在 `KimiPet.ini` 里改，不用重编译） |
| `ANCH_MOUTH_Y` 等 | 各锚点在球面上的位置，单位是球半径（=12） |


## 目录约定

遵循 `AGENTS.md`

## 许可

[MIT](LICENSE) © 2026 Minecraftgoose

## 声明

本软件与Moonshot AI无关，亦不是其官方产品
