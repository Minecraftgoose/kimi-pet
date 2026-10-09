/* =====================================================================
   Kimi 小圆球 · 原生 Win32 桌面宠物
   - 纯软件渲染 + UpdateLayeredWindow 透明置顶窗口
   - 兼容 Windows 7 / 8 / 10 / 11，零依赖、单文件 exe
   - 编译：x86_64-w64-mingw32-gcc -O2 -mwindows -o KimiPet.exe pet.c \
           -lgdi32 -luser32 -ladvapi32 -lwinmm -lshell32 -lm
   ===================================================================== */
#ifdef RENDER_TEST
/* 离线渲染验证：不依赖 Win32，直接跑渲染内核 */
#include <math.h>
#include <stdint.h>
typedef struct { int x,y; } POINT;
typedef void* HDC; typedef void* HBITMAP; typedef void* HWND;
typedef void* HMENU; typedef void* HHOOK;
extern double g_simNow;
#else
#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#ifndef RENDER_TEST
#include <windowsx.h>
#endif
#include <math.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef WM_NCMOUSELEAVE
#define WM_NCMOUSELEAVE 0x02A2
#endif

#define WINW 300
#define WINH 300

/* ---------------- 工具 ---------------- */
#ifdef RENDER_TEST
static double now_ms(void){ return g_simNow; }
#else
static double now_ms(void){ return (double)timeGetTime(); }
#endif
static double rnd(double a,double b){ return a + (b-a)*((double)rand()/(double)RAND_MAX); }
static double clampd(double v,double a,double b){ return v<a?a:(v>b?b:v); }
static double lerpd(double a,double b,double t){ return a+(b-a)*t; }
static int clampi(int v,int a,int b){ return v<a?a:(v>b?b:v); }

/* ---------------- 配色 / 参数（与网页版一致） ---------------- */
typedef struct { unsigned char r,g,b; } RGB8;

/* 球体五层色：中心深 → 边缘浅。运行时可变（8 色预设切换） */
static RGB8 g_skin[5]={
  {0x4a,0x86,0xd8},{0x5a,0x95,0xe0},{0x6c,0xa4,0xe8},{0x8c,0xb9,0xf0},{0xa9,0xcd,0xf6}
};

/* 8 个预设：第一个（淡蓝）沿用原版写死的色值，不经过 HSL 换算，
   否则会有肉眼可见的色差 —— 原版 HTML 就是这么处理的。 */
typedef struct { const wchar_t *name; int h; int fixed; } HUE;
static const HUE HUES[8]={
  {L"淡蓝",215,1}, {L"天蓝",201,0}, {L"靛蓝",230,0}, {L"紫", 266,0},
  {L"青",  186,0}, {L"绿",  146,0}, {L"粉",  337,0}, {L"橙",  36,0}
};
/* 原版淡蓝的固定色值（HSL 换算不出来的一模一样） */
static const RGB8 FIXED_BLUE[5]={
  {0x4a,0x86,0xd8},{0x5a,0x95,0xe0},{0x6c,0xa4,0xe8},{0x8c,0xb9,0xf0},{0xa9,0xcd,0xf6}
};
static const double HUE_S=65.0, HUE_L=57.0;   /* 默认饱和度 / 明度（原版） */

static int g_hueIdx=0;                        /* 当前预设索引 */
static void apply_hue(int idx);               /* 实现见下方 */
static void save_hue(void);
static void load_hue(void);

/* HSL → RGB（h:0..360, s/l:0..100）
   与原版 HTML 的 hsl2hex 逐位对齐：
     k = (n + h/30) % 12 ;  f = 255*(l - a*max(-1, min(k-3, min(9-k, 1)))) */
static RGB8 hsl2rgb(double h,double s,double l){
  s=clampd(s,0,100)/100.0; l=clampd(l,0,100)/100.0;
  double a=s*(l<1-l?l:1-l);
  const int n0[3]={0,8,4};
  RGB8 o; unsigned char *out[3]={&o.r,&o.g,&o.b};
  for(int i=0;i<3;i++){
    double k=fmod(n0[i]+h/30.0,12.0); if(k<0) k+=12.0;
    double inner=(9.0-k < 1.0)?(9.0-k):1.0;   /* min(9-k, 1) */
    double m=(k-3.0 < inner)?(k-3.0):inner;    /* min(k-3, ...) */
    if(m < -1.0) m=-1.0;                       /* max(-1, ...) */
    double v=255.0*(l-a*m);
    v=clampd(v,0,255);
    *out[i]=(unsigned char)lround(v);
  }
  return o;
}

/* 由色相生成 5 层：传进来的 l 是 core 的明度，往外逐层变浅 */
static void skin_from_hue(int h,double s,double l){
  g_skin[0]=hsl2rgb(h, s,   clampd(l,     12,88));
  g_skin[1]=hsl2rgb(h, s+2, clampd(l+7,   18,92));
  g_skin[2]=hsl2rgb(h, s+4, clampd(l+14,  26,94));
  g_skin[3]=hsl2rgb(h, s+6, clampd(l+22,  34,96));
  g_skin[4]=hsl2rgb(h, s+8, clampd(l+29,  44,97));
}

static void apply_hue(int idx){
  if(idx<0) idx=0; if(idx>7) idx=7;
  g_hueIdx=idx;
  if(HUES[idx].fixed) for(int i=0;i<5;i++) g_skin[i]=FIXED_BLUE[i];
  else skin_from_hue(HUES[idx].h, HUE_S, HUE_L);
}

/* 取某个预设的五层色（不改动全局状态，供设置面板画色块用） */
static void hue_skin(int idx, RGB8 out[5]){
  if(HUES[idx].fixed){ for(int i=0;i<5;i++) out[i]=FIXED_BLUE[i]; return; }
  double s=HUE_S, l=HUE_L, h=HUES[idx].h;
  out[0]=hsl2rgb(h, s,   clampd(l,     12,88));
  out[1]=hsl2rgb(h, s+2, clampd(l+7,   18,92));
  out[2]=hsl2rgb(h, s+4, clampd(l+14,  26,94));
  out[3]=hsl2rgb(h, s+6, clampd(l+22,  34,96));
  out[4]=hsl2rgb(h, s+8, clampd(l+29,  44,97));
}

static const double BODY_SIZE=24.0, BASE_W=5.0, EYE_SCALE=.53, EYE_ASPECT=2.24,
  EYE_GAP=2.1, EYE_Y=-3.0, EYE_ROUND=1.0,
  EYE_MOVE=3.4, EYE_MOVEY=.62, GAZE_RANGE=.60, SPEED=.45, BLINK=.87,
  PRESS_POP=.11, PRESS_HOLD=.03, POP_GROW_MS=230.0, POP_HOLD_MS=250.0,
  POP_GROW_STIFF=100.0, JELLY_K=.05, JELLY_MAX=.07,
  SHADOW_SCALE=1.0, SHADOW_DX=.10, SHADOW_DY=.14, SHADOW_ALPHA=.30,
  ANGRY_LID=.43, LID_SLANT=1.1, JITTER=.72, TURN_K=1.8, ROLL=1.6;

static const double RAGE_TAPS=5, RAGE_WINDOW=900.0, RAGE_COOL=2800.0;

static double eyeW(void){ return BASE_W*EYE_SCALE; }
static double eyeH(void){ return eyeW()*EYE_ASPECT; }

/* ---------------- 状态 ---------------- */
static double S=6.0, ES=6.0;
static int    g_win=WINW;                /* 窗口边长（设备像素，正方形），随球大小变化 */
static double g_D=144.0;                 /* 球体直径（逻辑像素，用户感知的大小），默认 144 = 原版观感 */
static double g_scale=1.0;               /* DPI 缩放因子（96 DPI = 1.0，150% = 1.5）*/
static void   set_ball_size(double D);   /* 实现见窗口过程段 */
static void   save_size(void);
static void   load_size(void);
static double cx0=WINW/2.0, cy0=WINH/2.0;
static double gazeX=0,gazeY=0,gazeVX=0,gazeVY=0,gazeTX=0,gazeTY=0,gazeNext=0,gazeSeed=0;
static double blinkT0=-1e9, blinkDur=170, blinkNext=0;
static double press=0, pressV=0, sq=0;
static int    holding=0, onBall=0;
static double popUntil=-1e9;
static double combo=0, lastTap=-1e9, rageUntil=-1e9, rage=0;
static int    angryMood=0;
static double sharedRot=0;

static int g_x=0, g_y=0;                 /* 窗口左上角（屏幕坐标） */
static int g_follow=0;                   /* 跟随鼠标（默认关：保留原版「自己到处看」的手感） */
static int g_topmost=1;                  /* 始终置顶 */
static POINT g_mouse; static int g_mouseValid=0;
static HHOOK g_hook=NULL;
static HWND  g_hwnd=NULL;
static HMENU g_menu=NULL;

/* ---------------- 位图缓冲 ---------------- */
static HDC      g_memdc=NULL;
static HBITMAP  g_bmp=NULL, g_old=NULL;
static unsigned char *g_bits=NULL;       /* BGRA premultiplied, top-down */

static void buf_clear(void){ memset(g_bits,0,(size_t)g_win*g_win*4); }

static void buf_blend(int x,int y,unsigned char br,unsigned char bg,unsigned char bb,double a){
  if(x<0||y<0||x>=g_win||y>=g_win||a<=0) return;
  if(a>1) a=1;
  unsigned char *p = g_bits + ((size_t)y*g_win + x)*4;
  /* premultiplied BGRA: out = src*a + dst*(1-a) */
  double ia = 1.0-a;
  p[0] = (unsigned char)(bb*a + p[0]*ia);
  p[1] = (unsigned char)(bg*a + p[1]*ia);
  p[2] = (unsigned char)(br*a + p[2]*ia);
  p[3] = (unsigned char)(255.0*a + p[3]*ia);
}

static RGB8 grad_color(double t){
  static const double st[5]={0,0.40,0.72,0.92,1.0};
  if(t<=0) return g_skin[0];
  if(t>=1) return g_skin[4];
  for(int i=0;i<4;i++){
    if(t>=st[i] && t<=st[i+1]){
      double u=(t-st[i])/(st[i+1]-st[i]);
      RGB8 o={ (unsigned char)lerpd(g_skin[i].r,g_skin[i+1].r,u),
               (unsigned char)lerpd(g_skin[i].g,g_skin[i+1].g,u),
               (unsigned char)lerpd(g_skin[i].b,g_skin[i+1].b,u) };
      return o;
    }
  }
  return g_skin[4];
}

/* ---------------- 运动 ---------------- */
static void step(double now,double dt){
  double ds = (dt>40?40:dt)/1000.0;
  int popping = now < popUntil;
  double pT = popping ? -PRESS_POP : ((holding&&onBall) ? PRESS_HOLD : 0.0);
  double st1 = popping ? POP_GROW_STIFF : 150.0;
  double d1 = 2.0*sqrt(st1)*0.42;
  int steps = (int)ceil(ds/(1.0/240.0)); if(steps<1) steps=1;
  double h = ds/steps;
  for(int i=0;i<steps;i++){
    pressV += ((pT-press)*st1 - pressV*d1)*h;
    press  += pressV*h;
  }
  sq = clampd(pressV*JELLY_K, -JELLY_MAX, JELLY_MAX);

  double target = (now<rageUntil)?1.0:0.0;
  double tau = target?0.10:0.05;
  rage += (target-rage)*(1.0-exp(-ds/tau));
  if(now < lastTap-RAGE_WINDOW) combo=0;
  angryMood = rage>0.30 ? 1 : 0;

  /* 注视方向 */
  double R = BODY_SIZE/2.0;
  double bodyRpx = R*(1-press)*S;
  /* 原版行为：按住才盯着你看；没人理它时就自己东张西望。
     「跟随鼠标」是可选增强，默认关 —— 否则会永久抢占自发环视。 */
  if(holding && g_mouseValid){
    double wx = (double)g_x + cx0, wy = (double)g_y + cy0;
    double dx = (double)g_mouse.x - wx, dy = (double)g_mouse.y - wy;
    double d = hypot(dx,dy); if(d<1) d=1;
    double sat = bodyRpx*0.8; if(sat<1) sat=1;
    double n = d/(d+sat);
    gazeTX = dx/d*n; gazeTY = dy/d*n;
    gazeNext = now + 1e9;                  /* 按住期间不自动换方向 */
  }else if(g_follow && g_mouseValid){
    double wx = (double)g_x + cx0, wy = (double)g_y + cy0;
    double dx = (double)g_mouse.x - wx, dy = (double)g_mouse.y - wy;
    double d = hypot(dx,dy); if(d<1) d=1;
    double sat = bodyRpx*0.8; if(sat<1) sat=1;
    double n = d/(d+sat);
    if(d > 520) n *= 0.25;                 /* 太远就别瞪着了 */
    gazeTX = dx/d*n; gazeTY = dy/d*n;
    gazeNext = now + 1e9;
  }else if(now > gazeNext){
    /* 自己随便看：换方向间隔 1.4~3.4 秒，跟原 HTML 一致 */
    gazeTX = rnd(-1,1)*.85; gazeTY = rnd(-1,1)*.7;
    gazeNext = now + rnd(1400,3400);
  }

  double st = 30.0+SPEED*340.0, dp = 2.0*sqrt(st)*0.62, t = now/1000.0;
  double mx=(sin(t*1.7+gazeSeed)*.6+sin(t*4.3+gazeSeed*2.7)*.3)*.08*JITTER;
  double my=(cos(t*2.3+gazeSeed*1.3)*.6+cos(t*5.1+gazeSeed*3.1)*.3)*.06*JITTER;
  gazeVX += ((gazeTX+mx-gazeX)*st - gazeVX*dp)*ds;
  gazeVY += ((gazeTY+my-gazeY)*st - gazeVY*dp)*ds;
  gazeX  += gazeVX*ds; gazeY += gazeVY*ds;

  if(now>blinkNext){
    if(now-blinkT0>blinkDur){ blinkT0=now; blinkDur=rnd(150,230); }
    blinkNext = now + 1000.0/(0.05+BLINK*0.6)*rnd(.55,1.6);
  }
}

static double eyeLid(double now){
  double bp=(now-blinkT0)/blinkDur;
  if(bp<0||bp>=1) return 0;
  if(bp<0.36){ double u=bp/0.36; return u*u*(3-2*u); }
  double u=(bp-0.36)/0.64, e=u*u*(3-2*u);
  return clampd(1.0-e-sin(u*M_PI)*0.09*(1-u),0,1);
}

static void updateSharedRot(void){
  double R = BODY_SIZE/2.0;
  double MA = EYE_MOVE*GAZE_RANGE/R*TURN_K;
  double Ax = gazeX*MA;
  double Ay = EYE_Y/R + gazeY*MA*EYE_MOVEY;
  sharedRot = ROLL*atan2(sin(Ay)*sin(Ax), cos(Ay));
}

/* ---------------- 绘制 ---------------- */
static void draw_shadow(double RX,double RY){
  double BASE = RX>RY?RX:RY;
  double ox = BASE*SHADOW_DX, oy = BASE*SHADOW_DY;
  double sr = BASE*SHADOW_SCALE;
  double r0 = sr*0.35, r1 = sr*1.16;
  double sx = cx0+ox, sy = cy0+oy;
  int x0=(int)floor(sx-r1-2), x1=(int)ceil(sx+r1+2);
  int y0=(int)floor(sy-r1-2), y1=(int)ceil(sy+r1+2);
  for(int y=y0;y<=y1;y++){
    for(int x=x0;x<=x1;x++){
      /* 反向施加果冻拉伸（等价于在缩放坐标系里画圆） */
      double ddx=(x+0.5-sx)*(BASE/RX), ddy=(y+0.5-sy)*(BASE/RY);
      double d=hypot(ddx,ddy);
      if(d>r1) continue;
      double u=(d-r0)/(r1-r0); if(u<0) u=0;
      double A=SHADOW_ALPHA;
      double a = (u<0.58) ? lerpd(A, A*0.82, u/0.58) : A*0.82*(1.0-(u-0.58)/0.42);
      if(a<=0) continue;
      buf_blend(x,y,34,70,124,a);
    }
  }
}

static void draw_body(double RX,double RY){
  double BASE = RX>RY?RX:RY;
  double fx = cx0-RX*0.14, fy = cy0-RY*0.18;
  double r0 = RX*0.05, r1 = BASE*1.02;
  int x0=(int)floor(cx0-RX-2), x1=(int)ceil(cx0+RX+2);
  int y0=(int)floor(cy0-RY-2), y1=(int)ceil(cy0+RY+2);
  for(int y=y0;y<=y1;y++){
    for(int x=x0;x<=x1;x++){
      double dx=(x+0.5)-cx0, dy=(y+0.5)-cy0;
      double e=hypot(dx/RX, dy/RY);
      if(e>1.02) continue;
      /* 近似屏幕空间距离，用于抗锯齿 */
      double gx=dx/(RX*RX), gy=dy/(RY*RY);
      double gn=hypot(gx,gy)/(e>1e-6?e:1e-6);
      double dpix=(e-1.0)/(gn>1e-9?gn:1e-9);
      double cov=clampd(0.5-dpix,0,1);
      if(cov<=0) continue;
      double t=(hypot((x+0.5)-fx,(y+0.5)-fy)-r0)/(r1-r0);
      RGB8 c=grad_color(t);
      buf_blend(x,y,c.r,c.g,c.b,cov);
    }
  }
}

static void draw_eye(int side,double lid){
  double ew=eyeW(), eh=eyeH();
  double rx=ew/2.0, ry=eh/2.0;
  double r=rx*EYE_ROUND, flat=ry-r;
  double bx=EYE_GAP+ew/2.0;

  double R=BODY_SIZE/2.0;
  double MA=EYE_MOVE*GAZE_RANGE/R*TURN_K;
  double ax=(side==0?-bx:bx)/R + gazeX*MA;
  double ay=EYE_Y/R + gazeY*MA*EYE_MOVEY;
  double px=sin(ax)*cos(ay)*R, py=sin(ay)*R;
  double rot=sharedRot;

  double ex=cx0+px*ES, ey=cy0+py*ES;

  double al = (angryMood && rage>0.01) ? ANGRY_LID*rage : 0.0;
  double lm = lid>al?lid:al;
  double cut = -ry + eh*lm;
  double sl = (angryMood && rage>0.01) ? LID_SLANT*(1.0-al*0.35) : 0.0;
  double yOut=cut-sl, yIn=cut+sl;
  double ya = (side==0)? yOut : yIn;     /* 局部 x = -rx-1 端 */
  double yb = (side==0)? yIn  : yOut;    /* 局部 x = +rx+1 端 */

  double cs=cos(rot), sn=sin(rot);
  double axL=-rx-1.0, ayL=ya, bxL=rx+1.0, byL=yb;
  double dirx=bxL-axL, diry=byL-ayL;
  double nx=-diry, ny=dirx;
  double nl=hypot(nx,ny); if(nl>1e-9){ nx/=nl; ny/=nl; }

  double m=(ry+2.0)*ES+3.0;
  int x0=(int)floor(ex-m), x1=(int)ceil(ex+m);
  int y0=(int)floor(ey-m), y1=(int)ceil(ey+m);
  for(int y=y0;y<=y1;y++){
    for(int x=x0;x<=x1;x++){
      double dx=(x+0.5)-ex, dy=(y+0.5)-ey;
      /* 逆旋转 + 逆缩放 → 局部坐标 */
      double lx=( dx*cs + dy*sn)/ES;
      double ly=(-dx*sn + dy*cs)/ES;
      /* 胶囊 SDF */
      double pa_y=ly+flat;
      double hh=clampd(pa_y/(2.0*flat),0,1);
      double dcap=hypot(lx, ly-(-flat+2.0*flat*hh))-r;
      /* 斜切上眼皮：保留直线下方 */
      double dline = -((lx-axL)*nx + (ly-ayL)*ny);
      double d = (dcap>dline)?dcap:dline;
      double cov = clampd(0.5 - d*ES, 0, 1);
      if(cov<=0) continue;
      buf_blend(x,y,255,255,255,cov);
    }
  }
}

static void render(double now){
  buf_clear();
  double R = BODY_SIZE/2.0*(1.0-press)*S;
  cx0=g_win/2.0; cy0=g_win/2.0;
  ES = S*(1.0-press);
  double RX=R*(1.0+sq*0.5), RY=R*(1.0-sq*0.5);
  draw_shadow(RX,RY);
  draw_body(RX,RY);
  updateSharedRot();
  double lid=eyeLid(now);
  draw_eye(0,lid);
  draw_eye(1,lid);
}

#ifndef RENDER_TEST
/* ---------------- 菜单 / 自启 ---------------- */
static void set_autorun(int on){
  HKEY hk;
  if(RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&hk)!=ERROR_SUCCESS) return;
  if(on){
    wchar_t path[MAX_PATH]; DWORD n=GetModuleFileNameW(NULL,path,MAX_PATH);
    RegSetValueExW(hk,L"KimiPet",0,REG_SZ,(const BYTE*)path,(n+1)*sizeof(wchar_t));
  }else{
    RegDeleteValueW(hk,L"KimiPet");
  }
  RegCloseKey(hk);
}
static int get_autorun(void){
  HKEY hk;
  if(RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS) return 0;
  int on = (RegQueryValueExW(hk,L"KimiPet",NULL,NULL,NULL,NULL)==ERROR_SUCCESS);
  RegCloseKey(hk);
  return on;
}

/* ---------------- 球大小 ----------------
   球直径 D 是「逻辑像素」——用户感知的大小，不随屏幕 DPI 变。
   实际渲染按 DPI 放大：设备像素 = D × g_scale。
   缩放系数 S = D×scale/24（逻辑直径固定 24）→ 半径 R = 12*S = D×scale/2。
   窗口边长 = 1.7 × D × scale（够画下阴影与拖动余量），以球心为锚点缩放，球不会跳。

   之所以区分逻辑/设备：不声明 DPI 感知时，Windows 会把整个窗口（含系统右键菜单）
   当成一张位图拉伸到 150%/200%，于是球和系统菜单一起发虚。
   声明 DPI 感知后系统不再拉伸，菜单由系统按 DPI 原生渲染（清晰），
   我们自己画的位图则按 scale 用更多设备像素来画 —— 视觉大小不变，但锐利了。 */
#define D_MIN 60.0
#define D_MAX 420.0
static const double PRESET[5]={96.0,144.0,200.0,264.0,340.0};  /* 小/中(默认)/大/特大/巨大 */

/* 窗口边长（设备像素）由球直径（逻辑像素）推导 */
static int win_px(double D){ int n=(int)round(D*1.7*g_scale); return n<120?120:n; }

static void rebuild_buffer(int n){
  if(g_bmp){ SelectObject(g_memdc,g_old); DeleteObject(g_bmp); g_bmp=NULL; }
  BITMAPINFO bi; memset(&bi,0,sizeof(bi));
  bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth=n; bi.bmiHeader.biHeight=-n;   /* top-down */
  bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
  g_bmp=CreateDIBSection(g_memdc,&bi,DIB_RGB_COLORS,(void**)&g_bits,NULL,0);
  g_old=(HBITMAP)SelectObject(g_memdc,g_bmp);
}

static void set_ball_size(double D){
  D = clampd(D, D_MIN, D_MAX);
  double oldCx = g_x + g_win/2.0, oldCy = g_y + g_win/2.0;
  int nw = win_px(D);
  g_D = D; g_win = nw;
  S = D*g_scale/BODY_SIZE;                /* 24 * S = D * scale（设备像素） */
  rebuild_buffer(nw);
  int nx=(int)round(oldCx - nw/2.0), ny=(int)round(oldCy - nw/2.0);
  g_x=nx; g_y=ny;
  if(g_hwnd) SetWindowPos(g_hwnd,NULL,nx,ny,nw,nw,SWP_NOZORDER|SWP_NOACTIVATE);
}

static void step_ball_size(int dir){      /* dir>0 放大，dir<0 缩小 */
  set_ball_size(g_D * (dir>0 ? 1.12 : 1.0/1.12));
  save_size();
}

static void save_size(void){
  HKEY hk; DWORD d=(DWORD)lround(g_D*10), disp=0;
  if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\KimiPet",0,NULL,REG_OPTION_NON_VOLATILE,
                     KEY_SET_VALUE,NULL,&hk,&disp)!=ERROR_SUCCESS) return;
  RegSetValueExW(hk,L"BallSize",0,REG_DWORD,(const BYTE*)&d,sizeof(d));
  RegCloseKey(hk);
}
static void load_size(void){
  HKEY hk; DWORD d=0, sz=sizeof(d);
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\KimiPet",0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS) return;
  if(RegQueryValueExW(hk,L"BallSize",NULL,NULL,(LPBYTE)&d,&sz)==ERROR_SUCCESS && d>0)
    g_D = clampd((double)d/10.0, D_MIN, D_MAX);
  RegCloseKey(hk);
}

static void save_hue(void){
  HKEY hk; DWORD d=(DWORD)g_hueIdx, disp=0;
  if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\KimiPet",0,NULL,REG_OPTION_NON_VOLATILE,
                     KEY_SET_VALUE,NULL,&hk,&disp)!=ERROR_SUCCESS) return;
  RegSetValueExW(hk,L"Hue",0,REG_DWORD,(const BYTE*)&d,sizeof(d));
  RegCloseKey(hk);
}
static void load_hue(void){
  HKEY hk; DWORD d=0, sz=sizeof(d);
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\KimiPet",0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS) return;
  if(RegQueryValueExW(hk,L"Hue",NULL,NULL,(LPBYTE)&d,&sz)==ERROR_SUCCESS)
    g_hueIdx = clampi((int)d, 0, 7);
  RegCloseKey(hk);
}

static void popup_menu(int x,int y){
  if(!g_menu){
    /* 球大小：最小 60 是滚轮能缩到的下限（D_MIN），单独给个入口免得要滚半天 */
    HMENU sub=CreatePopupMenu();
    AppendMenuW(sub,MF_STRING,1017,L"最小 (60 px)");
    AppendMenuW(sub,MF_STRING,1010,L"小 (96 px)");
    AppendMenuW(sub,MF_STRING,1011,L"中 · 默认 (144 px)");
    AppendMenuW(sub,MF_STRING,1012,L"大 (200 px)");
    AppendMenuW(sub,MF_STRING,1013,L"特大 (264 px)");
    AppendMenuW(sub,MF_STRING,1014,L"巨大 (340 px)");
    AppendMenuW(sub,MF_SEPARATOR,0,NULL);
    AppendMenuW(sub,MF_STRING,1015,L"放大一档\t滚轮向上");
    AppendMenuW(sub,MF_STRING,1016,L"缩小一档\t滚轮向下");

    /* 颜色：8 个预设直接做成子菜单，当前色打勾 */
    HMENU hue=CreatePopupMenu();
    for(int i=0;i<8;i++) AppendMenuW(hue,MF_STRING,1020+i,HUES[i].name);

    g_menu=CreatePopupMenu();
    AppendMenuW(g_menu,MF_POPUP,(UINT_PTR)hue,L"颜色");
    AppendMenuW(g_menu,MF_STRING,1001,L"始终置顶");
    AppendMenuW(g_menu,MF_STRING,1002,L"跟随鼠标");
    AppendMenuW(g_menu,MF_STRING,1003,L"开机自启");
    AppendMenuW(g_menu,MF_SEPARATOR,0,NULL);
    AppendMenuW(g_menu,MF_POPUP,(UINT_PTR)sub,L"球大小");
    AppendMenuW(g_menu,MF_STRING,1004,L"重新居中");
    AppendMenuW(g_menu,MF_STRING,1005,L"关于 Kimi 小圆球");
    AppendMenuW(g_menu,MF_STRING,1006,L"退出");
  }
  CheckMenuItem(g_menu,1001, g_topmost?MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(g_menu,1002, g_follow ?MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(g_menu,1003, get_autorun()?MF_CHECKED:MF_UNCHECKED);
  for(int i=0;i<8;i++)
    CheckMenuItem(g_menu,1020+i, (i==g_hueIdx)?MF_CHECKED:MF_UNCHECKED);
  SetForegroundWindow(g_hwnd);
  TrackPopupMenu(g_menu, TPM_LEFTALIGN|TPM_RIGHTBUTTON|TPM_NOANIMATION, x, y, 0, g_hwnd, NULL);
}

/* ---------------- 窗口过程 ---------------- */
static double ball_hit_r(void){
  return BODY_SIZE/2.0*(1.0-press)*S*1.25;
}

/* ---------------- 拖动：自己接管，绝不用系统 HTCAPTION 那套 ----------------
   系统默认拖动会进一个 modal 消息循环，期间只派发鼠标消息、WM_TIMER 被挡住
   → 渲染停摆、画面冻在原地，松手才一次性跳到新位置（就是「瞬移」的元凶）。
   这里改成：按下时记下「鼠标起点 + 窗口起点」，之后每次鼠标移动直接
   SetWindowPos 到 起点+位移。WM_TIMER 全程正常跑，拖动时动画照样流畅跟手。 */
static int g_dragging=0;
static POINT g_dragMouse0, g_dragWin0;
static int   g_dragMoved=0;

static void drag_to(int mx,int my){
  int nx = g_dragWin0.x + (mx - g_dragMouse0.x);
  int ny = g_dragWin0.y + (my - g_dragMouse0.y);
  /* 别让它彻底跑出工作区，至少留 48px 能看见 */
  RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
  const int keep=48;
  if(nx > wa.right-keep)          nx = wa.right-keep;
  if(nx + g_win < wa.left+keep)    nx = wa.left+keep-g_win;
  if(ny > wa.bottom-keep)         ny = wa.bottom-keep;
  if(ny + g_win < wa.top+keep)     ny = wa.top+keep-g_win;
  g_dragMoved = abs(mx-g_dragMouse0.x) + abs(my-g_dragMouse0.y);
  if(nx==g_x && ny==g_y) return;
  g_x=nx; g_y=ny;
  SetWindowPos(g_hwnd,NULL,nx,ny,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}

static LRESULT CALLBACK MouseProc(int code,WPARAM w,LPARAM l){
  if(code==HC_ACTION && (w==WM_MOUSEMOVE||w==WM_NCMOUSEMOVE)){
    MSLLHOOKSTRUCT *s=(MSLLHOOKSTRUCT*)l;
    g_mouse=s->pt; g_mouseValid=1;
    /* 拖动中：全局钩子保证鼠标飞出窗口/飞到别的窗口上也照样跟手 */
    if(g_dragging) drag_to(s->pt.x, s->pt.y);
  }
  /* 滚轮调大小：鼠标停在球上滚才生效，并吞掉滚轮避免同时滚动下面的窗口 */
  if(code==HC_ACTION && w==WM_MOUSEWHEEL){
    MSLLHOOKSTRUCT *s=(MSLLHOOKSTRUCT*)l;
    g_mouse=s->pt; g_mouseValid=1;
    int px=s->pt.x-g_x, py=s->pt.y-g_y;
    double d=hypot((double)px-g_win/2.0,(double)py-g_win/2.0);
    if(d<=ball_hit_r()){
      int delta=(short)HIWORD(s->mouseData);
      step_ball_size(delta>0?1:-1);
      return 1;                           /* 非 0 = 吞掉这条消息 */
    }
  }
  return CallNextHookEx(g_hook,code,w,l);
}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
  switch(m){
  case WM_NCHITTEST:{
    if(g_dragging) return HTCLIENT;          /* 拖动中：整窗都归我，别中途断掉 */
    int px=GET_X_LPARAM(l)-g_x, py=GET_Y_LPARAM(l)-g_y;
    double d=hypot((double)px-cx0,(double)py-cy0);
    if(d<=ball_hit_r()) return HTCLIENT;     /* 球内：可拖动可点击 */
    return HTTRANSPARENT;                    /* 球外：鼠标穿透到桌面 */
  }
  case WM_LBUTTONDOWN:{
    int mx=GET_X_LPARAM(l)+g_x, my=GET_Y_LPARAM(l)+g_y;
    double d=hypot((double)GET_X_LPARAM(l)-cx0,(double)GET_Y_LPARAM(l)-cy0);
    if(d>ball_hit_r()) return 0;
    g_mouse.x=mx; g_mouse.y=my; g_mouseValid=1;
    g_dragging=1; g_dragMoved=0;
    g_dragMouse0.x=mx; g_dragMouse0.y=my;
    g_dragWin0.x=g_x;  g_dragWin0.y=g_y;
    holding=1; onBall=1;
    popUntil = now_ms() + POP_GROW_MS + POP_HOLD_MS;   /* 一按下去就鼓起来 */
    SetCapture(h);                                     /* 拖出窗口外也收得到抬手 */
    return 0;
  }
  case WM_MOUSEMOVE:
    if(g_dragging) drag_to(GET_X_LPARAM(l)+g_x, GET_Y_LPARAM(l)+g_y);
    return 0;
  case WM_LBUTTONUP:{
    if(!g_dragging) return 0;
    double now=now_ms();
    int mx=GET_X_LPARAM(l)+g_x, my=GET_Y_LPARAM(l)+g_y;
    if(g_dragMoved<5){                       /* 基本没动 → 算点击 */
      if(now>=rageUntil){
        combo = (now-lastTap<RAGE_WINDOW)? combo+1 : 1;
        lastTap = now;
        if(combo>=RAGE_TAPS){
          double extra = (combo-RAGE_TAPS>10?10:(combo-RAGE_TAPS))*380.0;
          if(now+RAGE_COOL+extra > rageUntil) rageUntil = now+RAGE_COOL+extra;
        }
      }
    }
    g_dragging=0; holding=0; onBall=0;
    gazeNext = now + 300;                    /* 松手后停一下，再自己随便看（原版行为） */
    ReleaseCapture();
    return 0;
  }
  case WM_CAPTURECHANGED:                    /* 捕获被系统抢走也要收尾，别卡在拖动态 */
    if(g_dragging){ g_dragging=0; holding=0; onBall=0; gazeNext=now_ms()+300; }
    return 0;
  case WM_RBUTTONUP:
    popup_menu(GET_X_LPARAM(l)+g_x, GET_Y_LPARAM(l)+g_y);
    return 0;
  case WM_COMMAND:
    switch(LOWORD(w)){
    case 1001:
      g_topmost=!g_topmost;
      SetWindowPos(h, g_topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
      break;
    case 1002: g_follow=!g_follow; break;
    case 1003: set_autorun(!get_autorun()); break;
    case 1010: case 1011: case 1012: case 1013: case 1014:
      set_ball_size(PRESET[LOWORD(w)-1010]); save_size(); break;
    case 1017:                                   /* 最小 = 滚轮能缩到的下限 */
      set_ball_size(D_MIN); save_size(); break;
    case 1015: step_ball_size( 1); break;
    case 1016: step_ball_size(-1); break;
    case 1020: case 1021: case 1022: case 1023:
    case 1024: case 1025: case 1026: case 1027:
      apply_hue(LOWORD(w)-1020); save_hue(); break;
    case 1004:{
      RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
      g_x = (wa.left+wa.right)/2 - g_win/2;
      g_y = (wa.top+wa.bottom)/2 - g_win/2;
      SetWindowPos(h,NULL,g_x,g_y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
      break;
    }
    case 1005:
      /* 关于：内容以源码里的这段为准，改这里即可 */
      MessageBoxW(h,
        L"kimi小圆球\n"
        L"\n"
        L"作者：Minecraftgoose\n"
        L"https://github.com/Minecraftgoose/\n"
        L"\n"
        L"仓库：\n"
        L"https://github.com/Minecraftgoose/kimi-pet\n"
        L"\n"
        L"MIT许可证",
        L"关于", MB_OK|MB_ICONINFORMATION);
      break;
    case 1006:
      if(g_menu){ DestroyMenu(g_menu); g_menu=NULL; }
      PostQuitMessage(0);
      break;
    }
    return 0;
  case WM_WINDOWPOSCHANGED:{
    WINDOWPOS *wp=(WINDOWPOS*)l;
    if(!(wp->flags & SWP_NOMOVE)){ g_x=wp->x; g_y=wp->y; }
    return 0;
  }
  case WM_TIMER:{
    static double prev=0;
    double now=now_ms();
    double dt=prev?now-prev:16.0; prev=now;
    step(now,dt);
    render(now);
    POINT ptSrc={0,0}, ptDst={g_x,g_y};
    SIZE sz={g_win,g_win};
    BLENDFUNCTION bf; bf.BlendOp=AC_SRC_OVER; bf.BlendFlags=0; bf.SourceConstantAlpha=255; bf.AlphaFormat=AC_SRC_ALPHA;
    UpdateLayeredWindow(h,NULL,&ptDst,&sz,g_memdc,&ptSrc,0,&bf,ULW_ALPHA);
    return 0;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(h,m,w,l);
}

/* ---------------- DPI ----------------
   必须在创建任何窗口之前调用，否则系统已经按 DPI-unaware 建好了虚拟化层。
   SetProcessDPIAware 是 Vista+ 的系统级感知；Win8.1 的 Per-Monitor 版本
   需要动态加载，Win7 上没有也不影响 —— 系统级感知足够解决发虚。 */
static void init_dpi(void){
  typedef BOOL (WINAPI *F1)(void);
  F1 f=(F1)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDPIAware");
  if(f) f();

  HDC hdc=GetDC(NULL);
  if(hdc){
    int dpi=GetDeviceCaps(hdc,LOGPIXELSX);      /* 96 = 100%, 144 = 150%, 192 = 200% */
    ReleaseDC(NULL,hdc);
    if(dpi>0) g_scale = clampd((double)dpi/96.0, 1.0, 4.0);
  }
}

/* ---------------- 入口 ---------------- */
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,LPSTR cmd,int nShow){
  (void)hPrev;(void)cmd;(void)nShow;
  srand((unsigned)time(NULL));
  timeBeginPeriod(1);
  gazeSeed = rnd(0,99);
  blinkNext = now_ms()+900;

  init_dpi();                            /* 先声明 DPI 感知，再建窗口 */
  load_size();
  load_hue();
  apply_hue(g_hueIdx);
  g_win = win_px(g_D);                   /* 建窗口前先把尺寸算好 */
  S = g_D*g_scale/BODY_SIZE;

  WNDCLASSW wc; memset(&wc,0,sizeof(wc));
  wc.lpfnWndProc=WndProc; wc.hInstance=hInst; wc.lpszClassName=L"KimiPetCls";
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hIcon  =(HICON)LoadImageW(hInst,MAKEINTRESOURCEW(1),IMAGE_ICON,0,0,LR_DEFAULTCOLOR|LR_DEFAULTSIZE);
  wc.style=CS_HREDRAW|CS_VREDRAW;
  if(!RegisterClassW(&wc)) return 0;

  RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
  g_x = wa.right - g_win - 60;
  g_y = wa.bottom - g_win - 60;
  if(g_x < wa.left) g_x = wa.left + 20;
  if(g_y < wa.top)  g_y = wa.top + 20;

  g_hwnd = CreateWindowExW(
    WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,
    L"KimiPetCls",L"KimiPet",
    WS_POPUP|WS_VISIBLE,
    g_x,g_y,g_win,g_win,
    NULL,NULL,hInst,NULL);
  if(!g_hwnd) return 0;

  /* 位图缓冲（按设备像素尺寸） */
  HDC scr=GetDC(NULL);
  g_memdc=CreateCompatibleDC(scr);
  ReleaseDC(NULL,scr);
  set_ball_size(g_D);                    /* 建缓冲 + 定窗口尺寸 */
  if(!g_bits) return 0;

  /* 全局鼠标钩子：让小球能看见你的鼠标 */
  g_hook=SetWindowsHookExW(WH_MOUSE_LL,MouseProc,GetModuleHandleA(NULL),0);

  SetTimer(g_hwnd,1,16,NULL);

  MSG msg;
  while(GetMessageW(&msg,NULL,0,0)){ TranslateMessage(&msg); DispatchMessageW(&msg); }

  if(g_hook) UnhookWindowsHookEx(g_hook);
  timeEndPeriod(1);
  return (int)msg.wParam;
}
#endif /* !RENDER_TEST */
