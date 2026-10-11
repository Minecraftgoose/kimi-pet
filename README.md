# kimi小圆球

一个会自己东张西望的小Kimi，住在你的桌面上。

---

<div align="center"><img src="https://workspace-zb-cdn.qianwen.com/38d4e383eaaa4ba2b5b85683e7e54acc%2Fo%2F1791706079028-c.png?auth_key=1823244840-0-0-b41023e26e966ecc516e5791bcc45274" alt="小球图片" width="auto" /></div>

## 下载

到 [Releases](https://github.com/Minecraftgoose/kimi-pet/releases) 取最新的 `KimiPet-vX.Y.Z.zip`，里面有：

| 文件 | 说明 |
|---|---|
| `KimiPet_x64.exe` | 64 位系统用这个 |
| `KimiPet_x86.exe` | 32 位系统用这个 |


## 怎么玩

| 操作 | 效果 |
|---|---|
| 左键拖动 | 挪位置 |
| 左键单击 | 逗它，它会鼓一下 |
| 连点 5 次 | 它生气了，眯着眼瞪你几秒 |
| 滚轮停在球上 | 调大小，每档 ±12%，60px ~ 420px 无级 |
| 右键 | 菜单：颜色 / 始终置顶 / 跟随鼠标 / 开机自启 / 球大小 / 重新居中 / 关于 / 退出 |

### 换颜色

右键菜单 → **颜色**：

淡蓝（默认）· 天蓝 · 靛蓝 · 紫 · 青 · 绿 · 粉 · 橙

选中的颜色记在 `HKCU\Software\KimiPet\Hue`。

球大小记在 `HKCU\Software\KimiPet\BallSize`。

<div align="center"><img src="https://workspace-zb-cdn.qianwen.com/e1858eb497c44dfcbd60e36d9d659c1c%2Fo%2F1791707183835-c.png?auth_key=1823245860-0-0-8a60a55eb95fb7b84f8937a5a77ddb87" alt="颜色" width="auto" /></div>

## 大屏幕 / 高 DPI

已适配，可用于希沃白板

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
| `g_skin[]` / `HUES[]` | 球体五层配色与 8 色预设的色相 |
| `HUE_S` `HUE_L` | 预设色的默认饱和度与明度 |
| `D_MIN` `D_MAX` `PRESET[]` | 可调节的球大小范围与菜单档位 |


## 目录约定

遵循 `AGENTS.md`

## 许可

[MIT](LICENSE) © 2026 Minecraftgoose
