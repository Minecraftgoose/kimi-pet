# AI 行为规范文档

## 一、基本约定

1. 所有用于测试功能的文件必须放入 `./.tmp` 文件夹，没有则创建，**交付时需清空此文件夹**
2. 预览图放入 `./.preview` 文件夹，交付时无需清空，但须加入 `.gitignore`
3. **未经允许禁止擅自升级版本号**
4. 交付时将 exe 产物与源码包进一个 zip，此 zip 内置两个文件夹：一个是源码，一个是 exe 产物
5. 所有 C 源码与构建脚本放 `src/` 下，仓库根目录只放文档与 `.github/`
6. 也就是说，交付时的.zip应该长这样
```
KimiPet_源码与程序
├─ 软件
│  ├─ KimiPet.ini
│  ├─ KimiPet_x64.exe
│  ├─ KimiPet_x86.exe
│  └─ mod
│     └─ 出厂模组放在这.txt
└─ 源码
   ├─ .gitignore
   ├─ AGENTS.md
   ├─ LICENSE
   ├─ README.md
   ├─ src
   │  └─ 源码放在这.txt
   └─ .github
      └─ workflows
         └─ release.yml
```

## 二、环境

- **本地沙盒**（`setup_env.sh` 装 mingw / wine）软件源被封时换腾讯镜像 `mirrors.tencent.com`，别试外网源；沙盒会不定期重置（apt 源、mingw 都会丢），执行 `/data/workspace/setup_env.sh` 一键恢复
- **CI（`.github/workflows/release.yml`）恰好相反，绝不能换国内源**：Runner 在海外 Azure 机房，跨国路由只会更慢。默认指向 `azure.archive.ubuntu.com`，节点路由抽风或镜像同步时，下载 35MB 级 mingw 大包会假死。做法是①把源替换成官方主源 `archive.ubuntu.com`（同时处理老版 `sources.list` 和 24.04 的 deb822 `ubuntu.sources`），②用 `actions/cache@v4` 缓存 `/var/cache/apt/archives/*.deb`（前置 `sudo chmod -R a+rwX` 让 cache action 能写，key 带手动失效位 `v1`，Runner 基础镜像大版本升级时 bump）。两条都别和本地沙盒的镜像策略混用。
- 编译用 `cd src && sh build.sh`；验证用 wine 实跑 exe，不能只做语法检查

## 三、配置系统（KimiPet.ini）

**新增任何一个 ini 配置项，必须同时做这四件事，少一件用户就会看到「配置里有 bug」：**

1. 写进出厂文本 `DEFAULT_CFG_TEXT`（`config.c`）
2. 写进自动升级清单 `CFG_FIELDS`（`config.c`）
3. 在 `cfg_normalize()` 里做校验
4. 更新 README 的配置说明

原因：出厂配置**只在文件不存在时才生成**。老用户手上的 ini 不会自动长出新键，靠 `cfg_upgrade()` 补。漏了第 2 条，新功能对老用户就是隐形的。

**配置注释风格**：简洁，一行一句，不要啰嗦，不要写「不用重启软件」这类多余的话。

**不要替用户夹取值**：除无效值（0、负数、非数字）外，不要设下限/上限去偷偷改用户输入。曾经把 `sleep_delay = 2` 静默夹成 10，用户以为功能坏了。

**不是所有配置项都要进右键菜单**：如 `sleep` 系列只在 ini 里，菜单不加。

## 四、删除功能要删干净

删掉一个功能时，同步清理所有残留引用，否则会留下死配置：

- 出厂 `DEFAULT_CFG_TEXT` 里的键和注释
- 自动升级清单 `CFG_FIELDS`
- `cfg_normalize()` 里的处理
- 代码注释里提到它的地方
- README 的示例和说明

例：删掉滚轮调大小后，`size_step` / `size_presets` 还在出厂配置里躺了很久。

## 五、中文与编码

- ini 一律 **UTF-8 with BOM**（`EF BB BF`），程序靠 BOM 识别
- **不要把已经是 UTF-8 的中文再做一次转义/解码**（如 Python 的 `unicode_escape`），会产生 `éç½®` 这类 mojibake
- **交付包里的 `KimiPet.ini` 直接用 wine 跑 exe 生成的那份**，不要从源码手工提取文本
- 交付前必须校验：BOM 存在、中文正常、无 mojibake 字符、出厂值正确

## 六、交付前检查清单

1. `mingw` 编译零警告（两个架构都出）
2. wine 实跑：无异常日志，ini 与 `mod/` 正确生成
3. 跑一遍断言测试，新功能要有针对性的回归用例
4. **打包前先 `rm -f` 掉旧 zip** —— `zip -r` 对已存在归档是「更新」语义，删掉的文件会残留在包里
5. `软件/` 里的 `mod/` 目录内容要与 exe 同步（内置模组是首次运行生成的，交付时一并放好）
6. 清空 `.tmp`

## 七、已踩过的坑（改代码时留意）

- **初始化顺序**：`mods_scan()` 必须在 `cfg_parse()` 之后。曾经放在前面，导致启动时模组全关，只有点「重新加载配置」才戴上。凡是「读配置 → 应用」的流程，改完要实际验证启动路径。
- **宽字符串宏拼接**：`L "xxx"` 中间有空格时 `L` 不是前缀而是标识符。宏展开后必须写成 `L"xxx"`，即 `#define V L"a"` 用 `L"x" V L"y"`，不能写成 `L"x" V L "y"`。
- **Shell / CI 脚本**：全程用绝对路径，不要来回 `cd`。曾经因多写一层 `cd ..` 导致 `unzip` 跑在工作区根目录找不到文件。确实需要切目录时用 `(cd dir && cmd)` 子 shell。
- **`memset` 参数错位**：`memset(&P,sizeof(P),0)` 是错的，正确是 `memset(&P,0,sizeof P)`。
- **`_snwprintf` 宽字符串**必须用 `%ls`，不能用 `%s`。
- **需求有歧义时别自己挑一个解释闷头做**：先按上下文还原，仍拿不准就问一句。曾把「不动代表屏息」读成「不设屏息相」，把该做的四相循环砍成了两相，下一轮才补回来。
- **bat 里定位文件别用带引号的通配符，也别依赖 `pushd`**：`for %%f in ("%~dp0*.exe")` 拿到的是字面字符串，不展开；而 `pushd` 在部分 cmd / wine 环境会直接失败（实测 `PUSHD FAILED`）。可靠写法是 `dir /b /o-d "路径\*.exe" 2^>nul` 配 `for /f "delims="`，再自己拼全路径、`%%~ff` 取绝对路径。曾经因此找不到旧文件名 `MiniPet_x64.exe`。
- **所有 `.bat` 必须纯 ASCII**：注释、`echo`、`title`、`REM` 一律英文，不写中文、不用全角标点，也不要 `chcp 65001`（部分 cmd 代码页下 bat 文件按 ANSI 解析，一个中文就可能整段失效）。交付前用「字节 ≥ 128 计数为 0」校验，包括 `KimiPetUpdater.bat` 和 `build.bat`。**程序运行时生成的覆盖 bat 同样适用**：用 `CP_OEMCP` 编码、不加 chcp，否则中文路径在 GBK cmd 下乱码、`copy` 静默失败。
- **`docs/index.html` 里的小圆球用桌面版 Canvas 网页原型，别再手搓 SVG**：球面投影眼睛、果冻形变、眨眼、按住注视、连点 5 次生气、5 层渐变都要按原型来。原型原本是全屏 canvas，嵌进介绍页必须做四处局部化：收进 `.stage` 容器、resize 用舞台 rect、指针坐标换成相对 canvas 的局部坐标、去掉铺满全屏的白色 `fillRect` 改成 `clearRect` 透明。`touch-action:none` 只能加在 `.stage` 上，加到 `body` 会让整页没法滚动。换色走独立色点，别占用球本身的点击（那是「逗它」）。
- **两个 HTML 页面都要适配各种屏幕，别只顾桌面**：`docs/index.html` 和 `docs/downloads/index.html` 都是单文件、零外部资源。流式尺寸用 `clamp()`，列用 `repeat(auto-fit,minmax(min(100%,Npx),1fr))`（`minmax(250px,1fr)` 在窄屏会溢出），表格套 `.table-wrap{overflow-x:auto}` 且在 ≤720px 拆成卡片（`td::before` 读 `data-label`），断点覆盖 320 / 360 / 480 / 720 / 900 / 1600px、横屏矮屏、超宽屏、`prefers-color-scheme:dark`、`prefers-reduced-motion`、打印，还要带 `env(safe-area-inset-*)` 和 `text-size-adjust:100%`。canvas 舞台用 `aspect-ratio:1/1` + `clamp()`，除 `window.resize` 外要挂 `ResizeObserver`（字体加载/滚动条/分屏不会触发 resize）。交付前用 Chromium 在 ≥10 个视口实测 `scrollWidth-clientWidth == 0`、球有像素、JS 零错误。
- **`/downloads/` 表格由 CI 生成，绝不手工编辑**：`docs/downloads/index.html` 里用 `<!-- DOWNLOADS_HERO_BEGIN/END -->`、`<!-- DOWNLOADS_META_BEGIN/END -->`、`<!-- DOWNLOADS_ROWS_BEGIN/END -->` 三对标记包住动态内容。`.github/scripts/gen_downloads.py` 每次 push（含分支 push，与「只有 tag 才打包」不冲突，它只写文档）从 Releases API 拉全部非 draft 版本，写每个附件的 `browser_download_url` 直链，再用 contents API 写回默认分支；内容相同不提交，消息带 `[skip ci]`（GITHUB_TOKEN 创建的 commit 本来也不触发 workflow）。**写回时若远端页面已有标记，必须用远端内容当模板**，否则 tag 打在旧 commit 上会把 main 上更新的样式覆盖掉。draft 要排除、prerelease 要标注、只有 zip 的老版本要如实写「早期版本只提供 zip」、tag/URL 一律 HTML 转义。页面里预置当前真实数据，保证 CI 没跑时离线打开也有内容。
- **功能卡图标用内嵌的 Microsoft Fluent Emoji 3D PNG，不用字符 emoji**：从 `microsoft/fluentui-emoji` 的 `assets/<Name>/3D/<name>_3d.png` 取（jsDelivr 的 `cdn.jsdelivr.net/gh/...` 可下，`raw.githubusercontent.com` 在沙盒里不通），缩到 72×72 再 base64 内嵌进 `docs/index.html`，保持单文件离线可看、零外部请求。目录名是 CLDR 短名（`Sleeping face` / `Counterclockwise arrows button`，不是 Sleeping Face），写错会 404。别用字符 emoji 顶替 —— 各平台字体渲染不一致。
- **「关于 kimi 小圆球」直接打开介绍页，别弹信息框**：URL 形如 `https://minecraftgoose.github.io/kimi-pet/#<版本>&<配置文件路径>`。路径必须 UTF-8 percent-encoding，让 `decodeURIComponent` 能还原中文/空格/反斜杠，且 `&` `#` 要编码成 `%26` `%23`，否则网页解析会断在第一个 `&`。`about_url_build()` 放 `util.c`（纯字符串函数，RENDER_TEST 可断言），先算总长再写，缓冲不够就保持 `out[0]=0` 返回失败，别留半个 URL。网页端解析 hash 并在页面**最顶部**显示「来自本机的 kimi 小圆球」信息条（版本 + 配置路径 + 版本不一致提示），同时监听 `hashchange`；不带 hash 时完全隐藏。打开失败只显示 URL 与 Win32 错误码。
- **页脚别堆成几行散 `<p>`**：`.foot-top` 用 flex 两栏（左品牌+版权，右导航链接），下面单独一行 `.foot-note` 放免责声明，移动端堆叠。
- **版本号显示带 `v` 前缀**（`v1.4.6`），比较用数字宏不影响。
- **发版必须打 tag，且只有 tag / `workflow_dispatch` 才「打包」**：`release.yml` 里 `should_package` 控制「整理产物 + 上传 Artifacts」，`should_release` 控制「发 Release」，两个 output 分开写，别图省事共用一个。分支 push → 只编译 + 检查产物（编译校验仍要做），`dist/` 不生成、不传 Artifacts、不发布；tag push / dispatch → 全做。幂等跳过（同名 Release 已存在且附件齐全）只把 `should_release` 置 false，**`should_package` 保持 true**，产物包照样生成方便对照。早期版本曾用 `should_release` 兼管打包，结果重跑同名 tag 时连 Artifact 都消失。
- **Release 附件平铺、不打包 zip**；必须有独立的 `KimiPet_x64.exe` / `KimiPet_x86.exe`（「检查更新」按架构名找），外加 `KimiPetUpdater.bat` 供老版本用户手动升级。
- **老 .NET 没有 `Tls12` 具名枚举**：PowerShell 2.0 / .NET 3.5 的 `SecurityProtocolType` 只有 `Ssl3`、`Tls`，写 `[Net.SecurityProtocolType]::Tls12` 会在赋值阶段抛 `PropertyAssignmentException`（报错「可能的枚举值为 Ssl3、Tls」），下载还没开始就死。用数值 `3072` 经 `[Enum]::ToObject([Net.SecurityProtocolType],3072)` 设置并包 try/catch；兜底顺序里优先 Win7+ 自带的 `certutil.exe`（走系统 HTTP 栈，不受 .NET 版本限制）。
- **CI 按 tag 检出，推 main 不影响已打的 tag**：workflow 里有「切到目标 tag 的代码」步骤（`git checkout --force refs/tags/$TAG`），产物（含 `KimiPetUpdater.bat` 附件）全部来自那个 tag 的 commit。把最新代码推到 main、却拿旧 tag 触发构建，发出来的仍是旧附件，而且 CI 一路绿灯。**发版 tag 必须打在包含最新代码的 commit 上**，或删掉旧 tag 重打。「整理产物」步骤已加 `grep -q 'call :scan'` 校验，遇到旧 bat 直接 `::error::` 拦下。
- **Release 不能是草案（draft）**：`/releases/latest` 忽略 draft 与 prerelease，`KimiPetUpdater.bat` 走 `latest/download/` 直链，草案时会拿到上一个正式版甚至把本地新版覆盖成旧版；草案附件匿名下载还返回 404。CI 固定 `draft: false`。发版后刷新页面确认 tag 出现在列表且标着 Latest。
- **老机器上别指望命令行下载**：Win7 没有 `curl.exe`、系统常未启用 TLS 1.2、火绒等会拦 `certutil` / PowerShell，三条工具可能全灭而浏览器却完全能打开 GitHub。`KimiPetUpdater.bat` 要有「浏览器下载 + 拾取」模式：扫描 Downloads / 桌面 / 脚本目录 / TEMP 里已下载的 exe，校验 PE 后直接替换，彻底绕开命令行下载。安全软件点「允许」后要**重新运行一次**（弹窗到达时工具往往已放弃）。
- **网络请求一律跑工作线程，主线程只弹框**：`do_update()` 必须立刻返回（实测 0ms），API 与下载都在 `upd_thread`，看门狗另起线程；主线程 `WM_TIMER` 照常驱动 `step()`+`render()`。验证口径：本地起慢速服务器（>20 秒下载），用真实 `upd_thread` + 消息泵实跑，断言下载期间 UI 计时器帧数持续增长（22 秒应有 200+ 帧），且连点「检查更新」第二次被 `g_updBusy` 忽略。
- **别把下载塞进【API 的】短超时看门狗，但下载也必须有自己的看门狗**：1.3「修卡顿」把下载 exe 包进 20 秒的 API 看门狗，结果检测得到更新、exe 永远下不下来；后来改成「下载完全不接看门狗」，又留下一个更隐蔽的坑——`InternetReadFile` 若永久阻塞，`g_updBusy` 永远不清零，用户再点「检查更新」毫无反应。正确做法是按阶段分阈值（API 30 秒 / 下载 330 秒），连接超时单独设短一些，接收间隔给足（只要持续来数据再慢也不掐），并且**超时后必须关掉正在阻塞的 hUrl 句柄、给明确提示、释放 `g_updBusy` 允许重试**。只关父句柄 `hInet` 在部分实现里唤不醒阻塞读。同理，别因为一次沙盒里的 403 就把用户实证可用的 WinINet 链路整个换掉 —— 先查自己上一版改了什么。
- **检查更新出错时直接贴原始响应，不写猜测性提示**：弹框只给事实字段（阶段 / HTTP 状态 / Win32 错误码 / URL / WinINet 扩展文本），然后原样附上服务器响应体；禁止「可能是没网」「代理/防火墙拦截」「稍后再试即可」「球不会卡住」这类话术 —— 用户要看出问题就得看原文。为此 **4xx/5xx 也必须把 body 读回来**并返回给上层（失败时 `*out` 非空，调用方 free），不能像早期那样 `if(code>=400) return 0` 直接丢掉。200 但读不到 `tag_name`、版本号解析失败、缺 asset、下载拿到非 PE 内容，也都要贴原始 JSON / HTML。UTF-8 解码 + 控制字符替换 + 显示 2000 字符上限 + 完整字节落盘 `%TEMP%\KimiPet_update_error.txt`；每轮 `do_update()` 开头清掉上一轮的诊断文件，无响应体时也要清，别留旧内容。兜底链路失败时保留已落盘的 `dst`，由 `grab_dst()` 读回错误页（每条链路失败后立刻抓，下一条开头的 `DeleteFileW` 会抹掉）。
- **Win7 下载别指望命令行工具，要有一层纯 C 的 WinHTTP 兜底**：Win7 不自带 `curl.exe`（Win10 起才有）、系统 Schannel 常未启用 TLS 1.2、火绒等会拦 `certutil.exe` / PowerShell，而浏览器能开 GitHub 只说明浏览器自己的 TLS 栈通。WinHTTP 5.1 是系统 DLL，显式设 `WINHTTP_OPTION_SECURE_PROTOCOLS` 请求 TLS 1.2 后能绕开大半限制；设置失败要忽略继续，别因一个 option 判死。链接用 `-lwinhttp`，**不是 `-lws2_32`**（本程序不用裸 Winsock，别照搬通用 Winsock 教程的链接参数）。
- **`wininet.h` 与 `winhttp.h` 不能放进同一个翻译单元**：mingw-w64 的 `winhttp.h` 注释里就写着 `Yes, these values contradict wininet.h`，两者对 `INTERNET_SCHEME` / `URL_COMPONENTS` / `HTTP_VERSION_INFO` 的 typedef 冲突。WinHTTP 实现必须独立成 `winhttpdl.c`，该文件只 include `<windows.h>` + `<winhttp.h>`，不 include `pet.h`。用宏改名屏蔽冲突不可靠（结构体布局也不同）。
- **超时轮的收尾绝不能清 `g_updBusy`**：看门狗判定超时时已经清过 busy 让用户重试；旧工作线程可能晚一点才从阻塞读里醒来（实测 wine 下 `WinHttpCloseHandle` 唤不醒 `WinHttpQueryDataAvailable`，要等它自己的接收超时），此时 `upd_cleanup()` 若再清一次，会把**新一轮**检查更新的 busy 标志误清零，连点保护失效、两个 ctx 并存。判据是 `if(!READ(&c->timedOut)) InterlockedExchange(&g_updBusy,0)`。
- **响应体达到上限必须明确失败**：`http_get_ex` 以前在 `cap>=UPD_MAX_BODY` 时只 `break`，之后仍 `return 1`，截断的 PE 会过 MZ 校验并覆盖自己，把安装写坏。超限一律 `ok=0`。也别把「旧版不查状态码」当成优势：403 错误页读进来只会被当 JSON 解析成「版本号看不懂」，并不会更容易成功。
