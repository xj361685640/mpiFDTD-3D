#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

static int putBmpHeader(FILE *s, int x, int y, int c);
static int fputc4LowHigh(unsigned long d, FILE *s);
static int fputc2LowHigh(unsigned short d, FILE *s);


#ifdef USE_OPENGL
#include "drawer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include "function.h"
#include "myComplex.h"
#include "field.h"

#ifdef MAC_OS
#include <GLUT/glut.h>
#endif
#ifndef MAC_OS
#include <GL/glut.h>
#endif


typedef struct {
 GLfloat r,g,b;
}colorf;

#define TEX_SIZE 256
#define TEX_NX 512
#define TEX_NY 512

static const int vertexNum = 4; //頂点数
static colorf texColor[TEX_NX][TEX_NY]={};
static GLuint ver_buf, tex_buf;
static GLuint texId;
//static GLuint texIds[3];

static double (*colorMode)( dcomplex );
static void colorTransform(double p, colorf *c);


static GLfloat vertices[] =
  {-1.0f, -1.0f, 0.0f,
   +1.0f, -1.0f, 0.0f, 
   +1.0f, +1.0f, 0.0f, 
   -1.0f, +1.0f, 0.0f};

static GLfloat texCoords[] =
  { 0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f };

//--------------------prototype--------------------//
void drawer_paintImage(int l, int b,int r, int t, int wid,int hei, dcomplex*);
void drawer_paintModel(int l, int b,int r, int t, int wid,int hei, double *);
void drawer_draw();
//--------------------------------------//


//--------------public Method-----------------//
void (*drawer_getDraw(void))(void)
{
  return drawer_draw;
}
//--------------------------------------//

void drawer_init(enum COLOR_MODE cm)
{
  if(cm == CREAL)
    colorMode = creal;
  else
    colorMode = cnorm;//cabs;

  glGenBuffers(1, &ver_buf);

  glBindBuffer(GL_ARRAY_BUFFER, ver_buf);
  glBufferData(GL_ARRAY_BUFFER, 3*vertexNum*sizeof(GLfloat), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &tex_buf);
  glBindBuffer(GL_ARRAY_BUFFER, tex_buf);
  glBufferData(GL_ARRAY_BUFFER, 2*vertexNum*sizeof(GLfloat), texCoords, GL_STATIC_DRAW);

  //
  glEnable( GL_TEXTURE_2D );
  glGenTextures( 1, &texId );

  glActiveTexture( GL_TEXTURE0 );

  glBindTexture( GL_TEXTURE_2D, texId );
  glTexImage2D( GL_TEXTURE_2D, 0, 3, TEX_NX, TEX_NY, 0, GL_RGB, GL_FLOAT, texColor);

    //min, maxフィルタ
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );    //min, maxフィルター

}

void drawer_draw()
{  
  glBindBuffer( GL_ARRAY_BUFFER, ver_buf);
  glVertexPointer( 3, GL_FLOAT, 0, 0);

  glBindBuffer( GL_ARRAY_BUFFER, tex_buf);
  glTexCoordPointer(2, GL_FLOAT, 0, 0);
  
  glBindTexture( GL_TEXTURE_2D, texId );

  glTexImage2D( GL_TEXTURE_2D, 0, 3, TEX_NX, TEX_NY, 0, GL_RGB, GL_FLOAT, texColor);  

  glDrawArrays( GL_POLYGON, 0, vertexNum);  
}

static double _dbilinear(double *p, double x, double y, int index, int toNextX, int toNextY)
{
  int i = floor(x);
  int j = floor(y);
  double dx = x - i;
  double dy = y - j;
  return p[index]*(1.0-dx)*(1.0-dy)
    + p[index+toNextX]*dx*(1.0-dy)
    + p[index+toNextY]*(1.0-dx)*dy
    + p[index+toNextX+toNextY]*dx*dy;
}

static dcomplex _cbilinear(dcomplex *p, double x, double y, int index, int toNextX, int toNextY)
{
  int i = floor(x);
  int j = floor(y);
  double dx = x - i;
  double dy = y - j;
  return p[index]*(1.0-dx)*(1.0-dy)
    + p[index+toNextX]*dx*(1.0-dy)
    + p[index+toNextY]*(1.0-dx)*dy
    + p[index+toNextX+toNextY]*dx*dy;
}

void drawer_paintImage(int left, int bottom, int right, int top, int width, int height, dcomplex *phis)
{
  colorf c;
  dcomplex cphi;
  double ux = 1.0*(right-left)/TEX_NX;
  double uy = 1.0*(top-bottom)/TEX_NY;
  double u = max(ux,uy);
  int i,j;
  double x,y;
  SubFieldInfo_S sInfo = field_getSubFieldInfo_S();

  for(i=0,x=left; i<TEX_NX && x<right; i++, x+=u){
    for(j=0,y=bottom; j<TEX_NY && y<top; j++, y+=u){
      int index = field_subIndex( (int)x, (int)y ,sInfo.SUB_N_PZ/2);
      cphi = _cbilinear(phis,x, y, index, sInfo.SUB_N_PYZ, sInfo.SUB_N_PZ);
//      cphi = cbilinear(phis,x,y,width,height);
      colorTransform(colorMode(cphi), &c);
      texColor[i][j] = c;
    }
  }
}

/*
  3次元空間を3面図で描画する. [0]は横, [1]は縦に描画する
  phis : 3次元空間の複素数データ.
  eps  : 3次元空間のモデルデータ.
  startIndex       : テクスチャの左下に対応する3D空間のインデックス
  length[2]        : 平面の長さ
  offsetIndex[2]   : 各次元における.1次元配列で保存している為, となりのインデックスまでのオフセット量( 基本的にxはheight*depth, yはdepth, zは1 )
  u : テクスチャ配列の変化量に対するデータ配列の変化量の割合
  quadrant         : 描画位置(象限)
 */
static void paint(dcomplex *phis, double *eps, int startIndex, int lengthX, int lengthY, int offsetX, int offsetY, double u, int quadrant)
{
  colorf c;
  double amp = 1;
  int ox = (quadrant&1)*TEX_SIZE;       //原点
  int oy = ((quadrant>>1)&1)*TEX_SIZE;  //原点 
  double x, y;
  int i,j;
  for(i=0, x=0; i<TEX_SIZE && x<lengthX-1; i++, x+=u){    
    for(j=0, y=0; j<TEX_SIZE && y<lengthY-1; j++, y+=u){
      //中心には線を引く
      if(i==TEX_SIZE/2 || j==TEX_SIZE/2) {
        texColor[i+ox][j+oy].r = 1;
        texColor[i+ox][j+oy].g = 0;
        texColor[i+ox][j+oy].b = 0;
        continue;
      }

      int index = startIndex + floor(x)*offsetX + floor(y)*offsetY;
      dcomplex cphi = _cbilinear(phis, x, y, index , offsetX, offsetY);      
      colorTransform(colorMode(cphi)*amp, &c);
      texColor[i+ox][j+oy] = c;
      
      double deps = eps[index];//_dbilinear(eps, x, y, index, offsetX, offsetY);
      double n = 1-1.0/deps;

      texColor[i+ox][j+oy].r -= n;
      texColor[i+ox][j+oy].g -= n;
      texColor[i+ox][j+oy].b -= n;
    }
  }
}

void drawer_paintImage3(dcomplex *phis)
{
  FieldInfo_S sInfo = field_getFieldInfo_S();
  double ux = 1.0*sInfo.N_X/TEX_SIZE;
  double uy = 1.0*sInfo.N_Y/TEX_SIZE;
  double uz = 1.0*sInfo.N_Z/TEX_SIZE;
  double u = max(max(ux,uy),uz);
  
  colorf c;
  dcomplex cphi;

  int i,j;
  double x,y,z;

  //第一象限にxy平面を描画 xが横軸
  for(i=0,x=sInfo.N_PML; i<TEX_SIZE && x<sInfo.N_PX; i++, x+=u){
    for(j=0,y=sInfo.N_PML; j<TEX_SIZE && y<sInfo.N_PY; j++, y+=u){
      int index = field_index( (int)x, (int)y ,sInfo.N_PZ/2);
      cphi = _cbilinear(phis, x, y, index, sInfo.N_PYZ, sInfo.N_PZ);
      colorTransform(colorMode(cphi), &c);

      texColor[i+TEX_SIZE][j+TEX_SIZE] = c;
    }
  }  

  //第二象限にxz平面を描画  xが横軸
  for(i=0,x=sInfo.N_PML; i<TEX_SIZE && x<sInfo.N_PX; i++, x+=u){
    for(j=0,z=sInfo.N_PML; j<TEX_SIZE && z<sInfo.N_PZ; j++, z+=u){
      int index = field_index( (int)x, sInfo.N_PY/2, (int)z);
      cphi = _cbilinear(phis, x, z, index, sInfo.N_PYZ, 1);
      colorTransform(colorMode(cphi), &c);
      texColor[i][j+TEX_SIZE] = c;
    }
  }

  //第三象限にzy平面を描画  (zが横軸)
  for(i=0,y=sInfo.N_PML; i<TEX_SIZE && y<sInfo.N_PY; i++, y+=u){
    for(j=0,z=sInfo.N_PML; j<TEX_SIZE && z<sInfo.N_PZ; j++, z+=u){
      int index = field_subIndex( sInfo.N_PX/2, (int)y, (int)z);
      cphi = _cbilinear(phis, z, y, index, 1, sInfo.N_PZ);
      colorTransform(colorMode(cphi), &c);
      texColor[j][i] = c;
    }
  }
}

void drawer_subFieldPaintImage3(dcomplex *phis, double *eps, enum DISPLAY_PLANE plane)
{
  SubFieldInfo_S sInfo = field_getSubFieldInfo_S();

  //線の長さは,格子点の数よりも1小さい.
  double ux = (sInfo.SUB_N_X-1.0)/TEX_SIZE;
  double uy = (sInfo.SUB_N_Y-1.0)/TEX_SIZE;
  double uz = (sInfo.SUB_N_Z-1.0)/TEX_SIZE;
  double u = max(max(ux,uy),uz);
  int w;

  FieldInfo_S fInfo = field_getFieldInfo_S();
  bool XX = (sInfo.OFFSET_X < fInfo.N_PX/2) && ( fInfo.N_PX/2 <= sInfo.OFFSET_X + sInfo.SUB_N_X);
  bool YY = (sInfo.OFFSET_Y < fInfo.N_PY/2) && ( fInfo.N_PY/2 <= sInfo.OFFSET_Y + sInfo.SUB_N_Y);
  bool ZZ = (sInfo.OFFSET_Z < fInfo.N_PZ/2) && ( fInfo.N_PZ/2 <= sInfo.OFFSET_Z + sInfo.SUB_N_Z);

  int fixedX = fInfo.N_PX/2 - sInfo.OFFSET_X;
  int fixedY = fInfo.N_PY/2 - sInfo.OFFSET_Y;
  int fixedZ = fInfo.N_PZ/2 - sInfo.OFFSET_Z;

//第3象限(左下)にXY平面
  if(ZZ && (plane == XY_PLANE || plane == ALL_PLANE))
  {
    w = field_subIndex(1, 1, fixedZ);
    paint(phis,eps, w, sInfo.SUB_N_X, sInfo.SUB_N_Y, sInfo.SUB_N_PYZ, sInfo.SUB_N_PZ ,u, 0);
  }
  
  //第4象限(右下)にXZ平面
  if(YY && (plane == XZ_PLANE || plane == ALL_PLANE))
  {
    w = field_subIndex(1, fixedY, 1);  
    paint(phis, eps, w, sInfo.SUB_N_X, sInfo.SUB_N_Z, sInfo.SUB_N_PYZ, 1 ,u, plane == XZ_PLANE ? 0 : 1);
  }
  
  //第2象限(左上)にZY平面
  if(XX && (plane == ZY_PLANE || plane == ALL_PLANE))
  {
    w = field_subIndex(fixedX, 1, 1);
    paint(phis,eps, w, sInfo.SUB_N_Z, sInfo.SUB_N_Y, 1, sInfo.SUB_N_PZ, u, plane == ZY_PLANE ? 0 : 2);
  }
}

void drawer_paintModel(int left, int bottom, int right, int top, int width, int height, double *phis)
{
  double dphi;
  double ux = 1.0*(right-left)/TEX_NX;
  double uy = 1.0*(top-bottom)/TEX_NY;
  double u = max(ux,uy);
  int i,j;
  double x,y;
  
  for(i=0,x=left; i<TEX_NX && x<right; i++, x+=u)
  {
    for(j=0,y=bottom; j<TEX_NY && y<top; j++, y+=u)
    {      
      dphi = dbilinear(phis,x,y,width,height);
      double n = 1-1.0/dphi;
      texColor[i][j].r -= n;
      texColor[i][j].g -= n;
      texColor[i][j].b -= n;
    }
  }
}

void drawer_paintTest(void){
  colorf c;
  for(int i=0; i<TEX_NX; i++){
    for(int j=0; j<TEX_NY; j++){
      colorTransform(1.0*i/TEX_NX, &c);
      texColor[i][j] = c;
    }
  }
}

void drawer_finish()
{
  printf("drawer_finish not implemented\n");
}

#endif



//--------------------public Method--------------------//


//--------------------Color Trancform---------------------//
static void colorTransform(double phi, colorf *col)
{
  double range = 0.5; //波の振幅  
  double ab_phi = phi < 0 ? -phi : phi;
  double a = ab_phi < range ? (ab_phi <  range/3.0 ? 3.0/range*ab_phi : (-3.0/4.0/range*ab_phi+1.25) ) : 0.5;
  
  col->r = phi > 0 ? a:0;
  col->b = phi < 0 ? a:0;
  col->g = min(1.0, max(0.0, -3*ab_phi+2));
}

// 左上 XY平面, 左下 XZ平面, 右上 ZY平面
void drawer_outputImage(char *fileName, double *model, int width, int height, int depth, int (*indexMethod)(int, int, int))
{
  const int bpp = 24; //1ピクセルセル24ビット

  int whole_width    = width+depth;
  int whole_height   = height+depth;
  const int datasize = whole_width * whole_height * (bpp>>3);
  
  unsigned char *buf = (unsigned char*)malloc(sizeof(unsigned char)*datasize);

  //左上
  //XY平面
  colorf c;
  for(int j=0; j<height; j++)
    for(int i=0; i<width; i++)
    {
      colorTransform( 0 , &c);
      double n = 1.0-1.0/model[ (*indexMethod)(i, j, depth/2) ];
      int ind = (bpp>>3)*(j*whole_width + i);
      buf[ind]   = max(0, min(255, (c.b-n)*255));
      buf[ind+1] = max(0, min(255, (c.g-n)*255));
      buf[ind+2] = max(0, min(255, (c.r-n)*255));
    }

  //左下
  //XZ平面
  for(int k=0; k<depth; k++)
    for(int i=0; i<width; i++)
    {
      colorTransform( 0 , &c);
      double n = 1.0-1.0/model[ (*indexMethod)(i, height/2, k) ];
      int ind = (bpp>>3)*( (k+height)*whole_width + i);
      buf[ind]   = max(0, min(255, (c.b-n)*255));
      buf[ind+1] = max(0, min(255, (c.g-n)*255));
      buf[ind+2] = max(0, min(255, (c.r-n)*255));
    }

  //右上
  //ZY平面
  for(int k=0; k<depth; k++)
    for(int j=0; j<height; j++)
    {
      colorTransform( 0, &c);
      double n = 1.0-1.0/model[ (*indexMethod)(width/2, j, k) ];
      int ind = (bpp>>3)*( j*whole_width + (k+depth));
      buf[ind]   = max(0, min(255, (c.b-n)*255));
      buf[ind+1] = max(0, min(255, (c.g-n)*255));
      buf[ind+2] = max(0, min(255, (c.r-n)*255));
    }

  for(int i=0; i<whole_width; i++)
  {
    int ind = (bpp>>3)*( height*whole_width + i);
    buf[ind+0] = 255;
    buf[ind+1] = 0;
    buf[ind+2] = 0;
  }

  for(int j=0; j<whole_height; j++)
  {
    int ind = (bpp>>3)*( j*whole_width + width);
    buf[ind+0] = 255;
    buf[ind+1] = 0;
    buf[ind+2] = 0;
  }

  FILE *fp = fopen(fileName, "wb");
  if(fp==NULL)
  {
    printf("can not open file %s",fileName);
    free(buf);
    return;
  }

  if( !putBmpHeader(fp, whole_width, whole_height, bpp) ) {
    printf("can not write headers");
    fclose(fp);
    free(buf);
    return;
  }

  if( fwrite((unsigned char*)buf, sizeof(unsigned char), datasize, fp) != datasize)
  {
    printf("can not write data");
    fclose(fp);
    free(buf);
    return;
  }

  free(buf);
  fclose(fp);
  return;
}

/*
  putBmpHeader BMPヘッダ書出
	
  BMPファイルのヘッダを書き出す

  戻り値
  int:0…失敗, 1…成功
  
  引数
  FILE *s:[i] 出力ストリーム
  int x:[i] 画像Xサイズ(dot, 1〜)
  int y:[i] 画像Yサイズ(dot, 1〜)
  intc:[i] 色ビット数(bit/dot, 1 or 4 or 8 or 24)
*/
static int putBmpHeader(FILE *s, int x, int y, int c)
{
  int i;
  int color; /* 色数 */
  unsigned long int bfOffBits; /* ヘッダサイズ(byte) */

  /* 画像サイズが異常の場合,エラーでリターン */
  if (x <= 0 || y <= 0) {
    return 0;
  }

  /* 出力ストリーム異常の場合,エラーでリターン */
  if (s == NULL || ferror(s)) {
    return 0;
  }

  /* 色数を計算 */
  if (c == 24) {
    color = 0;
  } else {
    color = 1;
    for (i=1;i<=c;i++) {
      color *= 2;
    }
  }

  /* ヘッダサイズ(byte)を計算 */
  /* ヘッダサイズはビットマップファイルヘッダ(14) + ビットマップ情報ヘッダ(40) + 色数 */
  bfOffBits = 14 + 40 + 4 * color;

  /* ビットマップファイルヘッダ(計14byte)を書出 */
  /* 識別文字列 */
  fputs("BM", s);

  /* bfSize ファイルサイズ(byte) */
  fputc4LowHigh(bfOffBits + (unsigned long)x * y, s);

  /* bfReserved1 予約領域1(byte) */
  fputc2LowHigh(0, s);

  /* bfReserved2 予約領域2(byte) */
  fputc2LowHigh(0, s);

  /* bfOffBits ヘッダサイズ(byte) */
  fputc4LowHigh(bfOffBits, s);

  /* ビットマップ情報ヘッダ(計40byte) */
  /* biSize 情報サイズ(byte) */
  fputc4LowHigh(40, s);

  /* biWidth 画像Xサイズ(dot) */
  fputc4LowHigh(x, s);

  /* biHeight 画像Yサイズ(dot) */
  fputc4LowHigh(y, s);

  /* biPlanes 面数 */
  fputc2LowHigh(1, s);

  /* biBitCount 色ビット数(bit/dot) */
  fputc2LowHigh(c, s);

  /* biCompression 圧縮方式 */
  fputc4LowHigh(0, s);

  /* biSizeImage 圧縮サイズ(byte) */
  fputc4LowHigh(0, s);

  /* biXPelsPerMeter 水平解像度(dot/m) */
  fputc4LowHigh(0, s);

  /* biYPelsPerMeter 垂直解像度(dot/m) */
  fputc4LowHigh(0, s);

  /* biClrUsed 色数 */
  fputc4LowHigh(0, s);

  /* biClrImportant 重要色数 */
  fputc4LowHigh(0, s);

  /* 書出失敗ならエラーでリターン */
  if (ferror(s)) {
    return 0;
  }

  /* 成功でリターン */
  return 1;
}

/*
  fputc2LowHigh 2バイトデータ書出(下位〜上位)
	
  2バイトのデータを下位〜上位の順で書き出す

  戻り値
  int:EOF…失敗, EOF以外…成功
  引数
  unsigned short d:[i] データ
  FILE *s:[i] 出力ストリーム
*/
static int fputc2LowHigh(unsigned short d, FILE *s)
{
  putc(d & 0xFF, s);
  return putc(d >> CHAR_BIT, s);
}

/*
  fputc4LowHigh 4バイトデータ書出(下位〜上位)
	
  4バイトのデータを下位〜上位の順で書き出す

  ●戻り値
  int:EOF…失敗, EOF以外…成功
  ●引数
  unsigned long d:[i] データ
  FILE *s:[i] 出力ストリーム
*/
static int fputc4LowHigh(unsigned long d, FILE *s)
{
  putc(d & 0xFF, s);
  putc((d >> CHAR_BIT) & 0xFF, s);
  putc((d >> CHAR_BIT * 2) & 0xFF, s);
  return putc((d >> CHAR_BIT * 3) & 0xFF, s);
}


