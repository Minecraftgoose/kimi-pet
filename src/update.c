/* 检查更新 */
#include "pet.h"

/* ================= 检查更新 =================
   查 GitHub 上本仓库的最新 Release；比当前版本新的话，下载对应架构的 exe，
   写个 bat 等本进程退出后覆盖并重启自己。

   下载主路径用 WinINet 直下 —— 这是 v1.2 实证能走通的链路。曾经把它换掉，
   又在「修卡顿」时把下载包进 20 秒看门狗，结果检测得到更新、exe 永远下不下来。
   现在：看门狗按阶段分阈值，下载走宽松超时。

   下载兜底链：WinINet（主）→ WinHTTP（纯 C 系统组件，显式请求 TLS 1.2）
              → curl.exe / certutil.exe / PowerShell（外部工具）。
   WinHTTP 这一层专为 Win7 而加：系统 Schannel 常未启用 TLS 1.2，而外部
   curl.exe 在 Win7 上不存在、certutil/PowerShell 又容易被安全软件拦。
   WinHTTP 不依赖任何外部 exe，也不需要 libcurl。                       */

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
/* 注意：wininet.h（pet.h 引入）与 winhttp.h 在 mingw-w64 下 TYPEDEF 冲突
   （winhttp.h 自己就注明 "Yes, these values contradict wininet.h"）。
   WinHTTP 实现放在独立的 winhttpdl.c，那个翻译单元不 include wininet.h。 */

/* ==========================================================================
   Windows 实现

   以前整个检查流程都跑在 WndProc 的 WM_COMMAND 里：WinINet 同步请求直接
   阻塞 UI 线程，球会卡住不动；GitHub 403 时又因为 find_asset_url 的死循环
   彻底失去响应，只能等系统杀进程。

   现在做四层防护：
     1. 所有网络 IO 都在工作线程，主线程只弹对话框，球全程照常动；
     2. WinINet 设超时 + 先查状态码，403/429 立即返回友好提示；
     3. 独立看门狗线程：万一 WinINet 自己不理会超时（某些代理/wine 就是
        这样），强制关闭请求句柄把工作线程拽出来，绝不留挂死线程；
     4. 看门狗【同时覆盖下载阶段】，阈值比 API 长得多。以前只监控 API，
        下载若永久阻塞，g_updBusy 永远不清零，用户再点检查更新毫无反应。

   超时口径（都可用编译期宏覆盖，方便本地慢速测试）：
     API     请求 15s / 硬超时 30s
     下载    连接 20s（独立，别让"连不上"等满下载时长）
             接收间隔 300s（慢速但持续来数据就不算超时）
             硬超时 330s（总时长兜底，超时给明确提示并允许重试）
   ========================================================================== */

#ifndef UPD_TIMEOUT_MS
#define UPD_TIMEOUT_MS   15000       /* API 请求：小 JSON，15 秒（原 8 秒，慢速网络放宽） */
#endif
#ifndef UPD_CONNECT_MS
#define UPD_CONNECT_MS   20000       /* 连接超时独立出来 */
#endif
#ifndef UPD_DOWNLOAD_MS
#define UPD_DOWNLOAD_MS  300000      /* 下载：接收间隔超时 5 分钟 */
#endif
#ifndef UPD_HARD_MS
#define UPD_HARD_MS      30000       /* 看门狗：API 阶段硬超时 */
#endif
#ifndef UPD_DL_HARD_MS
#define UPD_DL_HARD_MS   330000      /* 看门狗：下载阶段硬超时（略大于 300s，留余量） */
#endif
#ifndef UPD_MAX_BODY
#define UPD_MAX_BODY     (16*1024*1024)
#endif

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
typedef struct { wchar_t msg[300]; int isDownload; } UPD_TMO_MSG;

/* 一次 WinINet 请求的句柄槽位。看门狗超时时要关掉的是【正在阻塞读的 hUrl】，
   只关父句柄 hInet 在部分实现里唤不醒 InternetReadFile。用 Interlocked
   交换取所有权，保证工作线程和看门狗不会重复关闭同一个句柄。 */
typedef struct {
  volatile PVOID inet;       /* WinINet 父句柄 */
  volatile PVOID url;        /* WinINet 请求句柄 */
  volatile PVOID whReq;      /* WinHTTP 请求句柄（兜底链路用） */
  volatile LONG  abort;      /* 看门狗置位，读循环每轮检查 */
} HTTP_SLOTS;

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
  volatile LONG  netKind;   /* 1 = API 请求，2 = 下载 exe */
  volatile LONG  netStart;  /* GetTickCount 起点 */
  volatile LONG  timedOut;  /* 看门狗已判定超时 */
  HTTP_SLOTS     slots;     /* 当前 WinINet 句柄，供看门狗强制关闭 */
} UPD_CTX;

static volatile LONG g_updBusy=0;

#define EXCHANGE(p,v) InterlockedExchange((volatile LONG*)(p),(LONG)(v))
#define READ(p)       InterlockedCompareExchange((volatile LONG*)(p),0,0)

static void net_begin(UPD_CTX *c,int kind){
  EXCHANGE(&c->netKind,kind);
  EXCHANGE(&c->netStart,(LONG)GetTickCount());
  EXCHANGE(&c->netActive,1);
}
static void net_end(UPD_CTX *c){
  EXCHANGE(&c->netActive,0);
  EXCHANGE(&c->netKind,0);
}

/* 取回句柄所有权后再关：看门狗可能已经取走并关闭了 */
static void hg_close(HTTP_SLOTS *s,HINTERNET hUrl,HINTERNET hInet){
  HINTERNET u = s ? (HINTERNET)InterlockedExchangePointer(&s->url,NULL) : hUrl;
  HINTERNET i = s ? (HINTERNET)InterlockedExchangePointer(&s->inet,NULL) : hInet;
  if(u) InternetCloseHandle(u);
  if(i) InternetCloseHandle(i);
}

/* --------------------------------------------------------------------------
   http_get_ex：带超时、带状态码、带响应体上限的一次 GET
   slots 存父/子句柄，看门狗超时时会关掉它们来中断阻塞中的读操作。
   -------------------------------------------------------------------------- */
static int http_get_ex(const char *url,const wchar_t *headers,
                       char **out,int *plen,DWORD *status,
                       HTTP_SLOTS *slots,DWORD tmo){
  *out=NULL; *plen=0; *status=0;

  HINTERNET hInet=InternetOpenW(L"KimiPet/" KP_VERSION_W,
                                INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,0);
  if(!hInet) return 0;
  if(slots) InterlockedExchangePointer(&slots->inet,hInet);

  /* 连接超时独立：慢速下载要给足接收时间，但"连不上"不该陪着等 5 分钟。
     其余超时用 tmo —— RECEIVE / DATA_RECEIVE 是"两次数据到达之间的间隔"，
     只要持续来数据，再慢也不算超时（这正是 v1.2 硬等的成功率来源）。 */
  DWORD ct = tmo<UPD_CONNECT_MS ? tmo : UPD_CONNECT_MS;
  DWORD t  = tmo;
  InternetSetOption(hInet,INTERNET_OPTION_CONNECT_TIMEOUT,     &ct,sizeof ct);
  InternetSetOption(hInet,INTERNET_OPTION_SEND_TIMEOUT,        &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_RECEIVE_TIMEOUT,     &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_DATA_SEND_TIMEOUT,   &t,sizeof t);
  InternetSetOption(hInet,INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,&t,sizeof t);

  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0){
    hg_close(slots,NULL,hInet); return 0;
  }
  /* 按协议决定要不要 SECURE —— asset 直链偶尔是 http，别误加标志 */
  DWORD flags = INTERNET_FLAG_RELOAD|INTERNET_FLAG_NO_CACHE_WRITE|
                INTERNET_FLAG_NO_UI|INTERNET_FLAG_PRAGMA_NOCACHE|
                INTERNET_FLAG_NO_COOKIES;
  if(strncmp(url,"https://",8)==0) flags |= INTERNET_FLAG_SECURE;
  HINTERNET hUrl=InternetOpenUrlW(hInet,wurl,headers,0,flags,0);
  if(!hUrl){ hg_close(slots,NULL,hInet); return 0; }
  if(slots) InterlockedExchangePointer(&slots->url,hUrl);
  /* 有些 WinINet 实现只对请求句柄认超时，句柄创建后再设一遍 */
  InternetSetOption(hUrl,INTERNET_OPTION_RECEIVE_TIMEOUT,     &t,sizeof t);
  InternetSetOption(hUrl,INTERNET_OPTION_DATA_RECEIVE_TIMEOUT,&t,sizeof t);
  InternetSetOption(hUrl,INTERNET_OPTION_SEND_TIMEOUT,        &t,sizeof t);

  /* 先读状态码：403/429 这类限流直接返回，不去解析错误页。
     这条不能退回 v1.2 的"不查状态码"——旧版只是把错误页当 JSON 解析，
     最后报"版本号看不懂"，并没有更容易成功。 */
  DWORD code=0, sz=sizeof code;
  if(HttpQueryInfoW(hUrl,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,
                    &code,&sz,NULL)) *status=code;
  if(code>=400){ hg_close(slots,hUrl,hInet); return 0; }

  size_t cap=16384, len=0;
  char *buf=(char*)malloc(cap+1);
  if(!buf){ hg_close(slots,hUrl,hInet); return 0; }
  int ok=1;
  for(;;){
    if(slots && READ(&slots->abort)){ ok=0; break; }   /* 看门狗已判定超时 */
    if(len+4096+1>cap){
      /* 响应体超过上限：明确失败。以前这里直接 break 却仍 return 1，
         截断的 PE 会过 MZ 校验并覆盖自己 —— 能把安装写坏。 */
      if(cap>=UPD_MAX_BODY){ ok=0; break; }
      size_t nc=cap*2; if(nc>UPD_MAX_BODY) nc=UPD_MAX_BODY;
      char *nb=(char*)realloc(buf,nc+1);
      if(!nb){ ok=0; break; }
      buf=nb; cap=nc;
    }
    DWORD got=0;
    if(!InternetReadFile(hUrl,buf+len,(DWORD)(cap-len-1),&got)){ ok=0; break; }
    if(got==0) break;
    len+=got;
    if(len>=UPD_MAX_BODY){ ok=0; break; }   /* 同上，绝不把超限数据当成功 */
  }
  buf[len]=0;                 /* 分配时多留了 1 字节，这里永远安全 */
  hg_close(slots,hUrl,hInet);
  if(!ok||len==0){ free(buf); return 0; }
  *out=buf; *plen=(int)len;
  return 1;
}

/* --------------------------------------------------------------------------
   下载兜底：系统自带的 curl.exe（老系统没有就 PowerShell WebClient）。

   只在 WinINet 主路径失败时才走这里，顺序：curl → certutil → PowerShell。
   注意不能只写裸 "curl.exe" 让 CreateProcess 去搜 PATH：32 位进程的
   System32 会被 WOW64 重定向到 SysWOW64，而 Windows 只随系统带了 64 位
   curl.exe，x86 版会找不到。所以先试系统目录全路径（含 Sysnative），
   再退回 PATH；没有 curl 就用 Win7+ 自带的 certutil；最后才 PowerShell。
   -------------------------------------------------------------------------- */
static int curl_download(const char *url,const wchar_t *dst,volatile LONG *abort){
  DeleteFileW(dst);
  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0) return 0;

  /* 可选镜像前缀：用户可设环境变量 KIMIPET_MIRROR，留空 = 直连 */
  wchar_t mirror[512]=L"";
  GetEnvironmentVariableW(L"KIMIPET_MIRROR",mirror,512);
  wchar_t full[2048];
  if(mirror[0]) _snwprintf(full,2048,L"%ls%ls",mirror,wurl);
  else          wcscpy(full,wurl);

  wchar_t sysdir[MAX_PATH]={0}, windir[MAX_PATH]={0};
  GetSystemDirectoryW(sysdir,MAX_PATH);
  GetWindowsDirectoryW(windir,MAX_PATH);
  wchar_t curlPath[4][MAX_PATH];
  _snwprintf(curlPath[0],MAX_PATH,L"%ls\\curl.exe",sysdir);
  /* 32 位进程访问真实 System32 要写 Sysnative（System32 会被重定向到 SysWOW64） */
  _snwprintf(curlPath[1],MAX_PATH,L"%ls\\Sysnative\\curl.exe",windir);
  _snwprintf(curlPath[2],MAX_PATH,L"%ls\\System32\\curl.exe",windir);
  wcscpy(curlPath[3],L"curl.exe");

  wchar_t cmd[4096];
  STARTUPINFOW si; memset(&si,0,sizeof si); si.cb=sizeof si;
  PROCESS_INFORMATION pi;
  BOOL started=FALSE;

  for(int i=0;i<4 && !started;i++){
    memset(&pi,0,sizeof pi);
    _snwprintf(cmd,4096,
      L"\"%ls\" -L -f -sS --retry 2 --connect-timeout 15 --max-time 180 "
      L"-o \"%ls\" \"%ls\"", curlPath[i], dst, full);
    started=CreateProcessW(NULL,cmd,NULL,NULL,FALSE,
                           CREATE_NO_WINDOW,NULL,NULL,&si,&pi);
  }
  if(!started){
    /* 次兜底：certutil.exe（Win7+ 系统自带，走系统 HTTP 栈，不受老 .NET 限制） */
    memset(&pi,0,sizeof pi);
    _snwprintf(cmd,4096,
      L"certutil.exe -urlcache -split -f \"%ls\" \"%ls\"", full,dst);
    started=CreateProcessW(NULL,cmd,NULL,NULL,FALSE,
                           CREATE_NO_WINDOW,NULL,NULL,&si,&pi);
  }
  if(!started){
    /* 最后兜底：PowerShell WebClient。TLS 1.2 用数值 3072 经 [Enum]::ToObject
       设置 —— .NET 2.0/3.5 的 SecurityProtocolType 没有 Tls12 具名成员，
       直接写 [Net.SecurityProtocolType]::Tls12 会在赋值阶段抛
       PropertyAssignmentException（提示「可能的枚举值为 Ssl3、Tls」），
       下载还没开始就失败。整段包 try/catch，赋值失败也继续尝试。 */
    memset(&pi,0,sizeof pi);
    _snwprintf(cmd,4096,
      L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
      L"\"$ProgressPreference='SilentlyContinue'; "
      L"try { [Net.ServicePointManager]::SecurityProtocol="
      L"[Enum]::ToObject([Net.SecurityProtocolType],3072) } catch {}; "
      L"try { (New-Object System.Net.WebClient).DownloadFile('%ls','%ls'); "
      L"exit 0 } catch { exit 1 }\"",
      full,dst);
    started=CreateProcessW(NULL,cmd,NULL,NULL,FALSE,
                           CREATE_NO_WINDOW,NULL,NULL,&si,&pi);
    if(!started){ DeleteFileW(dst); return 0; }
  }
  /* 分段等待：每 250ms 看一眼 abort，看门狗判定下载超时就能立刻掐掉进程，
     不用陪着 curl 的 --max-time 180 干等。总上限仍是 200 秒。 */
  DWORD waited=0, wr=WAIT_TIMEOUT;
  for(;;){
    if(abort && READ(abort)) break;
    wr=WaitForSingleObject(pi.hProcess,250);
    if(wr==WAIT_OBJECT_0) break;
    waited+=250;
    if(waited>=200000) break;
  }
  DWORD code=1;
  if(wr==WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess,&code);
  else TerminateProcess(pi.hProcess,1);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if(code!=0){ DeleteFileW(dst); return 0; }

  /* 校验 PE 头 + 合理体积 */
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

/* --------------------------------------------------------------------------
   下载主路径：WinINet 直下。

   v1.2 就是这么做的，用户实证能从 1.2 顺利升级到 1.3。1.3 那次「修卡顿」
   把这段包进 20 秒看门狗，下载几百 KB 一旦超过 20 秒就被强行关闭句柄，
   于是「检测得到更新、exe 下不下来」。

   现在下载【仍然接看门狗】，但阈值是独立的 330 秒（API 才 30 秒），
   并把 hUrl 句柄交给看门狗 —— 万一下载永久阻塞，能关掉真正阻塞的句柄、
   给用户明确提示、释放 g_updBusy 让他可以重试。既保留 v1.2 的硬等成功率，
   又不留挂死线程。
   -------------------------------------------------------------------------- */
static int wininet_download(const char *url,const wchar_t *dst,DWORD *status,
                            HTTP_SLOTS *slots){
  DeleteFileW(dst);
  char *data=NULL; int len=0; DWORD st=0;
  int ok=http_get_ex(url,NULL,&data,&len,&st,slots,UPD_DOWNLOAD_MS);
  if(status) *status=st;
  if(!ok||!data){ free(data); return 0; }
  /* 下下来的是不是 PE：看 MZ 头 + 体积，防止把 403/404 网页当 exe 覆盖自己 */
  if(len<65536 || (unsigned char)data[0]!='M' || (unsigned char)data[1]!='Z'){
    free(data); DeleteFileW(dst); return 0;
  }
  int w=write_file_all(dst,(const unsigned char*)data,(size_t)len);
  free(data);
  if(!w){ DeleteFileW(dst); return 0; }
  return 1;
}

static int download_exe(const char *url,const wchar_t *dst,DWORD *status,
                        HTTP_SLOTS *slots){
  if(wininet_download(url,dst,status,slots)) return 1; /* 主路径不变 */
  /* 看门狗已判定超时就别再起下一条链路 */
  if(slots && READ(&slots->abort)) return 0;
  /* Win7 纯 C 兜底：WinHTTP，显式请求 TLS 1.2，不依赖任何外部 exe。
     超时常量由本文件统一传入，避免两个翻译单元各存一份。 */
  if(winhttp_download(url,dst,status,
                      slots?&slots->abort:NULL,slots?&slots->whReq:NULL,
                      UPD_CONNECT_MS,UPD_DOWNLOAD_MS,UPD_MAX_BODY)) return 1;
  /* 外部工具兜底也吃同一个 abort 标志，超时能被立刻掐掉 */
  return curl_download(url,dst,slots?&slots->abort:NULL);
}

/* 写一个 bat：等本进程结束 → 用新 exe 覆盖 → 启动 → 自删 */
static int write_updater_bat(const wchar_t *bat,const wchar_t *src,const wchar_t *dst){
  DWORD pid=GetCurrentProcessId();
  wchar_t tmp[4096];
  _snwprintf(tmp,4096,
    L"@echo off\r\n"
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
  /* bat 由 cmd.exe 按 OEM 代码页逐字节解析。以前写 UTF-8 再靠 chcp 65001
     撑着，中文 Windows（GBK）下路径会乱码 → copy 失败 → 覆盖没做成。
     改成系统 OEM 代码页编码、不加 chcp，中文路径才能被正确读到。 */
  int k=WideCharToMultiByte(CP_OEMCP,0,tmp,-1,NULL,0,NULL,NULL);
  if(k<=0) return 0;
  unsigned char *b=(unsigned char*)malloc((size_t)k);
  if(!b) return 0;
  WideCharToMultiByte(CP_OEMCP,0,tmp,-1,(char*)b,k,NULL,NULL);
  int ok=write_file_all(bat,b,(size_t)k-1);
  free(b);
  return ok;
}

/* --------------------------------------------------------------------------
   看门狗：WinINet 的超时选项在部分环境（包括 wine、某些代理）根本不生效，
   所以这里自己计时。netKind 区分 API(1) / 下载(2)，阈值各自独立。
   超时就关掉【正在阻塞读的 hUrl】再关父句柄，把工作线程拽出来。
   -------------------------------------------------------------------------- */
static DWORD WINAPI upd_watch(LPVOID arg){
  UPD_CTX *c=(UPD_CTX*)arg;
  for(;;){
    if(READ(&c->finished)) break;
    if(READ(&c->netActive)){
      DWORD st=(DWORD)READ(&c->netStart);
      LONG  kind=READ(&c->netKind);
      DWORD limit = (kind==2) ? UPD_DL_HARD_MS : UPD_HARD_MS;
      if(GetTickCount()-st > limit){
        if(InterlockedCompareExchange(&c->timedOut,1,0)==0){
          int isDl = (kind==2);
          /* 先置 abort（读循环每轮检查），再关 hUrl（唤醒阻塞的读），
             最后关父句柄。顺序不能反：只关 hInet 在部分实现里唤不醒。 */
          EXCHANGE(&c->slots.abort,1);
          /* 三条链路各自持有不同的句柄，都要取回所有权再关：
             WinINet 的 hUrl / hInet，以及 WinHTTP 的 hRequest。
             只关其中一个，阻塞读可能仍在别的链路里醒不过来。 */
          PVOID r=InterlockedExchangePointer(&c->slots.url,NULL);
          if(r) InternetCloseHandle((HINTERNET)r);
          r=InterlockedExchangePointer(&c->slots.inet,NULL);
          if(r) InternetCloseHandle((HINTERNET)r);
          winhttp_abort(&c->slots.whReq);   /* 独立模块负责关闭自己的句柄 */
          /* 用独立的小消息体，不引用 ctx —— 工作线程可能永远醒不过来 */
          UPD_TMO_MSG *m=(UPD_TMO_MSG*)calloc(1,sizeof *m);
          if(m){
            m->isDownload=isDl;
            if(isDl)
              _snwprintf(m->msg,300,
                L"下载新版本超时（等了 5 分钟没完成），已自动放弃。\n\n"
                L"已试过 WinINet、WinHTTP 和命令行工具三条链路。\n"
                L"旧文件没有被改动，球也不会卡住；可以稍后重试，"
                L"或用随附的 KimiPetUpdater.bat 手动升级（浏览器下载 + 自动拾取）。");
            else
              _snwprintf(m->msg,300,
                L"检查更新超时（30 秒没响应），已自动放弃。\n\n"
                L"球不会卡住，稍后再试即可；也能手动到 Releases 页面看看。");
            if(!PostMessageW(c->h,KP_WM_UPDATE,1,(LPARAM)m)) free(m);
          }
          InterlockedExchange(&g_updBusy,0);   /* 关键：允许用户重试 */
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
  /* 看门狗判定超时时已经清过 g_updBusy，好让用户立刻重试。若本轮工作线程
     晚一点才从阻塞读里醒过来（WinHTTP 关句柄在部分实现里不能立刻唤醒，
     要等它自己的接收超时），这里绝不能再清一次 —— 那会把【新一轮】检查
     更新的 busy 标志误清零，导致连点保护失效、两个 ctx 并存。 */
  int timedOut = READ(&c->timedOut);
  free(c);
  if(!timedOut) InterlockedExchange(&g_updBusy,0);
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
      L"检查更新超时（30 秒没响应），已自动放弃。\n\n"
      L"球不会卡住，稍后再试即可；也能手动到 Releases 页面看看。");
  }
}

static DWORD WINAPI upd_thread(LPVOID arg){
  UPD_CTX *c=(UPD_CTX*)arg;
  char api[512];
  _snprintf(api,sizeof api,"https://api.github.com/repos/%s/%s/releases/latest",
            KP_REPO_OWNER,KP_REPO_NAME);

  char *json=NULL; int jlen=0; DWORD st=0;
  net_begin(c,1);   /* API 阶段：30 秒硬超时 */
  int ok=http_get_ex(api,UPD_GH_HEADERS,&json,&jlen,&st,&c->slots,UPD_TIMEOUT_MS);
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

  /* 下载：主路径 WinINet 直下（v1.2 实证可用），curl 仅兜底。
     看门狗同样覆盖这段，但用独立的 330 秒阈值 —— 1.3 那次是错在
     把下载塞进【20 秒】的 API 看门狗，不是错在"下载不该被监控"。 */
  DWORD dlStatus=0;
  net_begin(c,2);
  int dl=download_exe(c->url,c->src,&dlStatus,&c->slots);
  net_end(c);
  /* 看门狗已提示过，这里不能再弹一次框；直接收尾并允许用户重试 */
  if(READ(&c->timedOut)){ DeleteFileW(c->src); upd_cleanup(c); return 0; }
  if(!dl){
    c->stage=UPD_ERR_DOWNLOAD;
    _snwprintf(c->msg,600,
      L"下载新版本失败（HTTP %lu）。\n\n"
      L"检查更新用的 API 能通，但下载 exe 没成；常见原因是代理/杀软拦截，"
      L"或 Release 附件还没传好。\n\n"
      L"可以重试；仍不行就到 Releases 页面手动下载，或用随附的 "
      L"KimiPetUpdater.bat 手动升级（浏览器下载 + 自动拾取，不走命令行）。\n\n"
      L"旧文件没有被改动，本程序也不会退出。",
      (unsigned long)dlStatus);
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
