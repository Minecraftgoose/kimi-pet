/* 检查更新 */
#include "pet.h"

/* ================= 检查更新 =================
   查 GitHub 上本仓库的最新 Release；比当前版本新的话，下载对应架构的 exe，
   写个 bat 等本进程退出后覆盖并重启自己。                                    */

/* 目标 exe 文件名：64 位 / 32 位各一份 */
const char *upd_asset_name(void){
#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
  return "KimiPet_x64.exe";
#else
  return "KimiPet_x86.exe";
#endif
}

/* 把 "v1.10.2" / "1.10" / "v2" 之类拆成数字；返回 0 表示解析不出来 */
int parse_ver(const char *t,int *maj,int *min,int *pat){
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

int ver_cmp(int a0,int a1,int a2,int b0,int b1,int b2){
  if(a0!=b0) return a0<b0?-1:1;
  if(a1!=b1) return a1<b1?-1:1;
  if(a2!=b2) return a2<b2?-1:1;
  return 0;
}

/* 在 JSON 里找 "key":"value"，value 拷进 out；找不到返回 0 */
int json_str(const char *json,const char *key,char *out,int outn){
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

/* 在 assets 数组里定位某个文件名的下载直链。
   注意：每次循环 p 都必须前进 —— 以前遇到 `"name":` 后面不是引号时会
   continue 而不移动指针，直接死循环把 UI 线程卡死（403 响应最易触发）。 */
int find_asset_url(const char *json,const char *want,char *out,int outn){
  const char *p=json;
  while((p=strstr(p,"\"name\":"))!=NULL){
    const char *q=p+7;
    while(*q==' '||*q=='\t'||*q=='\r'||*q=='\n') q++;
    if(*q!='"'){ p=q+1; if(!*p) break; continue; }   /* 一定要前进 */
    const char *ns=q+1;
    const char *e=strchr(ns,'"');
    if(!e) break;
    int nl=(int)(e-ns);
    if(nl>0 && (int)strlen(want)==nl && strncmp(ns,want,nl)==0){
      /* 从这个 asset 对象往后找它自己的 browser_download_url */
      if(json_str(e+1,"browser_download_url",out,outn)) return 1;
    }
    p=e+1;                                             /* 一定要前进 */
  }
  return 0;
}

#ifndef RENDER_TEST
/* ==========================================================================
   Windows 实现

   以前整个检查流程都跑在 WndProc 的 WM_COMMAND 里：WinINet 同步请求直接
   阻塞 UI 线程，球会卡住不动；GitHub 403 时又因为 find_asset_url 的死循环
   彻底失去响应，只能等系统杀进程。

   现在做三层防护：
     1. 所有网络 IO 都在工作线程，主线程只弹对话框，球全程照常动；
     2. WinINet 设超时 + 先查状态码，403/429 立即返回友好提示；
     3. 独立看门狗线程：万一 WinINet 自己不理会超时（某些代理/wine 就是
        这样），20 秒后强制关闭请求句柄把工作线程拽出来，绝不留挂死线程。
   ========================================================================== */

#define UPD_TIMEOUT_MS   8000
#define UPD_HARD_MS      20000       /* 看门狗硬超时 */
#define UPD_MAX_BODY     (8*1024*1024)

/* GitHub API 强制要求 User-Agent，不带会直接 403 */
static const wchar_t *UPD_GH_HEADERS =
  L"Accept: application/vnd.github+json\r\n"
  L"User-Agent: KimiPet/" KP_VERSION_W L"\r\n";

/* 线程 → 主线程的消息阶段 */
enum {
  UPD_LATEST=1,        /* 已是最新 */
  UPD_ERR_NET,         /* 连不上 / 超时 */
  UPD_ERR_HTTP,        /* 403 / 429 / 其它 4xx 5xx */
  UPD_ERR_JSON,        /* 响应里读不到版本号 */
  UPD_ERR_NOASSET,     /* 新版本但缺对应 exe */
  UPD_ASK,             /* 问用户要不要更新（主线程必须回 answer） */
  UPD_ERR_DOWNLOAD,    /* 下载失败 */
  UPD_ERR_BAT,         /* 写覆盖脚本失败 */
  UPD_DO_REPLACE       /* 主线程启动 bat 并退出 */
};

/* 看门狗单独发的超时提示，不依赖 UPD_CTX 的生命周期 */
typedef struct { wchar_t msg[300]; } UPD_TMO_MSG;

typedef struct {
  HWND    h;
  int     stage;
  DWORD   status;
  wchar_t msg[600];
  char    tag[64], url[1024];
  wchar_t src[1200], dst[1200], bat[1200];

  HANDLE  ev;              /* 自动重置：每次 SetEvent 只唤醒一次 Wait */
  HANDLE  watch;           /* 看门狗线程句柄 */
  int     answer;          /* IDYES / IDNO */

  volatile LONG  finished;  /* 工作线程收尾了，看门狗可以退了 */
  volatile LONG  netActive; /* 正在做网络 IO 时才计时 */
  volatile LONG  netStart;  /* GetTickCount 起点 */
  volatile LONG  timedOut;  /* 看门狗已判定超时 */
  volatile PVOID hReq;      /* 当前 WinINet 句柄，供看门狗强制关闭 */
} UPD_CTX;

static volatile LONG g_updBusy=0;

#define EXCHANGE(p,v) InterlockedExchange((volatile LONG*)(p),(LONG)(v))
#define READ(p)       InterlockedCompareExchange((volatile LONG*)(p),0,0)

static void net_begin(UPD_CTX *c){
  EXCHANGE(&c->netStart,(LONG)GetTickCount());
  EXCHANGE(&c->netActive,1);
}
static void net_end(UPD_CTX *c){ EXCHANGE(&c->netActive,0); }

/* --------------------------------------------------------------------------
   http_get_ex：带超时、带状态码、带响应体上限的一次 GET
   reqSlot 存父句柄，看门狗超时时会关掉它来中断阻塞中的读操作。
   -------------------------------------------------------------------------- */
static int http_get_ex(const char *url,const wchar_t *headers,
                       char **out,int *plen,DWORD *status,volatile PVOID *reqSlot){
  *out=NULL; *plen=0; *status=0;

  HINTERNET hInet=InternetOpenW(L"KimiPet/" KP_VERSION_W,
                                INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
  if(!hInet) return 0;
  if(reqSlot) InterlockedExchangePointer(reqSlot,hInet);

  DWORD t=UPD_TIMEOUT_MS;
  InternetSetOption(hInet,INTERNET_OPTION_CONNECT_TIMEOUT,     &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_SEND_TIMEOUT,        &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_RECEIVE_TIMEOUT,     &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_DATA_SEND_TIMEOUT,   &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,&t,sizeof t);

  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0){
    if(reqSlot) InterlockedExchangePointer(reqSlot,NULL);
    InternetCloseHandle(hInet); return 0;
  }
  /* 按协议决定要不要 SECURE —— asset 直链偶尔是 http，别误加标志 */
  DWORD flags = INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE|
                INTERNET_FLAG_NO_UI|INTERNET_FLAG_PRAGMA_NOCACHE|
                INTERNET_FLAG_NO_COOKIES;
  if(strncmp(url,"https://",8)==0) flags |= INTERNET_FLAG_SECURE;
  HINTERNET hUrl=InternetOpenUrlW(hInet,wurl,headers,0,flags,0);
  if(!hUrl){
    if(reqSlot) InterlockedExchangePointer(reqSlot,NULL);
    InternetCloseHandle(hInet); return 0;
  }
  /* 有些 WinINet 实现只对请求句柄认超时，句柄创建后再设一遍 */
  InternetSetOption(hUrl,INTERNET_OPTION_RECEIVE_TIMEOUT,     &t,sizeof t);
  InternetSetOption(hUrl,INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,&t,sizeof t);
  InternetSetOption(hUrl,INTERNET_OPTION_SEND_TIMEOUT,        &t,sizeof t);

  /* 先读状态码：403/429 这类限流直接返回，不去解析错误页 */
  DWORD code=0, sz=sizeof code;
  if(HttpQueryInfoW(hUrl,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,
                    &code,&sz,NULL)) *status=code;
  if(code>=400){
    InternetCloseHandle(hUrl);
    if(reqSlot) InterlockedExchangePointer(reqSlot,NULL);
    InternetCloseHandle(hInet);
    return 0;
  }

  size_t cap=16384, len=0;
  char *buf=(char*)malloc(cap+1);
  if(!buf){
    InternetCloseHandle(hUrl);
    if(reqSlot) InterlockedExchangePointer(reqSlot,NULL);
    InternetCloseHandle(hInet); return 0;
  }
  int ok=1;
  for(;;){
    if(len+4096+1>cap){
      if(cap>=UPD_MAX_BODY) break;
      size_t nc=cap*2; if(nc>UPD_MAX_BODY) nc=UPD_MAX_BODY;
      char *nb=(char*)realloc(buf,nc+1);
      if(!nb){ ok=0; break; }
      buf=nb; cap=nc;
    }
    DWORD got=0;
    if(!InternetReadFile(hUrl,buf+len,(DWORD)(cap-len-1),&got)){ ok=0; break; }
    if(got==0) break;
    len+=got;
    if(len>=UPD_MAX_BODY) break;
  }
  buf[len]=0;                 /* 分配时多留了 1 字节，这里永远安全 */
  InternetCloseHandle(hUrl);
  if(reqSlot) InterlockedExchangePointer(reqSlot,NULL);
  InternetCloseHandle(hInet);
  if(!ok||len==0){ free(buf); return 0; }
  *out=buf; *plen=(int)len;
  return 1;
}

/* --------------------------------------------------------------------------
   下载新 exe：交给系统自带的 curl.exe，不再用 WinINet。

   为什么：Release 的 asset 直链在 github.com/.../releases/download/，会 302
   跳到 objects.githubusercontent.com；这条链路和 api.github.com 完全不是一回事，
   国内经常被拦/挂起，而 WinINet 的超时选项在部分环境根本不生效（实测能挂 30 秒）。
   curl 自带 -L 跟随重定向、--connect-timeout / --max-time 硬超时、--retry 重试，
   并且会读系统/环境代理设置，比 WinINet 可靠得多。老系统没有 curl 时退回
   PowerShell 的 WebClient（Win7 起自带，也会自动跟随重定向）。

   下完必须校验是 PE（MZ）且足够大 —— 防止把 403/404 错误页当 exe 覆盖自己。
   -------------------------------------------------------------------------- */
static int curl_download(const char *url,const wchar_t *dst){
  DeleteFileW(dst);                       /* 上次失败的残file */
  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0) return 0;

  /* 可选镜像前缀：国内连不上 github.com 时，用户可设环境变量 KIMIPET_MIRROR，
     例如 https://ghproxy.net/ —— 程序会把它拼在 asset 直链前面。留空=直连。 */
  wchar_t mirror[512]=L"";
  GetEnvironmentVariableW(L"KIMIPET_MIRROR",mirror,512);
  wchar_t full[2048];
  if(mirror[0]) _snwprintf(full,2048,L"%ls%ls",mirror,wurl);
  else          wcscpy(full,wurl);

  wchar_t cmd[4096];
  STARTUPINFOW si; memset(&si,0,sizeof si); si.cb=sizeof si;
  PROCESS_INFORMATION pi; memset(&pi,0,sizeof pi);

  _snwprintf(cmd,4096,
    L"curl.exe -L -f -sS --retry 2 --connect-timeout 15 --max-time 180 "
    L"-o \"%ls\" \"%ls\"", dst, full);

  BOOL started=CreateProcessW(NULL,cmd,NULL,NULL,FALSE,
                              CREATE_NO_WINDOW,NULL,NULL,&si,&pi);
  if(!started){
    /* 没有 curl.exe：PowerShell WebClient 兜底 */
    _snwprintf(cmd,4096,
      L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
      L"\"$ProgressPreference='SilentlyContinue'; "
      L"(New-Object System.Net.WebClient).DownloadFile('%ls','%ls')\"",
      full,dst);
    started=CreateProcessW(NULL,cmd,NULL,NULL,FALSE,
                           CREATE_NO_WINDOW,NULL,NULL,&si,&pi);
    if(!started){
      DeleteFileW(dst);
      return 0;
    }
  }
  /* curl 自己有 --max-time，这里再兜一层，防止极端情况下永不返回 */
  DWORD wr=WaitForSingleObject(pi.hProcess,200000);
  DWORD code=1;
  if(wr==WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess,&code);
  else TerminateProcess(pi.hProcess,1);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if(code!=0){ DeleteFileW(dst); return 0; }

  /* 校验 PE 头 + 合理体积（x86/x64 都是 400KB 量级，64KB 下限很宽松） */
  HANDLE hf=CreateFileW(dst,GENERIC_READ,FILE_SHARE_READ,NULL,
                        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
  if(hf==INVALID_HANDLE_VALUE){ DeleteFileW(dst); return 0; }
  LARGE_INTEGER sz; sz.QuadPart=0;
  GetFileSizeEx(hf,&sz);
  unsigned char mz[2]={0}; DWORD got=0;
  ReadFile(hf,mz,2,&got,NULL);
  CloseHandle(hf);
  if(sz.QuadPart<65536 || got!=2 || mz[0]!='M' || mz[1]!='Z'){
    DeleteFileW(dst); return 0;
  }
  return 1;
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

/* --------------------------------------------------------------------------
   看门狗：WinINet 的超时选项在部分环境（包括 wine、某些代理）根本不生效，
   所以这里自己计时。只在 netActive=1 期间累计，用户看确认框时不打扰。
   超时就关掉请求句柄，把工作线程从 InternetReadFile 里拽出来。
   -------------------------------------------------------------------------- */
static DWORD WINAPI upd_watch(LPVOID arg){
  UPD_CTX *c=(UPD_CTX*)arg;
  for(;;){
    if(READ(&c->finished)) break;
    if(READ(&c->netActive)){
      DWORD st=(DWORD)READ(&c->netStart);
      if(GetTickCount()-st > UPD_HARD_MS){
        if(InterlockedCompareExchange(&c->timedOut,1,0)==0){
          /* 尽力而为：关句柄在部分实现里能唤醒阻塞的读，唤不醒也没关系，
             UI 早就不在这个线程上了，下面照样给用户一个明确结果。 */
          PVOID r=InterlockedExchangePointer(&c->hReq,NULL);
          if(r) InternetCloseHandle(r);
          /* 用独立的小消息体，不引用 ctx —— 工作线程可能永远醒不过来 */
          UPD_TMO_MSG *m=(UPD_TMO_MSG*)calloc(1,sizeof *m);
          if(m){
            _snwprintf(m->msg,300,
              L"检查更新超时（20 秒没响应），已自动放弃。\n\n"
              L"球不会卡住，稍后再试即可；也能手动到 Releases 页面看看。");
            if(!PostMessageW(c->h,KP_WM_UPDATE,1,(LPARAM)m)) free(m);
          }
          InterlockedExchange(&g_updBusy,0);
        }
        break;
      }
    }
    Sleep(250);
  }
  return 0;
}

/* ctx 所有权始终在工作线程；这里统一收尾，并保证看门狗先退出 */
static void upd_cleanup(UPD_CTX *c){
  EXCHANGE(&c->finished,1);
  if(c->watch){
    WaitForSingleObject(c->watch,2000);
    CloseHandle(c->watch);
    c->watch=NULL;
  }
  CloseHandle(c->ev);
  free(c);
  InterlockedExchange(&g_updBusy,0);
}

/* 注意：这里必须等到主线程真的处理完（用户把对话框点掉）才能释放 ctx。
   以前设了 30 秒上限，超时就 free，可主线程的 MessageBox 可能还开着，
   之后 SetEvent(c->ev) 就是 use-after-free。弹框是用户主动触发的，等他点掉是合理的。 */
static void upd_notify(UPD_CTX *c){
  if(READ(&c->timedOut)){ upd_cleanup(c); return; }   /* 看门狗已经提示过了 */
  PostMessageW(c->h,KP_WM_UPDATE,0,(LPARAM)c);
  WaitForSingleObject(c->ev,INFINITE);
  upd_cleanup(c);
}

/* 主线程：wParam=1 的超时消息 */
void upd_on_timeout(HWND h,void *p){
  UPD_TMO_MSG *m=(UPD_TMO_MSG*)p;
  if(!m) return;
  MessageBoxW(h,m->msg,L"检查更新",MB_OK|MB_ICONWARNING);
  free(m);
}

/* 看门狗判定超时后，把任何失败文案统一换成超时提示 */
static void upd_override_timeout(UPD_CTX *c){
  if(READ(&c->timedOut)){
    c->stage=UPD_ERR_NET;
    _snwprintf(c->msg,600,
      L"检查更新超时（20 秒没响应），已自动放弃。\n\n"
      L"球不会卡住，稍后再试即可；也能手动到 Releases 页面看看。");
  }
}

static DWORD WINAPI upd_thread(LPVOID arg){
  UPD_CTX *c=(UPD_CTX*)arg;
  char api[512];
  _snprintf(api,sizeof api,"https://api.github.com/repos/%s/%s/releases/latest",
            KP_REPO_OWNER,KP_REPO_NAME);

  char *json=NULL; int jlen=0; DWORD st=0;
  net_begin(c);
  int ok=http_get_ex(api,UPD_GH_HEADERS,&json,&jlen,&st,&c->hReq);
  net_end(c);

  if(!ok){
    if(st==403||st==429){
      c->stage=UPD_ERR_HTTP;
      _snwprintf(c->msg,600,
        L"GitHub 接口返回 %lu（API 限流）。\n\n"
        L"未登录状态下 GitHub 每小时只放行 60 次 API 请求，"
        L"稍等一会儿再试就行，本程序不会卡住。\n\n"
        L"也可以直接打开 Releases 页面看：\n"
        L"https://github.com/" KP_REPO_OWNER_W L"/" KP_REPO_NAME_W L"/releases",
        (unsigned long)st);
    }else if(st>=400){
      c->stage=UPD_ERR_HTTP;
      _snwprintf(c->msg,600,
        L"GitHub 返回 HTTP %lu，检查更新没做成。\n\n"
        L"球不会卡住，稍后再试即可；也能手动到 Releases 页面看看。",
        (unsigned long)st);
    }else{
      c->stage=UPD_ERR_NET;
      _snwprintf(c->msg,600,
        L"没能连上 GitHub 检查更新。\n\n"
        L"可能是没网、代理/防火墙拦截，或请求超时（已自动放弃）。\n"
        L"本程序不会卡住，稍后再试即可。\n\n"
        L"手动查看：https://github.com/" KP_REPO_OWNER_W L"/" KP_REPO_NAME_W L"/releases");
    }
    upd_override_timeout(c);
    free(json);
    upd_notify(c);
    return 0;
  }

  char tag[64]={0}, url[1024]={0};
  int haveTag=json_str(json,"tag_name",tag,sizeof tag);
  int haveUrl=find_asset_url(json,upd_asset_name(),url,sizeof url);
  free(json);

  if(!haveTag){
    c->stage=UPD_ERR_JSON;
    _snwprintf(c->msg,600,
      L"没能读到最新版本号（GitHub 返回的内容看不懂）。\n"
      L"稍后再试，或到 Releases 页面手动查看。");
    upd_notify(c); return 0;
  }
  int nmaj=0,nmin=0,npat=0;
  if(!parse_ver(tag,&nmaj,&nmin,&npat)){
    c->stage=UPD_ERR_JSON;
    _snwprintf(c->msg,600,L"最新版本号 %hs 看不懂，跳过更新。",tag);
    upd_notify(c); return 0;
  }
  if(ver_cmp(nmaj,nmin,npat,KP_VERSION_MAJOR,KP_VERSION_MINOR,KP_VERSION_PATCH)<=0){
    c->stage=UPD_LATEST;
    _snwprintf(c->msg,600,L"已经是最新版了。\n\n当前版本：%hs\n最新版本：%hs",
               KP_VERSION_STR,tag);
    upd_notify(c); return 0;
  }
  if(!haveUrl){
    c->stage=UPD_ERR_NOASSET;
    _snwprintf(c->msg,600,
      L"找到新版本 %hs，但 Release 里没有本程序对应的安装包"
      L"（可能是发布时漏传了独立 exe）。\n\n请到 Releases 页面手动下载。",tag);
    upd_notify(c); return 0;
  }

  /* 有新版本：问一句再动手（确认框必须主线程弹） */
  strcpy(c->tag,tag); strcpy(c->url,url);
  c->stage=UPD_ASK;
  _snwprintf(c->msg,600,
    L"发现新版本 %hs（当前 %hs）。\n\n要现在更新吗？\n"
    L"会下载新 exe、覆盖旧文件并重启本程序；下载可能需要几十秒，"
    L"期间球照常动。\n你的配置文件和模组都不会动。",
    tag,KP_VERSION_STR);
  if(READ(&c->timedOut)){ upd_cleanup(c); return 0; }
  PostMessageW(c->h,KP_WM_UPDATE,0,(LPARAM)c);
  /* netActive 已为 0，看门狗不会打扰；确认框开多久都等，不能中途 free ctx */
  WaitForSingleObject(c->ev,INFINITE);
  if(c->answer!=IDYES){ upd_cleanup(c); return 0; }

  /* 下载也在工作线程，UI 全程不阻塞 */
  wchar_t tmpdir[1024],self[MAX_PATH];
  if(GetTempPathW(1024,tmpdir)==0) tmpdir[0]=0;
  if(!GetModuleFileNameW(NULL,self,MAX_PATH)){
    c->stage=UPD_ERR_BAT;
    _snwprintf(c->msg,600,L"取不到本程序的路径，没法更新。");
    upd_notify(c); return 0;
  }
  _snwprintf(c->src,1200,L"%lsKimiPet_update_%lu.exe",tmpdir,GetCurrentProcessId());
  _snwprintf(c->bat,1200,L"%lsKimiPet_update_%lu.bat",tmpdir,GetCurrentProcessId());
  wcscpy(c->dst,self);

  /* 下载交给外部 curl（自带超时与重定向），不套 WinINet 的看门狗计时 ——
     那个 20 秒硬超时是给 API 小请求用的，下载几百 KB 可能更久。 */
  int dl=curl_download(c->url,c->src);
  if(!dl){
    c->stage=UPD_ERR_DOWNLOAD;
    _snwprintf(c->msg,600,
      L"下载新版本失败。\n\n"
      L"GitHub 的下载域名（github.com → objects.githubusercontent.com）"
      L"和检查更新用的 API 不是同一条链路，国内网络经常只通 API 那一条。\n"
      L"可以重试；仍不行就到 Releases 页面手动下载，或用随附的 "
      L"KimiPetUpdater.bat 手动升级。\n\n"
      L"旧文件没有被改动，本程序也不会退出。");
    DeleteFileW(c->src);
    upd_notify(c); return 0;
  }
  if(!write_updater_bat(c->bat,c->src,c->dst)){
    c->stage=UPD_ERR_BAT;
    _snwprintf(c->msg,600,L"没能写出更新脚本，取消更新。旧文件没有被改动。");
    DeleteFileW(c->src);
    upd_notify(c); return 0;
  }

  c->stage=UPD_DO_REPLACE;
  upd_notify(c);
  return 0;
}

/* 主线程收到线程消息后调用：只负责弹框 / 启动脚本，绝不碰网络 */
void upd_on_message(HWND h,void *p){
  UPD_CTX *c=(UPD_CTX*)p;
  if(!c) return;
  switch(c->stage){
    case UPD_ASK:
      c->answer=MessageBoxW(h,c->msg,L"检查更新",MB_YESNO|MB_ICONINFORMATION);
      break;
    case UPD_DO_REPLACE:
      if((INT_PTR)ShellExecuteW(NULL,L"open",c->bat,NULL,NULL,SW_SHOW)<=32){
        DeleteFileW(c->src); DeleteFileW(c->bat);
        MessageBoxW(h,L"启动更新脚本失败，取消更新。旧文件没有被改动。",
                    L"检查更新",MB_OK|MB_ICONWARNING);
      }else{
        if(g_menu){ DestroyMenu(g_menu); g_menu=NULL; }
        PostQuitMessage(0);
      }
      break;
    default:
      MessageBoxW(h,c->msg,L"检查更新",
                  c->stage==UPD_LATEST?MB_OK|MB_ICONINFORMATION:MB_OK|MB_ICONWARNING);
      break;
  }
  SetEvent(c->ev);
}

void do_update(HWND h){
  /* 连点直接忽略，不弹框打断 —— 第一次的结果几秒内就会出来 */
  if(InterlockedCompareExchange(&g_updBusy,1,0)!=0) return;

  UPD_CTX *c=(UPD_CTX*)calloc(1,sizeof *c);
  if(!c){ InterlockedExchange(&g_updBusy,0); return; }
  c->h=h;
  c->ev=CreateEventW(NULL,FALSE,FALSE,NULL);   /* 自动重置 */
  if(!c->ev){ free(c); InterlockedExchange(&g_updBusy,0); return; }

  /* 先起看门狗，再起工作线程；两个句柄都由 ctx 收尾 */
  c->watch=CreateThread(NULL,0,upd_watch,c,0,NULL);
  HANDLE th=CreateThread(NULL,0,upd_thread,c,0,NULL);
  if(!th){
    EXCHANGE(&c->finished,1);
    if(c->watch){ WaitForSingleObject(c->watch,2000); CloseHandle(c->watch); }
    CloseHandle(c->ev); free(c);
    InterlockedExchange(&g_updBusy,0);
    MessageBoxW(h,L"没法启动检查更新线程。",L"检查更新",MB_OK|MB_ICONWARNING);
    return;
  }
  CloseHandle(th);
}
#endif /* !RENDER_TEST */
