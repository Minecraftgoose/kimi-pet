/* WinHTTP 下载兜底（Win7 专用）
 *
 * 为什么单独一个翻译单元：
 *   mingw-w64 的 winhttp.h 与 wininet.h 不能同时 include —— winhttp.h 里
 *   自己就写着 "Yes, these values contradict wininet.h"，两者对
 *   INTERNET_SCHEME / URL_COMPONENTS / HTTP_VERSION_INFO 的 typedef 不一致。
 *   update.c 通过 pet.h 已经引入 wininet.h，所以 WinHTTP 必须独立编译。
 *
 * 为什么需要这一层（Win7 特有）：
 *   · 系统 Schannel 默认常未启用 TLS 1.2，而 GitHub 早已强制 TLS 1.2；
 *   · Win7 不自带 curl.exe（Win10 起才有）；
 *   · certutil.exe / PowerShell WebClient 常被火绒等安全软件拦截；
 *   · .NET 3.5 的 WebClient 默认只走 SSL3 / TLS 1.0。
 *   WinHTTP 5.1 是系统 DLL，走自己的 HTTP 栈，不依赖外部 exe 和第三方库。
 *
 * 链接：-lwinhttp。不是 -lws2_32 —— 本程序不用裸 Winsock，WinHTTP 已封装
 * socket、HTTP 头、Content-Length 和重定向，不需要手工解析响应流。
 */
#ifndef RENDER_TEST

#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <winhttp.h>
#include <stdlib.h>
#include <string.h>

#define WH_READ(p) InterlockedCompareExchange((volatile LONG*)(p),0,0)

/* 由 update.c 调用：下载成功返回 1，失败返回 0（旧文件不会被留下）。
 *
 * abort   看门狗置位，读循环每轮检查；
 * reqSlot 正在阻塞读的 WinHTTP 请求句柄，看门狗通过 winhttp_abort() 关掉它；
 * connectMs / recvMs / maxBody 超时与上限统一由 update.c 传入，避免两处各存一份。 */
int winhttp_download(const char *url,const wchar_t *dst,DWORD *status,
                     volatile LONG *abort,volatile PVOID *reqSlot,
                     DWORD connectMs,DWORD recvMs,DWORD maxBody,
                     DWORD *doneBytes){
  DeleteFileW(dst);
  if(status) *status=0;
  if(doneBytes) *doneBytes=0;

  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0) return 0;

  /* 用 WinHttpCrackUrl 拆 URL，不手写字符串解析 */
  URL_COMPONENTSW uc; memset(&uc,0,sizeof uc);
  uc.dwStructSize=sizeof uc;
  wchar_t host[256]=L"", path[2048]=L"";
  uc.lpszHostName=host; uc.dwHostNameLength=256;
  uc.lpszUrlPath=path;  uc.dwUrlPathLength=2048;
  if(!WinHttpCrackUrl(wurl,0,0,&uc)) return 0;
  int secure = (uc.nScheme==INTERNET_SCHEME_HTTPS);
  INTERNET_PORT port = uc.nPort ? uc.nPort
                     : (INTERNET_PORT)(secure?INTERNET_DEFAULT_HTTPS_PORT
                                             :INTERNET_DEFAULT_HTTP_PORT);

  /* WINHTTP_ACCESS_TYPE_DEFAULT_PROXY：Win7 没有 Win8 才有的 AUTOMATIC_PROXY */
  HINTERNET hS=WinHttpOpen(L"KimiPet",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,0);
  if(!hS) return 0;
  DWORD ct=connectMs, rt=recvMs;
  WinHttpSetOption(hS,WINHTTP_OPTION_CONNECT_TIMEOUT,&ct,sizeof ct);
  WinHttpSetOption(hS,WINHTTP_OPTION_SEND_TIMEOUT,&rt,sizeof rt);
  WinHttpSetOption(hS,WINHTTP_OPTION_RECEIVE_TIMEOUT,&rt,sizeof rt);
  WinHttpSetOption(hS,WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT,&rt,sizeof rt);

  HINTERNET hC=WinHttpConnect(hS,host,port,0);
  if(!hC){ WinHttpCloseHandle(hS); return 0; }
  HINTERNET hR=WinHttpOpenRequest(hC,L"GET",path[0]?path:L"/",NULL,
      WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,
      secure?WINHTTP_FLAG_SECURE:0);
  if(!hR){ WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return 0; }
  if(reqSlot) InterlockedExchangePointer(reqSlot,hR);

  /* 显式启用 TLS 1.2。Win7 SP1 需 KB3140245 才真正握得上手；没打补丁时
     WinHttpSetOption 可能失败，忽略即可 —— 不能因一个 option 就放弃下载。 */
  if(secure){
    DWORD proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
                | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
                | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
                | WINHTTP_FLAG_SECURE_PROTOCOL_SSL3;
    WinHttpSetOption(hR,WINHTTP_OPTION_SECURE_PROTOCOLS,&proto,sizeof proto);
  }
  /* GitHub 的 asset 直链会 302 到 objects.githubusercontent.com，必须跟随 */
  DWORD rp=WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(hR,WINHTTP_OPTION_REDIRECT_POLICY,&rp,sizeof rp);

  int ok=0; DWORD code=0;
  HANDLE hf=INVALID_HANDLE_VALUE; __int64 total=0;

  if(!WinHttpSendRequest(hR,WINHTTP_NO_ADDITIONAL_HEADERS,0,
                         WINHTTP_NO_REQUEST_DATA,0,0,0) ||
     !WinHttpReceiveResponse(hR,NULL)) goto done;

  { DWORD sz=sizeof code;
    if(WinHttpQueryHeaders(hR,
         WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
         WINHTTP_HEADER_NAME_BY_INDEX,&code,&sz,WINHTTP_NO_HEADER_INDEX))
      if(status) *status=code; }
  if(code>=400){
    /* 4xx 错误页也落盘（限 64KB），让「贴原始响应」在这条链路同样成立。
       参考实现里 4xx 直接 goto done 不读 body，用户只能看到「无响应体」。 */
    HANDLE ef=CreateFileW(dst,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,NULL);
    if(ef!=INVALID_HANDLE_VALUE){
      DWORD left=65536;
      while(left){
        DWORD avail=0;
        if(!WinHttpQueryDataAvailable(hR,&avail)||!avail) break;
        if(avail>left) avail=left;
        char *b=(char*)malloc(avail);
        if(!b) break;
        DWORD got=0;
        if(!WinHttpReadData(hR,b,avail,&got)||!got){ free(b); break; }
        DWORD wr=0; WriteFile(ef,b,got,&wr,NULL);
        free(b); left-=got;
        if(got<avail) break;
      }
      CloseHandle(ef);
    }
    goto done;
  }

  hf=CreateFileW(dst,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                 FILE_ATTRIBUTE_NORMAL,NULL);
  if(hf==INVALID_HANDLE_VALUE) goto done;
  ok=1;
  for(;;){
    if(abort && WH_READ(abort)){ ok=0; break; }
    DWORD avail=0;
    if(!WinHttpQueryDataAvailable(hR,&avail)){ ok=0; break; }
    if(avail==0) break;
    if(total+(__int64)avail>(__int64)maxBody){ ok=0; break; } /* 超限明确失败 */
    char *b=(char*)malloc(avail?avail:1);
    if(!b){ ok=0; break; }
    DWORD got=0;
    if(!WinHttpReadData(hR,b,avail,&got)){ free(b); ok=0; break; }
    DWORD wr=0;
    if(got && !WriteFile(hf,b,got,&wr,NULL)){ free(b); ok=0; break; }
    total+=(__int64)got;
    if(doneBytes) *doneBytes=(DWORD)total;
    free(b);
    if(got==0) break;
  }
  CloseHandle(hf); hf=INVALID_HANDLE_VALUE;

done:
  if(hf!=INVALID_HANDLE_VALUE) CloseHandle(hf);
  /* 取回句柄所有权再关：看门狗可能已经取走并关闭了 */
  HINTERNET mine = reqSlot
    ? (HINTERNET)InterlockedExchangePointer(reqSlot,NULL) : hR;
  if(mine) WinHttpCloseHandle(mine);
  WinHttpCloseHandle(hC);
  WinHttpCloseHandle(hS);
  if(!ok || total<65536){ return 0; }   /* 内容保留给上层读回 */

  /* 下下来的是不是 PE：防止把 403/404 网页当 exe 覆盖自己 */
  { HANDLE rf=CreateFileW(dst,GENERIC_READ,FILE_SHARE_READ,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(rf==INVALID_HANDLE_VALUE){ return 0; }
    LARGE_INTEGER sz; sz.QuadPart=0; GetFileSizeEx(rf,&sz);
    unsigned char mz[2]={0}; DWORD got=0;
    ReadFile(rf,mz,2,&got,NULL); CloseHandle(rf);
    if(sz.QuadPart<65536 || got!=2 || mz[0]!='M' || mz[1]!='Z'){
      return 0;   /* 不是 PE：内容留着，当作原始响应展示 */
    } }
  return 1;
}


/* --------------------------------------------------------------------------
   API 阶段的 WinHTTP GET 兜底。

   Win7 上 Schannel 常未启用 TLS 1.2，api.github.com 的 WinINet 请求会直接
   握手失败 —— 这时候连「有没有新版本」都查不到，光给下载做兜底没用。
   WinHTTP 走自己的栈，显式请求 TLS 1.2，不依赖 curl / certutil / PowerShell。

   与 qi_winhttp_get 一致：失败（含 4xx）也把已读到的原始响应交给调用方，
   由调用方 free；句柄所有权交给 reqSlot，看门狗能中断阻塞读。
   -------------------------------------------------------------------------- */
int winhttp_get(const char *url,const wchar_t *headers,
                char **out,int *plen,DWORD *status,
                volatile LONG *abort,volatile PVOID *reqSlot,
                DWORD connectMs,DWORD recvMs,DWORD maxBody){
  *out=NULL; *plen=0;
  if(status) *status=0;

  wchar_t wurl[2048];
  if(MultiByteToWideChar(CP_UTF8,0,url,-1,wurl,2048)<=0) return 0;

  URL_COMPONENTSW uc; memset(&uc,0,sizeof uc);
  uc.dwStructSize=sizeof uc;
  wchar_t host[256]=L"", path[2048]=L"";
  uc.lpszHostName=host; uc.dwHostNameLength=256;
  uc.lpszUrlPath=path;  uc.dwUrlPathLength=2048;
  if(!WinHttpCrackUrl(wurl,0,0,&uc)) return 0;
  int secure=(uc.nScheme==INTERNET_SCHEME_HTTPS);
  INTERNET_PORT port = uc.nPort ? uc.nPort
                     : (INTERNET_PORT)(secure?INTERNET_DEFAULT_HTTPS_PORT
                                             :INTERNET_DEFAULT_HTTP_PORT);

  /* Win7 没有 Win8 才有的 AUTOMATIC_PROXY，只能用 DEFAULT_PROXY */
  HINTERNET hS=WinHttpOpen(L"KimiPet",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS,0);
  if(!hS) return 0;
  DWORD ct=connectMs, rt=recvMs;
  WinHttpSetOption(hS,WINHTTP_OPTION_CONNECT_TIMEOUT,&ct,sizeof ct);
  WinHttpSetOption(hS,WINHTTP_OPTION_SEND_TIMEOUT,&rt,sizeof rt);
  WinHttpSetOption(hS,WINHTTP_OPTION_RECEIVE_TIMEOUT,&rt,sizeof rt);
  WinHttpSetOption(hS,WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT,&rt,sizeof rt);

  HINTERNET hC=WinHttpConnect(hS,host,port,0);
  HINTERNET hR=NULL;
  /* ok 只在完整成功路径置 1：任何 goto done（连接失败、SendRequest 失败、
     malloc 失败）都必须返回 0，不能因为变量初值误报成功。 */
  int ok=0, readOk=1, failed; DWORD code=0;
  size_t cap=16384, len=0; char *buf=NULL;

  if(!hC) goto done;
  hR=WinHttpOpenRequest(hC,L"GET",path[0]?path:L"/",NULL,
      WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,
      secure?WINHTTP_FLAG_SECURE:0);
  if(!hR) goto done;
  if(reqSlot) InterlockedExchangePointer(reqSlot,hR);

  /* 显式启用 TLS 1.2。Win7 SP1 需 KB3140245；SetOption 失败忽略即可 */
  if(secure){
    DWORD proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
                | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
                | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1
                | WINHTTP_FLAG_SECURE_PROTOCOL_SSL3;
    WinHttpSetOption(hR,WINHTTP_OPTION_SECURE_PROTOCOLS,&proto,sizeof proto);
  }
  DWORD rp=WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(hR,WINHTTP_OPTION_REDIRECT_POLICY,&rp,sizeof rp);

  /* headers 长度传 0 = 以 NUL 结尾自动计算，GitHub 要 User-Agent */
  if(!WinHttpSendRequest(hR,headers,0,WINHTTP_NO_REQUEST_DATA,0,0,0) ||
     !WinHttpReceiveResponse(hR,NULL)) goto done;

  { DWORD sz=sizeof code;
    WinHttpQueryHeaders(hR,
        WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,&code,&sz,WINHTTP_NO_HEADER_INDEX);
    if(status) *status=code; }

  buf=(char*)malloc(cap+1);
  if(!buf) goto done;
  for(;;){
    if(abort && WH_READ(abort)){ readOk=0; break; }
    DWORD avail=0;
    if(!WinHttpQueryDataAvailable(hR,&avail)){ readOk=0; break; }
    if(avail==0) break;
    if(len+avail+1>cap){
      size_t nc=cap*2;
      if(nc>maxBody) nc=maxBody;
      if(nc<=len+avail+1){ readOk=0; break; }   /* 超限明确失败 */
      char *nb=(char*)realloc(buf,nc+1);
      if(!nb){ readOk=0; break; }
      buf=nb; cap=nc;
    }
    DWORD got=0;
    if(!WinHttpReadData(hR,buf+len,avail,&got)){ readOk=0; break; }
    len+=got;
    if(len>=maxBody){ readOk=0; break; }
    if(got==0) break;
  }
  buf[len]=0;

  /* 4xx 也判失败，但错误页原样交回调用方（403 JSON 里有 message） */
  failed=(!readOk || code>=400 || len==0);
  if(failed){
    if(len>0){ *out=buf; *plen=(int)len; buf=NULL; }
    ok=0;
  }else{
    *out=buf; *plen=(int)len; buf=NULL;
    ok=1;
  }

done:
  /* 取回句柄所有权再关：看门狗可能已经取走并关闭了 */
  { HINTERNET mine = reqSlot
      ? (HINTERNET)InterlockedExchangePointer(reqSlot,NULL) : hR;
    if(mine) WinHttpCloseHandle(mine); }
  if(hC) WinHttpCloseHandle(hC);
  if(hS) WinHttpCloseHandle(hS);
  free(buf);
  return ok;
}

/* 看门狗超时时调用：关掉正在阻塞读的 WinHTTP 请求句柄，把工作线程拽出来。
   用 Interlocked 交换取所有权，保证与工作线程不会重复关闭同一个句柄。 */
void winhttp_abort(volatile PVOID *reqSlot){
  if(!reqSlot) return;
  HINTERNET h=(HINTERNET)InterlockedExchangePointer(reqSlot,NULL);
  if(h) WinHttpCloseHandle(h);
}

#endif /* !RENDER_TEST */
