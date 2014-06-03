#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simulator.h"
#include "field.h"
#include "drawer.h"
#include "models.h"
#include "mpiFDTD3D_upml.h"
#include "mpiFDTD3D.h"
#include <sys/time.h>

/*
static double complex* (*getDataX)() = NULL;
static double complex* (*getDataY)() = NULL;
static double complex* (*getDataZ)() = NULL;
*/

static void (*updateMethod)() = NULL;
static void (* finishMethod)() = NULL;
static void (* initMethod)() = NULL;
static void (* resetMethod)() = NULL;

static double complex* (*getDrawData)() = NULL;
static double* (* getEpsMethod )() = NULL;

static struct timeval timer1, timer2;

static void setFDTD3D()
{
  updateMethod = fdtd3D_getUpdate();
  initMethod   = fdtd3D_getInit();
  finishMethod = fdtd3D_getFinish();
  getEpsMethod = fdtd3D_getEps;
  getDrawData  = fdtd3D_getEz;
}

static void setMPI3D()
{
  updateMethod = mpi_fdtd3D_upml_getUpdate();
  initMethod   = mpi_fdtd3D_upml_getInit();
  finishMethod = mpi_fdtd3D_upml_getFinish();
  getEpsMethod = mpi_fdtd3D_upml_getEps;
  getDrawData  = mpi_fdtd3D_upml_getEy;
}

static void setSolver(enum SOLVER solver)
{
  switch(solver){
  case MPI_FDTD_3D:
    setMPI3D();
    break;
  case FDTD_3D:
  default:
    setFDTD3D();
    break;
  }
}

void simulator_calc(){
  (*updateMethod)();
  
  field_nextStep();   //時間を一つ進める

  int time = (int)field_getTime();
  if( time%100 == 0)
    printf( "time = %d\n", (int)time );
}

void simulator_init(FieldInfo field_info, enum MODEL model, enum SOLVER solver){
  //横幅(nm), 縦幅(nm), 1セルのサイズ(nm), pmlレイヤの数, 波長(nm), 計算ステップ
  field_init(field_info);

  /*NO_MODEL. MIE_CYLINDER, SHELF(todo), NONSHELF(todo) */
  setModel(model);     //次にこれ,モデル(散乱体)を定義

  setSolver(solver);      //Solverの設定

  (*initMethod)();   //Solverの初期化, EPS, Coeffの設定

   /*POINT_LIGHT_IN_CENTER: 中心に点光源   SCATTER: 散乱波*/
  //field_setDefaultIncidence(SCATTER); //入射波の設定

  gettimeofday(&timer1, NULL);
}

void simulator_finish(){
  printf("finish\n");
  gettimeofday(&timer2,NULL);
  printf("time = %lf \n", timer2.tv_sec-timer1.tv_sec+(timer2.tv_usec-timer1.tv_usec)*1e-6);

  (*finishMethod)(); //メモリの解放等  
}

double complex* simulator_getDrawingData(void){
  return (* getDrawData)();
}

bool simulator_isFinish(void)
{
  return field_isFinish();
}

double* simulator_getEps()
{
  return (*getEpsMethod)();
}
  
