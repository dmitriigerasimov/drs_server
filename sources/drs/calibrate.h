/*
 * calibrate.h
 *
 *  Created on: 19 сент. 2019 г.
 *      Author: Denis
 *  Refactored: 2022 by Dmitry Gerasimov
 */
#pragma once

#include "drs.h"


#define GeneratorFrequency 50 //MHz
#define periodLength 38.912 //4.9152/2.4*19 || periodLength-длинна в отсчетах одного периода 4.9152 √√ц-частота ацп, 2400/19-ћ√ц частота синуса
#define maxPeriodsCount 28//26,315789473684210526315789473684- максимальное количество периодов в 1024 отсчетах->27, +1 дл€ нул€;
//#define freqDRS 4915200.0/1000000000.0
//extern const double freqDRS[];
extern int current_freq;

typedef struct
{
	double b9[2*1024];
	double k9[2*1024];
	double b[4*4096];
	double k[4*4096];
	double kTime[2*1024];
	double chanB[4];
	double chanK[4];
	double deltaTimeRef[2*1024];
	unsigned int indicator;
	unsigned int splash[4];
} coefficients;

void getXArray(double*x, unsigned int *shift,coefficients *coef,unsigned int key);
void xToReal(double*x);
void findSplash(double*Y,unsigned int *shift,coefficients *coef,unsigned int lvl);
void removeSplash(double*Y,unsigned int *shift,coefficients *coef);
void applyGlobalTimeCalibration(double *x,double *xMas, unsigned int *shift, coefficients *coef);
unsigned int globalTimeCalibration(unsigned int numCycle, coefficients *coef,parameter_t *prm);
void globalTimeCalibr(double *x, double *y,unsigned int shift,double *sumDeltaRef, double *stat);
void applyTimeCalibration(double *bufferX, coefficients *coef,unsigned int * shift);
unsigned int timeCalibration(unsigned int minN, coefficients *coef,parameter_t *prm);
double getMinDeltas(double*buffer,double *sumDeltaRef,double *stat,unsigned int *shift);
void doColibrateCurgr(unsigned short *buffer,double *dBuf,unsigned int *shift,coefficients *coef,unsigned int chanalLength,unsigned int chanalCount,unsigned int key,parameter_t *prm);
unsigned int calibrateAmplitude(coefficients *coef,double *calibLvl,unsigned int N, double *shiftDAC,parameter_t *prm, unsigned int count);
unsigned int calibrate_fin(double*calibLvl,unsigned int N,float *DAC_gain,float *DAC_offset,coefficients *coef,unsigned int count);
void getCoefficients(double *acc,coefficients *coef,double* calibLvl,int count,double *average);
unsigned int chanalsCalibration(double*calibLvl,float *DAC_gain,float *DAC_offset,coefficients *coef, unsigned int count,parameter_t *prm);
void collectStatisticsB(double *acc,unsigned short *buff,unsigned int *shift,unsigned int *stat);
void calcCoeffB(double *acc,unsigned int *statistic,double *average);


