# kimi小圆球

一个会自己东张西望的小蓝球，住在你的 Windows 桌面上。

**纯 C + Win32 原生程序** —— 不用 Electron、不用浏览器内核、不用 .NET、不用 VC++ 运行库。
单文件源码、零第三方库，编译出来 400KB 出头，双击就跑。

作者：[Minecraftgoose](https://github.com/Minecraftgoose/)
仓库：https://github.com/Minecraftgoose/kimi-pet
MIT许可证

---

## 下载

到 [Releases](https://github.com/Minecraftgoose/kimi-pet/releases) 取最新的 `KimiPet-vX.Y.Z.zip`，里面有：

| 文件 | 说明 |
|---|---|
| `KimiPet_x64.exe` | 64 位系统用这个（绝大多数电脑） |
| `KimiPet_x86.exe` | 32 位系统用这个 |
（Release 的 zip 里**只有两个 exe**，不放 README / ico / LICENSE。
Release 正文留空，由作者自己写。）

不用安装、不用联网、不用管理员权限。Windows 7 / 8 / 10 / 11 通吃 ——
**不依赖 Aero 主题**，经典主题下也是真正透明的（这点和 Electron 方案不一样）。

## 怎么玩

| 操作 | 效果 |
|---|---|
| 左键拖动 | 挪位置，全程跟手，拖动时动画不中断 |
| 左键单击 | 逗它，它会鼓一下 |
| 连点 5 次 | 它生气了，眯着眼瞪你几秒 |
| 滚轮停在球上 | 调大小，每档 ±12%，60px ~ 420px 无级 |
| 右键 | 菜单：颜色 / 始终置顶 / 跟随鼠标 / 开机自启 / 球大小 / 重新居中 / 关于 / 退出 |

### 换颜色

右键菜单 → **颜色**，8 个预设直接列在里面，当前色会打勾，点一下球立刻变色：

淡蓝（默认）· 天蓝 · 靛蓝 · 紫 · 青 · 绿 · 粉 · 橙

选中的颜色记在 `HKCU\Software\KimiPet\Hue`，下次打开还是它。

![8 色预设](hue_preview.png)

没人理它的时候它会自己东张西望，每隔 1.4~3.4 秒换一个方向；按住球就盯着你的鼠标看，
松手 0.3 秒后恢复自己乱看。

球大小记在 `HKCU\Software\KimiPet\BallSize`，下次打开还是这个尺寸。
开机自启写在 `HKCU\...\Run`，取消勾选即删除。

## 大屏幕 / 高 DPI

程序在启动时调用 `SetProcessDPIAware()` 声明自己是 DPI 感知的。
**不做这一步的话，Windows 会把整个窗口（连右键菜单一起）当成一张位图拉伸**，
在 125% / 150% / 200% 缩放大屏上就会发虚 —— 菜单文字糊、球边缘有毛边。

声明之后，窗口按设备像素 1:1 渲染：150% 缩放下球会画成 216×216 个真实像素
而不是把 144×144 拉大，所以视觉大小不变但清晰度翻倍。同理菜单由系统按原生
DPI 绘制，不再模糊。

球的尺寸参数 `g_D` 是**逻辑像素**（你在屏幕上感知到的大小），内部再乘 DPI
缩放得到实际渲染分辨率。所以你在 100% 和 200% 屏上调到同一个档位，看到的球
一样大，只是后者更清晰。

> 只处理系统 DPI 缩放，不做多显示器适配（拖到另一个不同 DPI 的显示器上不会
> 重新计算）。

## 自己编译

```bash
# Linux / macOS（需 mingw-w64）
sudo apt install mingw-w64      # macOS: brew install mingw-w64
sh build.sh

# Windows（需 MinGW-w64：MSYS2 的 mingw64 shell 或 w64devkit）
build.bat
```

两种脚本都会把 exe 输出到**仓库外**的 `../exe/`，源码目录里不留编译产物。
目录约定见 [`AGENTS.md`](AGENTS.md)。

CI 里不依赖 `../exe` 的相对解析：跑之前会显式把输出目录指到 `runner.temp`
（`OUT` 环境变量覆盖），产物同样落在仓库外，最后拷回工作区随 Release 上传。
换句话说——**本地和 CI 的产物都不会污染仓库**。

图标与版本信息由 `icon.rc` 自动嵌入，不需要额外步骤。

只依赖系统自带 DLL：`KERNEL32` / `USER32` / `GDI32` / `ADVAPI32` / `WINMM` / `msvcrt`。

## 发布

正常情况：打 tag 推上去，CI 自动构建并发 Release。

```bash
git tag v1.1.0
git push origin v1.1.0
```

### AtomGit 中转的注意事项

本仓库主推在 AtomGit，经中转同步到 GitHub。中转链路下 tag 有几种到不了 GitHub 的情况，
工作流已经逐条兜底：

| 情况 | 兜底方式 |
|---|---|
| tag 正常同步过来 | `push: tags: ['**']` 直接触发（用 `**` 而非 `v*`，glob 匹配写死 `v*` 会漏） |
| 只同步了分支、tag 没过来 | 分支 push 时检查 HEAD 是否正好落在 tag 上，是则照样发布 |
| 中转用 `GITHUB_TOKEN` 推送 | 这类 push 在 GitHub 上**不会再触发任何 workflow**，靠每 6 小时的 `schedule` 轮询补发 |
| 一次性同步多个 tag | GitHub 对单次超过 3 个 tag 的推送不产生事件，同上靠定时兜底 |
| 以上都没生效 | Actions → **Run workflow** → 填 tag 名手动补发 |

发布是幂等的：某个 tag 的 Release 里已经有 `KimiPet_x64.exe` 就会自动跳过，
不会重复上传。手动触发时勾 `force` 可强制重发。

**建议同时确认中转配置里打开了 tag 同步**（AtomGit 同步工具有个 `SYNC_TAGS` 开关，
默认可能关闭，关着的话 tag 根本不会同步过来）。

工作流跑完先看「诊断：仓库 refs 状态」那一步，里面列出了远程有哪些 tag、
HEAD 落在哪个 tag 上，一眼就能判断 tag 到底同步成功没有。

## 想改外观？

所有参数集中在 [`pet.c`](pet.c) 上方的配置区，改完重新编译即可：

| 参数 | 作用 |
|---|---|
| `bodySize` `baseW` `eyeAspect` `eyeGap` `eyeY` | 眼睛形状、大小与间距 |
| `eyeMove` `turnK` `roll` | 眼球转动幅度与球面倾斜感 |
| `pressPop` `jellyK` `jellyMax` | 点击时的鼓起与果冻形变 |
| `angryLid` `lidSlant` | 生气时的眯眼程度与斜切角度 |
| `blink` `jitter` `speed` | 眨眼频率、眼神抖动、追随速度 |
| `g_skin[]` / `HUES[]` | 球体五层配色与 8 色预设的色相（改 `HUES` 就能换预设） |
| `HUE_S` `HUE_L` | 预设色的默认饱和度与明度 |
| `D_MIN` `D_MAX` `PRESET[]` | 可调节的球大小范围与菜单档位 |

「关于」对话框的内容在 `pet.c` 的 `case 1005` 里，直接改那几行字符串。

## 它是怎么画的

每帧在内存里算一张 32 位 BGRA 位图（径向渐变球体 + 胶囊眼 SDF + 偏移阴影），
再通过 `UpdateLayeredWindow` 按逐像素 alpha 合成到桌面上。

因为是软件渲染，透明效果完全自己掌控 —— 不需要系统 DWM 合成，Win7 经典主题下照样透明。

两个值得一提的实现细节：

- **拖动不走 `HTCAPTION`**。系统那套拖动会进 modal 消息循环，`WM_TIMER` 停摆、
  画面冻住，松手才一次性跳过去（就是"瞬移"）。这里自己记录鼠标起点与窗口起点，
  每次 `WM_MOUSEMOVE` 直接算新位置。
- **菜单走 Unicode**。`AppendMenuW` 而非 `AppendMenuA`，中文以 UTF-16 交给系统，
  不经过系统 ANSI 代码页，不会出现 `æµ…ç„¦` 之类的乱码。

## 目录约定

遵循 `AGENTS.md`：

| 路径 | 用途 |
|---|---|
| `tmp/` | 测试用的临时文件，**交付时清空**，不入库 |
| `preview/` | 预览图，保留但不入库（已加进 `.gitignore`） |
| `../exe/` | 编译产物（`KimiPet_x64.exe` / `KimiPet_x86.exe`），在仓库外 |
| `dist/*/*.zip` | CI 打包的最终压缩包，Release 附件（内只含两个 exe） |

## 许可

[MIT](LICENSE) © 2026 Minecraftgoose
