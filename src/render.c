/* 状态 / 位图缓冲 / 运动 / 绘制 */
#include "pet.h"

/* ---------------- 配色 / 参数 ---------------- */

RGB8 g_skin[5]={
  {0x4a,0x86,0xd8},{0x5a,0x95,0xe0},{0x6c,0xa4,0xe8},{0x8c,0xb9,0xf0},{0xa9,0xcd,0xf6}
};
const double HUE_S=65.0, HUE_L=57.0;   


void skin_from_hue(int h,double s,double l){
  g_skin[0]=hsl2rgb(h, s,   clampd(l,     12,88));
  g_skin[1]=hsl2rgb(h, s+2, clampd(l+7,   18,92));
  g_skin[2]=hsl2rgb(h, s+4, clampd(l+14,  26,94));
  g_skin[3]=hsl2rgb(h, s+6, clampd(l+22,  34,96));
  g_skin[4]=hsl2rgb(h, s+8, clampd(l+29,  44,97));
}

void apply_color(int idx){
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

const double BODY_SIZE=24.0, BASE_W=5.0, EYE_SCALE=.53, EYE_ASPECT=2.24,
  EYE_GAP=2.1, EYE_Y=-3.0, EYE_ROUND=1.0,
  EYE_MOVE=3.4, EYE_MOVEY=.62, GAZE_RANGE=.60, SPEED=.45, BLINK=.87,
  PRESS_POP=.11, PRESS_HOLD=.03, POP_GROW_MS=230.0, POP_HOLD_MS=250.0,
  POP_GROW_STIFF=100.0, JELLY_K=.05, JELLY_MAX=.07,
  SHADOW_SCALE=1.0, SHADOW_DX=.10, SHADOW_DY=.14, SHADOW_ALPHA=.30,
  ANGRY_LID=.43, LID_SLANT=1.1, JITTER=.72, TURN_K=1.8, ROLL=1.6;

const double RAGE_TAPS=5, RAGE_WINDOW=900.0, RAGE_COOL=2800.0;

/* 睡觉：上下眼睑各合拢这么多，中间剩下 1-2*SLEEP_CLOSE 当作缝 */
const double SLEEP_CLOSE=.44, SLEEP_TAP_WINDOW=1200.0;
const int    SLEEP_WAKE_TAPS=3;
/* 睡着后鼠标要累计移动这么多像素才算「动了」，免得手抖一下就把它吵醒 */
const double SLEEP_WAKE_MOVE=14.0;

double eyeW(void){ return BASE_W*EYE_SCALE; }
double eyeH(void){ return eyeW()*EYE_ASPECT; }

/* ---------------- 状态 ---------------- */
double S=6.0, ES=6.0;
int    g_win=WINW;
double g_D=144.0;
double g_scale=1.0;
void   set_ball_size(double D);
void   reload_cfg(void);
double cx0=WINW/2.0, cy0=WINH/2.0;
double gazeX=0,gazeY=0,gazeVX=0,gazeVY=0,gazeTX=0,gazeTY=0,gazeNext=0,gazeSeed=0;
double blinkT0=-1e9, blinkDur=170, blinkNext=0;
double press=0, pressV=0, sq=0;
int    holding=0, onBall=0;
double popUntil=-1e9;
double combo=0, lastTap=-1e9, rageUntil=-1e9, rage=0;
int    angryMood=0;
double sharedRot=0;

/* ---- 睡觉 ---- */
double g_lastMove=0;      /* 最后一次鼠标活动的时刻 */
double g_refX=0, g_refY=0;    /* 累计移动的参照点 */
int    g_mouseSeen=0;
double g_sleepAmt=0;      /* 0 清醒 ~ 1 熟睡，平滑过渡 */
int    g_sleepTaps=0;     /* 睡着后被点的次数 */
double g_sleepTap=-1e9;
double g_breath=0;        /* 睡着时的呼吸起伏 */

int g_x=0, g_y=0;
int g_follow=0;
int g_topmost=1;
POINT g_mouse; int g_mouseValid=0;

/* ---------------- 位图缓冲 ---------------- */
HDC      g_memdc=NULL;
HBITMAP  g_bmp=NULL, g_old=NULL;
unsigned char *g_bits=NULL;       /* BGRA premultiplied, top-down */

void buf_clear(void){ memset(g_bits,0,(size_t)g_win*g_win*4); }

void buf_blend(int x,int y,unsigned char br,unsigned char bg,unsigned char bb,double a){
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

RGB8 grad_color(double t){
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
void step(double now,double dt){
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
void note_mouse_move(int x,int y){
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
int sleep_tap(double now){
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

double eyeLid(double now){
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

void render(double now){
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

#ifndef RENDER_TEST
int win_px(double D){ int n=(int)round(D*1.7*g_scale); return n<120?120:n; }

void rebuild_buffer(int n){
  if(g_bmp){ SelectObject(g_memdc,g_old); DeleteObject(g_bmp); g_bmp=NULL; }
  BITMAPINFO bi; memset(&bi,0,sizeof(bi));
  bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth=n; bi.bmiHeader.biHeight=-n;   /* top-down */
  bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
  g_bmp=CreateDIBSection(g_memdc,&bi,DIB_RGB_COLORS,(void**)&g_bits,NULL,0);
  g_old=(HBITMAP)SelectObject(g_memdc,g_bmp);
}

void set_ball_size(double D){
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

#endif
