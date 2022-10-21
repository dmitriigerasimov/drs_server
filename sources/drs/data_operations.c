/*
 * data_operations.c
 *
 *  Created on: 20 ���. 2019 �.
 *      Author: Denis
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "data_operations.h"
#include "commands.h"
#include "mem_ops.h"

/**
 * ���
 * double*yArr			������ Y
 * double *xArr 		������ X
 * unsigned int length  ������ ��������
 * double *b			����� ���������� ��� ������ b
 * double *k			����� ���������� ��� ������ k
 */
void getCoefLine(double*yArr,double *xArr,unsigned int length,double *b,double *k)
{
	double x=0,x2=0,xy=0,y=0;
	int i=0;
	for(i=0;i<length;i++)
	{
		x+=xArr[i];
		y+=yArr[i];
		xy+=xArr[i]*yArr[i];
		x2+=xArr[i]*xArr[i];
	}
	*k=(length*xy-x*y)/(length*x2-x*x);
	*b=(y-*k*x)/length;
	//printf("k=%f\tb=%f\n",*k,*b);
}
void arrayOperation(double *array,int length,void (*operation)(double *num)){
	int i;
	for(i=0;i<length;i++){
		(*operation)(&array[i]);
	}
}
void pageModeStart(unsigned int num)
{
	unsigned int flag=0;
	setNumPages(num);
//	setSizeSamples(num*1024);//Peter fix
//	pageMode(1);
	flag=readEnWrite()>>31;
	while(flag==0)
	{
		printf("flag=%d\n",flag);
		readExternalStatus(0x3d);
		flag=readEnWrite()>>31;
	}
	flagEndRead(1);
	pageMode(0);
	readExternalStatus(0x3d);
}

/**
 * unsigned short *buffer		����� ������;
 * unsigned int *shift			��������� ��  unsigned int � ������� ��������� �����;
 * unsigned short extStart		��������� �������� �������;
 */
unsigned int onceGet(unsigned short *buffer,unsigned int *shift,unsigned int calibrate,unsigned int extStart,unsigned int drsnum)
{
	unsigned int flag=0,i=0,end=0;
	if(extStart==0)
	{
		setWorkDRS(1);
		usleep(100);
		if(calibrate==0)
		{
			softStartRecorder(1);
		}
	}
	flag=readEnWrite()>>31;
	while(flag==0 && end==0)
	{
		flag=readEnWrite()>>31;
		i++;
		if(extStart==0)
		{
			if(i>100)
			{
				end=1;
			}
		}else{
			//if(ext_start==0){end=1;)
		}
		//readExternalStatus(0xc); //Peter fix
	}
	if(flag==1){
        readNPage(&buffer[0],&shift[0],0,drsnum);
//		readNPage(&buffer[8192],&shift[1024],0,1,drsnum);
	}
	flagEndRead(1);
	return flag;
}

/**
 * unsigned short *buffer		����� ������;
 * unsigned int *shift			��������� ��  unsigned int � ������� ��������� �����;
 * unsigned short extStart		��������� �������� �������;
 */
unsigned int onceGet1024(unsigned short *buffer,unsigned int *shift,unsigned int calibrate,unsigned int extStart,unsigned int drsnum)
{
	unsigned int flag=0,i=0,end=0;
	if(extStart==0)
	{
		setWorkDRS(1);
		usleep(100);
		if(calibrate==0)
		{
			softStartRecorder(1);
		}
	}
	flag=readEnWrite()>>31;
	while(flag==0 && end==0)
	{
		flag=readEnWrite()>>31;
		i++;
		if(extStart==0)
		{
			if(i>100)
			{
				end=1;
			}
		}else{
			//if(ext_start==0){end=1;)
		}
		//readExternalStatus(0xc); //Peter fix
	}
	if(flag==1){
        readNPage(&buffer[0],&shift[0],0,drsnum);
//		readNPage(&buffer[8192],&shift[1024],0,1,drsnum);
	}
	flagEndRead(1);
	return flag;
}

/**
 * unsigned int drsnum		����� drs ��� ����������� ������
 * return 					������ ������;
 */
unsigned int getShiftIndex(unsigned int drsnum)//npage
{
    unsigned short tmpshift;
    if (drsnum==0)
     tmpshift=((unsigned long *)data_map_shift_drs1)[0]&1023;
    else
     tmpshift=((unsigned long *)data_map_shift_drs2)[0]&1023;
	return tmpshift;
}

/**
 * double *average					������ �� �������� ���������� �������;
 * double *data						������ ������
 * unsigned int chanalLength		������ ������;
 * unsigned int chanalCount			����� �������;
 */
void getAverage(double *average,double *data,unsigned int chanalLength,unsigned int chanalCount)
{
	int i,j;
	for(i=0;i<chanalCount;i++)
	{
		average[i]=0;
		for(j=0;j<chanalLength;j++)
		{
			average[i]+=data[j*chanalCount+i];
		}
		average[i]/=chanalLength;
		//printf("average[%d]=%f\n",i,average[i]);
	}
}

/**
 * double *average					������ �� �������� ���������� �������;
 * unsigned short *data				������ ������
 * unsigned int chanalLength		������ ������;
 * unsigned int chanalCount		����� �������;
 */
void getAverageInt(double *average,unsigned short *data,unsigned int chanalLength,unsigned int chanalCount)
{
	int i,j;
	for(i=0;i<chanalCount;i++)
	{
		average[i]=0;
		for(j=0;j<chanalLength;j++)
		{
			average[i]+=data[j*chanalCount+i];
		}
		average[i]/=chanalLength;
		//printf("average[%d]=%f\n",i,average[i]);
	}
}

 double absf(double value)
 {
	 if(value<0)
	 {
		 return value*-1;
	 }else{
		 return value;
	 }
 }

 void fillArray(unsigned char *array,unsigned char *value,unsigned int arrayLength,unsigned int sizeOfValue)
 {
	 int i;
	 for(i=0;i<arrayLength;i++)
	 {
		 memcpy(&array[i*sizeOfValue],value,sizeOfValue);
	 }
 }

 /**
  * unsigned short *buffer		������ ������
  * unsigned int *shift			�����;
  * unsigned int pageCount		����� ��������;
  */
 void readNPage(unsigned short *buffer,unsigned int shift,unsigned int numPage, unsigned int drsnum)
 {
    if (drsnum==0)
  	 memcpy(buffer, &(((unsigned short *)data_map_drs1)[numPage*16384]), 0x8000);
  	else
  	 memcpy(buffer, &(((unsigned short *)data_map_drs2)[numPage*16384]), 0x8000);
 	shift=getShiftIndex(drsnum);
 }

 /**
  * unsigned short *buffer		������ ������
  * unsigned int *shift			�����;
  * unsigned int pageCount		����� ��������;
  */
 void writeNPage(unsigned short *buffer,unsigned int numPage, unsigned int drsnum)
 {
    if (drsnum==0)
 	 memcpy(&(((unsigned short *)data_map_drs1)[numPage*16384]),buffer, 0x8000);
 	else
 	 memcpy(&(((unsigned short *)data_map_drs2)[numPage*16384]),buffer, 0x8000);
 }
/**
 * unsigned short *buffer		������ ������
 * unsigned int *shift			������;
 * unsigned int pageCount		����� �������;
 */
void readNPages(unsigned short *buffer,unsigned int *shift,unsigned int pageCount, unsigned int step, unsigned int drsnum)
{
	 int t;
	 for (t=0; t<pageCount;t++)
	 {
         readNPage(&buffer[t*step],&shift[t],t, drsnum);
	 }
}



