/* 贴图：PNG 解码（自带 inflate，不依赖 zlib / GDI+ / 任何第三方库）+ 锚点贴图绘制
   只做解码与绘制，文件加载和缓存放在 mod.c（那里才知道 mod 目录在哪）。 */
#include "pet.h"

/* ==================== 轻量 inflate（RFC 1951） ==================== */
typedef struct {
  const unsigned char *in;
  size_t len, pos;
  unsigned bitbuf;
  int bitcnt;
} BitR;

static int br_bits(BitR *b,int need){
  while(b->bitcnt<need){
    if(b->pos>=b->len) return -1;
    b->bitbuf |= (unsigned)b->in[b->pos++] << b->bitcnt;
    b->bitcnt += 8;
  }
  unsigned v=b->bitbuf & ((need>=32?0xFFFFFFFFu:((1u<<need)-1u)));
  b->bitbuf >>= need; b->bitcnt -= need;
  return (int)v;
}

typedef struct { short count[16]; short symbol[288]; } Huff;

static int huff_build(Huff *h,const unsigned char *lens,int n){
  int i;
  for(i=0;i<16;i++) h->count[i]=0;
  for(i=0;i<n;i++) if(lens[i]) h->count[lens[i]]++;
  short offs[16]; offs[0]=0; offs[1]=0;
  for(i=1;i<15;i++) offs[i+1]=(short)(offs[i]+h->count[i]);
  for(i=0;i<n;i++) if(lens[i]) h->symbol[offs[lens[i]]++]=(short)i;
  return 0;
}

static int huff_sym(BitR *b,Huff *h){
  int code=0, first=0, index=0, len;
  for(len=1;len<=15;len++){
    int bit=br_bits(b,1);
    if(bit<0) return -1;
    code |= bit;
    int cnt=h->count[len];
    if(code-first<cnt) return h->symbol[index+(code-first)];
    index += cnt;
    first = (first+cnt)<<1;
    code <<= 1;
  }
  return -1;
}

static const short lbase[29]={
  3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const short lext[29]={
  0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const short dbase[30]={
  1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,
  2049,3073,4097,6145,8193,12289,16385,24577};
static const short dext[30]={
  0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const unsigned char cl_order[19]={
  16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

/* 从 deflate 流解压到 out（容量 cap），返回写入字节数，失败 -1 */
static long inflate_raw(const unsigned char *in,size_t len,unsigned char *out,size_t cap){
  BitR b; b.in=in; b.len=len; b.pos=0; b.bitbuf=0; b.bitcnt=0;
  size_t op=0;
  Huff lit,dst;
  int final;
  do{
    int f=br_bits(&b,1), type=br_bits(&b,2);
    if(f<0||type<0) return -1;
    final=f;
    if(type==0){                       /* stored */
      b.bitbuf=0; b.bitcnt=0;
      if(b.pos+4>b.len) return -1;
      unsigned n=(unsigned)in[b.pos] | ((unsigned)in[b.pos+1]<<8);
      unsigned nn=(unsigned)in[b.pos+2] | ((unsigned)in[b.pos+3]<<8);
      b.pos+=4;
      if((n^0xFFFFu)!=nn || b.pos+n>b.len || op+n>cap) return -1;
      memcpy(out+op,in+b.pos,n); op+=n; b.pos+=n;
      continue;
    }
    if(type==1){                       /* fixed Huffman */
      unsigned char l[288]; int i;
      for(i=0;i<144;i++) l[i]=8;
      for(;i<256;i++) l[i]=9;
      for(;i<280;i++) l[i]=7;
      for(;i<288;i++) l[i]=8;
      huff_build(&lit,l,288);
      unsigned char d[30]; for(i=0;i<30;i++) d[i]=5;
      huff_build(&dst,d,30);
    }else if(type==2){                /* dynamic Huffman */
      int hlen=br_bits(&b,5), hdist=br_bits(&b,5), hclen=br_bits(&b,4);
      if(hlen<0||hdist<0||hclen<0) return -1;
      hlen+=257; hdist+=1; hclen+=4;
      if(hlen>286||hdist>30) return -1;
      unsigned char cl[19]; memset(cl,0,sizeof cl);
      int i;
      for(i=0;i<hclen;i++){
        int v=br_bits(&b,3); if(v<0) return -1;
        cl[cl_order[i]]=(unsigned char)v;
      }
      Huff cdh; huff_build(&cdh,cl,19);
      unsigned char *lens=(unsigned char*)malloc((size_t)hlen+hdist);
      if(!lens) return -1;
      int k=0, bad=0;
      while(k<hlen+hdist){
        int s=huff_sym(&b,&cdh);
        if(s<0){ bad=1; break; }
        if(s<16){ lens[k++]=(unsigned char)s; continue; }
        int rep=0; unsigned char v=0;
        if(s==16){
          if(k==0){ bad=1; break; }
          int e=br_bits(&b,2); if(e<0){ bad=1; break; }
          rep=3+e; v=lens[k-1];
        }else if(s==17){
          int e=br_bits(&b,3); if(e<0){ bad=1; break; }
          rep=3+e; v=0;
        }else{
          int e=br_bits(&b,7); if(e<0){ bad=1; break; }
          rep=11+e; v=0;
        }
        if(k+rep>hlen+hdist){ bad=1; break; }
        while(rep--) lens[k++]=v;
      }
      if(bad){ free(lens); return -1; }
      huff_build(&lit,lens,hlen);
      huff_build(&dst,lens+hlen,hdist);
      free(lens);
    }else return -1;

    for(;;){
      int s=huff_sym(&b,&lit);
      if(s<0){ return -1; }
      if(s<256){
        if(op>=cap) return -1;
        out[op++]=(unsigned char)s;
      }else if(s==256){
        break;
      }else{
        s-=257;
        if(s>=29) return -1;
        int eb=br_bits(&b,lext[s]); if(eb<0) return -1;
        int length=lbase[s]+eb;
        int ds=huff_sym(&b,&dst);
        if(ds<0||ds>=30) return -1;
        int db=br_bits(&b,dext[ds]); if(db<0) return -1;
        unsigned dist=(unsigned)(dbase[ds]+db);
        if(dist==0||dist>op) return -1;
        if(op+(size_t)length>cap) return -1;
        while(length--) out[op]=out[op-dist], op++;
      }
    }
  }while(!final);
  return (long)op;
}

/* ==================== PNG ==================== */
static unsigned be32(const unsigned char *p){
  return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|(unsigned)p[3];
}

void tex_free(TexImg *im){
  if(!im) return;
  free(im->px); im->px=NULL; im->w=im->h=0;
}

static int paeth(int a,int b,int c){
  int p=a+b-c, pa=abs(p-a), pb=abs(p-b), pc=abs(p-c);
  if(pa<=pb && pa<=pc) return a;
  if(pb<=pc) return b;
  return c;
}

/* 解码 8-bit 非隔行 PNG → RGBA8（行主序，从上到下）。失败返回 0。 */
int tex_decode(const unsigned char *data,size_t len,TexImg *out){
  if(!out) return 0;
  out->px=NULL; out->w=out->h=0;
  static const unsigned char sig[8]={137,80,78,71,13,10,26,10};
  if(!data||len<8||memcmp(data,sig,8)!=0) return 0;

  size_t pos=8;
  unsigned w=0,h=0; int depth=0,ctype=-1,interlace=0;
  unsigned char palette[256*3]; int palN=0;
  unsigned char trns[256]; int trnsN=0;
  unsigned char *idat=NULL; size_t idatLen=0, idatCap=0;
  int haveIHDR=0, ok=0;

  while(pos+12<=len && !ok){
    unsigned clen=be32(data+pos);
    const unsigned char *ct=data+pos+4;
    if((size_t)clen>len-pos-12) goto done;      /* 声明长度越界 */
    const unsigned char *cd=data+pos+8;
    if(memcmp(ct,"IHDR",4)==0 && clen>=13){
      w=be32(cd); h=be32(cd+4);
      depth=cd[8]; ctype=cd[9]; interlace=cd[12];
      /* IHDR 13 字节：w(4) h(4) depth(1) ctype(1) compression(1) filter(1) interlace(1) */
      if(cd[10]!=0||cd[11]!=0) goto done;       /* 压缩 / 滤波方法必须为 0 */
      haveIHDR=1;
    }else if(memcmp(ct,"PLTE",4)==0){
      if(clen%3||clen>768) goto done;
      memcpy(palette,cd,clen); palN=(int)(clen/3);
    }else if(memcmp(ct,"tRNS",4)==0){
      if(clen>256) goto done;
      memcpy(trns,cd,clen); trnsN=(int)clen;
    }else if(memcmp(ct,"IDAT",4)==0){
      if(idatLen+clen>idatCap){
        size_t nc=idatCap? idatCap*2 : 16384;
        while(nc<idatLen+clen) nc*=2;
        if(nc>64u*1024*1024) goto done;
        unsigned char *nb=(unsigned char*)realloc(idat,nc);
        if(!nb) goto done;
        idat=nb; idatCap=nc;
      }
      memcpy(idat+idatLen,cd,clen); idatLen+=clen;
    }else if(memcmp(ct,"IEND",4)==0){
      ok=1;
    }
    pos += 12+(size_t)clen;
  }

  if(!haveIHDR||!idat||!w||!h||depth!=8||interlace!=0||ctype<0||ctype>6||ctype==1||ctype==5)
    goto done;
  if(w>4096||h>4096||(unsigned long long)w*h>2000000ull) goto done;
  int bpp;
  switch(ctype){
  case 0: bpp=1; break;
  case 2: bpp=3; break;
  case 3: bpp=1; break;
  case 4: bpp=2; break;
  default:bpp=4; break;
  }
  if(ctype==3 && palN==0) goto done;
  size_t rowlen=(size_t)w*bpp;
  size_t expected=(rowlen+1)*(size_t)h;
  if(expected>48u*1024*1024) goto done;

  /* IDAT 拼起来是 zlib 流（RFC1950），不是裸 deflate：
     前面 2 字节 CMF/FLG、末尾 4 字节 adler32 都要剥掉。 */
  if(idatLen<6) goto done;
  unsigned cmf=idat[0], flg=idat[1];
  if((cmf&0x0F)!=8 || (cmf>>4)>7) goto done;          /* CM=deflate, CINFO<=7 */
  if((((unsigned)cmf<<8)|flg)%31u!=0) goto done;       /* FCHECK */
  if(flg&0x20) goto done;                               /* 不支持预设字典 */
  const unsigned char *zdata=idat+2;
  size_t zlen=idatLen-6;

  unsigned char *raw=(unsigned char*)malloc(expected);
  if(!raw) goto done;
  long got=inflate_raw(zdata,zlen,raw,expected);
  if(got!=(long)expected){ free(raw); goto done; }

  unsigned char *rgba=(unsigned char*)malloc((size_t)w*h*4);
  if(!rgba){ free(raw); goto done; }
  unsigned char *prev=(unsigned char*)calloc(rowlen,1);
  unsigned char *cur=(unsigned char*)malloc(rowlen);
  if(!prev||!cur){ free(prev); free(cur); free(raw); free(rgba); goto done; }

  unsigned y;
  for(y=0;y<h;y++){
    const unsigned char *src=raw+y*(rowlen+1);
    int ft=src[0];
    memcpy(cur,src+1,rowlen);
    size_t x;
    for(x=0;x<rowlen;x++){
      int a=x>=(size_t)bpp?cur[x-bpp]:0;
      int bb=y>0?prev[x]:0;
      int c=(y>0&&x>=(size_t)bpp)?prev[x-bpp]:0;
      int v=(int)cur[x];
      switch(ft){
      case 0: break;
      case 1: v+=a; break;
      case 2: v+=bb; break;
      case 3: v+=(a+bb)/2; break;
      case 4: v+=paeth(a,bb,c); break;
      default: free(prev); free(cur); free(raw); free(rgba); goto done;
      }
      cur[x]=(unsigned char)v;
    }
    unsigned char *d=rgba+(size_t)y*w*4;
    for(x=0;x<w;x++){
      unsigned char r=0,g=0,bl=0,al=255;
      switch(ctype){
      case 0:{
        unsigned char gray=cur[x];
        r=g=bl=gray;
        if(trnsN>=2 && ((unsigned)cur[x]==((unsigned)trns[0]<<8|trns[1]))) al=0;
        break;}
      case 2:{
        r=cur[x*3]; g=cur[x*3+1]; bl=cur[x*3+2];
        if(trnsN>=6 && r==trns[1] && g==trns[3] && bl==trns[5]) al=0;
        break;}
      case 3:{
        int idx=cur[x];
        if(idx<palN){ r=palette[idx*3]; g=palette[idx*3+1]; bl=palette[idx*3+2];
                      al=idx<trnsN?trns[idx]:255; }
        else al=0;
        break;}
      case 4:{
        unsigned char gray=cur[x*2]; r=g=bl=gray; al=cur[x*2+1]; break;}
      default:{
        r=cur[x*4]; g=cur[x*4+1]; bl=cur[x*4+2]; al=cur[x*4+3]; break;}
      }
      d[x*4]=r; d[x*4+1]=g; d[x*4+2]=bl; d[x*4+3]=al;
    }
    unsigned char *t=prev; prev=cur; cur=t;   /* 交换，prev 是本行重建结果 */
  }
  free(prev); free(cur); free(raw);
  out->px=rgba; out->w=(int)w; out->h=(int)h;
  free(idat); return 1;

done:
  free(idat);
  return 0;
}

/* ==================== 贴到锚点上 ====================
   本体空间：w/h 是要显示的宽高（和 circle 的 r 同一套单位），
   会跟着 ES 缩放、跟着锚点 rot 旋转、跟着果冻形变走位。 */
void tex_draw(const TexImg *im,double ox,double oy,double rot,double scl,
              double w,double h,double alpha){
  if(!im||!im->px||im->w<=0||im->h<=0||w<=0||h<=0||alpha<=0||scl<=0||ES<=0) return;
  double cs=cos(rot), sn=sin(rot);
  double wx=cx0+ox*ES, wy=cy0+oy*ES;
  double rp=hypot(w,h)*0.5*scl*ES+2.0;
  int x0=(int)floor(wx-rp), x1=(int)ceil(wx+rp);
  int y0=(int)floor(wy-rp), y1=(int)ceil(wy+rp);
  if(x0<0) x0=0;
  if(y0<0) y0=0;
  if(x1>g_win) x1=g_win;
  if(y1>g_win) y1=g_win;
  if(x0>=x1||y0>=y1) return;
  double inv=1.0/(ES*scl);
  double iw=im->w, ih=im->h;

  for(int y=y0;y<y1;y++){
    double dy=(y+0.5)-wy;
    for(int x=x0;x<x1;x++){
      double dx=(x+0.5)-wx;
      double lx=( dx*cs + dy*sn)*inv;
      double ly=(-dx*sn + dy*cs)*inv;
      if(lx<-w*0.5||lx>w*0.5||ly<-h*0.5||ly>h*0.5) continue;
      double u=(lx/w+0.5)*iw-0.5, v=(ly/h+0.5)*ih-0.5;
      /* 双线性，四邻按 alpha 一起插值，边缘 clamp */
      int x0i=(int)floor(u), y0i=(int)floor(v);
      double fx=u-x0i, fy=v-y0i;
      int xa=x0i<0?0:(x0i>=im->w?im->w-1:x0i);
      int xb=x0i+1<0?0:(x0i+1>=im->w?im->w-1:x0i+1);
      int ya=y0i<0?0:(y0i>=im->h?im->h-1:y0i);
      int yb=y0i+1<0?0:(y0i+1>=im->h?im->h-1:y0i+1);
      const unsigned char *p00=im->px+((size_t)ya*im->w+xa)*4;
      const unsigned char *p10=im->px+((size_t)ya*im->w+xb)*4;
      const unsigned char *p01=im->px+((size_t)yb*im->w+xa)*4;
      const unsigned char *p11=im->px+((size_t)yb*im->w+xb)*4;
      double w00=(1-fx)*(1-fy), w10=fx*(1-fy), w01=(1-fx)*fy, w11=fx*fy;
      double r=p00[0]*w00+p10[0]*w10+p01[0]*w01+p11[0]*w11;
      double g=p00[1]*w00+p10[1]*w10+p01[1]*w01+p11[1]*w11;
      double bb=p00[2]*w00+p10[2]*w10+p01[2]*w01+p11[2]*w11;
      double aa=(p00[3]*w00+p10[3]*w10+p01[3]*w01+p11[3]*w11)/255.0*alpha;
      if(aa<=0) continue;
      buf_blend(x,y,(unsigned char)r,(unsigned char)g,(unsigned char)bb,aa);
    }
  }
}
