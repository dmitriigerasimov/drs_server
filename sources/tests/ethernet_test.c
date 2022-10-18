/*
 * ethernet_test.c
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 */
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/fs.h>
#include <sys/socket.h>  /* sockaddr_in */
#include <netinet/in.h>  /* AF_INET, etc. */
#include <ctype.h>
#include <sys/select.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "ethernet_test.h"
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
unsigned short mas[0x4000];                   
unsigned short tmas[MAX_SLOW_ADC_SIZE_IN_BYTE+8192*2];// + shift size
unsigned short tmasFast[SIZE_FAST];
unsigned char buf_in[SIZE_BUF_IN];
float gainAndOfsetSlow[16];
unsigned int *cmd_data=(uint32_t *)&buf_in[0];//(unsigned int *)
struct sockaddr_in name, cname;
struct timeval tv = { 0, POLLDELAY };
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

//int fp;

int s, ns;
long int nbyte=0;
unsigned int clen;
char disconnect;
fd_set rfds;

int StartServer(void)
{
   int yes=1;
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("client: socket");
      return(-1);
   }
   //fcntl(s, F_SETFL,O_NONBLOCK);
   if (setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
   {
       perror("setsockopt");
       exit(1);
   }
   name.sin_family = AF_INET;
   name.sin_addr.s_addr = INADDR_ANY;
   name.sin_port = htons(PORT);
   if (bind(s, (struct sockaddr *)&name, sizeof(name)) < 0)
   {
       perror("SERVER_NAME: bind");
       exit(1);
   }
   if (listen(s, 1) < 0)
   {
       perror(" SERVER_NAME: listen");
       exit(1);
   }
   //FD_ZERO(&rfds);
//   FD_SET(s, &rfds);
   printf("%s: runnig\n\r", SERVER_NAME), fflush(stdout);
   return 0;
}

int send_ini_file(void)
{
 char buffer[500000];
 ssize_t read_return;
 int filefd;
    filefd = open(inipath, O_RDONLY);
    if (filefd == -1) {
        perror("open");
        return -1;
    }
       while (1)
       {
           read_return = read(filefd, buffer, BUFSIZ);
           if (read_return == 0)
               break;
           if (read_return == -1) {
               perror("read");
               return -2;
           }
           if (send(ns, buffer, read_return, MSG_NOSIGNAL) == -1) {
               perror("error!");
               return -3;
           }
       }
  return 0;
}

int main(void)
{
    int ret;
	unsigned int adr, val, t,shift[1024];
	long int nbyte;
	double ddata[8192];
	double *shiftDAC, *calibLvl;
	parameter_t ini;
	coefficients COEFF;
	//ZeroMemory
	memset(&ini, 0, sizeof(parameter_t));
//	save_to_ini(inipath, &ini);
	load_from_ini(inipath, &ini);
printf("ROFS=%u\n\r", ini.fastadc.ROFS);
printf("OFS=%u\n\r", ini.fastadc.OFS);
//	test_data=(uint32_t *)malloc(SIZE_BUF_OUT);
//	printf("server: memset "),fflush(stdout);
//	memset(test_data, 1, SIZE_BUF_OUT);
//	printf("ok!\n\r"),fflush(stdout);
//	if (init_mem()!=0) { printf("init error"),fflush(stdout); return -1; }
   if (!init_mem())
   {
        memw(0xFFC25080,0x3fff); //инициализация работы с SDRAM
//	memw(0xFF200060,SDRAM_64_BASE>>4);
//	memw(0xFF200064,0x8000);
        write_reg(24, SDRAM_BASE_FAST>>4);
//        write_reg(25, 0x4000);
        write_reg(25, 0x400);
        write_reg(27, SDRAM_BASE_SLOW>>4);
//        write_reg(28, 0x10000);
        write_reg(28, MAX_SLOW_ADC_CHAN_SIZE);
//	memw(0xFF200040, 1); //start DMA
    initialization(&ini);
	StartServer();
	COEFF.indicator=0;
	while(1)
	{
        	clen=sizeof(struct sockaddr_in);
        	if ((ns = accept(s, (struct sockaddr *)&cname, &clen)) < 0) { perror("server: accept"); exit(1);}
       		printf("s=%d ns=%d\n\r",s ,ns);
        	printf("server: client %s connected\n\r",(char *)inet_ntoa(cname.sin_addr)), fflush(stdout);
        	disconnect=0;
    		while(!disconnect)
    		{
    				nbyte = recv(ns, buf_in, SIZE_BUF_IN, 0);
		   	   	switch(nbyte)
	    			{
	      	    			case -1:
 	     	      				perror("server: recv()");
   		     	    			break;
  		          		case  0:
    	        				disconnect = 1;
		            			printf("disconnect=1\n\r"), fflush(stdout);
    		        			break;
   		        		default:
                                switch(cmd_data[0])
                                {
                                  case 1: //write size 12
    		        		              adr=cmd_data[1];
    		        		              val=cmd_data[2];
                                          write_reg(adr, val);
    		        		              printf("write: adr=0x%08x, val=0x%08x\n\r", adr, val), fflush(stdout);
                                          val=0;
    		        		              ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

                                  case 2: //read reg  size 8
    		        		              adr=cmd_data[1];
                                          val=read_reg(adr);
    		        		              printf("read: adr=0x%08x, val=0x%08x\n\r", adr, val), fflush(stdout);
    		        		              ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

                                  case 3: //read data  size 4
    		        		              printf("read data: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n\r", ((unsigned short *)data_map)[0], ((unsigned short *)data_map)[1], ((unsigned short *)data_map)[2], ((unsigned short *)data_map)[3], ((unsigned short *)data_map)[4], ((unsigned short *)data_map)[5], ((unsigned short *)data_map)[6], ((unsigned short *)data_map)[7]), fflush(stdout);
    		        		              memcpy(mas, data_map, 0x4000);
    		        		              ret=send(ns, mas, 0x4000, MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

                                  case 4: //read data old
    		        		              printf("read shifts: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n\r", ((unsigned short *)data_map_shift)[0], ((unsigned short *)data_map_shift)[1], ((unsigned short *)data_map_shift)[2], ((unsigned short *)data_map_shift)[3], ((unsigned short *)data_map_shift)[4], ((unsigned short *)data_map_shift)[5], ((unsigned short *)data_map_shift)[6], ((unsigned short *)data_map_shift)[7]), fflush(stdout);
    		        		              ret=send(ns, data_map_shift, 0x8000, MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

                                  case 5: //read data slow
                                	  	  printf("read slow ADC data: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n\r", ((unsigned short *)data_map_slow)[0], ((unsigned short *)data_map_slow)[1], ((unsigned short *)data_map_slow)[2], ((unsigned short *)data_map_slow)[3], ((unsigned short *)data_map_slow)[4], ((unsigned short *)data_map_slow)[5], ((unsigned short *)data_map_slow)[6], ((unsigned short *)data_map_slow)[7]), fflush(stdout);
                                	  	  memcpy(tmas, data_map_slow, MAX_SLOW_ADC_SIZE_IN_BYTE);
                                	  	  ret=send(ns, tmas, MAX_SLOW_ADC_SIZE_IN_BYTE, MSG_NOSIGNAL); //MSG_DONTWAIT
                                	      break;

    		        		      case 6: //read ini
    		        		              printf("read ini\n\r"), fflush(stdout);
    		        		              printf("sizeof(ini)=%d\n\r", sizeof(ini)), fflush(stdout);
    		        		              ret=send(ns, &ini, sizeof(ini), MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

    		        		      case 7: //write ini
    		        		              printf("write ini\n\r"), fflush(stdout);
    		        		              memcpy(&ini, cmd_data, sizeof(ini));
                                          val=0;
    		        		              ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

    		        		      case 8: //read file ini
    		        		              printf("read ini file\n\r"), fflush(stdout);
    		        		              printf("result=%d\n\r", send_ini_file()), fflush(stdout);
    		        		              break;

    		        		      case 9: //write file ini
    		        		              printf("write ini file\n\r"), fflush(stdout);
    		        		              memcpy(&ini, cmd_data, sizeof(ini));
                                          val=0;
    		        		              ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

                                 case 0xA://read N page data
                                	 	  printf("%u\n",cmd_data[1]);
    		        		              for (t=0; t<cmd_data[1];t++)
    		        		              {
    		        		               //printf("read page %d data: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n\r", t, ((unsigned short *)data_map)[t*8192+0], ((unsigned short *)data_map)[t*8192+1], ((unsigned short *)data_map)[t*8192+2], ((unsigned short *)data_map)[t*8192+3], ((unsigned short *)data_map)[t*8192+4], ((unsigned short *)data_map)[t*8192+5], ((unsigned short *)data_map)[t*8192+6], ((unsigned short *)data_map)[t*8192+7]), fflush(stdout);
    		        		               //printf("read shift: %4lu\n\r", ((unsigned long *)data_map_shift)[t]), fflush(stdout);
    		        		               memcpy(&tmas[t*8192], &(((unsigned short *)data_map)[t*8192]), 0x4000);
    		        		              }
    		        		              if (t>0) t--;
    		        		              printf("read page %d data: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n\r", t, ((unsigned short *)data_map)[t*8192+0], ((unsigned short *)data_map)[t*8192+1], ((unsigned short *)data_map)[t*8192+2], ((unsigned short *)data_map)[t*8192+3], ((unsigned short *)data_map)[t*8192+4], ((unsigned short *)data_map)[t*8192+5], ((unsigned short *)data_map)[t*8192+6], ((unsigned short *)data_map)[t*8192+7]), fflush(stdout);
    		        		              printf("read shift: %4lu\n\r", ((unsigned long *)data_map_shift)[t]), fflush(stdout);
    		        		              ret=send(ns, tmas, cmd_data[1]*(0x4000), MSG_NOSIGNAL); //MSG_DONTWAIT
    		        		              break;

                                 case 0xB:
                                	 /*
                                 	 calibrate

                                 	 cmd_data[1] - ключи калибровки, 1 бит амплитудная,2 локальная временная,3 глобальная временная
                                 	 cmd_data[2] - N c клиента, количество проходов аплитудной калибровки для каждого уровня цапов
                                 	 cmd_data[3] - Min N с клиента, минимальное число набора статистики для каждой ячейки в локальной калибровке
                                 	 cmd_data[4] - numCylce, число проходов в глобальной колибровке
                                 	 cmd_data[5] - количество уровней у амплитудной калибровки count,
                                 	 	 	 	   для каждого будет N(из cmd_data[2]) проходов,
                                 	 	 	 	   при нуле будут выполняться два прохода для уровней BegServ и EndServ(о них ниже),
                                 	 	 	 	   при не нулевом значении, между  BegServ и EndServ будут включены count дополнительных уровней цапов для амплитудной калибровки
                                 	 с cmd_data[6] идет массив даблов, первые 2 элемента BegServ и EndServ
                                  	  	  	  	  остальные 8 сдвиги цапов
                                  	 */
                                	 calibLvl=(double *)(&cmd_data[6]);
                                	 shiftDAC=&calibLvl[2];
                                	 val=0;
                                	 for(t=0;t<6;t++)
                                	 {
                                		 printf("%u\n",cmd_data[t]);
                                	 }
                                	 for(t=0;t<2;t++)
                                	 {
                                	    printf("%f\n",calibLvl[t]);
                                	 }
                                	 for(t=0;t<8;t++)
                                	 {
                                	    printf("%f\n",shiftDAC[t]);
                                	 }
                            		 setNumPages(1);
                            		 setSizeSamples(1024);
                                	 if((cmd_data[1]&1)==1)
                                	 {
                                		 printf("start amplitude calibrate\n");
                                		 val=calibrateAmplitude(&COEFF,calibLvl,cmd_data[2],shiftDAC,&ini,cmd_data[5]+2)&1;
                                		 if(val==1)
                                		 {
                                			 printf("end amplitude calibrate\n");
                                		 }else{
                                			 printf("amplitude calibrate error\n");
                                		 }
                                	 }
                                	 if((cmd_data[1]&2)==2)
                                	 {
                                		 printf("start time calibrate\n");
                                		 val|=(timeCalibration(cmd_data[3],&COEFF,&ini)<<1)&2;
                                		 if((val&2)==2){
                                			 printf("end time calibrate\n");
                                		 }else{
                                			 printf("time calibrate error\n");
                                		 }
                            			                                	 }
                                	 if((cmd_data[1]&4)==4)
                                	 {
                                		 printf("start global time calibrate\n");
                                		 val|=(globalTimeCalibration(cmd_data[4],&COEFF,&ini)<<2)&4;
                                		 if((val&4)==4){
                                			 printf("end global time calibrate\n");
                                		 }else{
                                			 printf("global time calibrate error\n");
                                		 }
                                	 }
                                	 ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                	 break;

                                 case 0xC:
                                	 /*
                                 	 read
                                 	 cmd_data[1]- число страниц для чтения
                                 	 cmd_data[2]- флаг для soft start
                                 	 cmd_data[3]- флаг для передачи массивов X
                                 	 */
                                	 val=4*(1+((cmd_data[3]&24)!=0));
                                	 printf("Npage %u\n",cmd_data[1]);
                                	 printf("soft start %u\n",cmd_data[2]);
                                	 if((cmd_data[2]&1)==1)//soft start
                                	 {
                                		 setNumPages(1);
                                		 setSizeSamples(1024);
                                		 onceGet(tmasFast,shift,0,0);
                                	 }else{
                                		 readNPages(tmasFast,shift,cmd_data[1],val*8192);
                                	 }
                                	 for(t=0;t<cmd_data[1];t++)
                                	 {
                                		 doColibrateCurgr(&tmasFast[t*8192*val],ddata,shift[t],&COEFF,1024,8,cmd_data[3],&ini);
                                		 printf("tmasFast[%d]=%f shift[%d]=%d\n",t*8192,ddata[0],t,shift[t]);
                                		 memcpy(&tmasFast[t*8192*val],ddata,sizeof(double)*8192);
                                		 if(val==8)
                                		 {
                                			 getXArray(ddata,shift[t],&COEFF,cmd_data[3]);
                                			 memcpy(&tmasFast[(t*val+4)*8192],ddata,sizeof(double)*8192);
                                		 }
                                	 }
                                	 printf("buferSize=%u\n",2*8192*cmd_data[1]*val);
                                	 ret=send(ns, tmasFast,2*8192*cmd_data[1]*val, MSG_NOSIGNAL);
                                	 break;
                                 case 0xD:
                                	 //set shift DAC
                                	 shiftDAC=(double *)(&cmd_data[1]);
                                	 for(t=0;t<8;t++)
                                	 {
                                	    printf("%f\n",shiftDAC[t]);
                                	 }
                                	 setShiftAllDac(shiftDAC,ini.fastadc.dac_gains, ini.fastadc.dac_offsets);
                                	 setDAC(1,DAC1);
                                	 val=1;
                                	 ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                	 break;

                                 case 0xE:
                                	 printf("ff %d\n",cmd_data[0]);
                                	 val=15;
                                	 ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                	 break;
                                 case 0xF:
                                	 //получение массива сдвигов ячеек
                                	 //memcpy(tmasFast,shift,sizeof(unsigned int)*cmd_data[1]);
                                	 ret=send(ns, &shift,sizeof(unsigned int)*cmd_data[1], MSG_NOSIGNAL);
                                	 break;
                                 case 0x10:
                                	 //старт
                                 	 //cmd_data[1]- mode
                                 	 //		00 - page mode and read (N page)
                                 	 //		01 - soft start and read (one page)
                                 	 //		02 - only read
                                 	 //		04 - external start and read (one page)
                                 	 //cmd_data[2] - pages
                                	 switch(cmd_data[1])
                                	 {
                                	 case 0: //00 - page mode and read (N page)
                                		 	 setNumPages(cmd_data[2]);
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000025, cmd_data[2]), fflush(stdout);
                                    		 setSizeSamples(cmd_data[2]*1024);
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000019, cmd_data[2]*1024), fflush(stdout);
                                         	 write_reg(0x00000015, 1<<1);  //page mode
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000015, 1<<1), fflush(stdout);
                                         	 usleep(100);
                                         	 write_reg(0x00000027, 1);  //external enable
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000027, 1), fflush(stdout);
                                		 	 break;
                                	 case 1: //01 - soft start and read (one page)
                                         	 printf("01 - soft start and read (one page)\n");
                                         	 write_reg(0x00000015, 0);  //page mode disable
                                         	 write_reg(0x00000027, 0);  //external disable
                                         	 usleep(100);
  /*                                       	 write_reg(0x00000025, 1);   	//pages
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000025, 1), fflush(stdout);
                                         	 write_reg(0x00000019, 1024);  //size samples
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000019, 1024), fflush(stdout);
                                         	 //write_reg(0x00000015, 1<<1);  //page mode
                                         	 usleep(100);
                                         	 write_reg(0x00000010, 1);  //soft start
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000010, 1), fflush(stdout);
*/
                                		 	 break;
                                	 case 2: //02 - only read
                                		 	 break;
                                	 case 4: //04 - external start and read (one page)
                                 		 	 printf("04 - external start and read");
                                    		 setNumPages(1);
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000025, 1), fflush(stdout);
                                    		 setSizeSamples(1024);
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000019, 1024), fflush(stdout);
                                         	 write_reg(0x00000015, 1<<1);  //page mode
                                         	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000015, 1<<1), fflush(stdout);
                                         	 usleep(100);
                                 		 	 write_reg(0x00000027, 1);  //external enable
                                 		 	 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000027, 1), fflush(stdout);
                                		 	 break;
                                	 default:
                                		 break;
                                	 }
                                 	 val=0;
                                	 ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                	 break;
                                 case 0x11: //start slow ADC
                                	 switch(cmd_data[1])
                                	 {
                                	 	 case 0:
                                	 		 write_reg(0x0000001A, cmd_data[2]);   	//start slow ADC
                                	 		 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x0000001A, cmd_data[2]), fflush(stdout);
                                	 		 val=0;
                                	 		 ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                	 		 break;
                                	 	 case 1:
                                	 		 write_reg(0x00000028, cmd_data[2]);   	//extern start slow ADC
                                	 		 printf("write: adr=0x%08x, val=0x%08x\n\r", 0x00000028, cmd_data[2]), fflush(stdout);
                                	 		 val=0;
                                	 		 ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                	 		 break;
                                	 	 default:
                                	 		 break;
                                	 }
                                	 break;
                                 case 0x12: //read status and page
                                     cmd_data[0]=read_reg(0x00000031);//fast complite
                                     cmd_data[1]=read_reg(0x0000003D);//num of page complite
                                     cmd_data[2]=read_reg(0x00000039);//slow complite
                                     //printf("fast complite: %d, num page complite: %d, slow complite %d\n", cmd_data[0], cmd_data[1], cmd_data[2]),fflush(stdout);
                                	 ret=send(ns, cmd_data,3*sizeof(cmd_data[0]), MSG_NOSIGNAL);
                                	 break;
                                 case 0x13:
                                	 /*
                                 	 read
                                 	 cmd_data[1]- число страниц для чтения
                                 	 cmd_data[2]- флаг для soft start
                                 	 cmd_data[3]- флаг для передачи массивов X
                                 	 */
                                	 val=4*(1+((cmd_data[3]&24)!=0));
                                	 printf("Npage %u\n",cmd_data[1]);
                                	 printf("soft start %u\n",cmd_data[2]);
                                	 if((cmd_data[2]&1)==1)//soft start
                                	 {
//                                		 readNPage(tmasFast,shift,0,1);
                                		 setNumPages(1);
                                		 setSizeSamples(1024);
                                		 onceGet(tmasFast,shift,0,0);
                                	 }else{
                                		 readNPages(tmasFast,shift,cmd_data[1],val*8192);
                                	 }
                                	 for(t=0;t<cmd_data[1];t++)
                                	 {
                                		 doColibrateCurgr(&tmasFast[t*8192*val],ddata,shift[t],&COEFF,1024,8,cmd_data[3],&ini);
                                		 printf("tmasFast[%d]=%f shift[%d]=%d\n",t*8192,ddata[0],t,shift[t]);
                                		 memcpy(&tmasFast[t*8192*val],ddata,sizeof(double)*8192);
                                		 if(val==8)
                                		 {
                                			 getXArray(ddata,shift[t],&COEFF,cmd_data[3]);
                                			 memcpy(&tmasFast[(t*val+4)*8192],ddata,sizeof(double)*8192);
                                		 }
                                	 }
                                	 printf("buferSize=%u\n",2*8192*cmd_data[1]*val);
                                	 ret=send(ns, tmasFast,2*8192*cmd_data[1]*val, MSG_NOSIGNAL);
                                	 break;
                                 case 0x14://read ofset gain slow adc
                                	 memcpy(gainAndOfsetSlow,ini.slowadc.adc_gains,sizeof(float)*8);
                                	 memcpy(&gainAndOfsetSlow[8],ini.slowadc.adc_offsets,sizeof(float)*8);
                                	 for(t=0;t<16;t++)
                                	 {
                                		 printf("gaf[%d]=%f\n",t,gainAndOfsetSlow[t]),fflush(stdout);
                                	 }
                                	 ret=send(ns, gainAndOfsetSlow, sizeof(float)*16, MSG_NOSIGNAL); //MSG_DONTWAIT
                                	 break;
                                 case 0x15: //read data slow
                                  	 val=cmd_data[1];
                                  	 t=cmd_data[2];
                                  	 printf("size=%d\tposition=%d\n",val,t ), fflush(stdout);
                     		         memcpy(tmas, data_map_slow+t, val);
                     		         ret=send(ns, tmas, val, MSG_NOSIGNAL); //MSG_DONTWAIT
                     		         break;
                                 case 0x16: //read data slow end
                                	 write_reg(0x0000001D, 1);
                                	 val=0;
                                     ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
                                     printf("end read slow adc\n"),fflush(stdout);
                                     break;
                                 default:
                                	 break;
    		        		    }
    		        		    if ( ret < 0 )
		        		        {
    		        		      printf("error!\n\r"), fflush(stdout);
    		        		    }
				}

		}
		close(ns);
	//	FD_CLR(ns, &rfds);

		printf("%s: client disconnected...\n\r", SERVER_NAME), fflush(stdout);
		printf("Power OFF\n\r"), fflush(stdout);
        write_reg(0x00000023, 0);   	//power off
		printf("%s: listen...\n\r", SERVER_NAME), fflush(stdout);
	}// } //
   }
//   free(test_data);
   deinit_mem();
//   close(fp);
   return 0;
}


