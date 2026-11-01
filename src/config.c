/* 配置：KimiPet.ini 的解析、写回、升级 */
#include "pet.h"

CFG        g_cfg;
CFG_COLOR  g_colors[MAX_COLORS];
int        g_nColors=0;

/* ---------------- 配置文件 KimiPet.ini ---------------- */
wchar_t g_cfgPath[1024];

const char *DEFAULT_CFG_TEXT =
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


int parse_color_value(char *v, CFG_COLOR *c){
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

int find_color(const wchar_t *name){
  for(int i=0;i<g_nColors;i++) if(wcscmp(g_colors[i].name,name)==0) return i;
  return -1;
}
void add_color(const wchar_t *name, const CFG_COLOR *c){
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

void cfg_reset(void){
  memset(&g_cfg,0,sizeof g_cfg);
  memset(g_colors,0,sizeof g_colors);
  g_nColors=0;
  g_cfg.topmost=1; g_cfg.follow=0; g_cfg.autorun=0;
  g_cfg.sleep=1; g_cfg.sleep_delay=180.0;      /* 3 分钟不动就睡 */
  g_cfg.sleep_wake_on_move=1;
  g_cfg.size=60.0; g_cfg.size_min=60.0; g_cfg.size_max=420.0;
}

void cfg_parse(const char *text);
void modsw_set(const wchar_t *n,int on);

int g_normGuard=0;
void cfg_normalize(void){
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

void cfg_parse(const char *text){
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

/* 读回配置文件的全部文本（UTF-8 → wchar_t），读不到就退回出厂文本 */
wchar_t *cfg_read_text(void){
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
int cfg_write_text(const wchar_t *text){
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

void cfg_locate_sec(const wchar_t *text,const wchar_t *sec,CFG_SEC *r){
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
int cfg_locate_key(const wchar_t *text,const CFG_SEC *r,const wchar_t *key,
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

void cfg_set(const wchar_t *sec,const wchar_t *key,const wchar_t *val);

/* 出厂键清单：老配置文件升级到新版时，缺哪个就补哪个（带注释），
   已有的键一律不动 —— 用户自己改过的值不会被覆盖。 */

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

int cfg_has_key(const wchar_t *text,const wchar_t *sec,const wchar_t *key){
  CFG_SEC r; cfg_locate_sec(text,sec,&r);
  size_t a,b,c;
  return cfg_locate_key(text,&r,key,&a,&b,&c);
}

/* 往配置文件里补一批「注释 + 键 = 值」；section 不存在就顺手建一个。
   一次性成块写，所以补进来的键是连在一起的，不会互相夹空行。 */
int cfg_insert_fields(const wchar_t *sec,const CFG_FIELD *fs,int n){
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
int cfg_upgrade(void){
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

void cfg_set(const wchar_t *sec,const wchar_t *key,const wchar_t *val){
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

#ifndef RENDER_TEST
void save_color(void){
  if(g_nColors>0) cfg_set(L"general",L"color",g_colors[g_cfg.colorIdx].name);
}
void save_flag(const wchar_t *key,int on){
  cfg_set(L"general",key,on?L"1":L"0");
}

/* ---- 配置文件定位：exe 同目录优先，写不进去就退到 %APPDATA% ---- */
static int cfg_writable(const wchar_t *p){
  HANDLE h=CreateFileW(p,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,
                       OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  if(h==INVALID_HANDLE_VALUE) return 0;
  CloseHandle(h);
  return 1;
}
void cfg_path_init(void){
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
void cfg_load(void){
  int existed=0;
  size_t len=0;
  g_nModSw=0;                              /* 主配置里的模组开关，读之前先清 */
  mod_dir_init();                          /* 保证 mod 目录存在、内置模组写进去 */
  /* 注意：mods_scan() 必须等配置读完再调 —— 它要靠 [mods] 段决定谁开着。
     以前放在这里，此时 g_nModSw 刚被清零，结果启动时模组全是关的，
     只有点一次「重新加载配置」才戴上。 */
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
  mods_scan();                             /* 配置读完了，这时才知道谁该戴上 */
  g_topmost = g_cfg.topmost?1:0;
  g_follow  = g_cfg.follow ?1:0;
  g_D       = g_cfg.size;
  apply_color(g_cfg.colorIdx);
  set_autorun(g_cfg.autorun);              /* 配置文件说了算 */
}

/* 菜单里「重新加载配置」 */
void reload_cfg(void){
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
#endif
