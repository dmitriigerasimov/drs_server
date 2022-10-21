/*
 * commands.c
 *
 *  Created on: 20 дек. 2019 г.
 *      Author: Denis
 */
#include <unistd.h>
#include "commands.h"
#include "mem_ops.h"

/**
 * double *shiftDAC		сдвиги с фронтпанели;
 * float *DAC_gain		массив из ini
 * float *DAC_offset	массив из ini
 */
void setShiftAllDac(double *shiftDAC,float *DAC_gain,float *DAC_offset)//fix
{
	int i;
	unsigned short shiftDACValues[4];
	for(i=0;i<4;i++)
	{
		shiftDACValues[i]=(shiftDAC[i]*DAC_gain[i]+DAC_offset[i]);
		//printf("shiftDAC[%d]=%f\tshiftDACValues[%d]=%d\n",i,shiftDAC[i],i,shiftDACValues[i]);
	}
	setAllDAC(shiftDACValues);
	setDAC(1);
}

/**
 * unsigned short *shiftValue 		масиив сдивгов для ЦАП
 */
void setAllDAC(unsigned short *shiftValue)//fix
{
	int j;
	for(j=0;j<2;j++)
	{
		setDACInputShift(j,((shiftValue[j*2]<<16)&0xFFFF0000)|shiftValue[j*2+1]);
	}
}

void setNumPages(unsigned int num)//fix
{
	write_reg(19,num);
	write_reg(20,num);
	usleep(100);
}

void setSizeSamples(unsigned int num)
{
	write_reg(0x19,num);
	usleep(100);
}

void pageMode(unsigned int on)//fix
{
    //TODO implement it!
//	write_reg(16,(on&1)<<1);
//	usleep(300);
}

/*void externalStart(unsigned int on)//fix
{
	write_reg(14,(on&1)<<2);
	write_reg(15,(on&1)<<2);
	usleep(100);
}*/

void StartDRSs(unsigned int on)//fix
{
	write_reg(14,(on&1)<<2);
	write_reg(15,(on&1)<<2);
	usleep(100);
}

void setMode(unsigned int mode)//unsigned int calibr=0x1, pachka=0<<1;
{
//0 - soft start
//1 - external start
//2 - page mode
//3 - amplitude calibrate
//4 - time calibrate
//5 - calibration chan 
	write_reg(16, mode);
	usleep(100);
	write_reg(14, 1);//initDRS1
	write_reg(15, 1);//initDRS2
	usleep(100);
}

void setTimeCalibrate(unsigned int enable)//fix
{
	write_reg(16,(enable&1)<<2);
	usleep(100);
}

void setDACInputShift(unsigned int addrShift,unsigned int value)//fix
{
	write_reg(0x8+addrShift,value);
	usleep(100);
}

void setDAC(unsigned int onAH)//fix
{
	//unsigned int onAH=1,dacSelect=2;
	write_reg(0x07,(onAH&1));
	usleep(200);
}

void setWorkDRS(unsigned int enable)//fix
{
	write_reg(14,(enable&1)<<1);
	write_reg(15,(enable&1)<<1);
	usleep(100);
}

void softStartRecorder(unsigned int enable)
{
//	write_reg(14, enable&1);
//	write_reg(15, enable&1);
//	usleep(100);
}

void flagEndRead(unsigned int enable)
{
	write_reg(21,enable&1);
	write_reg(22,enable&1);
	usleep(100);
}

unsigned int readEnWrite ()
{
	usleep(100);
	return read_reg(49)&read_reg(50);
}

unsigned int readExternalStatus (unsigned int stat)
{
	usleep(100);
	return read_reg(0x30|(0xf & stat));
}
