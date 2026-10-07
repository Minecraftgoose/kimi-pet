# kimi小圆球

一个会自己东张西望的小蓝球，住在你的 Windows 桌面上。

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
| `KimiPet.ico` | 全尺寸图标（16~256 共 14 档） |

Windows 7 / 8 / 10 / 11 通吃

## 怎么玩

| 操作 | 效果 |
|---|---|
| 左键拖动 | 挪位置，全程跟手，拖动时动画不中断 |
| 左键单击 | 逗它，它会鼓一下 |
| 连点 5 次 | 它生气了，眯着眼瞪你几秒 |
| 滚轮停在球上 | 调大小，每档 ±12%，60px ~ 420px 无级 |
| 右键 | 菜单：始终置顶 / 跟随鼠标 / 开机自启 / 球大小 / 重新居中 / 关于 / 退出 |

没人理它的时候它会自己东张西望，每隔 1.4~3.4 秒换一个方向；按住球就盯着你的鼠标看，
松手 0.3 秒后恢复自己乱看。

球大小记在 `HKCU\Software\KimiPet\BallSize`，下次打开还是这个尺寸。
开机自启写在 `HKCU\...\Run`，取消勾选即删除。

## 自己编译

```bash
# Linux / macOS（需 mingw-w64）
sudo apt install mingw-w64      # macOS: brew install mingw-w64
sh build.sh

# Windows（需 MinGW-w64：MSYS2 的 mingw64 shell 或 w64devkit）
build.bat
```

图标与版本信息由 `icon.rc` 自动嵌入，不需要额外步骤。

只依赖系统自带 DLL：`KERNEL32` / `USER32` / `GDI32` / `ADVAPI32` / `WINMM` / `msvcrt`。

## 想改外观？

所有参数集中在 [`pet.c`](pet.c) 上方的配置区，改完重新编译即可：

| 参数 | 作用 |
|---|---|
| `bodySize` `baseW` `eyeAspect` `eyeGap` `eyeY` | 眼睛形状、大小与间距 |
| `eyeMove` `turnK` `roll` | 眼球转动幅度与球面倾斜感 |
| `pressPop` `jellyK` `jellyMax` | 点击时的鼓起与果冻形变 |
| `angryLid` `lidSlant` | 生气时的眯眼程度与斜切角度 |
| `blink` `jitter` `speed` | 眨眼频率、眼神抖动、追随速度 |
| `SKIN_CORE` `SKIN_IN1` `SKIN_IN2` `SKIN_EDGE` `SKIN_RIM` | 球体配色，中心深边缘浅 |
| `D_MIN` `D_MAX` `PRESET[]` | 可调节的球大小范围与菜单档位 |

「关于」对话框的内容在 `pet.c` 的 `case 1005` 里，直接改那几行字符串。
「关于」对话框的内容在 `pet.c` 的 `case 1005` 里，直接改那几行字符串。

## 许可

[MIT](LICENSE) © 2026 Minecraftgoose
