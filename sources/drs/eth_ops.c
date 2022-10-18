/*
 * ethernet_test.c
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 */
#include <stdio.h>
#include <unistd.h>
#include <math.h>


#include "eth_test.h"
#include "minIni.h"
#include "calibrate.h"
#include "commands.h"
#include "data_operations.h"


#define inipath "/media/card/config.ini"
#define DEBUG
#define SERVER_NAME "test_server"
#define PORT 3000
#define MAX_SLOWBLOCK_SIZE 1024*1024
#define SIZE_BUF_IN 128
#define MAX_PAGE_COUNT 1000
#define SIZE_FAST MAX_PAGE_COUNT*1024*8*8
#define SIZE_NET_PACKAGE 1024//0x100000 // 0x8000 = 32k
#define POLLDELAY 1000 //ns
#define MAX_SLOW_ADC_CHAN_SIZE 0x800000 
#define MAX_SLOW_ADC_SIZE_IN_BYTE MAX_SLOW_ADC_CHAN_SIZE*8*2

static unsigned short mas[0x4000];
static unsigned short tmas[MAX_SLOW_ADC_SIZE_IN_BYTE+8192*2];// + shift size
static unsigned short tmasFast[SIZE_FAST];
static unsigned char buf_in[SIZE_BUF_IN];
static float gainAndOfsetSlow[16];
static unsigned int *cmd_data=(uint32_t *)&buf_in[0];//(unsigned int *)
static struct timeval tv = { 0, POLLDELAY };


void initialization(parameter_t *prm)
{
	write_reg(0xB,1);//Sync
	printf("initialization\tprm->fastadc.OFS=%u\tprm->fastadc.ROFS=%u\n",prm->fastadc.OFS,prm->fastadc.ROFS);
	usleep(3);
	write_reg(0xA,((prm->fastadc.OFS<<16)&0xffff0000)|prm->fastadc.ROFS);// OFS&ROFS
	usleep(3);
	write_reg(0x13,((18350<<16)&0xffff0000)|0);// BIAS&DSPEED
	usleep(3);
	setDAC(1,DAC0);//Set DAC ROFS, OFS
	setDAC(1,DAC5);//Set DAC BIAS, DSPEED
	write_reg(0x0,1<<3|0<<2|0<<1|0);//Start_DRS Reset_DRS Stop_DRS Soft reset
}
void load_from_ini(const char *inifile, parameter_t *prm)
{
  char sDAC_gain[]="DAC_gain_X";
  char sADC_offset[]="ADC_offset_X";
  char sADC_gain[]="ADC_gain_X";
  char sDAC_offset[]="DAC_offset_X";
  unsigned char t;
//  char IP[16];
//  long n;
  /* string reading */
//  n = ini_gets("COMMON", "host", "dummy", IP, sizearray(IP), inifile);
//  printf("Host = %s\n\r", IP);
//  n = ini_gets("COMMON", "firmware", "dummy", prm->firmware_path, sizearray(prm->firmware_path), inifile);
  prm->fastadc.ROFS 			= ini_getl("FASTADC_SETTINGS", "ROFS", 55000, inifile);
  prm->fastadc.OFS				= ini_getl("FASTADC_SETTINGS", "OFS", 28500, inifile);
  for (t=0;t<8;t++)
  {
   sDAC_gain[strlen(sDAC_gain)-1]=t+49;
   prm->fastadc.dac_gains[t]	= ini_getf("FASTADC_SETTINGS", sDAC_gain, 1.0, inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_offset[strlen(sADC_offset)-1]=t+49;
   prm->fastadc.adc_offsets[t]	= ini_getf("FASTADC_SETTINGS", sADC_offset, 0.0, inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_gain[strlen(sADC_gain)-1]=t+49;
   prm->fastadc.adc_gains[t]		= ini_getf("FASTADC_SETTINGS", sADC_gain, 1.0, inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_offset[strlen(sADC_offset)-1]=t+49;
   prm->slowadc.adc_offsets[t]	= ini_getf("SLOWADC_SETTINGS", sADC_offset, 0.0, inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_gain[strlen(sADC_gain)-1]=t+49;
   prm->slowadc.adc_gains[t]		= ini_getf("SLOWADC_SETTINGS", sADC_gain, 1.0, inifile);
  }
  for (t=0;t<8;t++)
  {
	  sDAC_offset[strlen(sDAC_offset)-1]=t+49;
	  prm->fastadc.dac_offsets[t]		= ini_getf("FASTADC_SETTINGS", sDAC_offset, 0.0, inifile);
  }
}

void save_to_ini(const char *inifile, parameter_t *prm)
{
  char sDAC_gain[]="DAC_gain_X";
  char sADC_offset[]="ADC_offset_X";
  char sADC_gain[]="ADC_gain_X";
  unsigned char t;
//  char IP[16];
//  long n;
  /* string reading */
//  n = ini_puts("COMMON", "host", IP, inifile);
//  printf("Host = %s\n\r", IP);
//  ini_puts("COMMON", "firmware", prm.firmware_path, inifile);
  ini_putl("FASTADC_SETTINGS", "ROFS", prm->fastadc.ROFS, inifile);
  ini_putl("FASTADC_SETTINGS", "OFS", prm->fastadc.OFS, inifile);
  for (t=0;t<8;t++)
  {
   sDAC_gain[strlen(sDAC_gain)-1]=t+49;
   ini_putf("FASTADC_SETTINGS", sDAC_gain, prm->fastadc.dac_gains[t], inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_offset[strlen(sADC_offset)-1]=t+49;
   ini_putf("FASTADC_SETTINGS", sADC_offset, prm->fastadc.adc_offsets[t], inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_gain[strlen(sADC_gain)-1]=t+49;
   ini_putf("FASTADC_SETTINGS", sADC_gain, prm->fastadc.adc_gains[t], inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_offset[strlen(sADC_offset)-1]=t+49;
   ini_putf("SLOWADC_SETTINGS", sADC_offset, prm->slowadc.adc_offsets[t], inifile);
  }
  for (t=0;t<8;t++)
  {
   sADC_gain[strlen(sADC_gain)-1]=t+49;
   ini_putf("SLOWADC_SETTINGS", sADC_gain, prm->slowadc.adc_gains[t], inifile);
  }
}



