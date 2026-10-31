#ifdef RENDER_TEST
#include <math.h>
#include <stdint.h>
typedef struct { int x,y; } POINT;
typedef void* HDC; typedef void* HBITMAP; typedef void* HWND;
typedef void* HMENU; typedef void* HHOOK;
extern double g_simNow;
#define _snwprintf swprintf
#define _snprintf snprintf
#else
#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#ifndef RENDER_TEST
#include <windowsx.h>
#include <shellapi.h>
#include <wininet.h>
#endif
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

/* ---------------- 配色 / 参数 ---------------- */
typedef struct { unsigned char r,g,b; } RGB8;

static RGB8 g_skin[5]={
  {0x4a,0x86,0xd8},{0x5a,0x95,0xe0},{0x6c,0xa4,0xe8},{0x8c,0xb9,0xf0},{0xa9,0xcd,0xf6}
};
static const double HUE_S=65.0, HUE_L=57.0;   

/* ---------------- 编码 / 文件 IO ---------------- */
#ifdef RENDER_TEST
/* 本机测试用：真正的 UTF-8 <-> wchar_t（UCS-4）转换 */
static int u8_to_w(const char *s, wchar_t *out, int n){
  const unsigned char *p=(const unsigned char*)s;
  int i=0; if(!s||n<=0) return 0;
  while(*p && i<n-1){
    unsigned c=p[0]; int extra=0;
    if(c<0x80)        { extra=0; c&=0x7F; }
    else if((c&0xE0)==0xC0){ extra=1; c&=0x1F; }
    else if((c&0xF0)==0xE0){ extra=2; c&=0x0F; }
    else if((c&0xF8)==0xF0){ extra=3; c&=0x07; }
    else { c='?'; extra=0; }
    p++;
    for(int k=0;k<extra;k++){
      if((*p & 0xC0)!=0x80){ c='?'; break; }
      c=(c<<6)|(unsigned)(*p++ & 0x3F);
    }
    out[i++]=(wchar_t)c;
  }
  out[i]=0; return i;
}
static int w_to_u8(const wchar_t *s, char *out, int n){
  int i=0; if(!s||n<=0) return 0;
  while(*s && i<n-1){
    unsigned c=(unsigned)*s++;
    if(c<0x80){
      out[i++]=(char)c;
    }else if(c<0x800){
      if(i+2>=n) break;
      out[i++]=(char)(0xC0|(c>>6)); out[i++]=(char)(0x80|(c&0x3F));
    }else if(c<0x10000){
      if(i+3>=n) break;
      out[i++]=(char)(0xE0|(c>>12));
      out[i++]=(char)(0x80|((c>>6)&0x3F));
      out[i++]=(char)(0x80|(c&0x3F));
    }else{
      if(i+4>=n) break;
      out[i++]=(char)(0xF0|(c>>18));
      out[i++]=(char)(0x80|((c>>12)&0x3F));
      out[i++]=(char)(0x80|((c>>6)&0x3F));
      out[i++]=(char)(0x80|(c&0x3F));
    }
  }
  out[i]=0; return i;
}
static unsigned char* read_file_all(const wchar_t *path, size_t *outLen){
  char p[1024]; w_to_u8(path,p,sizeof p);
  FILE *f=fopen(p,"rb"); if(!f) return NULL;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  if(n<0){ fclose(f); return NULL; }
  unsigned char *b=(unsigned char*)malloc((size_t)n+1); if(!b){ fclose(f); return NULL; }
  size_t got=fread(b,1,(size_t)n,f); fclose(f);
  b[got]=0; if(outLen) *outLen=got; return b;
}
static int write_file_all(const wchar_t *path, const unsigned char *data, size_t len){
  char p[1024]; w_to_u8(path,p,sizeof p);
  FILE *f=fopen(p,"wb"); if(!f) return 0;
  size_t w=fwrite(data,1,len,f); fclose(f); return (int)(w==len);
}
#else
static int u8_to_w(const char *s, wchar_t *out, int n){
  if(!s||n<=0) return 0;
  int r=MultiByteToWideChar(CP_UTF8,0,s,-1,out,n);
  if(r<=0){ if(out) out[0]=0; return 0; }
  return r-1;
}
static int w_to_u8(const wchar_t *s, char *out, int n){
  if(!s||n<=0) return 0;
  int r=WideCharToMultiByte(CP_UTF8,0,s,-1,out,n,NULL,NULL);
  if(r<=0){ if(out) out[0]=0; return 0; }
  return r-1;
}
static unsigned char* read_file_all(const wchar_t *path, size_t *outLen){
  HANDLE h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,NULL);
  if(h==INVALID_HANDLE_VALUE) return NULL;
  DWORD hi=0, lo=GetFileSize(h,&hi);
  size_t n = ((unsigned long long)hi<<32) | (unsigned long long)lo;
  unsigned char *b=(unsigned char*)malloc(n+1);
  if(!b){ CloseHandle(h); return NULL; }
  DWORD got=0;
  BOOL ok=ReadFile(h,b,(DWORD)n,&got,NULL);
  CloseHandle(h);
  if(!ok && got!=(DWORD)n){ free(b); return NULL; }
  b[got]=0; if(outLen) *outLen=got; return b;
}
static int write_file_all(const wchar_t *path, const unsigned char *data, size_t len){
  HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,NULL);
  if(h==INVALID_HANDLE_VALUE) return 0;
  DWORD w=0;
  BOOL ok=WriteFile(h,data,(DWORD)len,&w,NULL);
  CloseHandle(h);
  return (int)(ok && w==(DWORD)len);
}
#endif

/* ---------------- 颜色表（来自配置文件） ---------------- */
#define MAX_COLORS   32
#define NAME_LEN     48

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
  double   size, size_min, size_max;
  wchar_t  color[NAME_LEN];
  int      colorIdx;
} CFG;

static CFG        g_cfg;
static CFG_COLOR  g_colors[MAX_COLORS];
static int        g_nColors=0;

static void apply_color(int idx);
static void save_color(void);
static void save_flag(const wchar_t *key,int on);

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

static void skin_from_hue(int h,double s,double l){
  g_skin[0]=hsl2rgb(h, s,   clampd(l,     12,88));
  g_skin[1]=hsl2rgb(h, s+2, clampd(l+7,   18,92));
  g_skin[2]=hsl2rgb(h, s+4, clampd(l+14,  26,94));
  g_skin[3]=hsl2rgb(h, s+6, clampd(l+22,  34,96));
  g_skin[4]=hsl2rgb(h, s+8, clampd(l+29,  44,97));
}

static void apply_color(int idx){
  if(g_nColors<=0){                                   /* 极端兜底：没解析出任何颜色 */
    for(int i=0;i<5;i++) g_skin[i]=hsl2rgb(215,HUE_S,HUE_L);
    return;
  }
  if(idx<0) idx=0; if(idx>=g_nColors) idx=g_nColors-1;
  g_cfg.colorIdx=idx;
  wcsncpy(g_cfg.color, g_colors[idx].name, NAME_LEN-1);
  g_cfg.color[NAME_LEN-1]=0;
  if(g_colors[idx].mode==1){
    for(int i=0;i<5;i++) g_skin[i]=g_colors[idx].layers[i];
  }else{
    skin_from_hue(g_colors[idx].h, g_colors[idx].s, g_colors[idx].l);
  }
}

static const double BODY_SIZE=24.0, BASE_W=5.0, EYE_SCALE=.53, EYE_ASPECT=2.24,
  EYE_GAP=2.1, EYE_Y=-3.0, EYE_ROUND=1.0,
  EYE_MOVE=3.4, EYE_MOVEY=.62, GAZE_RANGE=.60, SPEED=.45, BLINK=.87,
  PRESS_POP=.11, PRESS_HOLD=.03, POP_GROW_MS=230.0, POP_HOLD_MS=250.0,
  POP_GROW_STIFF=100.0, JELLY_K=.05, JELLY_MAX=.07,
  SHADOW_SCALE=1.0, SHADOW_DX=.10, SHADOW_DY=.14, SHADOW_ALPHA=.30,
  ANGRY_LID=.43, LID_SLANT=1.1, JITTER=.72, TURN_K=1.8, ROLL=1.6;

#define KP_VERSION_MAJOR 1
#define KP_VERSION_MINOR 2
#define KP_VERSION_PATCH 0
#define KP_VERSION_STR   "v1.2"
#define KP_VERSION_W    L"v1.2"
#define KP_REPO_OWNER    "Minecraftgoose"
#define KP_REPO_NAME     "kimi-pet"
#define KP_REPO_OWNER_W  L"Minecraftgoose"
#define KP_REPO_NAME_W   L"kimi-pet"

static const double RAGE_TAPS=5, RAGE_WINDOW=900.0, RAGE_COOL=2800.0;

/* 睡觉：上下眼睑各合拢这么多，中间剩下 1-2*SLEEP_CLOSE 当作缝 */
static const double SLEEP_CLOSE=.44, SLEEP_TAP_WINDOW=1200.0;
static const int    SLEEP_WAKE_TAPS=3;
/* 睡着后鼠标要累计移动这么多像素才算「动了」，免得手抖一下就把它吵醒 */
static const double SLEEP_WAKE_MOVE=14.0;

static double eyeW(void){ return BASE_W*EYE_SCALE; }
static double eyeH(void){ return eyeW()*EYE_ASPECT; }

/* ---------------- 配置文件 KimiPet.ini ---------------- */
static wchar_t g_cfgPath[1024];

static const char *DEFAULT_CFG_TEXT =
"# KimiPet 配置文件\r\n"
"#\r\n"
"#   改完保存，在球上点右键 →「重新加载配置」即可生效。\r\n"
"#   把本文件删掉，下次启动会按出厂设置重新生成一份。\r\n"
"#   行首写 # 或 ; 就是注释；其余行一律写成「键 = 值」。\r\n"
"\r\n"
"[general]\r\n"
"# 窗口始终置顶：1 开 / 0 关\r\n"
"topmost = 1\r\n"
"# 眼神跟随鼠标：1 开 / 0 关\r\n"
"follow = 0\r\n"
"# 开机自启动：1 开 / 0 关\r\n"
"autorun = 0\r\n"
"# 球当前直径（px）\r\n"
"size = 60\r\n"
"# 直径允许范围\r\n"
"size_min = 60\r\n"
"size_max = 420\r\n"
"# 当前配色，名字要和 [colors] 里某一行对得上\r\n"
"color = 淡蓝\r\n"
"\r\n"
"# 允许睡觉：1 允许 / 0 永远不睡\r\n"
"sleep = 1\r\n"
"# 鼠标静止多少秒后闭上眼睡觉，秒为单位\r\n"
"sleep_delay = 180\r\n"
"# 移动鼠标时：1 = 立刻醒（默认）/ 0 = 鼠标动没用，只能点球 3 下叫醒\r\n"
"sleep_wake_on_move = 1\r\n"
"\r\n"
"[colors]\r\n"
"# 想加颜色就自己加一行，右键菜单的「颜色」里会自动多一项（最多 32 个）。三种写法：\r\n"
"#   名字 = 色相,饱和度,亮度          HSL，例：红 = 0,70,55\r\n"
"#   名字 = #RRGGBB                  只给基色，程序自动推 5 层明暗\r\n"
"#   名字 = #RRGGBB,#RRGGBB,...      5 层渐变完全自己定，从暗到亮\r\n"
"淡蓝 = #4a86d8,#5a95e0,#6ca4e8,#8cb9f0,#a9cdf6\r\n"
"天蓝 = 201,65,57\r\n"
"靛蓝 = 230,65,57\r\n"
"紫 = 266,65,57\r\n"
"青 = 186,65,57\r\n"
"绿 = 146,65,57\r\n"
"粉 = 337,65,57\r\n"
"橙 = 36,65,57\r\n"
"\r\n"
"[mods]\r\n"
"# 模组开关：名字要和 exe 同级 mod\\ 文件夹里某个模组的 name 对得上\r\n"
"# 写成 1 就戴上，0 就摘掉。右键菜单「模组」里也能直接勾选。\r\n"
"# 兔耳 = 1\r\n";

/* ---- 文本小工具 ---- */
static int cieq(const char *a,const char *b){
  while(*a && *b){
    int ca=*a, cb=*b;
    if(ca>='A'&&ca<='Z') ca+=32;
    if(cb>='A'&&cb<='Z') cb+=32;
    if(ca!=cb) return 0;
    a++; b++;
  }
  return *a==*b;
}
static char *ltrim(char *s){
  while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++; return s;
}
static char *rtrim(char *s){
  size_t n=strlen(s);
  while(n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
  return s;
}
static char *trim(char *s){ rtrim(s); return ltrim(s); }
static int is_blank_or_comment(const char *s){
  s=ltrim((char*)s);
  return *s==0 || *s=='#' || *s==';';
}
static wchar_t *wltrim(wchar_t *s){
  while(*s==L' '||*s==L'\t'||*s==L'\r'||*s==L'\n') s++; return s;
}
static void wrtrim(wchar_t *s){
  size_t n=wcslen(s);
  while(n && (s[n-1]==L' '||s[n-1]==L'\t'||s[n-1]==L'\r'||s[n-1]==L'\n')) s[--n]=0;
}

static int parse_bool(const char *v){
  if(!v || !*v) return 0;
  if(cieq(v,"0")||cieq(v,"false")||cieq(v,"off")||cieq(v,"no")) return 0;
  if(atoi(v)!=0) return 1;
  return (cieq(v,"true")||cieq(v,"on")||cieq(v,"yes")||strcmp(v,"开")==0);
}

static int hexv(char c){
  if(c>='0'&&c<='9') return c-'0';
  if(c>='a'&&c<='f') return c-'a'+10;
  if(c>='A'&&c<='F') return c-'A'+10;
  return -1;
}
static int parse_hex_rgb(const char *p, RGB8 *out){
  if(!p || p[0]!='#') return 0;
  const char *q=p+1; size_t n=strlen(q);
  unsigned char r,g,b;
  if(n==6){
    int v[6]; for(int i=0;i<6;i++){ v[i]=hexv(q[i]); if(v[i]<0) return 0; }
    r=(unsigned char)(v[0]*16+v[1]); g=(unsigned char)(v[2]*16+v[3]); b=(unsigned char)(v[4]*16+v[5]);
  }else if(n==3){
    int v[3]; for(int i=0;i<3;i++){ v[i]=hexv(q[i]); if(v[i]<0) return 0; }
    r=(unsigned char)(v[0]*17); g=(unsigned char)(v[1]*17); b=(unsigned char)(v[2]*17);
  }else return 0;
  out->r=r; out->g=g; out->b=b; return 1;
}

static void rgb2hsl(RGB8 c, double *H,double *S,double *L){
  double r=c.r/255.0, g=c.g/255.0, b=c.b/255.0;
  double mx = r>g ? (r>b?r:b) : (g>b?g:b);
  double mn = r<g ? (r<b?r:b) : (g<b?g:b);
  double l=(mx+mn)/2.0, d=mx-mn, h=0.0, s=0.0;
  if(d>1e-9){
    s = l>0.5 ? d/(2.0-mx-mn) : d/((mx+mn)>1e-9?(mx+mn):1e-9);
    if(mx==r)      h=60.0*fmod((g-b)/d,6.0);
    else if(mx==g) h=60.0*((b-r)/d+2.0);
    else           h=60.0*((r-g)/d+4.0);
    if(h<0) h+=360.0;
  }
  *H=h; *S=s*100.0; *L=l*100.0;
}

/* 值 → 颜色：HSL 三元组 / 单个基色 / 5 层显式渐变 */
static int parse_color_value(char *v, CFG_COLOR *c){
  char *seg[8]; int n=0;
  char *p=v;
  for(;;){
    if(n<8) seg[n]=p;
    char *comma=strchr(p,',');
    if(!comma){ n++; break; }
    *comma=0; n++;
    if(n>=8) break;
    p=comma+1;
  }
  for(int i=0;i<n;i++) seg[i]=trim(seg[i]);
  if(seg[0][0]=='#'){
    if(n>=5){
      RGB8 tmp[5];
      for(int i=0;i<5;i++) if(!parse_hex_rgb(seg[i],&tmp[i])) return 0;
      c->mode=1; memcpy(c->layers,tmp,sizeof tmp); return 1;
    }
    RGB8 base; if(!parse_hex_rgb(seg[0],&base)) return 0;
    rgb2hsl(base,&c->h,&c->s,&c->l);
    c->mode=0; return 1;
  }
  char *endp=NULL;
  double h=strtod(seg[0],&endp);
  if(endp==seg[0]) return 0;              /* 压根不是数字，这行作废 */
  c->mode=0;
  c->h = h;
  c->s = (n>1)?atof(seg[1]):HUE_S;
  c->l = (n>2)?atof(seg[2]):HUE_L;
  return 1;
}

static int find_color(const wchar_t *name){
  for(int i=0;i<g_nColors;i++) if(wcscmp(g_colors[i].name,name)==0) return i;
  return -1;
}
static void add_color(const wchar_t *name, const CFG_COLOR *c){
  int idx=find_color(name);
  if(idx<0){
    if(g_nColors>=MAX_COLORS) return;
    idx=g_nColors++;
  }
  wcsncpy(g_colors[idx].name,name,NAME_LEN-1); g_colors[idx].name[NAME_LEN-1]=0;
  g_colors[idx].mode=c->mode;
  g_colors[idx].h=c->h; g_colors[idx].s=c->s; g_colors[idx].l=c->l;
  memcpy(g_colors[idx].layers,c->layers,sizeof c->layers);
}

static void cfg_reset(void){
  memset(&g_cfg,0,sizeof g_cfg);
  memset(g_colors,0,sizeof g_colors);
  g_nColors=0;
  g_cfg.topmost=1; g_cfg.follow=0; g_cfg.autorun=0;
  g_cfg.sleep=1; g_cfg.sleep_delay=180.0;      /* 3 分钟不动就睡 */
  g_cfg.sleep_wake_on_move=1;
  g_cfg.size=60.0; g_cfg.size_min=60.0; g_cfg.size_max=420.0;
}

static void cfg_parse(const char *text);
static void modsw_set(const wchar_t *n,int on);

static int g_normGuard=0;
static void cfg_normalize(void){
  if(g_nColors<=0){
    /* 一条颜色都没有就补出厂配色，但保留用户在 [general] 里已经写好的值 */
    if(!g_normGuard){
      g_normGuard=1;
      CFG keep=g_cfg;
      cfg_parse(DEFAULT_CFG_TEXT);
      CFG_COLOR defc[MAX_COLORS]; int defn=g_nColors;
      memcpy(defc,g_colors,sizeof defc);
      g_cfg=keep;
      memcpy(g_colors,defc,sizeof defc);
      g_nColors=defn;
      g_normGuard=0;
    }
  }
  if(g_cfg.size_min<8.0)  g_cfg.size_min=8.0;
  if(g_cfg.size_max<=g_cfg.size_min) g_cfg.size_max=g_cfg.size_min+1.0;
  if(g_cfg.size_max>4000.0) g_cfg.size_max=4000.0;
  /* 无效值（<=0 / 不是数字）回到默认的 180 秒；其余一律尊重用户的设置，
     哪怕只写 2 秒 —— 想快速看效果就写小点。 */
  if(!(g_cfg.sleep_delay>0.0)) g_cfg.sleep_delay=180.0;
  if(g_cfg.sleep_delay<0.5)    g_cfg.sleep_delay=0.5;
  if(g_cfg.sleep_delay>86400.0) g_cfg.sleep_delay=86400.0;
  if(!(g_cfg.size>0.0))     g_cfg.size=60.0;
  g_cfg.size=clampd(g_cfg.size,g_cfg.size_min,g_cfg.size_max);
  if(g_nColors>0){
    int idx=find_color(g_cfg.color);
    if(idx<0) idx=0;                          /* 名字对不上就退回第一个 */
    g_cfg.colorIdx=idx;
    wcsncpy(g_cfg.color,g_colors[idx].name,NAME_LEN-1);
    g_cfg.color[NAME_LEN-1]=0;
  }
}

static void cfg_parse(const char *text){
  if(!text) return;
  cfg_reset();
  size_t n=strlen(text);
  char *buf=(char*)malloc(n+1);
  if(!buf) return;
  memcpy(buf,text,n+1);

  char sec[64]; sec[0]=0;
  char *p=buf;
  while(*p){
    char *eol=strchr(p,'\n');
    if(eol) *eol=0;
    char *line=p;
    p = eol ? eol+1 : p+strlen(p);
    if(is_blank_or_comment(line)) continue;
    char *t=trim(line);
    if(t[0]=='['){
      char *rb=strchr(t,']'); if(rb) *rb=0;
      char *nm=trim(t+1);
      strncpy(sec,nm,sizeof(sec)-1); sec[sizeof(sec)-1]=0;
      continue;
    }
    char *eq=strchr(t,'=');
    if(!eq) continue;
    *eq=0;
    char *key=rtrim(t);
    char *val=ltrim(eq+1);
    char *cm=strstr(val," #"), *cs=strstr(val," ;");
    char *cc = cm ? (cs ? (cm<cs?cm:cs) : cm) : cs;
    if(cc) *cc=0;
    rtrim(val);
    if(!*key) continue;

    if(cieq(sec,"mods")){
      wchar_t nm[NAME_LEN];
      if(u8_to_w(key,nm,NAME_LEN)>0) modsw_set(nm,parse_bool(val));
      continue;
    }
    if(cieq(sec,"colors")){
      CFG_COLOR c; memset(&c,0,sizeof c);
      if(parse_color_value(val,&c)){
        wchar_t nm[NAME_LEN];
        if(u8_to_w(key,nm,NAME_LEN)>0) add_color(nm,&c);
      }
      continue;
    }
    if(!cieq(sec,"general")) continue;

    if(cieq(key,"topmost"))           g_cfg.topmost = parse_bool(val);
    else if(cieq(key,"follow"))       g_cfg.follow  = parse_bool(val);
    else if(cieq(key,"autorun"))      g_cfg.autorun = parse_bool(val);
    else if(cieq(key,"sleep"))        g_cfg.sleep   = parse_bool(val);
    else if(cieq(key,"sleep_delay")||cieq(key,"sleep_after")||cieq(key,"sleep_sec"))
                                      g_cfg.sleep_delay = atof(val);
    else if(cieq(key,"sleep_wake_on_move")||cieq(key,"wake_on_move"))
                                      g_cfg.sleep_wake_on_move = parse_bool(val);
    else if(cieq(key,"size"))         g_cfg.size      = atof(val);
    else if(cieq(key,"size_min"))     g_cfg.size_min  = atof(val);
    else if(cieq(key,"size_max"))     g_cfg.size_max  = atof(val);
    else if(cieq(key,"color"))        u8_to_w(val,g_cfg.color,NAME_LEN);
  }
  free(buf);
  cfg_normalize();
}

/* 把某个键的值原地写回配置文件：保留注释、空行和文件里已有的顺序 */
typedef struct { size_t secStart, secEnd, T; int secFound; } CFG_SEC;

/* 读回配置文件的全部文本（UTF-8 → wchar_t），读不到就退回出厂文本 */
static wchar_t *cfg_read_text(void){
  size_t len=0;
  unsigned char *raw=read_file_all(g_cfgPath,&len);
  wchar_t *text=NULL;
  if(raw){
    size_t skip=(len>=3 && raw[0]==0xEF && raw[1]==0xBB && raw[2]==0xBF)?3:0;
    text=(wchar_t*)malloc((len-skip+1)*sizeof(wchar_t));
    if(text) u8_to_w((const char*)raw+skip,text,(int)(len-skip+1));
    free(raw);
  }
  if(!text){                                   /* 文件没了/读不了 → 按出厂文本重建 */
    size_t dn=strlen(DEFAULT_CFG_TEXT);
    text=(wchar_t*)malloc((dn+2)*sizeof(wchar_t));
    if(!text) return NULL;
    u8_to_w(DEFAULT_CFG_TEXT,text,(int)dn+1);
  }
  return text;
}

/* 把 wchar_t 文本写回文件（带 BOM 的 UTF-8） */
static int cfg_write_text(const wchar_t *text){
  size_t bsz = wcslen(text)*4 + 16;
  unsigned char *bytes=(unsigned char*)malloc(bsz);
  if(!bytes) return 0;
  size_t oi=0;
  bytes[oi++]=0xEF; bytes[oi++]=0xBB; bytes[oi++]=0xBF;
  int k=w_to_u8(text,(char*)bytes+oi,(int)(bsz-oi));
  size_t total = oi + (k>0?(size_t)k:0);
  int ok = write_file_all(g_cfgPath,bytes,total);
  free(bytes);
  return ok;
}

static void cfg_locate_sec(const wchar_t *text,const wchar_t *sec,CFG_SEC *r){
  size_t T=wcslen(text);
  r->T=T; r->secStart=0; r->secEnd=T; r->secFound=0;
  size_t p=0;
  while(p<T){
    size_t le=p; while(le<T && text[le]!=L'\n') le++;
    size_t s=p; while(s<le && (text[s]==L' '||text[s]==L'\t'||text[s]==L'\r')) s++;
    if(s<le && text[s]==L'['){
      size_t e=s+1; while(e<le && text[e]!=L']') e++;
      size_t ns=e-s-1; if(ns>63) ns=63;
      wchar_t nbuf[64]; wcsncpy(nbuf,text+s+1,ns); nbuf[ns]=0;
      wchar_t *nm=wltrim(nbuf); wrtrim(nm);
      if(r->secFound){ r->secEnd=p; break; }
      if(wcscmp(nm,sec)==0){ r->secFound=1; r->secStart=(le<T)?le+1:le; }
    }
    if(le>=T) break;
    p=le+1;
  }
}

/* 在 section 内找 key；找到返回 1 并给出值的区间 */
static int cfg_locate_key(const wchar_t *text,const CFG_SEC *r,const wchar_t *key,
                          size_t *vs,size_t *ve,size_t *lineEnd){
  if(!r->secFound) return 0;
  size_t T=r->T, p=r->secStart;
  while(p<r->secEnd){
    size_t le=p; while(le<T && text[le]!=L'\n') le++;
    size_t s=p; while(s<le && (text[s]==L' '||text[s]==L'\t'||text[s]==L'\r')) s++;
    if(!(s<le && (text[s]==L'#'||text[s]==L';'))){
      size_t q=s; while(q<le && text[q]!=L'=') q++;
      if(q<le){
        size_t kn=q-s; if(kn>255) kn=255;
        wchar_t kbuf[256]; wcsncpy(kbuf,text+s,kn); kbuf[kn]=0;
        wchar_t *k=wltrim(kbuf); wrtrim(k);
        if(wcscmp(k,key)==0){
          size_t v0=q+1;
          while(v0<le && (text[v0]==L' '||text[v0]==L'\t')) v0++;
          size_t raw=le;
          if(raw>v0 && text[raw-1]==L'\r') raw--;
          size_t v1=v0;
          while(v1<raw && text[v1]!=L' ' && text[v1]!=L'\t') v1++;
          *vs=v0; *ve=v1; *lineEnd=le;
          return 1;
        }
      }
    }
    if(le>=T) break;
    p=le+1;
  }
  return 0;
}

static void cfg_set(const wchar_t *sec,const wchar_t *key,const wchar_t *val);

/* 出厂键清单：老配置文件升级到新版时，缺哪个就补哪个（带注释），
   已有的键一律不动 —— 用户自己改过的值不会被覆盖。 */
typedef struct { const wchar_t *sec,*key,*val,*cmt; } CFG_FIELD;

static const CFG_FIELD CFG_FIELDS[] = {
  {L"general",L"topmost",L"1",L"窗口始终置顶：1 开 / 0 关"},
  {L"general",L"follow", L"0",L"眼神跟随鼠标：1 开 / 0 关"},
  {L"general",L"autorun",L"0",L"开机自启动：1 开 / 0 关"},
  {L"general",L"size",L"60",L"球当前直径（px）"},
  {L"general",L"size_min",L"60",L"直径允许范围"},
  {L"general",L"size_max",L"420",NULL},
  {L"general",L"color",L"淡蓝",L"当前配色，名字要和 [colors] 里某一行对得上"},
  {L"general",L"sleep",  L"1",L"允许睡觉：1 允许 / 0 永远不睡"},
  {L"general",L"sleep_delay",L"180",
   L"鼠标静止多少秒后闭上眼睡觉，秒为单位"},
  {L"general",L"sleep_wake_on_move",L"1",
   L"移动鼠标时：1 = 立刻醒（默认）/ 0 = 鼠标动没用，只能点球 3 下叫醒"},
};
#define N_CFG_FIELDS (int)(sizeof(CFG_FIELDS)/sizeof(CFG_FIELDS[0]))

static int cfg_has_key(const wchar_t *text,const wchar_t *sec,const wchar_t *key){
  CFG_SEC r; cfg_locate_sec(text,sec,&r);
  size_t a,b,c;
  return cfg_locate_key(text,&r,key,&a,&b,&c);
}

/* 往配置文件里补一批「注释 + 键 = 值」；section 不存在就顺手建一个。
   一次性成块写，所以补进来的键是连在一起的，不会互相夹空行。 */
static int cfg_insert_fields(const wchar_t *sec,const CFG_FIELD *fs,int n){
  if(n<=0) return 0;
  wchar_t *text=cfg_read_text();
  if(!text) return 0;
  size_t T=wcslen(text);
  const wchar_t *NL = (wcschr(text,L'\r')!=NULL) ? L"\r\n" : L"\n";
  CFG_SEC r; cfg_locate_sec(text,sec,&r);

  /* 先拼好要插入的那一块 */
  wchar_t *blk=(wchar_t*)malloc((size_t)(n*900+64)*sizeof(wchar_t));
  if(!blk){ free(text); return 0; }
  blk[0]=0;
  for(int i=0;i<n;i++){
    if(fs[i].cmt){ wcscat(blk,L"# "); wcscat(blk,fs[i].cmt); wcscat(blk,NL); }
    wcscat(blk,fs[i].key); wcscat(blk,L" = "); wcscat(blk,fs[i].val); wcscat(blk,NL);
  }

  size_t extra=wcslen(blk)+wcslen(sec)+64;
  wchar_t *out=(wchar_t*)malloc((T+extra)*sizeof(wchar_t));
  if(!out){ free(blk); free(text); return 0; }

  if(r.secFound){
    wcsncpy(out,text,r.secEnd); out[r.secEnd]=0;
    if(r.secEnd>0 && text[r.secEnd-1]!=L'\n') wcscat(out,NL);
    /* 前面已经有空行就不用再空一格 */
    size_t L=wcslen(out);
    if(!(L>=2 && out[L-1]==L'\n' && out[L-2]==L'\n')) wcscat(out,NL);
    wcscat(out,blk);
    if(r.secEnd<r.T && text[r.secEnd]==L'[') wcscat(out,NL);  /* 下一个段前留空行 */
    wcscat(out,text+r.secEnd);
  }else{
    wcscpy(out,text);
    if(T>0 && text[T-1]!=L'\n') wcscat(out,NL);
    wcscat(out,NL);
    wcscat(out,L"["); wcscat(out,sec); wcscat(out,L"]"); wcscat(out,NL);
    wcscat(out,blk);
  }
  free(blk); free(text);
  int ok=cfg_write_text(out);
  free(out);
  return ok;
}

/* 补齐缺失的出厂键；返回补了几个 */
static int cfg_upgrade(void){
  wchar_t *text=cfg_read_text();
  if(!text) return 0;
  CFG_FIELD miss[32];
  int nm=0;
  for(int i=0;i<N_CFG_FIELDS;i++){
    if(!cfg_has_key(text,CFG_FIELDS[i].sec,CFG_FIELDS[i].key)){
      if(nm<32) miss[nm++]=CFG_FIELDS[i];
    }
  }
  free(text);
  if(nm>0) cfg_insert_fields(L"general",miss,nm);

  /* [mods] 段整个缺的话，补一个带说明的示例（默认关闭，不会真戴上） */
  text=cfg_read_text();
  if(text){
    CFG_SEC r; cfg_locate_sec(text,L"mods",&r);
    free(text);
    if(!r.secFound){
      CFG_FIELD m={L"mods",L"兔耳",L"0",
        L"模组开关：名字要和 exe 同级 mod\\ 文件夹里某个模组的 name 对得上。"
        L"写成 1 就戴上，0 就摘掉。右键菜单「模组」里也能直接勾选。"};
      cfg_insert_fields(L"mods",&m,1);
    }
  }
  return nm;
}

static void cfg_set(const wchar_t *sec,const wchar_t *key,const wchar_t *val){
  wchar_t *text=cfg_read_text();
  if(!text) return;
  size_t T=wcslen(text);
  const wchar_t *NL = (wcschr(text,L'\r')!=NULL) ? L"\r\n" : L"\n";

  CFG_SEC r;
  cfg_locate_sec(text,sec,&r);

  size_t valStart=0, valEnd=0, keyLineEnd=T;
  int keyFound=cfg_locate_key(text,&r,key,&valStart,&valEnd,&keyLineEnd);

  size_t need = T + wcslen(key) + wcslen(val) + 128;
  wchar_t *out=(wchar_t*)malloc(need*sizeof(wchar_t));
  if(!out){ free(text); return; }

  if(keyFound){
    wcsncpy(out,text,valStart); out[valStart]=0;
    wcscat(out,val);
    wcsncat(out,text+valEnd,keyLineEnd-valEnd);
    wcscat(out,text+keyLineEnd);
  }else if(r.secFound){
    wchar_t lbuf[600];
    wcsncpy(out,text,r.secEnd); out[r.secEnd]=0;
    if(r.secEnd>0 && text[r.secEnd-1]!=L'\n') wcscat(out,NL);
    _snwprintf(lbuf,600,L"%ls = %ls%ls",key,val,NL);
    wcscat(out,lbuf);
    wcscat(out,text+r.secEnd);
  }else{
    wchar_t lbuf[600];
    wcscpy(out,text);
    if(T>0 && text[T-1]!=L'\n') wcscat(out,NL);
    _snwprintf(lbuf,600,L"%ls[%ls]%ls%ls = %ls%ls",NL,sec,NL,key,val,NL);
    wcscat(out,lbuf);
  }
  free(text);

  int ok = cfg_write_text(out);
  free(out);
  if(!ok){
#ifndef RENDER_TEST
    static int warned=0;
    if(!warned){
      warned=1;
      MessageBoxW(NULL,
        L"配置文件没能写进去（磁盘只读或没有权限）。\n"
        L"这次的修改只在本次运行有效，重启后会回到上次保存的状态。",
        L"KimiPet", MB_OK|MB_ICONWARNING);
    }
#endif
  }
}

/* ---------------- 状态 ---------------- */
static double S=6.0, ES=6.0;
static int    g_win=WINW;
static double g_D=144.0;
static double g_scale=1.0;
static void   set_ball_size(double D);
static void   reload_cfg(void);
static double cx0=WINW/2.0, cy0=WINH/2.0;
static double gazeX=0,gazeY=0,gazeVX=0,gazeVY=0,gazeTX=0,gazeTY=0,gazeNext=0,gazeSeed=0;
static double blinkT0=-1e9, blinkDur=170, blinkNext=0;
static double press=0, pressV=0, sq=0;
static int    holding=0, onBall=0;
static double popUntil=-1e9;
static double combo=0, lastTap=-1e9, rageUntil=-1e9, rage=0;
static int    angryMood=0;
static double sharedRot=0;

/* ---- 睡觉 ---- */
static double g_lastMove=0;      /* 最后一次鼠标活动的时刻 */
static double g_refX=0, g_refY=0;    /* 累计移动的参照点 */
static int    g_mouseSeen=0;
static double g_sleepAmt=0;      /* 0 清醒 ~ 1 熟睡，平滑过渡 */
static int    g_sleepTaps=0;     /* 睡着后被点的次数 */
static double g_sleepTap=-1e9;
static double g_breath=0;        /* 睡着时的呼吸起伏 */

static int g_x=0, g_y=0;
static int g_follow=0;
static int g_topmost=1;
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

  /* 睡觉：鼠标静够了就慢慢合眼，醒了就快点睁开 */
  double sleepTarget = (g_cfg.sleep && (now - g_lastMove) > g_cfg.sleep_delay*1000.0) ? 1.0 : 0.0;
  double sleepTau = sleepTarget>g_sleepAmt ? 0.45 : 0.20;   /* 入睡慢，醒来快 */
  g_sleepAmt += (sleepTarget-g_sleepAmt)*(1.0-exp(-ds/sleepTau));
  if(g_sleepAmt<1e-4) g_sleepAmt=0;
  if(g_sleepAmt>0.9999) g_sleepAmt=1;
  /* 呼吸：只在睡着时有，幅度很小，纯装饰 */
  g_breath = g_sleepAmt * sin(now/1000.0*(2.0*M_PI/4.0)) * 0.012;

  double target = (now<rageUntil)?1.0:0.0;
  double tau = target?0.10:0.05;
  rage += (target-rage)*(1.0-exp(-ds/tau));
  if(now < lastTap-RAGE_WINDOW) combo=0;
  angryMood = rage>0.30 ? 1 : 0;

  /* 注视方向 */
  double R = BODY_SIZE/2.0;
  double bodyRpx = R*(1-press)*S;
  if(g_sleepAmt>0.6){
    gazeTX=0; gazeTY=.12;                      /* 睡着了就别乱看 */
    /* 只推 100ms，别推到天荒地老 —— 醒来后这个值一过期就会重新到处看 */
    gazeNext = now + 100;
  }else if(holding && g_mouseValid){
    double wx = (double)g_x + cx0, wy = (double)g_y + cy0;
    double dx = (double)g_mouse.x - wx, dy = (double)g_mouse.y - wy;
    double d = hypot(dx,dy); if(d<1) d=1;
    double sat = bodyRpx*0.8; if(sat<1) sat=1;
    double n = d/(d+sat);
    gazeTX = dx/d*n; gazeTY = dy/d*n;
    gazeNext = now + 1e9;
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
    gazeTX = rnd(-1,1)*.85; gazeTY = rnd(-1,1)*.7;
    gazeNext = now + rnd(1400,3400);
  }

  double st = 30.0+SPEED*340.0, dp = 2.0*sqrt(st)*0.62, t = now/1000.0;
  double mx=(sin(t*1.7+gazeSeed)*.6+sin(t*4.3+gazeSeed*2.7)*.3)*.08*JITTER;
  double my=(cos(t*2.3+gazeSeed*1.3)*.6+cos(t*5.1+gazeSeed*3.1)*.3)*.06*JITTER;
  gazeVX += ((gazeTX+mx-gazeX)*st - gazeVX*dp)*ds;
  gazeVY += ((gazeTY+my-gazeY)*st - gazeVY*dp)*ds;
  gazeX  += gazeVX*ds; gazeY += gazeVY*ds;

  if(g_sleepAmt<0.3 && now>blinkNext){
    if(now-blinkT0>blinkDur){ blinkT0=now; blinkDur=rnd(150,230); }
    blinkNext = now + 1000.0/(0.05+BLINK*0.6)*rnd(.55,1.6);
  }
}

/* 鼠标动了：累计位移够 SLEEP_WAKE_MOVE 才算一次「活动」，抖动不算 */
static void note_mouse_move(int x,int y){
  double now=now_ms();
  if(!g_mouseSeen){ g_mouseSeen=1; g_refX=x; g_refY=y; g_lastMove=now; return; }
  if(!g_cfg.sleep_wake_on_move){
    g_refX=x; g_refY=y;                          /* 记住位置，但别把它吵醒 */
    return;
  }
  if(hypot((double)x-g_refX,(double)y-g_refY) >= SLEEP_WAKE_MOVE){
    g_refX=x; g_refY=y;
    g_lastMove=now;
  }
}

/* 睡着时被点一下；够 3 下就醒，返回 1 表示这次叫醒了 */
static int sleep_tap(double now){
  if(g_sleepAmt<=0.5) return 0;
  g_sleepTaps = (now-g_sleepTap<SLEEP_TAP_WINDOW)? g_sleepTaps+1 : 1;
  g_sleepTap  = now;
  if(g_sleepTaps>=SLEEP_WAKE_TAPS){
    g_sleepTaps=0; g_sleepTap=-1e9;
    g_lastMove=now;                    /* 醒过来，计时重新开始 */
    gazeNext  = now + 200;
    return 1;
  }
  return 0;
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
  double sl = (angryMood && rage>0.01) ? LID_SLANT*(1.0-al*0.35) : 0.0;
  /* 睡着：上眼睑压下来、下眼睑抬上去，中间留一条缝 */
  double sleepLid = g_sleepAmt*SLEEP_CLOSE;
  double lm = lid>al?lid:al;
  if(sleepLid>lm){ lm=sleepLid; sl=0.0; }
  double cut = -ry + eh*lm;
  double cutBot = ry - eh*sleepLid;             /* 下眼睑 */
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
      double lx=( dx*cs + dy*sn)/ES;
      double ly=(-dx*sn + dy*cs)/ES;
      double pa_y=ly+flat;
      double hh=clampd(pa_y/(2.0*flat),0,1);
      double dcap=hypot(lx, ly-(-flat+2.0*flat*hh))-r;
      double dline = -((lx-axL)*nx + (ly-ayL)*ny);
      double d = (dcap>dline)?dcap:dline;
      if(sleepLid>0.001){                       /* 下眼睑：线以下的切掉 */
        double db = ly - cutBot;
        if(db>d) d=db;
      }
      double cov = clampd(0.5 - d*ES, 0, 1);
      if(cov<=0) continue;
      buf_blend(x,y,255,255,255,cov);
    }
  }
}

/* ================= 锚点 =================
   本体空间：球心 (0,0)，半径 12（BODY_SIZE/2），y 向下为正。
   屏幕坐标 = (cx0 + x*ES, cy0 + y*ES)。mod 里的所有长度都用这套单位，
   所以装饰会跟着球一起缩放、鼓起、挤压。                                */
enum { A_BODY=0, A_EYE_L, A_EYE_R, A_EYES, A_MOUTH, A_HEAD_TOP,
       A_EAR_L, A_EAR_R, A_FOOT_L, A_FOOT_R, ANCH_N };

static const char *ANCH_NAMES[ANCH_N]={
  "body","eye_l","eye_r","eyes","mouth","head","ear_l","ear_r","foot_l","foot_r"
};
/* 几个顺手的别名 */
static const char *ANCH_ALIAS[][2]={
  {"center","body"},{"centre","body"},{"core","body"},
  {"eye","eyes"},{"eyes_center","eyes"},{"eye_left","eye_l"},{"eye_right","eye_r"},
  {"head_top","head"},{"top","head"},
  {"ear","ear_l"},{"ear_left","ear_l"},{"ear_right","ear_r"},
  {"foot","foot_l"},{"foot_left","foot_l"},{"foot_right","foot_r"},
  {"mouse","mouth"}
};

static const double ANCH_MOUTH_Y= 4.6,
                    ANCH_HEAD_Y =-11.7,
                    ANCH_EAR_X  = 7.4,  ANCH_EAR_Y = -8.3,
                    ANCH_FOOT_X = 5.2,  ANCH_FOOT_Y= 11.3;

static double g_anchX[ANCH_N], g_anchY[ANCH_N], g_anchRot[ANCH_N];

static int find_anchor(const char *n){
  for(int i=0;i<ANCH_N;i++) if(cieq(n,ANCH_NAMES[i])) return i;
  for(size_t i=0;i<sizeof(ANCH_ALIAS)/sizeof(ANCH_ALIAS[0]);i++)
    if(cieq(n,ANCH_ALIAS[i][0])){
      for(int j=0;j<ANCH_N;j++) if(cieq(ANCH_ALIAS[i][1],ANCH_NAMES[j])) return j;
    }
  return -1;
}

/* 眼睛中心（本体空间），跟 draw_eye 里的算法完全一致 */
static void eye_center(int side,double *px,double *py){
  double ew=eyeW();
  double bx=EYE_GAP+ew/2.0;
  double R=BODY_SIZE/2.0;
  double MA=EYE_MOVE*GAZE_RANGE/R*TURN_K;
  double ax=(side==0?-bx:bx)/R + gazeX*MA;
  double ay=EYE_Y/R + gazeY*MA*EYE_MOVEY;
  *px=sin(ax)*cos(ay)*R;
  *py=sin(ay)*R;
}

/* 固定在球面上的锚点：跟着果冻挤压 + 整体 roll */
static void anch_on_body(int id,double bx,double by){
  double sx=bx*(1.0+sq*0.5), sy=by*(1.0-sq*0.5);
  double cs=cos(sharedRot), sn=sin(sharedRot);
  g_anchX[id]=sx*cs - sy*sn;
  g_anchY[id]=sx*sn + sy*cs;
  g_anchRot[id]=sharedRot;
}
static void anch_at(int id,double bx,double by,double rot){
  g_anchX[id]=bx; g_anchY[id]=by; g_anchRot[id]=rot;
}

static void update_anchors(void){
  double lx,ly,rx,ry;
  eye_center(0,&lx,&ly); eye_center(1,&rx,&ry);
  anch_at(A_EYE_L,lx,ly,sharedRot);
  anch_at(A_EYE_R,rx,ry,sharedRot);
  anch_at(A_EYES,(lx+rx)*0.5,(ly+ry)*0.5,sharedRot);
  anch_at(A_BODY,0,0,sharedRot);
  anch_on_body(A_MOUTH,   0.0, ANCH_MOUTH_Y);
  anch_on_body(A_HEAD_TOP,0.0, ANCH_HEAD_Y);
  anch_on_body(A_EAR_L, -ANCH_EAR_X, ANCH_EAR_Y);
  anch_on_body(A_EAR_R,  ANCH_EAR_X, ANCH_EAR_Y);
  anch_on_body(A_FOOT_L,-ANCH_FOOT_X,ANCH_FOOT_Y);
  anch_on_body(A_FOOT_R, ANCH_FOOT_X,ANCH_FOOT_Y);
}

/* ================= 图元：统一用 SDF + coverage 抗锯齿 ================= */
typedef double (*SDF)(double,double,const double*);

static double sdf_circle(double x,double y,const double*p){ return hypot(x,y)-p[0]; }

static double sdf_ring(double x,double y,const double*p){
  return fabs(hypot(x,y)-p[0]) - p[1]*0.5;
}
static double sdf_ellipse(double x,double y,const double*p){
  /* 精确椭圆 SDF 太贵，这里用足够准的近似 */
  double rx=p[0], ry=p[1];
  if(rx<1e-6||ry<1e-6) return 1e9;
  double k0=hypot(x/rx,y/ry);
  if(k0<1e-9) return -fmin(rx,ry);
  double k1=hypot(x/(rx*rx),y/(ry*ry));
  return (k0-1.0)*k0/(k1>1e-9?k1:1e-9);
}
static double sdf_rect(double x,double y,const double*p){
  double hw=p[0]*0.5-p[2], hh=p[1]*0.5-p[2];
  if(hw<0) hw=0; if(hh<0) hh=0;
  double dx=fabs(x)-hw, dy=fabs(y)-hh;
  double out=hypot(dx>0?dx:0, dy>0?dy:0);
  double inn=fmin(dx>dy?dx:dy, 0.0);
  return out+inn-p[2];
}
static double sdf_line(double x,double y,const double*p){
  /* p[2],p[3] = 终点（局部空间，起点恒为原点）；p[4] = 线宽（圆头） */
  double ex=p[2], ey=p[3];
  double vx=ex, vy=ey;
  double L2=vx*vx+vy*vy;
  double t = L2>1e-12 ? (x*vx+y*vy)/L2 : 0.0;
  t = t<0?0:(t>1?1:t);
  return hypot(x-vx*t, y-vy*t) - p[4]*0.5;
}
static double sdf_arc(double x,double y,const double*p){
  double r=p[0], w=p[1], a0=p[2], a1=p[3];
  double d=hypot(x,y);
  double span=a1-a0;
  double t=atan2(y,x)-a0;
  while(t<0)        t+=2*M_PI;
  while(t>=2*M_PI)  t-=2*M_PI;
  if(t<=span) return fabs(d-r)-w*0.5;          /* 在弧上 */
  double ea = (t-span < 2*M_PI-t) ? a1 : a0;   /* 取更近的那个端点 */
  return hypot(x-r*cos(ea), y-r*sin(ea)) - w*0.5;
}

static void draw_shape(double ox,double oy,double rot,double scl,double bound,
                       RGB8 col,double alpha,SDF f,const double *p){
  if(bound<=0 || alpha<=0) return;
  double cs=cos(rot), sn=sin(rot);
  double R=bound*scl;
  double wx=cx0+ox*ES, wy=cy0+oy*ES, rp=R*ES+2.0;
  int x0=(int)floor(wx-rp), x1=(int)ceil(wx+rp);
  int y0=(int)floor(wy-rp), y1=(int)ceil(wy+rp);
  if(x0<0) x0=0; if(y0<0) y0=0;
  if(x1>g_win) x1=g_win; if(y1>g_win) y1=g_win;
  if(x0>=x1||y0>=y1) return;
  double inv=1.0/(ES*scl);
  for(int y=y0;y<y1;y++){
    double dy=(y+0.5)-wy;
    for(int x=x0;x<x1;x++){
      double dx=(x+0.5)-wx;
      double lx=( dx*cs + dy*sn)*inv;
      double ly=(-dx*sn + dy*cs)*inv;
      double d=f(lx,ly,p)*scl;
      double cov=clampd(0.5-d*ES,0,1)*alpha;
      if(cov<=0) continue;
      buf_blend(x,y,col.r,col.g,col.b,cov);
    }
  }
}

/* ================= 模组 ================= */
#define MAX_MODS   32
#define MAX_PARTS  256

enum { SH_CIRCLE=0, SH_RING, SH_ELLIPSE, SH_RECT, SH_LINE, SH_ARC };

typedef struct {
  int    mod;
  int    layer;      /* 0 = 画在球体之前（耳朵/脚这类该被挡住的），1 = 画在眼睛之后 */
  int    kind;
  /* 语义字段：解析时先存这些，落盘前按 kind 组装成 p[]，避免 w/h/corner 抢槽位 */
  double rad, thick, sw, sh, rx, ry, a0deg, a1deg, corner;
  int    hasThick;
  double p[6];       /* 组装后的最终参数，各 shape 各用各的，互不干扰 */
  RGB8   col;
  double alpha;
  int    a0, a1;     /* 起点 / 终点锚点，a1 = -1 表示终点用相对偏移 */
  double dx, dy;     /* 起点偏移 */
  double tx, ty;     /* 终点偏移（a1<0 时是相对起点的偏移） */
  double rot, scl;
} ModPart;

/* 按形状把语义字段打包成 p[]：
     circle  p0=r        ring    p0=r p1=线宽
     ellipse p0=rx p1=ry rect    p0=宽 p1=高 p2=圆角
     arc     p0=r p1=线宽 p2=起始角 p3=结束角（弧度）
     line    p4=线宽（p2/p3 每帧填端点）                                   */
static void part_reset(ModPart *P){
  memset(P,0,sizeof *P);
  P->a0=A_BODY; P->a1=-1;
  P->scl=1.0; P->alpha=1.0;
  P->layer=1; P->kind=SH_CIRCLE;
  P->rad=1.0; P->thick=0.5; P->sw=1.0; P->sh=1.0;
  P->rx=1.0;  P->ry=1.0;  P->corner=0.0;
  P->col.r=P->col.g=P->col.b=0;
}

static void part_build(ModPart *P){
  memset(P->p,0,sizeof P->p);
  switch(P->kind){
  case SH_CIRCLE:  P->p[0]=P->rad; break;
  case SH_RING:    P->p[0]=P->rad; P->p[1]=P->thick; break;
  case SH_ELLIPSE: P->p[0]=P->rx;  P->p[1]=P->ry;   break;
  case SH_RECT:    P->p[0]=P->sw;  P->p[1]=P->sh;   P->p[2]=P->corner; break;
  case SH_ARC:     P->p[0]=P->rad; P->p[1]=P->thick;
                   P->p[2]=P->a0deg*M_PI/180.0; P->p[3]=P->a1deg*M_PI/180.0; break;
  default:         P->p[4]=P->thick; break;      /* SH_LINE */
  }
}

typedef struct { wchar_t name[NAME_LEN]; int enabled; } Mod;

static Mod      g_mods[MAX_MODS];
static int      g_nMods=0;
static ModPart  g_parts[MAX_PARTS];
static int      g_nParts=0;

/* 主配置 [mods] 段里的开关（按名字覆盖 mod 文件自带的 enabled） */
typedef struct { wchar_t name[NAME_LEN]; int on; } ModSw;
static ModSw g_modSw[MAX_MODS];
static int   g_nModSw=0;

static void modsw_set(const wchar_t *n,int on){
  for(int i=0;i<g_nModSw;i++) if(wcscmp(g_modSw[i].name,n)==0){ g_modSw[i].on=on; return; }
  if(g_nModSw>=MAX_MODS) return;
  wcsncpy(g_modSw[g_nModSw].name,n,NAME_LEN-1);
  g_modSw[g_nModSw].name[NAME_LEN-1]=0;
  g_modSw[g_nModSw].on=on;
  g_nModSw++;
}
static int modsw_get(const wchar_t *n,int *found){
  for(int i=0;i<g_nModSw;i++) if(wcscmp(g_modSw[i].name,n)==0){ if(found)*found=1; return g_modSw[i].on; }
  if(found)*found=0;
  return 0;
}

static int find_shape(const char *s){
  if(cieq(s,"circle")||cieq(s,"dot"))      return SH_CIRCLE;
  if(cieq(s,"ring")||cieq(s,"circle_o"))   return SH_RING;
  if(cieq(s,"ellipse")||cieq(s,"oval"))    return SH_ELLIPSE;
  if(cieq(s,"rect")||cieq(s,"box"))        return SH_RECT;
  if(cieq(s,"line")||cieq(s,"capsule"))    return SH_LINE;
  if(cieq(s,"arc"))                        return SH_ARC;
  return -1;
}

/* 解析一个 mod 文件的文本；parts 追加到全局池，返回新建的 mod 索引（失败 -1） */
static int mod_parse_text(const char *text,const wchar_t *fallbackName){
  if(!text) return -1;
  if(g_nMods>=MAX_MODS) return -1;

  size_t n=strlen(text);
  char *buf=(char*)malloc(n+1); if(!buf) return -1;
  memcpy(buf,text,n+1);

  int mi=g_nMods++;
  memset(&g_mods[mi],0,sizeof g_mods[mi]);
  wcsncpy(g_mods[mi].name,fallbackName,NAME_LEN-1);
  g_mods[mi].name[NAME_LEN-1]=0;
  g_mods[mi].enabled=0;

  char sec[64]; sec[0]=0;
  int  inPart=0;
  ModPart P;
  part_reset(&P);
  int hasPart=0;

  char *p=buf;
  while(*p){
    char *eol=strchr(p,'\n');
    if(eol) *eol=0;
    char *line=p;
    p = eol ? eol+1 : p+strlen(p);
    if(is_blank_or_comment(line)) continue;
    char *t=trim(line);
    if(t[0]=='['){
      char *rb=strchr(t,']'); if(rb) *rb=0;
      char *nm=trim(t+1);
      strncpy(sec,nm,sizeof(sec)-1); sec[sizeof(sec)-1]=0;
      if(cieq(sec,"part")){
        if(hasPart && g_nParts<MAX_PARTS){ part_build(&P); P.mod=mi; g_parts[g_nParts++]=P; }
        part_reset(&P);
        inPart=1; hasPart=1;
      }else inPart=0;
      continue;
    }
    char *eq=strchr(t,'=');
    if(!eq) continue;
    *eq=0;
    char *key=rtrim(t);
    char *val=ltrim(eq+1);
    char *cm=strstr(val," #"), *cs2=strstr(val," ;");
    char *cc = cm ? (cs2 ? (cm<cs2?cm:cs2) : cm) : cs2;
    if(cc) *cc=0;
    rtrim(val);
    if(!*key) continue;

    if(cieq(sec,"mod") && !inPart){
      if(cieq(key,"name"))        u8_to_w(val,g_mods[mi].name,NAME_LEN);
      else if(cieq(key,"enabled")||cieq(key,"on")) g_mods[mi].enabled=parse_bool(val);
      continue;
    }
    if(!cieq(sec,"part") || !inPart) continue;

    if(cieq(key,"shape")){
      int k=find_shape(val); if(k>=0) P.kind=k;
    }else if(cieq(key,"at")||cieq(key,"anchor")){
      int a=find_anchor(val); if(a>=0) P.a0=a;
    }else if(cieq(key,"to")){
      int a=find_anchor(val); if(a>=0) P.a1=a;
    }else if(cieq(key,"dx")||cieq(key,"x"))        P.dx=atof(val);
    else if(cieq(key,"dy")||cieq(key,"y"))        P.dy=atof(val);
    else if(cieq(key,"dx2")||cieq(key,"to_x"))    P.tx=atof(val);
    else if(cieq(key,"dy2")||cieq(key,"to_y"))    P.ty=atof(val);
    else if(cieq(key,"rot")||cieq(key,"rotate"))  P.rot=atof(val)*M_PI/180.0;
    else if(cieq(key,"scale"))                    P.scl=atof(val)>1e-6?atof(val):1.0;
    else if(cieq(key,"alpha")||cieq(key,"opacity")) P.alpha=clampd(atof(val),0,1);
    else if(cieq(key,"layer")){
      if(cieq(val,"back")||cieq(val,"behind")||cieq(val,"0")) P.layer=0;
      else P.layer=1;
    }
    else if(cieq(key,"color")||cieq(key,"colour")){
      RGB8 c;
      if(parse_hex_rgb(val,&c)) P.col=c;
      else{
        /* 也认 "r,g,b" */
        char *q=val; char *c1=strchr(q,',');
        if(c1){
          *c1=0; char *c2=strchr(c1+1,',');
          int rr=atoi(q), gg=atoi(c1+1), bb=c2?atoi(c2+1):0;
          P.col.r=(unsigned char)clampi(rr,0,255);
          P.col.g=(unsigned char)clampi(gg,0,255);
          P.col.b=(unsigned char)clampi(bb,0,255);
        }
      }
    }
    else if(cieq(key,"r")||cieq(key,"radius"))    P.rad =atof(val);
    else if(cieq(key,"rx"))                       P.rx  =atof(val);
    else if(cieq(key,"ry"))                       P.ry  =atof(val);
    else if(cieq(key,"w")||cieq(key,"width")){
      P.sw=atof(val);
      if(!P.hasThick) P.thick=atof(val);          /* w 也能当线宽用 */
    }
    else if(cieq(key,"thick")||cieq(key,"linewidth")){ P.thick=atof(val); P.hasThick=1; }
    else if(cieq(key,"h")||cieq(key,"height"))    P.sh  =atof(val);
    else if(cieq(key,"a0")||cieq(key,"from"))     P.a0deg=atof(val);
    else if(cieq(key,"a1")||cieq(key,"to_angle")||cieq(key,"angle")) P.a1deg=atof(val);
    else if(cieq(key,"corner")||cieq(key,"round"))P.corner=atof(val);
  }
  if(hasPart && g_nParts<MAX_PARTS){
    part_build(&P);
    P.mod=mi; g_parts[g_nParts++]=P;
  }
  free(buf);

  int found=0;
  int sw=modsw_get(g_mods[mi].name,&found);
  if(found) g_mods[mi].enabled=sw;
  return mi;
}

static void mods_clear(void){ g_nMods=0; g_nParts=0; }

/* ---------------- 出厂自带的几个示例模组 ----------------
   首次运行会写进 exe 同级的 mod\ 目录，全都默认关闭，
   想戴就把对应 ini 里的 enabled 改成 1，或在右键菜单里勾上。 */
typedef struct { const char *file; const char *text; } BuiltinMod;

static const BuiltinMod BUILTIN_MODS[] = {
{ "兔耳.ini",
"[mod]\n"
"name = 兔耳\n"
"enabled = 0\n"
"\n"
"; layer = back 表示画在球体之前，耳朵根会被球挡住，像是从头后面长出来的\n"
"[part]\n"
"at = ear_l\n"
"shape = ellipse\n"
"rx = 1.9\n"
"ry = 5.2\n"
"rot = -13\n"
"dy = -4.2\n"
"color = #fdf3f6\n"
"layer = back\n"
"\n"
"[part]\n"
"at = ear_r\n"
"shape = ellipse\n"
"rx = 1.9\n"
"ry = 5.2\n"
"rot = 13\n"
"dy = -4.2\n"
"color = #fdf3f6\n"
"layer = back\n"
"\n"
"; 内耳\n"
"[part]\n"
"at = ear_l\n"
"shape = ellipse\n"
"rx = .85\n"
"ry = 3.0\n"
"rot = -13\n"
"dy = -4.0\n"
"color = #f7b8c8\n"
"layer = back\n"
"\n"
"[part]\n"
"at = ear_r\n"
"shape = ellipse\n"
"rx = .85\n"
"ry = 3.0\n"
"rot = 13\n"
"dy = -4.0\n"
"color = #f7b8c8\n"
"layer = back\n"
},
{ "锚点调试.ini",
"[mod]\n"
"name = 锚点调试\n"
"enabled = 0\n"
"\n"
"; 在每个锚点上点一个红点，写自己的模组时先开这个确认锚点在哪\n"
"[part]\n"
"at = body\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
"[part]\n"
"at = eye_l\n"
"shape = ring\n"
"r = .9\n"
"thick = .3\n"
"color = #ff0000\n"
"[part]\n"
"at = eye_r\n"
"shape = ring\n"
"r = .9\n"
"thick = .3\n"
"color = #ff0000\n"
"[part]\n"
"at = eyes\n"
"shape = circle\n"
"r = .4\n"
"color = #ff8800\n"
"[part]\n"
"at = mouth\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
"[part]\n"
"at = head\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
"[part]\n"
"at = ear_l\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
"[part]\n"
"at = ear_r\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
"[part]\n"
"at = foot_l\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
"[part]\n"
"at = foot_r\n"
"shape = circle\n"
"r = .5\n"
"color = #ff0000\n"
}
};
#define N_BUILTIN_MODS (int)(sizeof(BUILTIN_MODS)/sizeof(BUILTIN_MODS[0]))

/* 每个 part 在局部空间的包围半径，用来算屏幕包围盒 */
static double part_bound(ModPart *P,double *pp){
  switch(P->kind){
  case SH_CIRCLE:  return P->p[0]+1.0;
  case SH_RING:    return P->p[0]+P->p[1]*0.5+1.0;
  case SH_ELLIPSE: return (P->p[0]>P->p[1]?P->p[0]:P->p[1])+1.0;
  case SH_RECT:    return hypot(P->p[0],P->p[1])*0.5+1.0;
  case SH_ARC:     return P->p[0]+P->p[1]*0.5+1.0;
  case SH_LINE:    return hypot(pp[2],pp[3])+P->p[4]*0.5+1.0;
  }
  return 4.0;
}

static void draw_mods(int layer){
  for(int i=0;i<g_nParts;i++){
    ModPart *P=&g_parts[i];
    if(P->layer!=layer) continue;
    if(P->mod<0||P->mod>=g_nMods||!g_mods[P->mod].enabled) continue;

    double rot=g_anchRot[P->a0]+P->rot;
    double cs=cos(rot), sn=sin(rot), scl=P->scl;
    /* 起点：锚点 + 偏移（偏移按当前朝向转） */
    double ox=g_anchX[P->a0] + (P->dx*cs - P->dy*sn);
    double oy=g_anchY[P->a0] + (P->dx*sn + P->dy*cs);

    double pp[6];
    memcpy(pp,P->p,sizeof pp);
    if(P->kind==SH_LINE){
      double ex,ey;
      if(P->a1>=0){
        ex=g_anchX[P->a1] + (P->tx*cs - P->ty*sn);
        ey=g_anchY[P->a1] + (P->tx*sn + P->ty*cs);
      }else{
        ex=ox + (P->tx*cs - P->ty*sn);
        ey=oy + (P->tx*sn + P->ty*cs);
      }
      /* 终点换算到局部空间（原点 = ox,oy） */
      double ddx=(ex-ox), ddy=(ey-oy);
      pp[2]=( ddx*cs + ddy*sn)/scl;
      pp[3]=(-ddx*sn + ddy*cs)/scl;
      pp[4]=P->p[4];
    }

    double bound=part_bound(P,pp);
    SDF f;
    switch(P->kind){
    case SH_CIRCLE:  f=sdf_circle;  break;
    case SH_RING:    f=sdf_ring;    break;
    case SH_ELLIPSE: f=sdf_ellipse; break;
    case SH_RECT:    f=sdf_rect;    break;
    case SH_ARC:     f=sdf_arc;     break;
    default:         f=sdf_line;    break;
    }
    draw_shape(ox,oy,rot,scl,bound,P->col,P->alpha,f,pp);
  }
}

static void render(double now){
  buf_clear();
  double br = 1.0 + g_breath;
  double R = BODY_SIZE/2.0*(1.0-press)*S*br;
  cx0=g_win/2.0; cy0=g_win/2.0;
  ES = S*(1.0-press)*br;
  double RX=R*(1.0+sq*0.5), RY=R*(1.0-sq*0.5);
  draw_shadow(RX,RY);
  updateSharedRot();
  update_anchors();
  draw_mods(0);                    /* back：耳朵、脚之类被球挡住的 */
  draw_body(RX,RY);
  double lid=eyeLid(now);
  draw_eye(0,lid);
  draw_eye(1,lid);
  draw_mods(1);                    /* front：盖在球体之上的 */
}

/* ================= 检查更新 =================
   查 GitHub 上本仓库的最新 Release；比当前版本新的话，下载对应架构的 exe，
   写个 bat 等本进程退出后覆盖并重启自己。                                    */

/* 目标 exe 文件名：64 位 / 32 位各一份 */
static const char *upd_asset_name(void){
#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
  return "KimiPet_x64.exe";
#else
  return "KimiPet_x86.exe";
#endif
}

/* 把 "v1.10.2" / "1.10" / "v2" 之类拆成数字；返回 0 表示解析不出来 */
static int parse_ver(const char *t,int *maj,int *min,int *pat){
  while(*t==' '||*t=='v'||*t=='V'||*t=='\t') t++;
  if(*t<'0'||*t>'9') return 0;
  int v[3]={0,0,0}, n=0;
  for(int i=0;i<3;i++){
    if(*t<'0'||*t>'9') break;
    int x=0;
    while(*t>='0'&&*t<='9'){ x=x*10+(*t-'0'); if(x>99999){x=99999;} t++; }
    v[i]=x; n=i+1;
    if(*t!='.') break;
    t++;
  }
  *maj=v[0]; *min=v[1]; *pat=v[2];
  return n>0;
}

static int ver_cmp(int a0,int a1,int a2,int b0,int b1,int b2){
  if(a0!=b0) return a0<b0?-1:1;
  if(a1!=b1) return a1<b1?-1:1;
  if(a2!=b2) return a2<b2?-1:1;
  return 0;
}

/* 在 JSON 里找 "key":"value"，value 拷进 out；找不到返回 0 */
static int json_str(const char *json,const char *key,char *out,int outn){
  char pat[128];
  int k=_snprintf(pat,sizeof pat,"\"%s\":\"",key);
  if(k<=0||k>=(int)sizeof pat) return 0;
  const char *p=strstr(json,pat);
  if(!p) return 0;
  p+=k;
  int i=0;
  while(*p && *p!='"' && i<outn-1){
    if(*p=='\\' && p[1]){ p++; }              /* 简单跳过转义 */
    out[i++]=*p++;
  }
  out[i]=0;
  return i>0;
}

/* 在 assets 数组里定位某个文件名的下载直链 */
static int find_asset_url(const char *json,const char *want,char *out,int outn){
  const char *p=json;
  while((p=strstr(p,"\"name\":"))!=NULL){
    p+=7;
    while(*p==' ') p++;
    if(*p!='"'){ continue; }
    p++;
    const char *ns=p;
    while(*p && *p!='"') p++;
    int nl=(int)(p-ns);
    if(nl>0 && (int)strlen(want)==nl && strncmp(ns,want,nl)==0){
      /* 从这个 asset 对象往后找 browser_download_url */
      if(json_str(p,"browser_download_url",out,outn)) return 1;
    }
    if(!*p) break;
  }
  return 0;
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


/* 用 WinINet 抓一个 URL 的全部内容；返回 malloc 出来的 buffer（末尾补 0），失败返回 NULL */
static char *http_get(const char *url,int *plen){
  *plen=0;
  HINTERNET hInet=InternetOpenW(L"KimiPet/" KP_VERSION_W,
                                INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
  if(!hInet) return NULL;
  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0){
    InternetCloseHandle(hInet); return NULL;
  }
  HINTERNET hUrl=InternetOpenUrlW(hInet,wurl,NULL,0,
      INTERNET_FLAG_SECURE|INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE|
      INTERNET_FLAG_NO_UI|INTERNET_FLAG_PRAGMA_NOCACHE,0);
  if(!hUrl){ InternetCloseHandle(hInet); return NULL; }

  size_t cap=65536, len=0;
  char *buf=(char*)malloc(cap);
  if(!buf){ InternetCloseHandle(hUrl); InternetCloseHandle(hInet); return NULL; }
  for(;;){
    if(len+4096+1>cap){
      size_t nc=cap*2;
      char *nb=(char*)realloc(buf,nc);
      if(!nb) break;
      buf=nb; cap=nc;
    }
    DWORD got=0;
    if(!InternetReadFile(hUrl,buf+len,(DWORD)(cap-len-1),&got)) break;
    if(got==0) break;
    len+=got;
  }
  buf[len]=0;
  InternetCloseHandle(hUrl);
  InternetCloseHandle(hInet);
  *plen=(int)len;
  return buf;
}

/* 下载到文件；成功返回 1 */
static int http_download(const char *url,const wchar_t *dst){
  int len=0;
  char *data=http_get(url,&len);
  if(!data||len<=0){ free(data); return 0; }
  /* 下下来的是不是 PE 文件：看 MZ 头，防止拿到 HTML 错误页就当 exe 用 */
  if(len<64 || (unsigned char)data[0]!='M' || (unsigned char)data[1]!='Z'){
    free(data); return 0;
  }
  int ok=write_file_all(dst,(const unsigned char*)data,(size_t)len);
  free(data);
  return ok;
}

/* 写一个 bat：等本进程结束 → 用新 exe 覆盖 → 启动 → 自删 */
static int write_updater_bat(const wchar_t *bat,const wchar_t *src,const wchar_t *dst){
  DWORD pid=GetCurrentProcessId();
  wchar_t tmp[4096];
  _snwprintf(tmp,4096,
    L"@echo off\r\n"
    L"chcp 65001 >nul\r\n"
    L":wait\r\n"
    L"tasklist /FI \"PID eq %lu\" 2>nul | find \"%lu\" >nul\r\n"
    L"if not errorlevel 1 (\r\n"
    L"  ping -n 2 127.0.0.1 >nul\r\n"
    L"  goto wait\r\n"
    L")\r\n"
    L"copy /y \"%ls\" \"%ls\" >nul\r\n"
    L"if errorlevel 1 (\r\n"
    L"  ping -n 3 127.0.0.1 >nul\r\n"
    L"  copy /y \"%ls\" \"%ls\" >nul\r\n"
    L")\r\n"
    L"del /f /q \"%ls\" >nul 2>&1\r\n"
    L"start \"\" \"%ls\"\r\n"
    L"del \"%%~f0\"\r\n",
    pid,pid,src,dst,src,dst,src,dst);
  int k=WideCharToMultiByte(CP_UTF8,0,tmp,-1,NULL,0,NULL,NULL);
  if(k<=0) return 0;
  unsigned char *b=(unsigned char*)malloc((size_t)k);
  if(!b) return 0;
  WideCharToMultiByte(CP_UTF8,0,tmp,-1,(char*)b,k,NULL,NULL);
  int ok=write_file_all(bat,b,(size_t)k-1);
  free(b);
  return ok;
}

static void do_update(HWND h){
  char api[512];
  _snprintf(api,sizeof api,"https://api.github.com/repos/%s/%s/releases/latest",
            KP_REPO_OWNER,KP_REPO_NAME);

  int jlen=0;
  char *json=http_get(api,&jlen);
  if(!json){
    MessageBoxW(h,
      L"没能连上 GitHub 检查更新。\n"
      L"可能是没网，或者被代理/防火墙挡住了。\n\n"
      L"你也可以手动到 Releases 页面看看：\n"
      L"https://github.com/" KP_REPO_OWNER_W L"/" KP_REPO_NAME_W L"/releases",
      L"检查更新", MB_OK|MB_ICONWARNING);
    return;
  }

  char tag[64]={0}, url[1024]={0};
  int haveTag=json_str(json,"tag_name",tag,sizeof tag);
  int haveUrl=find_asset_url(json,upd_asset_name(),url,sizeof url);
  free(json);

  if(!haveTag){
    MessageBoxW(h,L"没能读到最新版本号（GitHub 返回的内容看不懂）。",L"检查更新",
                MB_OK|MB_ICONWARNING);
    return;
  }

  int nmaj=0,nmin=0,npat=0;
  if(!parse_ver(tag,&nmaj,&nmin,&npat)){
    MessageBoxW(h,L"最新版本号看不懂，跳过更新。",L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }
  if(ver_cmp(nmaj,nmin,npat,KP_VERSION_MAJOR,KP_VERSION_MINOR,KP_VERSION_PATCH)<=0){
    wchar_t msg[300];
    _snwprintf(msg,300,L"已经是最新版了。\n\n当前版本：%hs\n最新版本：%hs",
               KP_VERSION_STR,tag);
    MessageBoxW(h,msg,L"检查更新",MB_OK|MB_ICONINFORMATION);
    return;
  }
  if(!haveUrl){
    MessageBoxW(h,
      L"找到新版本，但 Release 里没有本程序对应的安装包（可能是发布时漏传了）。\n"
      L"请到 Releases 页面手动下载。",
      L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }

  /* 新版本：问一句再动手，因为要重启 */
  wchar_t msg[400];
  _snwprintf(msg,400,
    L"发现新版本 %hs（当前 %hs）。\n\n要现在更新吗？\n"
    L"更新会自动下载、覆盖旧文件并重启本程序。\n"
    L"你的配置文件和模组都不会动。",
    tag,KP_VERSION_STR);
  if(MessageBoxW(h,msg,L"检查更新",MB_YESNO|MB_ICONINFORMATION)!=IDYES) return;

  wchar_t tmpdir[1024], src[1200], dst[1200], bat[1200];
  if(GetTempPathW(1024,tmpdir)==0){ tmpdir[0]=0; }
  wchar_t self[MAX_PATH];
  if(!GetModuleFileNameW(NULL,self,MAX_PATH)){
    MessageBoxW(h,L"取不到本程序的路径，没法更新。",L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }
  _snwprintf(src,1200,L"%lsKimiPet_update_%lu.exe",tmpdir,GetCurrentProcessId());
  _snwprintf(bat,1200,L"%lsKimiPet_update_%lu.bat",tmpdir,GetCurrentProcessId());

  if(!http_download(url,src)){
    MessageBoxW(h,L"下载新版本失败。\n可能是网络中断，或被杀软拦了。",
                L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }
  wcscpy(dst,self);
  if(!write_updater_bat(bat,src,dst)){
    MessageBoxW(h,L"没能写出更新脚本，取消更新。",L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }

  /* 交给 bat 收尾，本程序立刻退出 */
  if((INT_PTR)ShellExecuteW(NULL,L"open",bat,NULL,NULL,SW_SHOW)<=32){
    MessageBoxW(h,L"启动更新脚本失败，取消更新。",L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }
  if(g_menu){ DestroyMenu(g_menu); g_menu=NULL; }
  PostQuitMessage(0);
}

/* 菜单命令 ID */
#define ID_TOPMOST  1001
#define ID_FOLLOW   1002
#define ID_AUTORUN  1003
#define ID_RECENTER 1004
#define ID_ABOUT    1005
#define ID_QUIT     1006
#define ID_RELOAD   1007
#define ID_OPENCFG  1008
#define ID_CHECKUPD 1009
#define ID_COLOR0   1100                  /* +0 ~ MAX_COLORS-1 */
#define ID_MOD0     1200                  /* +0 ~ MAX_MODS-1   */
#define ID_OPENMOD  1240

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
  D = clampd(D, g_cfg.size_min, g_cfg.size_max);
  double oldCx = g_x + g_win/2.0, oldCy = g_y + g_win/2.0;
  int nw = win_px(D);
  g_D = D; g_win = nw;
  S = D*g_scale/BODY_SIZE;                /* 24 * S = D * scale */
  rebuild_buffer(nw);
  int nx=(int)round(oldCx - nw/2.0), ny=(int)round(oldCy - nw/2.0);
  g_x=nx; g_y=ny;
  if(g_hwnd) SetWindowPos(g_hwnd,NULL,nx,ny,nw,nw,SWP_NOZORDER|SWP_NOACTIVATE);
}

/* ---- 写回配置文件 ---- */
static void save_color(void){
  if(g_nColors>0) cfg_set(L"general",L"color",g_colors[g_cfg.colorIdx].name);
}
static void save_flag(const wchar_t *key,int on){
  cfg_set(L"general",key,on?L"1":L"0");
}

/* ---- 读注册表（仅用于旧版本设置迁移 / 自启项） ---- */
static int reg_get_dword(const wchar_t *name, DWORD *out){
  HKEY hk; DWORD d=0, sz=sizeof(d);
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\KimiPet",0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS)
    return 0;
  int ok = (RegQueryValueExW(hk,name,NULL,NULL,(LPBYTE)&d,&sz)==ERROR_SUCCESS);
  RegCloseKey(hk);
  if(ok) *out=d;
  return ok;
}

/* ---- exe 所在目录 ---- */
static int exe_dir(wchar_t *out,int n){
  DWORD k=GetModuleFileNameW(NULL,out,n);
  if(k==0||k>=(DWORD)n) return 0;
  wchar_t *bs=wcsrchr(out,L'\\');
  if(!bs) return 0;
  *(bs+1)=0;
  return 1;
}

/* ---- 模组目录：exe 同目录 mod\ ---- */
static wchar_t g_modDir[1024];

static void mods_write_builtin(void);

static void mod_dir_init(void){
  wchar_t dir[1024];
  if(exe_dir(dir,1024)){
    _snwprintf(g_modDir,1024,L"%lsmod",dir);
    DWORD a=GetFileAttributesW(g_modDir);
    if(a!=INVALID_FILE_ATTRIBUTES && (a&FILE_ATTRIBUTE_DIRECTORY)) return;
    if(CreateDirectoryW(g_modDir,NULL)){ mods_write_builtin(); return; }
  }
  wchar_t app[1024];
  if(GetEnvironmentVariableW(L"APPDATA",app,1024)>0){
    _snwprintf(g_modDir,1024,L"%ls\\KimiPet",app);
    CreateDirectoryW(g_modDir,NULL);
    _snwprintf(g_modDir,1024,L"%ls\\KimiPet\\mod",app);
    if(CreateDirectoryW(g_modDir,NULL)) mods_write_builtin();
    return;
  }
  wcscpy(g_modDir,L"mod");
}

static void mods_write_builtin(void){
  for(int i=0;i<N_BUILTIN_MODS;i++){
    wchar_t path[1024], fname[128];
    if(u8_to_w(BUILTIN_MODS[i].file,fname,128)<=0) continue;
    _snwprintf(path,1024,L"%ls\\%ls",g_modDir,fname);
    write_file_all(path,(const unsigned char*)BUILTIN_MODS[i].text,
                   strlen(BUILTIN_MODS[i].text));
  }
}

static void mods_scan(void){
  mods_clear();
  wchar_t pat[1024];
  _snwprintf(pat,1024,L"%ls\\*.ini",g_modDir);

  WIN32_FIND_DATAW fd;
  HANDLE h=FindFirstFileW(pat,&fd);
  if(h==INVALID_HANDLE_VALUE) return;

  /* 先收齐再按文件名排序，保证菜单顺序稳定 */
  wchar_t names[MAX_MODS][260];
  int n=0;
  do{
    if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    if(n>=MAX_MODS) break;
    wcsncpy(names[n],fd.cFileName,259); names[n][259]=0;
    n++;
  }while(FindNextFileW(h,&fd));
  FindClose(h);

  for(int i=1;i<n;i++)                       /* 插入排序 */
    for(int j=i;j>0 && wcscmp(names[j-1],names[j])>0;j--){
      wchar_t tmp[260]; wcscpy(tmp,names[j-1]);
      wcscpy(names[j-1],names[j]); wcscpy(names[j],tmp);
    }

  for(int i=0;i<n;i++){
    wchar_t path[1024];
    _snwprintf(path,1024,L"%ls\\%ls",g_modDir,names[i]);
    size_t len=0;
    unsigned char *raw=read_file_all(path,&len);
    if(!raw||len==0){ free(raw); continue; }
    size_t skip=(len>=3&&raw[0]==0xEF&&raw[1]==0xBB&&raw[2]==0xBF)?3:0;
    /* 没写 name 时拿文件名当名字 */
    wchar_t fb[260]; wcscpy(fb,names[i]);
    wchar_t *dot=wcsrchr(fb,L'.'); if(dot) *dot=0;
    mod_parse_text((const char*)raw+skip,fb);
    free(raw);
  }
}

/* ---- 配置文件定位：exe 同目录优先，写不进去就退到 %APPDATA% ---- */
static int cfg_writable(const wchar_t *p){
  HANDLE h=CreateFileW(p,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,
                       OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(h==INVALID_HANDLE_VALUE) return 0;
  CloseHandle(h);
  return 1;
}
static void cfg_path_init(void){
  wchar_t dir[1024], path[1024];
  if(exe_dir(dir,1024)){
    _snwprintf(path,1024,L"%lsKimiPet.ini",dir);
    if(cfg_writable(path)){ wcscpy(g_cfgPath,path); return; }
  }
  wchar_t app[1024];
  DWORD an=GetEnvironmentVariableW(L"APPDATA",app,1024);
  if(an>0 && an<1024){
    _snwprintf(path,1024,L"%s\\KimiPet",app);
    CreateDirectoryW(path,NULL);
    _snwprintf(path,1024,L"%s\\KimiPet\\KimiPet.ini",app);
    wcscpy(g_cfgPath,path);
    return;
  }
  wcscpy(g_cfgPath,L"KimiPet.ini");
}

/* 启动时读配置；文件不存在就按出厂设置生成一份 */
static void cfg_load(void){
  int existed=0;
  size_t len=0;
  g_nModSw=0;                              /* 主配置里的模组开关，读之前先清 */
  mod_dir_init();
  mods_scan();
  unsigned char *raw=read_file_all(g_cfgPath,&len);
  if(raw){
    if(len>0){
      size_t skip=(len>=3 && raw[0]==0xEF && raw[1]==0xBB && raw[2]==0xBF)?3:0;
      cfg_parse((const char*)raw+skip);
      existed=1;
    }
    free(raw);
  }
  if(!existed){
    cfg_parse(DEFAULT_CFG_TEXT);
    /* 老版本把大小 / 颜色存在注册表里，迁移一次，别让人丢设置 */
    DWORD d=0;
    if(reg_get_dword(L"BallSize",&d) && d>0)
      g_cfg.size = clampd((double)d/10.0, g_cfg.size_min, g_cfg.size_max);
    if(reg_get_dword(L"Hue",&d) && d<(DWORD)g_nColors){
      wcsncpy(g_cfg.color,g_colors[d].name,NAME_LEN-1);
      g_cfg.color[NAME_LEN-1]=0;
    }
    g_cfg.autorun = get_autorun();
    cfg_normalize();
    write_file_all(g_cfgPath,(const unsigned char*)DEFAULT_CFG_TEXT,strlen(DEFAULT_CFG_TEXT));
    wchar_t v[32];
    _snwprintf(v,32,L"%g",g_cfg.size);
    cfg_set(L"general",L"size",v);
    cfg_set(L"general",L"color",g_colors[g_cfg.colorIdx].name);
    cfg_set(L"general",L"autorun",g_cfg.autorun?L"1":L"0");
  }
  cfg_upgrade();                           /* 老配置缺的键自动补上 */
  g_topmost = g_cfg.topmost?1:0;
  g_follow  = g_cfg.follow ?1:0;
  g_D       = g_cfg.size;
  apply_color(g_cfg.colorIdx);
  set_autorun(g_cfg.autorun);              /* 配置文件说了算 */
}

/* 菜单里「重新加载配置」 */
static void reload_cfg(void){
  size_t len=0;
  unsigned char *raw=read_file_all(g_cfgPath,&len);
  g_nModSw=0;
  if(raw){
    if(len>0){
      size_t skip=(len>=3 && raw[0]==0xEF && raw[1]==0xBB && raw[2]==0xBF)?3:0;
      cfg_parse((const char*)raw+skip);
    }
    free(raw);
  }
  g_topmost = g_cfg.topmost?1:0;
  g_follow  = g_cfg.follow ?1:0;
  if(g_hwnd)
    SetWindowPos(g_hwnd, g_topmost?HWND_TOPMOST:HWND_NOTOPMOST,
                 0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  cfg_upgrade();
  set_autorun(g_cfg.autorun);
  apply_color(g_cfg.colorIdx);
  mods_scan();                             /* mod 文件改了也能热加载 */
  if(!g_cfg.sleep) g_lastMove=now_ms();    /* 设成不许睡 → 立刻醒 */
  set_ball_size(g_cfg.size);
}

static void popup_menu(int x,int y){
  if(g_menu){ DestroyMenu(g_menu); g_menu=NULL; }   /* 每次重建：改了配置菜单就跟着变 */

  HMENU hue=CreatePopupMenu();
  for(int i=0;i<g_nColors;i++)
    AppendMenuW(hue,MF_STRING,ID_COLOR0+i,g_colors[i].name);

  g_menu=CreatePopupMenu();
  AppendMenuW(g_menu,MF_POPUP,(UINT_PTR)hue,L"颜色");
  if(g_nMods>0){
    HMENU mm=CreatePopupMenu();
    for(int i=0;i<g_nMods;i++)
      AppendMenuW(mm,MF_STRING,ID_MOD0+i,g_mods[i].name);
    AppendMenuW(mm,MF_SEPARATOR,0,NULL);
    AppendMenuW(mm,MF_STRING,ID_OPENMOD,L"打开模组文件夹…");
    AppendMenuW(g_menu,MF_POPUP,(UINT_PTR)mm,L"模组");
  }
  AppendMenuW(g_menu,MF_STRING,ID_TOPMOST,L"始终置顶");
  AppendMenuW(g_menu,MF_STRING,ID_FOLLOW,L"跟随鼠标");
  AppendMenuW(g_menu,MF_STRING,ID_AUTORUN,L"开机自启");
  AppendMenuW(g_menu,MF_SEPARATOR,0,NULL);
  AppendMenuW(g_menu,MF_STRING,ID_RECENTER,L"重新居中");
  AppendMenuW(g_menu,MF_SEPARATOR,0,NULL);
  AppendMenuW(g_menu,MF_STRING,ID_RELOAD,L"重新加载配置");
  AppendMenuW(g_menu,MF_STRING,ID_CHECKUPD,L"检查更新…");
  AppendMenuW(g_menu,MF_STRING,ID_OPENCFG,L"打开配置文件…");
  AppendMenuW(g_menu,MF_STRING,ID_ABOUT,L"关于 Kimi 小圆球");
  AppendMenuW(g_menu,MF_STRING,ID_QUIT,L"退出");

  CheckMenuItem(g_menu,ID_TOPMOST,g_topmost    ?MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(g_menu,ID_FOLLOW, g_follow     ?MF_CHECKED:MF_UNCHECKED);
  CheckMenuItem(g_menu,ID_AUTORUN,g_cfg.autorun?MF_CHECKED:MF_UNCHECKED);
  for(int i=0;i<g_nColors;i++)
    CheckMenuItem(g_menu,ID_COLOR0+i,(i==g_cfg.colorIdx)?MF_CHECKED:MF_UNCHECKED);
  for(int i=0;i<g_nMods;i++)
    CheckMenuItem(g_menu,ID_MOD0+i,g_mods[i].enabled?MF_CHECKED:MF_UNCHECKED);
  SetForegroundWindow(g_hwnd);
  TrackPopupMenu(g_menu, TPM_LEFTALIGN|TPM_RIGHTBUTTON|TPM_NOANIMATION, x, y, 0, g_hwnd, NULL);
}

/* ---------------- 窗口过程 ---------------- */
static double ball_hit_r(void){
  return BODY_SIZE/2.0*(1.0-press)*S*1.25;
}

static int g_dragging=0;
static POINT g_dragMouse0, g_dragWin0;
static int   g_dragMoved=0;

static void drag_to(int mx,int my){
  int nx = g_dragWin0.x + (mx - g_dragMouse0.x);
  int ny = g_dragWin0.y + (my - g_dragMouse0.y);
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
    note_mouse_move(s->pt.x,s->pt.y);
    if(g_dragging) drag_to(s->pt.x, s->pt.y);
  }
  if(code==HC_ACTION && w==WM_MOUSEWHEEL){
    MSLLHOOKSTRUCT *s=(MSLLHOOKSTRUCT*)l;
    g_mouse=s->pt; g_mouseValid=1;
    note_mouse_move(s->pt.x,s->pt.y);      /* 滚轮也算活动，能把它叫醒；但不再改大小 */
  }
  return CallNextHookEx(g_hook,code,w,l);
}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
  switch(m){
  case WM_NCHITTEST:{
    if(g_dragging) return HTCLIENT;
    int px=GET_X_LPARAM(l)-g_x, py=GET_Y_LPARAM(l)-g_y;
    double d=hypot((double)px-cx0,(double)py-cy0);
    if(d<=ball_hit_r()) return HTCLIENT;
    return HTTRANSPARENT;
  }
  case WM_LBUTTONDOWN:{
    int mx=GET_X_LPARAM(l)+g_x, my=GET_Y_LPARAM(l)+g_y;
    double d=hypot((double)GET_X_LPARAM(l)-cx0,(double)GET_Y_LPARAM(l)-cy0);
    if(d>ball_hit_r()) return 0;
    g_mouse.x=mx; g_mouse.y=my; g_mouseValid=1;
    /* 睡着时点击不算「活动」：得靠 sleep_tap 凑够 3 下才醒，
       否则点第一下就把计时器刷了，后面两下永远数不上 */
    if(g_sleepAmt<=0.5) g_lastMove=now_ms();
    g_dragging=1; g_dragMoved=0;
    g_dragMouse0.x=mx; g_dragMouse0.y=my;
    g_dragWin0.x=g_x;  g_dragWin0.y=g_y;
    holding=1; onBall=1;
    popUntil = now_ms() + POP_GROW_MS + POP_HOLD_MS;
    SetCapture(h);
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
      if(g_sleepAmt>0.5) sleep_tap(now);      /* 睡着：连点 3 下叫醒 */
      else if(now>=rageUntil){
        combo = (now-lastTap<RAGE_WINDOW)? combo+1 : 1;
        lastTap = now;
        if(combo>=RAGE_TAPS){
          double extra = (combo-RAGE_TAPS>10?10:(combo-RAGE_TAPS))*380.0;
          if(now+RAGE_COOL+extra > rageUntil) rageUntil = now+RAGE_COOL+extra;
        }
      }
    }
    g_dragging=0; holding=0; onBall=0;
    gazeNext = now + 300;                    /* 松手后停一下，再自己随便看 */
    ReleaseCapture();
    return 0;
  }
  case WM_CAPTURECHANGED:
    if(g_dragging){ g_dragging=0; holding=0; onBall=0; gazeNext=now_ms()+300; }
    return 0;
  case WM_RBUTTONUP:
    popup_menu(GET_X_LPARAM(l)+g_x, GET_Y_LPARAM(l)+g_y);
    return 0;
  case WM_COMMAND:{
    int id=LOWORD(w);
    if(id>=ID_COLOR0 && id<ID_COLOR0+MAX_COLORS){
      int i=id-ID_COLOR0;
      if(i<g_nColors){ apply_color(i); save_color(); }
      return 0;
    }
    if(id==ID_OPENMOD){
      ShellExecuteW(NULL,L"explore",g_modDir,NULL,NULL,SW_SHOW);
      return 0;
    }
    if(id>=ID_MOD0 && id<ID_MOD0+MAX_MODS){
      int i=id-ID_MOD0;
      if(i<g_nMods){
        g_mods[i].enabled=!g_mods[i].enabled;
        cfg_set(L"mods",g_mods[i].name,g_mods[i].enabled?L"1":L"0");
      }
      return 0;
    }
    switch(id){
    case ID_TOPMOST:
      g_topmost=!g_topmost;
      g_cfg.topmost=g_topmost;
      SetWindowPos(h, g_topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
      save_flag(L"topmost",g_topmost);
      break;
    case ID_FOLLOW:
      g_follow=!g_follow;
      g_cfg.follow=g_follow;
      save_flag(L"follow",g_follow);
      break;
    case ID_AUTORUN:
      g_cfg.autorun=!g_cfg.autorun;
      set_autorun(g_cfg.autorun);
      save_flag(L"autorun",g_cfg.autorun);
      break;
    case ID_RELOAD:
      reload_cfg();
      break;
    case ID_CHECKUPD:
      do_update(h);
      break;
    case ID_OPENCFG:{
      if((INT_PTR)ShellExecuteW(NULL,L"open",g_cfgPath,NULL,NULL,SW_SHOW)<=32)
        ShellExecuteW(NULL,L"open",L"notepad.exe",g_cfgPath,NULL,SW_SHOW);
      break;
    }
    case ID_RECENTER:{
      RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
      g_x = (wa.left+wa.right)/2 - g_win/2;
      g_y = (wa.top+wa.bottom)/2 - g_win/2;
      SetWindowPos(h,NULL,g_x,g_y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
      break;
    }
    case ID_ABOUT:{
      wchar_t info[1600];
      _snwprintf(info,1600,
        L"kimi小圆球\n"
        L"版本 " KP_VERSION_W L"\n"
        L"\n"
        L"作者：Minecraftgoose\n"
        L"https://github.com/Minecraftgoose/\n"
        L"\n"
        L"仓库：\n"
        L"https://github.com/Minecraftgoose/kimi-pet\n"
        L"\n"
        L"配置文件：\n%ls\n"
        L"\n"
        L"MIT许可证",
        g_cfgPath);
      MessageBoxW(h,info,L"关于", MB_OK|MB_ICONINFORMATION);
      break;
    }
    case ID_QUIT:
      if(g_menu){ DestroyMenu(g_menu); g_menu=NULL; }
      PostQuitMessage(0);
      break;
    }
    return 0;
  }
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
  g_lastMove = now_ms();

  init_dpi();
  cfg_path_init();
  cfg_load();
  g_win = win_px(g_D);
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
    WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|(g_topmost?WS_EX_TOPMOST:0),
    L"KimiPetCls",L"KimiPet",
    WS_POPUP|WS_VISIBLE,
    g_x,g_y,g_win,g_win,
    NULL,NULL,hInst,NULL);
  if(!g_hwnd) return 0;

  HDC scr=GetDC(NULL);
  g_memdc=CreateCompatibleDC(scr);
  ReleaseDC(NULL,scr);
  set_ball_size(g_D);
  if(!g_bits) return 0;

  g_hook=SetWindowsHookExW(WH_MOUSE_LL,MouseProc,GetModuleHandleA(NULL),0);

  SetTimer(g_hwnd,1,16,NULL);

  MSG msg;
  while(GetMessageW(&msg,NULL,0,0)){ TranslateMessage(&msg); DispatchMessageW(&msg); }

  if(g_hook) UnhookWindowsHookEx(g_hook);
  timeEndPeriod(1);
  return (int)msg.wParam;
}
#endif /* !RENDER_TEST */
