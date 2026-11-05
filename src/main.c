/* 窗口 / 右键菜单 / 鼠标钩子 / 入口 */
#include "pet.h"

#ifndef RENDER_TEST

HHOOK g_hook=NULL;
HWND  g_hwnd=NULL;
HMENU g_menu=NULL;

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

/* ---------------- 菜单 / 自启 ---------------- */
void set_autorun(int on){
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
int get_autorun(void){
  HKEY hk;
  if(RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS) return 0;
  int on = (RegQueryValueExW(hk,L"KimiPet",NULL,NULL,NULL,NULL)==ERROR_SUCCESS);
  RegCloseKey(hk);
  return on;
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

double ball_hit_r(void){
  return BODY_SIZE/2.0*(1.0-press)*S*1.25;
}

static int g_dragging=0;
static POINT g_dragMouse0, g_dragWin0;
static int   g_dragMoved=0;

void drag_to(int mx,int my){
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
  case KP_WM_UPDATE:
    /* 检查更新的工作线程把结果送回来，这里只弹框 / 启动脚本。
       wParam=1 是看门狗的超时提示，消息体是独立的，不碰工作线程的 ctx。 */
    if(w) upd_on_timeout(h,(void*)l);
    else  upd_on_message(h,(void*)l);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(h,m,w,l);
}

static void init_dpi(void){
  /* 直接强转函数指针会触发 -Wcast-function-type，先过一遍 void* 绕开 */
  FARPROC fp=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDPIAware");
  if(fp){ union { FARPROC raw; BOOL (WINAPI *fn)(void); } u; u.raw=fp; u.fn(); }

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
