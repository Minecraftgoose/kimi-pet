# Kimi-Pet

一个会自己东张西望的 Kimi-Pet，住在你的桌面上。


<div align="center"><img width="137" height="137" alt="图片" src="https://github.com/user-attachments/assets/bdbb817d-cf9e-4abf-8092-ee0d0b525b55" /></div>


## 源码结构

```
README.md  LICENSE  AGENTS.md  .gitignore
.github/
  ├─ workflows/release.yml
  └─ scripts/gen_downloads.py   # 生成 /downloads 表格
docs/
  ├─ favicon.ico                # 软件图标
  ├─ index.html                 # 介绍页
  └─ downloads/index.html       # 历史版本下载页
src/
  ├─ pet.h       公共声明：类型、常量、全局变量 extern、函数原型
  ├─ util.c      数学工具、UTF-8 ↔ 宽字符转换、文件读写、文本/颜色解析
  ├─ config.c    KimiPet.ini：解析、写回、缺键自动升级、路径定位
  ├─ mod.c       锚点、图元（SDF 绘制）、mod\ 目录里的模组
  ├─ tex.c       PNG 解码（自带 inflate）与锚点贴图绘制
  ├─ builtin_png.h 内置「星星眼」用的 PNG 字节（用户提供的星星，黑底已抠透明）
  ├─ render.c    球体状态、位图缓冲、运动与绘制
  ├─ update.c    检查更新：查 GitHub Release、流式下载、纯 C 自替换并重启
  ├─ winhttpdl.c WinHTTP 下载
  ├─ main.c      窗口、右键菜单、鼠标钩子、开机自启、入口
  ├─ icon.rc  KimiPet.ico
  └─ build.sh
```

编译：

```bash
cd src && sh build.sh        # 产物写到 ../../exe（仓库外）
```

改 `build.sh` 里的 `SRCS` 就能加减文件。

## 下载

到 [Releases](https://github.com/Minecraftgoose/kimi-pet/releases) 直接取对应架构的 exe

| 文件 | 说明 |
|---|---|
| `KimiPet_x64.exe` | 64 位系统用这个 |
| `KimiPet_x86.exe` | 32 位系统用这个 |

### 历代下载页

[点击前往](https://minecraftgoose.github.io/kimi-pet/downloads/)


## 怎么玩

| 操作 | 效果 |
|---|---|
| 左键拖动 | 挪位置 |
| 左键单击 | 逗它，它会鼓一下 |
| 连点 5 次 | 它生气了，眯着眼瞪你几秒 |
| 鼠标 3 分钟不动 | 它会闭上眼睡觉，留下一条缝，还会轻轻呼吸 |
| 睡着时连点 3 下 | 把它叫醒 |
| 右键 → 模组 | 勾选即可戴上/摘下 `mod\` 里的模组，改完立刻生效 |
| 右键 → 呼吸训练 | 球放大 = 吸气、缩小 = 呼气、不动 = 屏息，眼睛跟着睁合，一直循环 |
| 右键 → 呼吸节奏 | 等长 / 长呼气 / 慢一点，点一下自动写好吸气、呼气秒数 |
| 右键 | 菜单：颜色 / 模组 / 始终置顶 / 跟随鼠标 / 开机自启 / 呼吸训练 / 呼吸节奏 / 重新居中 / 检查更新… / 重新加载配置 / 打开配置文件… / 关于 Kimi-Pet / 退出 |

### 换颜色

右键菜单 → **颜色**：

淡蓝（默认）· 天蓝 · 靛蓝 · 紫 · 青 · 绿 · 粉 · 橙

不够用就自己加，见下面的配置文件。选中的颜色、几个开关全部记在 `KimiPet.ini`。


### 调大小

**只能在 `KimiPet.ini` 里改 `size`** —— 为了防止同学恶搞。出厂是 **60px**，这也是下限；想更大就往上写，想更小就顺便把 `size_min` 一起改小。改完右键 → **重新加载配置**（或重启）生效。


## 配置文件

第一次启动，exe 旁边会生成 `KimiPet.ini`（UTF-8）。exe 放在没写权限的目录时（比如 Program Files），改放到 `%APPDATA%\KimiPet\KimiPet.ini`。右键 → **关于 Kimi-Pet** 打开介绍页，页面弹出「关于」对话框显示版本和这个**实际路径**；把文件删掉，下次启动会按出厂设置重新生成一份。

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



### 睡觉

鼠标静止 `sleep_delay` 秒（默认 180，即 3 分钟）后，它会睡觉。

睡着后**连点 3 下**叫醒（要在 1.2 秒内点满，中间停太久会重数。

`sleep_delay` 想写多小都行（最小 0.5 秒。

默认情况下鼠标一晃也会把它吵醒，但鼠标要累计移动 14 像素以上才算数，手抖一下不至于。想让它睡踏实点，把 `sleep_wake_on_move` 改成 `0`，那样鼠标怎么动都没用，只能点 3 下叫醒。

想让它永远精神，把 `sleep` 改成 `0`。这两项都只在配置文件里， **重新加载配置** 生效。

### 呼吸训练

 **呼吸训练**，在 ini 里写 `breathe = 1`，球开始做四相循环：

| 阶段 | 球 | 眼睛 |
|---|---|---|
| 吸气 | 缓慢放大 | 逐渐睁开 |
| 吸气末屏息 | **不放大也不缩小**（保持最大） | 保持睁开 |
| 呼气 | 缓慢缩小 | 逐渐闭上 |
| 呼气末屏息 | **不放大也不缩小**（保持最小） | 保持闭合，留一条缝 |

#### 三个节奏预设

右键 → **呼吸节奏**，点一下就把 `breathe_in` / `breathe_out` 写回 ini：

| 预设 | 写入 | 一轮 | 说明 |
|---|---|---:|---|
| 等长 5-5 | `in=5` `out=5` | 10 秒 | 默认，约每分钟 6 次 |
| 长呼气 4-6 | `in=4` `out=6` | 10 秒 | 吸呼比 2:3，呼气稍长 |
| 慢一点 6-6 | `in=6` `out=6` | 12 秒 | 整体放慢，约每分钟 5 次 |




<div align="center"><img width="748" height="420" alt="彩色" src="https://github.com/user-attachments/assets/696fa47d-9d35-444a-888f-e4a960e07638" /></div>


## 大屏幕 / 高 DPI

已适配，可用于希沃白板

## 模组

exe 同级会有一个 `mod\` 文件夹，里面一个 `.ini` 就是一个模组。首次运行会自动生成四个内置模组（默认全关）：`兔耳.ini`、`猫耳（白）.ini`、`锚点调试.ini`、`星星眼.ini`。星星眼用的一张 `星星.png` 也一起写进来。

升级到新版时，新增的内置模组会**自动补进已有的 `mod\` 文件夹**，已经存在的文件一律不覆盖 —— 你改过参数的、自己加的模组都原样保留。

想戴上，两种方式都行：

- 右键菜单 → **模组** → 勾上名字（会写回 `KimiPet.ini` 的 `[mods]` 段）
- 直接把对应 ini 里的 `enabled = 0` 改成 `1`

改完用右键 → **重新加载配置** 立刻生效。


### 锚点

装饰不是写死坐标，而是挂在锚点上，所以会跟着球一起缩放、鼓起、歪头、眼神转动。
鼻子和嘴也会朝视线方向**轻微**偏一点（幅度远小于眼睛，不会五官错位）；正视时完全不动，出厂外观不变。

| 锚点 | 位置 |
|---|---|
| `body` | 球心 |
| `eye_l` `eye_r` | 左右眼中心，**跟着眼神动** |
| `eyes` | 双眼中点（画鼻梁、眉毛用这个） |
| `nose` | 鼻子（眼睛与嘴之间） |
| `mouth` | 嘴 |
| `head` | 头顶 |
| `ear_l` `ear_r` | 左右耳根 |
| `foot_l` `foot_r` | 左右脚 |

`eye_l` `eye_r` `eyes` 完全跟着眼球滑动；`nose` `mouth` 只做轻微跟随，仍吃挤压与整球歪头。想画两眼之间的东西（鼻梁、眉毛）用 `eyes`，想画贴在脸上的鼻子用 `nose`。

别名：`center`→body、`head_top`/`top`→head、`nose_tip`/`snout`→nose、`eye`→eyes、`ear`→ear_l、`foot`→foot_l。

坐标单位统一是**球半径 = 12**，球心为原点，y 向下为正。想确认锚点在哪，把「锚点调试」模组打开，每个锚点会点一个红点（`nose` 是洋红点，方便和 `mouth` 区分）。


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
| `image` / `png` / `pic` / `texture` / `sticker` | `src` `w` `h` | 在锚点贴一张 PNG。`src` 是 `mod\` 目录里的文件名，`w` 是显示宽度；不写 `h` 就按图片比例自动算高度 |

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
| `src` / `file` / `image` / `png` | 仅 `shape = image` 用：PNG 文件名，只能是 `mod\` 里的纯文件名 |
| `w` / `h` | `image` 里是显示宽 / 高，单位与 `circle` 的 `r` 一致；`h` 省略时按图片宽高比推算 |

`w` 在 `rect` / `triangle` 里是宽度，在其他图元里当线宽用；想明确点就写 `thick`。`triangle` 的 `lean` 是顶点相对底边中点的水平偏移，`corner` 把尖角磨圆，配合 `rot` 能做出向外撇的耳朵。

### 贴图

`shape = image` 可以直接把一张 PNG 贴到锚点上，和其它图元一样跟着球缩放、鼓起、歪头，也能用 `dx` `dy` `rot` `scale` `alpha` `layer` 调位置。图片放在 `mod\` 里，和 ini 同级。

内置的**星星眼**就是这么做的：两个 `[part]` 分别挂在 `eye_l` / `eye_r`，共用一张 `星星.png`，星星会跟着眼神动。右键 → 模组 → 勾上「星星眼」即可，默认关闭。

除了内置那个，你随时可以自己加：把 PNG 和 ini 一起放进 `mod\` 就行，不需要改程序：

```ini
[mod]
name = 我的贴图
enabled = 1

[part]
at = head
shape = image
src = star.png
w = 7        ; 显示宽度，单位与 circle 的 r 一致
dy = -1      ; 再往上挪一点
```

- 支持 8 位 RGBA / RGB / 灰度 / 灰度+Alpha / 调色板（含 `tRNS` 透明）PNG，非隔行；透明像素按 alpha 混合，缩放用双线性采样。
- `src` 只认纯文件名，写路径分隔符、盘符或 `..` 会被忽略，图片也不会被读进来。文件缺失或解码失败时这个图元不显示，其它模组照常工作。
- 同一张图片被多个图元引用时只解码一次；重新加载配置会一起释放重读。
- 解码是纯 C 自带的 inflate，不依赖 zlib、GDI+ 或任何第三方 DLL，单个 exe 照旧能跑。

## 自己编译

```bash
sh build.sh
```

需要 mingw-w64。Windows 下用 MSYS2 / w64devkit 自带的 `sh` 跑同一条命令即可。


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
| `ANCH_MOUTH_Y` `ANCH_NOSE_Y` 等 | 各锚点在球面上的位置，单位是球半径（=12） |
| `ANCH_FACE_FOLLOW_X/Y` | 鼻/嘴跟随视线方向的比例；保持远小于眼睛的滑动幅度，别调大 |


## 目录约定

遵循 `AGENTS.md`

## 许可

[MIT](LICENSE) © 2026 Minecraftgoose

## 声明

本软件与Moonshot AI无关，亦不是其官方产品
