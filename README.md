# kimi小圆球

一个会自己东张西望的小Kimi，住在你的桌面上。

---
<div align="center"><img width="137" height="137" alt="图片" src="https://github.com/user-attachments/assets/bdbb817d-cf9e-4abf-8092-ee0d0b525b55" /></div>


## 源码结构

按职责拆成了 6 个文件，共用的类型、常量和全局变量都在 `pet.h` 里：

所有 C 源码都在 **`src/`** 目录下，仓库根目录只放文档和 CI：

```
README.md  LICENSE  AGENTS.md  .gitignore
.github/workflows/release.yml
src/
  ├─ pet.h       公共声明：类型、常量、全局变量 extern、函数原型
  ├─ util.c      数学工具、UTF-8 ↔ 宽字符转换、文件读写、文本/颜色解析
  ├─ config.c    KimiPet.ini：解析、写回、缺键自动升级、路径定位
  ├─ mod.c       锚点、图元（SDF 绘制）、mod\ 目录里的模组
  ├─ render.c    球体状态、位图缓冲、运动与绘制
  ├─ update.c    检查更新：查 GitHub Release、下载、写覆盖脚本
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

另有 `SHA256SUMS.txt` 可校验。


## 怎么玩

| 操作 | 效果 |
|---|---|
| 左键拖动 | 挪位置 |
| 左键单击 | 逗它，它会鼓一下 |
| 连点 5 次 | 它生气了，眯着眼瞪你几秒 |
| 鼠标 3 分钟不动 | 它会闭上眼睡觉，留下一条缝，还会轻轻呼吸（想马上试就把 `sleep_delay` 改成 2） |
| 睡着时连点 3 下 | 把它叫醒（1.2 秒内点满 3 下，中间停太久要重数） |
| 右键 → 模组 | 勾选即可戴上/摘下 `mod\` 里的模组，改完立刻生效 |
| 右键 | 菜单：颜色 / 模组 / 始终置顶 / 跟随鼠标 / 开机自启 / 重新居中 / 检查更新… / 重新加载配置 / 打开配置文件… / 关于 / 退出 |

### 换颜色

右键菜单 → **颜色**：

淡蓝（默认）· 天蓝 · 靛蓝 · 紫 · 青 · 绿 · 粉 · 橙

不够用就自己加，见下面的配置文件。选中的颜色、几个开关全部记在 `KimiPet.ini`，不再写注册表。

### 检查更新

右键 → **检查更新…**，会去 GitHub 查本仓库的最新 Release。有新版本就下载对应架构的 exe、覆盖旧文件并自动重启，配置文件和模组都不动。

检查全程在后台线程进行，球照常动、不会卡住：

- 网络请求带 8 秒超时，另有 20 秒硬超时兜底，超时就明确提示并放弃，不留挂死线程
- GitHub 返回 **403 / 429**（未登录时每小时 60 次 API 限流）会直接给提示，不会去解析错误页
- 没网、代理拦截、下载到的不是 exe，都只提示、不改动旧文件

发版时 Release 附件里必须有**独立的 `KimiPet_x64.exe` / `KimiPet_x86.exe`**（程序按架构名直接找这两个文件下载），否则检查更新会提示找不到安装包。

### 调大小

**只能在 `KimiPet.ini` 里改 `size`** —— 右键菜单和滚轮都不动它了。出厂是 **60px**，这也是下限；想更大就往上写，想更小就顺便把 `size_min` 一起改小。改完右键 → **重新加载配置**（或重启）生效。


## 配置文件

第一次启动，exe 旁边会生成 `KimiPet.ini`（UTF-8）。exe 放在没写权限的目录时（比如 Program Files），改放到 `%APPDATA%\KimiPet\KimiPet.ini`，右键 → **关于** 里能看到实际路径。把文件删掉，下次启动会按出厂设置重新生成一份。

改完保存 → 右键菜单点 **重新加载配置** 立刻生效，不用重启；点 **打开配置文件…** 直接用记事本打开它。

```ini
[general]
topmost      = 1      ; 窗口始终置顶：1 开 / 0 关
follow       = 0      ; 眼神跟随鼠标：1 开 / 0 关
autorun      = 0      ; 开机自启动：1 开 / 0 关
sleep        = 1      ; 允许睡觉：1 允许 / 0 永远不睡（只在 ini 里，菜单没有这项）
sleep_delay  = 180    ; 鼠标静止多少秒后入睡，180 = 3 分钟
sleep_wake_on_move = 1 ; 睡着后鼠标一动就醒？0 = 只能点 3 下叫醒
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
