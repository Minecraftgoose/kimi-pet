set -e

cd "$(dirname "$0")"

OUT="${OUT:-$(cd .. && pwd)/exe}"
mkdir -p "$OUT"
echo "产物目录：$OUT"

CFLAGS="-O2 -mwindows"
LIBS="-lgdi32 -luser32 -ladvapi32 -lwinmm -lshell32 -lm"

WR_CP=""; WR_X64=""; WR_X86=""
if command -v x86_64-w64-mingw32-windres >/dev/null 2>&1; then
  WR_X64="x86_64-w64-mingw32-windres"
elif command -v windres >/dev/null 2>&1; then
  WR_X64="windres"
else
  echo "错误：找不到 windres，请先安装 mingw-w64" >&2; exit 1
fi
if command -v i686-w64-mingw32-windres >/dev/null 2>&1; then
  WR_X86="i686-w64-mingw32-windres"
else
  WR_X86="$WR_X64"
fi

if $WR_X64 --codepage=65001 icon.rc -O coff -o /tmp/_kprobe.res 2>/dev/null; then
  WR_CP="--codepage=65001"
fi
rm -f /tmp/_kprobe.res

echo "  -> 编译图标资源"
$WR_X64 $WR_CP icon.rc -O coff -o icon_x64.res
$WR_X86 $WR_CP icon.rc -O coff -o icon_x86.res

echo "  -> KimiPet_x64.exe（64 位）"
x86_64-w64-mingw32-gcc $CFLAGS -o "$OUT/KimiPet_x64.exe" pet.c icon_x64.res $LIBS

if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
  echo "  -> KimiPet_x86.exe（32 位）"
  i686-w64-mingw32-gcc $CFLAGS -o "$OUT/KimiPet_x86.exe" pet.c icon_x86.res $LIBS
else
  echo "  -> 跳过 32 位（未安装 i686-w64-mingw32-gcc）"
fi

rm -f icon_x64.res icon_x86.res
echo "完成：$OUT"
