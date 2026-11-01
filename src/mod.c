/* 锚点 / 图元 / 模组 */
#include "pet.h"

/* ================= 锚点 =================
   本体空间：球心 (0,0)，半径 12（BODY_SIZE/2），y 向下为正。
   屏幕坐标 = (cx0 + x*ES, cy0 + y*ES)。mod 里的所有长度都用这套单位，
   所以装饰会跟着球一起缩放、鼓起、挤压。                                */

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

double g_anchX[ANCH_N], g_anchY[ANCH_N], g_anchRot[ANCH_N];

int find_anchor(const char *n){
  for(int i=0;i<ANCH_N;i++) if(cieq(n,ANCH_NAMES[i])) return i;
  for(size_t i=0;i<sizeof(ANCH_ALIAS)/sizeof(ANCH_ALIAS[0]);i++)
    if(cieq(n,ANCH_ALIAS[i][0])){
      for(int j=0;j<ANCH_N;j++) if(cieq(ANCH_ALIAS[i][1],ANCH_NAMES[j])) return j;
    }
  return -1;
}

/* 眼睛中心（本体空间），跟 draw_eye 里的算法完全一致 */
void eye_center(int side,double *px,double *py){
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

void update_anchors(void){
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


Mod      g_mods[MAX_MODS];
int      g_nMods=0;
ModPart  g_parts[MAX_PARTS];
int      g_nParts=0;

/* 主配置 [mods] 段里的开关（按名字覆盖 mod 文件自带的 enabled） */
ModSw g_modSw[MAX_MODS];
int   g_nModSw=0;

void modsw_set(const wchar_t *n,int on){
  for(int i=0;i<g_nModSw;i++) if(wcscmp(g_modSw[i].name,n)==0){ g_modSw[i].on=on; return; }
  if(g_nModSw>=MAX_MODS) return;
  wcsncpy(g_modSw[g_nModSw].name,n,NAME_LEN-1);
  g_modSw[g_nModSw].name[NAME_LEN-1]=0;
  g_modSw[g_nModSw].on=on;
  g_nModSw++;
}
int modsw_get(const wchar_t *n,int *found){
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
int mod_parse_text(const char *text,const wchar_t *fallbackName){
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

void mods_clear(void){ g_nMods=0; g_nParts=0; }

/* ---------------- 出厂自带的几个示例模组 ----------------
   首次运行会写进 exe 同级的 mod\ 目录，全都默认关闭，
   想戴就把对应 ini 里的 enabled 改成 1，或在右键菜单里勾上。 */

const BuiltinMod BUILTIN_MODS[] = {
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
const int N_BUILTIN_MODS = (int)(sizeof(BUILTIN_MODS)/sizeof(BUILTIN_MODS[0]));

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

void draw_mods(int layer){
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

#ifndef RENDER_TEST
/* ---- 模组目录：exe 同目录 mod\ ---- */
wchar_t g_modDir[1024];

void mods_write_builtin(void);

void mod_dir_init(void){
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

void mods_write_builtin(void){
  for(int i=0;i<N_BUILTIN_MODS;i++){
    wchar_t path[1024], fname[128];
    if(u8_to_w(BUILTIN_MODS[i].file,fname,128)<=0) continue;
    _snwprintf(path,1024,L"%ls\\%ls",g_modDir,fname);
    write_file_all(path,(const unsigned char*)BUILTIN_MODS[i].text,
                   strlen(BUILTIN_MODS[i].text));
  }
}

void mods_scan(void){
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
#endif
