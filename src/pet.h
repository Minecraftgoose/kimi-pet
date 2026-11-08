/* ==========================================================================
   KimiPet —— 公共声明
   所有模块共用的类型、常量、全局变量与函数原型都在这里。
   实现分散在 util.c / config.c / mod.c / render.c / update.c / main.c。
   ========================================================================== */
#ifndef KIMIPET_H
#define KIMIPET_H

/* ---------------- 平台 ---------------- */
#ifdef RENDER_TEST
#include <math.h>
#include <stdint.h>
typedef struct { int x,y; } POINT;
typedef void* HDC; typedef void* HBITMAP; typedef void* HWND;
typedef void* HMENU; typedef void* HHOOK;
typedef unsigned long DWORD; typedef DWORD *LPDWORD;
typedef void* HKEY; typedef void* HINSTANCE; typedef void* HICON;
typedef void* HCURSOR; typedef void* HGDIOBJ; typedef void* HFONT;
extern double g_simNow;
#define _snwprintf swprintf
#define _snprintf snprintf
#else
#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <wininet.h>
#include <math.h>
#endif
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef WM_NCMOUSELEAVE
#define WM_NCMOUSELEAVE 0x02A2
#endif

#define WINW 300
#define WINH 300

/* ---------------- 版本 / 仓库（自动更新用） ---------------- */
#define KP_VERSION_MAJOR 1
#define KP_VERSION_MINOR 4
#define KP_VERSION_PATCH 1
#define KP_VERSION_STR   "v1.4.1"
#define KP_VERSION_W    L"v1.4.1"
#define KP_REPO_OWNER    "Minecraftgoose"
#define KP_REPO_NAME     "kimi-pet"
#define KP_REPO_OWNER_W  L"Minecraftgoose"
#define KP_REPO_NAME_W   L"kimi-pet"

/* ---------------- 配置容量 ---------------- */
#define MAX_COLORS   32
#define NAME_LEN     48
#define MAX_MODS     32
#define MAX_PARTS    256

/* ---------------- 基础类型 ---------------- */
typedef struct { unsigned char r,g,b; } RGB8;

typedef struct {
  wchar_t name[NAME_LEN];
  int     mode;                 /* 0 = HSL 自动生成 5 层，1 = 5 层显式 RGB */
  double  h, s, l;
  RGB8    layers[5];
} CFG_COLOR;

typedef struct {
  int      topmost, follow, autorun;
  int      sleep;                   /* 1 = 允许睡觉（出厂默认），0 = 永远不睡 */
  double   sleep_delay;             /* 鼠标静止多少秒后入睡 */
  int      sleep_wake_on_move;      /* 1 = 鼠标一动就醒（默认），0 = 只能点 3 下叫醒 */
  int      breathe;                 /* 1 = 开启呼吸训练（四相无限循环，不限时长） */
  double   breathe_in;              /* 吸气秒数：球放大、眼睛睁开 */
  double   breathe_hold_in;         /* 吸气末屏息秒数：球保持最大，0 = 不屏息 */
  double   breathe_out;             /* 呼气秒数：球缩小、眼睛闭上 */
  double   breathe_hold_out;        /* 呼气末屏息秒数：球保持最小，0 = 不屏息 */
  double   size, size_min, size_max;
  wchar_t  color[NAME_LEN];
  int      colorIdx;
} CFG;

/* 配置文件定位用的小节信息 */
typedef struct { size_t secStart, secEnd, T; int secFound; } CFG_SEC;
/* 出厂键清单条目（缺键自动补齐用） */
typedef struct { const wchar_t *sec,*key,*val,*cmt; } CFG_FIELD;

/* ---------------- 锚点 ---------------- */
enum { A_BODY=0, A_EYE_L, A_EYE_R, A_EYES, A_MOUTH, A_HEAD_TOP,
       A_EAR_L, A_EAR_R, A_FOOT_L, A_FOOT_R, ANCH_N };

/* ---------------- 图元 ---------------- */
enum { SH_CIRCLE=0, SH_RING, SH_ELLIPSE, SH_RECT, SH_LINE, SH_ARC, SH_TRIANGLE };

typedef struct {
  int    mod;
  int    layer;      /* 0 = 画在球体之前（耳朵/脚这类该被挡住的），1 = 画在眼睛之后 */
  int    kind;
  /* 语义字段：解析时先存这些，落盘前按 kind 组装成 p[]，避免 w/h/corner 抢槽位 */
  double rad, thick, sw, sh, rx, ry, a0deg, a1deg, corner, lean;
  int    hasThick;
  double p[6];       /* 组装后的最终参数，各 shape 各用各的，互不干扰 */
  RGB8   col;
  double alpha;
  int    a0, a1;     /* 起点 / 终点锚点，a1 = -1 表示终点用相对偏移 */
  double dx, dy;     /* 起点偏移 */
  double tx, ty;     /* 终点偏移（a1<0 时是相对起点的偏移） */
  double rot, scl;
} ModPart;

typedef struct { const char *file; const char *text; } BuiltinMod;
extern const BuiltinMod BUILTIN_MODS[];
extern const int N_BUILTIN_MODS;

typedef struct { wchar_t name[NAME_LEN]; int enabled; } Mod;
typedef struct { wchar_t name[NAME_LEN]; int on; } ModSw;       /* 主配置 [mods] 段的开关 */

/* ---------------- 可调参数（定义在 render.c） ---------------- */
extern const double BODY_SIZE;
extern const double BASE_W;
extern const double EYE_SCALE;
extern const double EYE_ASPECT;
extern const double EYE_GAP;
extern const double EYE_Y;
extern const double EYE_ROUND;
extern const double EYE_MOVE;
extern const double EYE_MOVEY;
extern const double GAZE_RANGE;
extern const double SPEED;
extern const double BLINK;
extern const double PRESS_POP;
extern const double PRESS_HOLD;
extern const double POP_GROW_MS;
extern const double POP_HOLD_MS;
extern const double POP_GROW_STIFF;
extern const double JELLY_K;
extern const double JELLY_MAX;
extern const double SHADOW_SCALE;
extern const double SHADOW_DX;
extern const double SHADOW_DY;
extern const double SHADOW_ALPHA;
extern const double ANGRY_LID;
extern const double LID_SLANT;
extern const double BREATHE_AMP;      /* 呼吸训练时球半径相对变化幅度，±12% */
extern const double JITTER;
extern const double TURN_K;
extern const double ROLL;
extern const double RAGE_TAPS;
extern const double RAGE_WINDOW;
extern const double RAGE_COOL;
extern const double SLEEP_CLOSE;
extern const double SLEEP_TAP_WINDOW;
extern const int SLEEP_WAKE_TAPS;
extern const double SLEEP_WAKE_MOVE;
extern const double HUE_S;
extern const double HUE_L;

/* ---------------- 全局状态 ---------------- */
/* render.c —— 球体状态 / 位图缓冲 */
extern RGB8  g_skin[5];
extern double S, ES;
extern int    g_win;
extern double g_D, g_scale;
extern double cx0, cy0;
extern double gazeX,gazeY,gazeVX,gazeVY,gazeTX,gazeTY,gazeNext,gazeSeed;
extern double blinkT0, blinkDur, blinkNext;
extern double press, pressV, sq;
extern int    holding, onBall;
extern double popUntil;
extern double combo, lastTap, rageUntil, rage;
extern int    angryMood;
extern double sharedRot;
extern double g_lastMove, g_refX, g_refY;
extern int    g_mouseSeen;
extern double g_sleepAmt;
extern int    g_sleepTaps;
extern double g_sleepTap;
extern double g_breath;
/* 呼吸训练：无屏息、无时长限制的两相循环（状态与渲染在 render.c） */
extern double g_trainAmt;             /* 0 = 呼尽（最小、眼闭） ~ 1 = 吸满（最大、眼睁） */
extern double g_trainLid;             /* 训练产生的闭眼量 */
extern double g_trainT0;              /* 本轮循环起点（毫秒），-1e9 = 未启动 */
extern int    g_trainPhase;           /* 0 吸气 / 1 吸末屏息 / 2 呼气 / 3 呼末屏息 */
extern int    g_x, g_y;
extern int    g_follow, g_topmost;
extern POINT  g_mouse;
extern int    g_mouseValid;
extern HDC      g_memdc;
extern HBITMAP  g_bmp, g_old;
extern unsigned char *g_bits;

/* main.c —— 窗口 / 钩子 */
extern HHOOK g_hook;
extern HWND  g_hwnd;
extern HMENU g_menu;

/* mod.c —— 锚点 / 模组 */
extern double g_anchX[ANCH_N], g_anchY[ANCH_N], g_anchRot[ANCH_N];
extern wchar_t g_modDir[1024];
extern Mod     g_mods[MAX_MODS];
extern int     g_nMods;
extern ModPart g_parts[MAX_PARTS];
extern int     g_nParts;
extern ModSw   g_modSw[MAX_MODS];
extern int     g_nModSw;

/* config.c —— 配置 */
extern CFG        g_cfg;
extern CFG_COLOR  g_colors[MAX_COLORS];
extern int        g_nColors;
extern wchar_t    g_cfgPath[1024];
extern int        g_normGuard;

/* ==========================================================================
   函数原型
   ========================================================================== */

/* ---- util.c：数学 / 编码 / 文件 IO / 文本 ---- */
double now_ms(void);
double rnd(double a,double b);
double clampd(double v,double a,double b);
double lerpd(double a,double b,double t);
int    clampi(int v,int a,int b);
int    u8_to_w(const char *s, wchar_t *out, int n);
int    w_to_u8(const wchar_t *s, char *out, int n);
unsigned char *read_file_all(const wchar_t *path, size_t *outLen);
int    write_file_all(const wchar_t *path, const unsigned char *data, size_t len);
int    cieq(const char *a,const char *b);
char  *ltrim(char *s);
char  *rtrim(char *s);
char  *trim(char *s);
int    is_blank_or_comment(const char *s);
wchar_t *wltrim(wchar_t *s);
void   wrtrim(wchar_t *s);
int    parse_bool(const char *v);
int    hexv(char c);
int    parse_hex_rgb(const char *p, RGB8 *out);
void   rgb2hsl(RGB8 c, double *H,double *S,double *L);
RGB8   hsl2rgb(double h,double s,double l);
int    exe_dir(wchar_t *out,int n);
int    reg_get_dword(const wchar_t *name, DWORD *out);

/* ---- config.c：KimiPet.ini ---- */
extern const char *DEFAULT_CFG_TEXT;
void   cfg_reset(void);
void   cfg_normalize(void);
void   cfg_parse(const char *text);
wchar_t *cfg_read_text(void);
int    cfg_write_text(const wchar_t *text);
void   cfg_locate_sec(const wchar_t *text,const wchar_t *sec,CFG_SEC *r);
int    cfg_locate_key(const wchar_t *text,const CFG_SEC *r,const wchar_t *key,
                      size_t *vs,size_t *ve,size_t *lineEnd);
void   cfg_set(const wchar_t *sec,const wchar_t *key,const wchar_t *val);
int    cfg_has_key(const wchar_t *text,const wchar_t *sec,const wchar_t *key);
int    cfg_insert_fields(const wchar_t *sec,const CFG_FIELD *fs,int n);
int    cfg_upgrade(void);
void   cfg_path_init(void);
void   cfg_load(void);
void   reload_cfg(void);
int    find_color(const wchar_t *name);
void   add_color(const wchar_t *name, const CFG_COLOR *c);
int    parse_color_value(char *v, CFG_COLOR *c);
void   save_color(void);
void   save_flag(const wchar_t *key,int on);

/* ---- mod.c：锚点 / 图元 / 模组 ---- */
int    find_anchor(const char *n);
void   eye_center(int side,double *px,double *py);
void   update_anchors(void);
void   draw_mods(int layer);
int    mod_parse_text(const char *text,const wchar_t *fallbackName);
void   mods_clear(void);
void   mods_write_builtin(void);
void   mod_dir_init(void);
void   mods_scan(void);
void   modsw_set(const wchar_t *n,int on);
int    modsw_get(const wchar_t *n,int *found);

/* ---- render.c：绘制 / 运动 ---- */
void   skin_from_hue(int h,double s,double l);
void   apply_color(int idx);
double eyeW(void);
double eyeH(void);
void   step(double now,double dt);
void   breathe_reset(void);              /* 开关切换 / 重载配置时重置相位 */
void   note_mouse_move(int x,int y);
int    sleep_tap(double now);
double eyeLid(double now);
void   render(double now);
int    win_px(double D);
void   rebuild_buffer(int n);
void   set_ball_size(double D);
void   buf_clear(void);
void   buf_blend(int x,int y,unsigned char br,unsigned char bg,unsigned char bb,double a);
RGB8   grad_color(double t);

/* ---- update.c：检查更新 ---- */
#define KP_WM_UPDATE 0x8001       /* 工作线程 → 主线程的私有消息 */
void   upd_on_message(HWND h, void *ctx);
void   upd_on_timeout(HWND h, void *msg);
const char *upd_asset_name(void);
int    parse_ver(const char *t,int *maj,int *min,int *pat);
int    ver_cmp(int a0,int a1,int a2,int b0,int b1,int b2);
int    json_str(const char *json,const char *key,char *out,int outn);
int    find_asset_url(const char *json,const char *want,char *out,int outn);
void   do_update(HWND h);

/* ---- main.c：窗口 / 自启 ---- */
void   set_autorun(int on);
int    get_autorun(void);
double ball_hit_r(void);
void   drag_to(int mx,int my);

#endif /* KIMIPET_H */
