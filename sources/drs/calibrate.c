/*
 * calibrate.c
 *
 *  Created on: 19 ����. 2019 �.
 *      Author: Denis
 */
#include "calibrate.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "commands.h"
#include "data_operations.h"
#include <string.h>
/**
 * double*x -������ ��� �������� X
 * unsigned int shift - ����� �����
 * coefficients *coef - ��������� � ��������������
 * unsigned int key - ���� ���������� ���������� 4 ���-��������� ���������, 5 ���-���������� ���������, 6 ���-���������� � ���������� ���������
 */
const double freqDRS[]= {1.024, 2.048, 3.072, 4.096, 4.915200};
int current_freq;

void getXArray(double*x, unsigned int shift,coefficients *coef,unsigned int key)
{
	double xMas[8192];
	if((key&16)!=0)
	{
		applyTimeCalibration(xMas,coef,shift);
	}
	if((key&32)!=0)
	{
		applyGlobalTimeCalibration(x,xMas,shift,coef);
	}else{
		memcpy(x,xMas,65536);
	}
	if((key&64)!=0)
	{
		arrayOperation(x,8192,xToReal);
	}

}

void xToReal(double*x)
{
	*x=(*x)/(freqDRS[current_freq]);
}

void findSplash(double*Y,unsigned int *shift,coefficients *coef,unsigned int lvl)
{
	unsigned int i = 0, j = 0;
	for(j=0;j<4;j++)
	{
		coef->splash[j] = 1024;
		for(i=1;i<1000;i++)
		{
			if(absf(Y[(i+1)*4+j]-Y[i*4+j])>lvl && absf(Y[(i-1)*4+j]-Y[i*4+j])>lvl)
			{
				printf("find splash for %d chanal in %d cell\n",j+1,(i+shift[j>>1])&1023);
				coef->splash[j] = ( i + shift[j>>1] ) & 1023;
				break;
			}
		}
		if(absf(Y[1*4+j]-Y[j])>lvl && absf(Y[1023*4+j]-Y[j])>lvl)
		{
			coef->splash[j] = 0;
			printf("find splash for %d chanal in %d cell\n",j+1,shift);
		}
//		if(absf(Y[1023*8+j]-Y[1022*8+j])>lvl && absf(Y[1023*8+j]-Y[j])>lvl)
//		{
//			coef->splash[j] = 1023;
//			printf("find splash for %d chanal in %d cell\n",j+1,shift-1);
//		}
	}
}

void removeSplash(double*Y,unsigned int *shift,coefficients *coef)
{
	unsigned int i=0,j=0;
	for(j=0;j<4;j++)
	{
		i = (coef->splash[j] - shift[j>>1] + 1023) & 1023;
		if((i > 0) && (i < 1023))
		{
			Y[i*4+j] = (Y[(i + 1) * 4 + j] + Y[(i - 1) * 4 + j]) / 2;
		}
		if(i == 0)
		{
			Y[j] = (Y [4*1 + j] + Y[4*2 + j]) / 2;
		}
		if(i == 1023)
		{
			Y[1023 * 4 + j]=(Y[j] + Y[1022 * 4 + j]) / 2;
		}
	}
}
/**
 * double *x			��������� �� ������ ��� �������� x;
 * double *xMas;				��������� applyTimeCalibration;
 * unsigned int shift		����� � ������;
 * coefficients *coef		��������� � ��������������;
 */
void applyGlobalTimeCalibration(double *x,double *xMas, unsigned int *shift, coefficients *coef)
{
	unsigned int i,j,pz;
	double tmpX;
	for(i=0;i<4;i++)
	{
		x[i]=0;
		tmpX=0;
		for(j=1;j<1024;j++)
		{
			pz=(shift[i>>1]+j)&1023;
			tmpX+=(xMas[j*4+i] - xMas[(j-1)*4+i])*coef->kTime[pz*4+i];
			x[j*4+i]=tmpX;
		}
	}
}
/**
 * unsigned int numCycle		���������� ������;
 * coefficients *coef		��������� � ��������������;
 */
unsigned int globalTimeCalibration(unsigned int numCycle, coefficients *coef,parameter_t *prm)
{
	double sumDeltaRef[2048], statistic[32768],x[32768],dBuf[32768],dn=0;
	unsigned short pageBuffer[2048],ind;
	unsigned int shift[2],k,t;
	//unsigned char i=0;
	fillArray((unsigned char*)(statistic),(unsigned char*)&dn,32768,sizeof(dn));
	fillArray((unsigned char*)(sumDeltaRef),(unsigned char*)&dn,32768,sizeof(dn));
	fillArray((unsigned char*)(x),(unsigned char*)&dn,32768,sizeof(dn));
	if((coef->indicator&3)!=3)
	{
		printf("before global timer calibration you need to do the timer calibration and amplitude calibration\n");
		return 0;
	}
	setMode(4);
//setCalibrate(1,0);
//	setTimeCalibrate(1);
	write_reg(0x22,1);
	usleep(300);
	write_reg(0xB,1);//Sync
	usleep(300);
	while(ind<numCycle)
	{
		ind++;
		if(onceGet1024(&pageBuffer[0],&shift[0],1,0,0)==0)
		{
			printf("data not read\n");
			//setCalibrate(0,0);
			//setTimeCalibrate(0);
			setMode(0);
			write_reg(0x22,0);
			usleep(300);
			write_reg(0xB,1);
			usleep(300);
			return 0;
		}
		if(onceGet1024(&pageBuffer[1024],&shift[1],1,0,1)==0)
		{
			printf("data not read\n");
//			setCalibrate(0,0);
//			setTimeCalibrate(0);
			setMode(0);
			write_reg(0x22,0);
			usleep(300);
			write_reg(0xB,1);
			usleep(300);
			return 0;
		}

		doColibrateCurgr(pageBuffer,dBuf,shift,coef,1024,4,3,prm);
		applyTimeCalibration(x,coef,shift);
		globalTimeCalibr(x,dBuf,shift,sumDeltaRef,statistic);
	}
	setCalibrate(0,0);
	setTimeCalibrate(0);
	write_reg(0x22,0);
	usleep(100);
	write_reg(0xB,1);
	usleep(100);

	for(k=0;k<4;k++)
	{
		for(t=0;t<1024;t++)
		{
			if(statistic[t*4+k]==0)
			{
				statistic[t*4+k]=1;
			}
			coef->kTime[t*4+k]=sumDeltaRef[t*4+k]/statistic[t*4+k];
		}
	}
	return 1;
}

void globalTimeCalibr(double *x, double *y,unsigned int shift,double *sumDeltaRef, double *statistic)
{
	double average[4],lastX,lastY,period[maxPeriodsCount],periodDelt[maxPeriodsCount],deltX;
	unsigned int i,j,pz,indexs[maxPeriodsCount],count=0,l;
	getAverage(average,y,1000,4);
	for(i=0;i<4;i++)
	{
		lastY=0;
		lastX=0;
		count=0;
		for(j=0;j<1001;j++)
		{
			if((average[i]>=lastY)&&(y[j*4+i]>=average[i])&&(j!=0))
			{
				deltX=x[j*4+i]-lastX;
				if(x[j*4+i]<lastX)
				{
					deltX+=1024;
				}
				period[count]=deltX/(y[j*4+i]-lastY)*(average[i]-lastY)+lastX;
				if(count>0)
				{
					periodDelt[count-1]=period[count]-period[count-1];
				}
				indexs[count]=shift+j;
				count++;
			}
			lastY=y[j*4+i];
			lastX=x[j*4+i];
		}
		for(j=1;j<count;j++)
		{
			for(l=indexs[j-1];l<indexs[j];l++)
			{
				pz=l&1023;

				sumDeltaRef[pz*4+i]+=periodLength/periodDelt[j-1];
				statistic[pz*4+i]++;
			}
		}
	}

}

void applyTimeCalibration( double*x, coefficients *coef,unsigned int *shift)
{
	unsigned int k,t,pz;
	double average[4],tmpX;
	for(k=0;k<4;k++)
	{
		average[k]=0;
		for(t=0;t<1024;t++)
		{
			average[k]+=coef->deltaTimeRef[t*4+k];
		}
		average[k]=1023/average[k];
	}
	for(k=0;k<4;k++)
	{
		tmpX=0;
		for(t=0;t<1024;t++)
		{
			pz=(shift[k>>1]+t)&1023;
			x[t*4+k]=tmpX;
			tmpX+=coef->deltaTimeRef[pz*4+k]*average[k];
		}
	}
}

/**
 * unsigned int minN		minN � �����������;
 * coefficients *coef		��������� � ��������������;
 */
unsigned int timeCalibration(unsigned int minN, coefficients *coef,parameter_t *prm)
{
	double minValue=0,sumDeltaRef[8192], statistic[8192],dBuf[8192],dn=0;
	unsigned short pageBuffer[8192];
	unsigned int shift[2],k=0,t,n=0;
	fillArray((unsigned char*)(statistic),(unsigned char*)&dn,8192,sizeof(dn));
	fillArray((unsigned char*)(sumDeltaRef),(unsigned char*)&dn,8192,sizeof(dn));
	if((coef->indicator&1)!=1){
			printf("before timer calibration you need to do the amplitude calibration\n");
			return 0;
	}
	setCalibrate(1,0);
	setTimeCalibrate(1);
	write_reg(0x22,1);
	usleep(300);
	write_reg(0xB,1);//  Sync
	usleep(300);
	while(minValue<minN)
	{
		k++;
		n++;
		//printf("min=%u\tminValue=%f\n",minN,minValue);
		if(onceGet(pageBuffer,shift,1,0)==0){
			printf("data not read\n");
			setCalibrate(0,0);
			setTimeCalibrate(0);
			write_reg(0x22,0);
			usleep(300);
			write_reg(0xB,1);
			usleep(300);
			return 0;
		}
		doColibrateCurgr(pageBuffer,dBuf,shift,coef,1024,4,3,prm);
		minValue=getMinDeltas(dBuf,sumDeltaRef,statistic,shift);

	}
	for(k=0;k<4;k++)
	{
		for(t=0;t<1024;t++)
		{
			coef->deltaTimeRef[t*4+k]=sumDeltaRef[t*4+k]/statistic[t*4+k];
		}
	}
	setCalibrate(0,0);
	setTimeCalibrate(0);
	write_reg(0x22,0);
	usleep(100);
	write_reg(0xB,1);
	usleep(100);
	coef->indicator|=2;
	return 1;
}

/**
 * double *buffer				������ � �������;
 * double *sumDeltaRef			������ �����;
 * double *statistic			������ ���������� �� �������;
 * unsigned int shift			���� � ������;
 */
double getMinDeltas(double*buffer,double *sumDeltaRef,double *statistic,unsigned int *shift)
{
	unsigned int i,j, pz, pz1;
	double average[4];
	double vmin,vmax, vtmp,min;
	getAverage(average,buffer,1000,4);
	for(i=0;i<4;i++)
	{
		if ((16383-average[i])>average[i])
		{
			vtmp=average[i]/2;
		}else{
			vtmp=(16383-average[i])/2;
		}
		vmin=average[i]-vtmp;
		vmax=average[i]+vtmp;
		for(j=0;j<1024;j++)
		{
			pz=(shift[i>>1]+j)&1023;
			pz1=(j+1)&1023;
			if ((buffer[j*4+i]<=vmax) && (buffer[pz1*4+i]>=vmin) )
			{
				sumDeltaRef[pz*4+i]+=absf(buffer[j*4+i]-buffer[pz1*4+i]);
				statistic[pz*4+i]++;
			}
		}
	}
	min=statistic[0];
	for(i=0;i<4;i++)
	{
		for(j=0;j<1024;j++)
		{
			if(statistic[j*4+i]<min)
			{
				min=statistic[j*4+i];
			}
		}
	}
	return min;
}

/*
 * ��������� ����������� ���������� � ������
 * unsigned short *buffer			������ ������;
 * double *dBuf 					������ ������ � ����������� ���������� ����������� ����������
 * unsigned int shift 				����� ���������� ����� getShiftIndex;
 * coefficients *coef				��������� � �������������;
 * unsigned int chanalLength		������ ������� ��� 1 ������;
 * unsigned int chanalCount			���������� �������
 * unsigned int key					0 ���- ���������� ���������� ��� �����, 1 ���- ������������ ����������,2 ���- ���������� �� ���������, 3 ���- ���������� � ���������� ���������
 * */
void doColibrateCurgr(unsigned short *buffer,double *dBuf,unsigned int shift,coefficients *coef,unsigned int chanalLength,unsigned int chanalCount,unsigned int key,parameter_t *prm)
{
	int j,k,koefIndex;
	double average[4];
	getAverageInt(average,buffer,1000,4);
	for(j=0;j<chanalCount;j++)
	{
		for(k=0;k<chanalLength;k++)
		{
			koefIndex=(shift+k)&1023;
			dBuf[k*chanalCount+j]=buffer[k*chanalCount+j];
			if((key&1)!=0)
			{
				dBuf[k*chanalCount+j]=(dBuf[k*chanalCount+j]-coef->b[koefIndex*4+j])/(coef->k[koefIndex*4+j]+1);
			}
			if((key&2)!=0)
			{
				dBuf[k*chanalCount+j]-=coef->chanB[j]+coef->chanK[j]*average[j];
			}
			if((key&8)!=0)
			{
				dBuf[k*chanalCount+j]=(dBuf[k*chanalCount+j]-prm->fastadc.adc_offsets[j])/prm->fastadc.adc_gains[j];
			}
		}
	}
	if((key&4)!=0)
	{
		removeSplash(dBuf,shift,coef);
	}
}

/*	����������� ������������ ��� ����������� ����������;
 * coefficients *coef 			��������� � �������������;
 * double *calibLvl 			������ [Beg,Mid,End] � ����� ������;
 * unsigned int N 				N � ����� ������;
 * double *shiftDAC				������ shiftDAC � ����� ������;
 * parameter_t *prm				ini ���������;
 * unsigned int count			���������� ������� �����
 */
unsigned int calibrateAmplitude(coefficients *coef,double *calibLvl,unsigned int N, double *shiftDAC,parameter_t *prm, unsigned int count)
{
	printf("count=%d\n",count);
	if(calibrate_fin(calibLvl,N,prm->fastadc.dac_gains, prm->fastadc.dac_offsets,coef,count)==0)
	{
		return 0;
	}else{
		printf("calibrate_fin end\n");
		if(chanalsCalibration(calibLvl,prm->fastadc.dac_gains, prm->fastadc.dac_offsets,coef,count,prm)==0)
		{
			return 0;
		}
		setShiftAllDac(shiftDAC,prm->fastadc.dac_gains, prm->fastadc.dac_offsets);
		setDAC(1);
		coef->indicator|=1;
		return 1;
	}
}

/**
 * �������� ������ �� ������� ����� �����������
 * double *calibLvl 			������ [Beg,Mid,End] � ����� ������;
 * unsigned int N				N � ����� ������;
 * float *DAC_gain				������ DAC_gain �� ini;
 * float *DAC_offset			������ DAC_offset �� ini;
 * coefficients *coef			��������� � �������������;
 */
unsigned int calibrate_fin(double*calibLvl,unsigned int N,float *DAC_gain,float *DAC_offset,coefficients *coef, unsigned int count)
{
	int i,k;
	double shiftDACValues[4],acc[count*32768],dn=0, average[4*count],dh=0,lvl=0;
	unsigned short loadData[32768];
	unsigned int shift[2],statistic[32768],intn=0;
	fillArray((unsigned char*)(&coef->b),(unsigned char*)&dn,32768,sizeof(dn));
	fillArray((unsigned char*)(&coef->k),(unsigned char*)&dn,32768,sizeof(dn));
	setCalibrate(1,0);
	setTimeCalibrate(1);
	dh=(calibLvl[1]-calibLvl[0])/(count-1);
	for(i=0;i<count;i++)
	{
		lvl=calibLvl[0]+dh*i;
		fillArray((unsigned char*)(statistic),(unsigned char*)&intn,32768,sizeof(intn));
		fillArray((unsigned char*)(&acc[i*32768]),(unsigned char*)&dn,32768,sizeof(dn));
		fillArray((unsigned char*)(shiftDACValues),(unsigned char*)&lvl,4,sizeof(dn));
		setShiftAllDac(shiftDACValues,DAC_gain,DAC_offset);
		setDAC(1);

		for(k=0;k<N;k++)
		{
			if(onceGet(&loadData[0],&shift[0],1,0,0)==0)
			{
				printf("data not read\n");
				setTimeCalibrate(0);
				setCalibrate(0,0);
				return 0;
			}
			if(onceGet(&loadData[32768],&shift[1],1,0,1)==0)
			{
				printf("data not read\n");
				setTimeCalibrate(0);
				setCalibrate(0,0);
				return 0;
			}
            else
            {
                if((shift[0]>1023)||(shift[1]>1023))
                {
			 	   printf("shift index went beyond\n");
				   return 0;
			    }
			}
			collectStatisticsB(&acc[i*32768],loadData,shift,statistic);
		}
		calcCoeffB(&acc[i*32768],statistic,&average[i*4]);
	}
	setTimeCalibrate(0);
	setCalibrate(0,0);
	getCoefficients(acc,coef,calibLvl,count,average);
	return 1;
}


/**
 * double *acc					����� �������� ��� ������;
 * coefficients *coef			��������� � �������������;
 * double *calibLvl 			������ [Beg,Mid,End] � ����� ������;
 * double* average				������� �������� �� �������;
 */
void getCoefficients(double *acc,coefficients *coef,double* calibLvl,int count,double *average)
{
	unsigned int i,j,k;
	double yArr[count],xArr[count],dh;
	dh=(calibLvl[1]-calibLvl[0])/(count-1);
	for(j=0;j<4;j++)
	{
		for(i=0;i<1024;i++)
		{
			for(k=0;k<count;k++)
			{
				xArr[k]=(0.5-calibLvl[0]-dh*k)*16384;
				yArr[k]=acc[k*8192+i*4+j]-average[k*4+j];
			}
			getCoefLine(yArr,xArr,count,&coef->b[i*4+j],&coef->k[i*4+j]);
		}
	}
	for(j=0;j<4;j++)
	{
		for(k=0;k<count;k++)
		{
			yArr[k]=average[k*4+j];
			xArr[k]=(0.5-calibLvl[0]-dh*k)*16384;
		}
	}
}

unsigned int chanalsCalibration(double*calibLvl,float *DAC_gain,float *DAC_offset,coefficients *coef, unsigned int count,parameter_t *prm)
{
	double dh,dn,shiftDACValues[4],lvl,average[4*count],dBuf[16384], xArr[count],yArr[count];
	unsigned short loadData[16384];
	unsigned int t=0,shift[2],i;
	dh=(calibLvl[1]-calibLvl[0])/(count-1);
	printf("\ncalib cahnal\n");
	for(t=0;t<count;t++)
	{
		lvl=calibLvl[0]+dh*t;
		fillArray((unsigned char*)(shiftDACValues),(unsigned char*)&lvl,4,sizeof(dn));
		setShiftAllDac(shiftDACValues,DAC_gain,DAC_offset);
		usleep(200);
		setDAC(1);
		usleep(200);
		if(onceGet(loadData,shift,1,0,0)==0)
		{
			printf("data not read\n");
			setCalibrate(0,0);
			setTimeCalibrate(0);
			return 0;
		}else if(shift[0]>1023){
			printf("shift index went beyond\n");
			return 0;
		}
		doColibrateCurgr(loadData,dBuf,shift,coef,1024,4,1,prm);
		getAverage(&average[t*4],dBuf,1000,4);
	}
	findSplash(dBuf,shift,coef,100);
	for(i=0;i<4;i++)
	{
		for(t=0;t<count;t++)
		{
			xArr[t]=average[4*t+i];
			yArr[t]=average[4*t+i]-(0.5-calibLvl[0]-dh*t)*16384;
		}
		getCoefLine(yArr,xArr,count,&coef->chanB[i],&coef->chanK[i]);
	}
	return 1;
}





/**
 * �������� ���������� ��� ������ �������
 * coefficients *acc			����� �������� ��� ������;
 * unsigned short *buff 		������ ������;
 * unsigned int shift			������ ��������� ������;
 * unsigned int *statistic		���������� ��� �����;
 */
void collectStatisticsB(double *acc,unsigned short *buff,unsigned *shift,unsigned int *statistic)
{
	unsigned int j,k,rotateIndex;
		for(j=0;j<4;j++)
		{
			for(k=0;k<1000;k++)
			{
				rotateIndex=(shift[j>>1]+k)&1023;
				acc[rotateIndex*4+j]+=buff[k*4+j];
				statistic[rotateIndex*4+j]++;
			}
		}
}

/**
 *	��������� ����������� ��� ����������� ���������� �����
 */
void calcCoeffB(double *acc,unsigned int *statistic,double *average)
{
	unsigned int k,j;
	double val=0;
	for(j=0;j<4;j++)
	{
		average[j]=0;
		for(k=0;k<1024;k++)
		{
			val=acc[k*4+j]/statistic[k*4+j];
			average[j]+=val;
			acc[k*4+j]=val;
		}
		average[j]=average[j]/1024;

	}
}


