/*
 * commands.h
 *
 *  Created on: 20 дек. 2019 г.
 *      Author: Denis
 */

#pragma once

enum DAC_SELECTOR{
	DAC0,
	DAC1,
	DAC2,
	DAC3
};
void setShiftAllDac(double *shiftDAC,float *DAC_gain,float *DAC_offset);
void setAllDAC(unsigned short *shiftValue);
void setNumPages(unsigned int num);
void setSizeSamples(unsigned int num);
void pageMode(unsigned int on);
void setMode(unsigned int mode);
void externalStart(unsigned int on);
void setTimeCalibrate(unsigned int enable);
void setDACInputShift(unsigned int addrShift,unsigned int value);
void setDAC(unsigned int onAH);
void setWorkDRS(unsigned int enable);
void softStartRecorder(unsigned int enable);
void flagEndRead(unsigned int enable);
unsigned int readEnWrite ();
unsigned int readExternalStatus (unsigned int stat);
