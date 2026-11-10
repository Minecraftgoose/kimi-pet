/* 工具：数学 / 编码 / 文件 IO / 文本处理 */
#include "pet.h"

/* ---------------- 工具 ---------------- */
#ifdef RENDER_TEST
double now_ms(void){ return g_simNow; }
#else
double now_ms(void){ return (double)timeGetTime(); }
#endif
double rnd(double a,double b){ return a + (b-a)*((double)rand()/(double)RAND_MAX); }
double clampd(double v,double a,double b){ return v<a?a:(v>b?b:v); }
double lerpd(double a,double b,double t){ return a+(b-a)*t; }
int clampi(int v,int a,int b){ return v<a?a:(v>b?b:v); }


/* ---------------- 编码 / 文件 IO ---------------- */
#ifdef RENDER_TEST
/* 本机测试用：真正的 UTF-8 <-> wchar_t（UCS-4）转换 */
int u8_to_w(const char *s, wchar_t *out, int n){
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
int w_to_u8(const wchar_t *s, char *out, int n){
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
unsigned char* read_file_all(const wchar_t *path, size_t *outLen){
  char p[1024]; w_to_u8(path,p,sizeof p);
  FILE *f=fopen(p,"rb"); if(!f) return NULL;
  fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  if(n<0){ fclose(f); return NULL; }
  unsigned char *b=(unsigned char*)malloc((size_t)n+1); if(!b){ fclose(f); return NULL; }
  size_t got=fread(b,1,(size_t)n,f); fclose(f);
  b[got]=0; if(outLen) *outLen=got; return b;
}
int write_file_all(const wchar_t *path, const unsigned char *data, size_t len){
  char p[1024]; w_to_u8(path,p,sizeof p);
  FILE *f=fopen(p,"wb"); if(!f) return 0;
  size_t w=fwrite(data,1,len,f); fclose(f); return (int)(w==len);
}
#else
int u8_to_w(const char *s, wchar_t *out, int n){
  if(!s||n<=0) return 0;
  int r=MultiByteToWideChar(CP_UTF8,0,s,-1,out,n);
  if(r<=0){ if(out) out[0]=0; return 0; }
  return r-1;
}
int w_to_u8(const wchar_t *s, char *out, int n){
  if(!s||n<=0) return 0;
  int r=WideCharToMultiByte(CP_UTF8,0,s,-1,out,n,NULL,NULL);
  if(r<=0){ if(out) out[0]=0; return 0; }
  return r-1;
}
unsigned char* read_file_all(const wchar_t *path, size_t *outLen){
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
int write_file_all(const wchar_t *path, const unsigned char *data, size_t len){
  HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,NULL);
  if(h==INVALID_HANDLE_VALUE) return 0;
  DWORD w=0;
  BOOL ok=WriteFile(h,data,(DWORD)len,&w,NULL);
  CloseHandle(h);
  return (int)(ok && w==(DWORD)len);
}
#endif

RGB8 hsl2rgb(double h,double s,double l){
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

/* ---- 文本小工具 ---- */
int cieq(const char *a,const char *b){
  while(*a && *b){
    int ca=*a, cb=*b;
    if(ca>='A'&&ca<='Z') ca+=32;
    if(cb>='A'&&cb<='Z') cb+=32;
    if(ca!=cb) return 0;
    a++; b++;
  }
  return *a==*b;
}
char *ltrim(char *s){
  while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++; return s;
}
char *rtrim(char *s){
  size_t n=strlen(s);
  while(n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
  return s;
}
char *trim(char *s){ rtrim(s); return ltrim(s); }
int is_blank_or_comment(const char *s){
  s=ltrim((char*)s);
  return *s==0 || *s=='#' || *s==';';
}
wchar_t *wltrim(wchar_t *s){
  while(*s==L' '||*s==L'\t'||*s==L'\r'||*s==L'\n') s++; return s;
}
void wrtrim(wchar_t *s){
  size_t n=wcslen(s);
  while(n && (s[n-1]==L' '||s[n-1]==L'\t'||s[n-1]==L'\r'||s[n-1]==L'\n')) s[--n]=0;
}

int parse_bool(const char *v){
  if(!v || !*v) return 0;
  if(cieq(v,"0")||cieq(v,"false")||cieq(v,"off")||cieq(v,"no")) return 0;
  if(atoi(v)!=0) return 1;
  return (cieq(v,"true")||cieq(v,"on")||cieq(v,"yes")||strcmp(v,"开")==0);
}

int hexv(char c){
  if(c>='0'&&c<='9') return c-'0';
  if(c>='a'&&c<='f') return c-'a'+10;
  if(c>='A'&&c<='F') return c-'A'+10;
  return -1;
}
int parse_hex_rgb(const char *p, RGB8 *out){
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

void rgb2hsl(RGB8 c, double *H,double *S,double *L){
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

/* ---- 右键「关于」打开的介绍页地址 ----
   格式： https://minecraftgoose.github.io/kimi-pet/#<版本>&<配置文件路径>
   路径按 UTF-8 percent-encoding（只保留 unreserved 字符），网页端用
   decodeURIComponent 还原中文、空格、反斜杠和 %APPDATA% 展开后的实际路径。
   纯字符串处理，不依赖 Windows API，RENDER_TEST 下也能直接断言。 */
#define ABOUT_PAGE_BASE L"https://minecraftgoose.github.io/kimi-pet/"
int about_url_build(wchar_t *out,int outn,const wchar_t *cfgPath){
  if(!out||outn<=0) return 0;
  out[0]=0;
  char u8[3072];
  int n=w_to_u8(cfgPath?cfgPath:L"",u8,sizeof u8);
  if(n<0) n=0;
  static const char HEX[]="0123456789ABCDEF";
  wchar_t enc[8192]; int e=0;
  for(int i=0;i<n;i++){
    unsigned char c=(unsigned char)u8[i];
    if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
       c=='-'||c=='.'||c=='_'||c=='~'){
      if(e>=8191) break;
      enc[e++]=(wchar_t)c;
    }else{
      if(e+3>=8192) break;
      enc[e++]=L'%';
      enc[e++]=(wchar_t)HEX[c>>4];
      enc[e++]=(wchar_t)HEX[c&15];
    }
  }
  enc[e]=0;
  /* 先算总长再写：缓冲不够就保持 out[0]=0 原样返回失败，
     别留下半个 URL 让 ShellExecute 去打开一个错地址。 */
  size_t need=wcslen(ABOUT_PAGE_BASE)+1+wcslen(KP_VERSION_W)+1+(size_t)e+1;
  if(need>(size_t)outn) return 0;
  int r=_snwprintf(out,outn,L"%ls#%ls&%ls",ABOUT_PAGE_BASE,KP_VERSION_W,enc);
  return r>0 && r<outn;
}

#ifndef RENDER_TEST
/* ---- 读注册表（仅用于旧版本设置迁移 / 自启项） ---- */
int reg_get_dword(const wchar_t *name, DWORD *out){
  HKEY hk; DWORD d=0, sz=sizeof(d);
  if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\KimiPet",0,KEY_QUERY_VALUE,&hk)!=ERROR_SUCCESS)
    return 0;
  int ok = (RegQueryValueExW(hk,name,NULL,NULL,(LPBYTE)&d,&sz)==ERROR_SUCCESS);
  RegCloseKey(hk);
  if(ok) *out=d;
  return ok;
}

/* ---- exe 所在目录 ---- */
int exe_dir(wchar_t *out,int n){
  DWORD k=GetModuleFileNameW(NULL,out,n);
  if(k==0||k>=(DWORD)n) return 0;
  wchar_t *bs=wcsrchr(out,L'\\');
  if(!bs) return 0;
  *(bs+1)=0;
  return 1;
}
#endif
