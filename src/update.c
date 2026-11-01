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

/* 在 assets 数组里定位某个文件名的下载直链 */
int find_asset_url(const char *json,const char *want,char *out,int outn){
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

void do_update(HWND h){
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
#endif
