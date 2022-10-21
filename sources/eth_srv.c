/*
 * eth_srv.c
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 *  Refactored on : 2022
 *      Author: Dmitriy Gerasimov <naeper@demlabs.net>
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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "eth_srv.h"
#include "mem_ops.h"
#include "drs.h"

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
static struct sockaddr_in name, cname;
static struct timeval tv = { 0, POLLDELAY };

//int fp;

static int s, ns;
static long int nbyte=0;
static unsigned int clen;
static char disconnect;
static fd_set rfds;

static parameter_t ini;

/**
 * @brief etc_srv_start
 * @return
 */
int eth_srv_start()
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

/**
 * @brief eth_srv_init
 * @return
 */
int eth_srv_init(void)
{
    //ZeroMemory
    memset(&ini, 0, sizeof(parameter_t));
    //	save_to_ini(inipath, &ini);
    drs_ini_load(inipath, &ini);
    printf("ROFS1=%u\n", ini.fastadc.ROFS1);
    printf("OFS1=%u\n", ini.fastadc.OFS1);
    printf("ROFS2=%u\n", ini.fastadc.ROFS2);
    printf("OFS2=%u\n", ini.fastadc.OFS2);
    printf("CLK_PHASE=%u\n", ini.fastadc.CLK_PHASE);

    //	test_data=(uint32_t *)malloc(SIZE_BUF_OUT);
    //	printf("server: memset "),fflush(stdout);
    //	memset(test_data, 1, SIZE_BUF_OUT);
    //	printf("ok!\n\r"),fflush(stdout);
    //	if (init_mem()!=0) { printf("init error"),fflush(stdout); return -1; }
    if (init_mem())
        return -1;

    memw(0xFFC25080,0x3fff); //инициализация работы с SDRAM
    write_reg(23, 0x8000000);// DRS1
    write_reg(25, 0x4000);
    write_reg(24, 0xC000000);// DRS2
    write_reg(26, 0x4000);
    write_reg(28, 0xBF40000);
    write_reg(29, 0xFF40000);


    drs_init(&ini);
    return 0;
}

/**
 * @brief eth_srv_deinit
 */
void eth_srv_deinit()
{
   //   free(test_data);
      deinit_mem();
   //   close(fp);
}

/**
 * @brief eth_srv_loop
 * @return
 */
int eth_srv_loop(void)
{
    int ret;
    unsigned int adr, val, t,shift[1024];
    long int nbyte;
    double ddata[8192];
    double *shiftDAC, *calibLvl;
    unsigned short tmpshift;

    coefficients COEFF;
    COEFF.indicator=0;
    while(1)
    {
            clen=sizeof(struct sockaddr_in);
            if ((ns = accept(s, (struct sockaddr *)&cname, &clen)) < 0) { perror("server: accept"); exit(1);}
            printf("s=%d ns=%d\n",s ,ns);
            printf("server: client %s connected\n",(char *)inet_ntoa(cname.sin_addr)), fflush(stdout);
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
                                printf("disconnect=1\n"), fflush(stdout);
                                break;
                        default:
                                switch(cmd_data[0])
                                {
                                  case 1: //write size 12
                                          adr=cmd_data[1];
                                          val=cmd_data[2];
                                          write_reg(adr, val);
                                          printf("write: adr=0x%08x, val=0x%08x\n", adr, val), fflush(stdout);
                                          val=0;
                                          ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                  case 2: //read reg  size 8
                                          adr=cmd_data[1];
                                          val=read_reg(adr);
                                          printf("read: adr=0x%08x, val=0x%08x\n", adr, val), fflush(stdout);
                                          ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                  case 3: //read data from drs1 size 16384
                                          printf("read data DRS1: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n", ((unsigned short *)data_map_drs1)[0], ((unsigned short *)data_map_drs1)[1], ((unsigned short *)data_map_drs1)[2], ((unsigned short *)data_map_drs1)[3], ((unsigned short *)data_map_drs1)[4], ((unsigned short *)data_map_drs1)[5], ((unsigned short *)data_map_drs1)[6], ((unsigned short *)data_map_drs1)[7]), fflush(stdout);
                                          memcpy(mas, data_map_drs1, 0x4000);
                                          ret=send(ns, mas, 0x4000, MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                case 103: //read data from drs2 size 16384
                                          printf("read data DRS2: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n", ((unsigned short *)data_map_drs2)[0], ((unsigned short *)data_map_drs2)[1], ((unsigned short *)data_map_drs2)[2], ((unsigned short *)data_map_drs2)[3], ((unsigned short *)data_map_drs2)[4], ((unsigned short *)data_map_drs2)[5], ((unsigned short *)data_map_drs2)[6], ((unsigned short *)data_map_drs2)[7]), fflush(stdout);
                                          memcpy(mas, data_map_drs2, 0x4000);
                                          ret=send(ns, mas, 0x4000, MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                  case 4: //read shift drs1
                                          printf("read shifts: shift DRS1 = 0x%04x, shift DRS1=0x%04x\n", ((unsigned short *)data_map_shift_drs1)[0], ((unsigned short *)data_map_shift_drs2)[0]), fflush(stdout);
                                          tmpshift=((unsigned long *)data_map_shift_drs1)[0];
                                          ret=send(ns, &tmpshift, 2, MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                 case 104: //read shift drs2
                                          printf("read shifts: shift DRS2 = 0x%04x, shift DRS1=0x%04x\n", ((unsigned short *)data_map_shift_drs1)[0], ((unsigned short *)data_map_shift_drs2)[0]), fflush(stdout);
                                          tmpshift=((unsigned long *)data_map_shift_drs2)[0];
                                          ret=send(ns, &tmpshift, 2, MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                  case 6: //read ini
                                          printf("read ini\n"), fflush(stdout);
                                          printf("sizeof(ini)=%d\n", sizeof(ini)), fflush(stdout);
                                          ret=send(ns, &ini, sizeof(ini), MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                  case 7: //write ini
                                          printf("write ini\n"), fflush(stdout);
                                          memcpy(&ini, cmd_data, sizeof(ini));
                                          val=0;
                                          ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                  case 8: //read file ini
                                          printf("read ini file\n"), fflush(stdout);
                                          printf("result=%d\n", send_ini_file()), fflush(stdout);
                                          break;

                                  case 9: //write file ini
                                          printf("write ini file\n"), fflush(stdout);
                                          memcpy(&ini, cmd_data, sizeof(ini));
                                          val=0;
                                          ret=send(ns, &val, sizeof(val), MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                 case 0xA://read N page data from DRS1
                                          printf("%u\n",cmd_data[1]);
                                          for (t=0; t<cmd_data[1];t++)
                                          {
                                           memcpy(&tmas[t*8192], &(((unsigned short *)data_map_drs1)[t*8192]), 0x4000);
                                          }
                                          if (t>0) t--;
                                          printf("read page %d data: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n", t, ((unsigned short *)data_map_drs1)[t*8192+0], ((unsigned short *)data_map_drs1)[t*8192+1], ((unsigned short *)data_map_drs1)[t*8192+2], ((unsigned short *)data_map_drs1)[t*8192+3], ((unsigned short *)data_map_drs1)[t*8192+4], ((unsigned short *)data_map_drs1)[t*8192+5], ((unsigned short *)data_map_drs1)[t*8192+6], ((unsigned short *)data_map_drs1)[t*8192+7]), fflush(stdout);
                                          printf("read shift: %4lu\n", ((unsigned long *)data_map_shift_drs1)[0]), fflush(stdout);
                                          ret=send(ns, tmas, cmd_data[1]*(0x4000), MSG_NOSIGNAL); //MSG_DONTWAIT
                                          break;

                                 case 110://read N page data from DRS2
                                          printf("%u\n",cmd_data[1]);
                                          for (t=0; t<cmd_data[1];t++)
                                          {
                                           memcpy(&tmas[t*8192], &(((unsigned short *)data_map_drs2)[t*8192]), 0x4000);
                                          }
                                          if (t>0) t--;
                                          printf("read page %d data: [0]=0x%04x,[1]=0x%04x [2]=0x%04x,[3]=0x%04x [4]=0x%04x,[5]=0x%04x [6]=0x%04x,[7]=0x%04x\n", t, ((unsigned short *)data_map_drs2)[t*8192+0], ((unsigned short *)data_map_drs2)[t*8192+1], ((unsigned short *)data_map_drs2)[t*8192+2], ((unsigned short *)data_map_drs2)[t*8192+3], ((unsigned short *)data_map_drs2)[t*8192+4], ((unsigned short *)data_map_drs2)[t*8192+5], ((unsigned short *)data_map_drs2)[t*8192+6], ((unsigned short *)data_map_drs2)[t*8192+7]), fflush(stdout);
                                          printf("read shift: %4lu\n", ((unsigned long *)data_map_shift_drs2)[0]), fflush(stdout);
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
                                     //setSizeSamples(1024);//Peter fix
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
                                         //setSizeSamples(1024);//Peter fix
                                         onceGet(&tmasFast[0],&shift[0],0,0,0);
//                                		 onceGet(&tmasFast[8192],&shift[1024],0,0,1);
                                     }else{
                                         readNPages(&tmasFast[0],shift,cmd_data[1], val*16384, 0);//8192
//                                		 readNPages(&tmasFast[??],shift,cmd_data[1], val*16384, 1);//8192

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
                                     for(t=0;t<4;t++)
                                     {
                                        printf("%f\n",shiftDAC[t]);
                                     }
                                     setShiftAllDac(shiftDAC,ini.fastadc.dac_gains, ini.fastadc.dac_offsets);
                                     setDAC(1);
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
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000025, cmd_data[2]), fflush(stdout);
                                             setSizeSamples(cmd_data[2]*1024);
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000019, cmd_data[2]*1024), fflush(stdout);
                                             write_reg(0x00000015, 1<<1);  //page mode
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000015, 1<<1), fflush(stdout);
                                             usleep(100);
                                             write_reg(0x00000027, 1);  //external enable
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000027, 1), fflush(stdout);
                                             break;
                                     case 1: //01 - soft start and read (one page)
                                             printf("01 - soft start and read (one page)\n");
                                             write_reg(0x00000015, 0);  //page mode disable
                                             write_reg(0x00000027, 0);  //external disable
                                             usleep(100);
  /*                                       	 write_reg(0x00000025, 1);   	//pages
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000025, 1), fflush(stdout);
                                             write_reg(0x00000019, 1024);  //size samples
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000019, 1024), fflush(stdout);
                                             //write_reg(0x00000015, 1<<1);  //page mode
                                             usleep(100);
                                             write_reg(0x00000010, 1);  //soft start
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000010, 1), fflush(stdout);
*/
                                             break;
                                     case 2: //02 - only read
                                             break;
                                     case 4: //04 - external start and read (one page)
                                             printf("04 - external start and read");
                                             setNumPages(1);
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000025, 1), fflush(stdout);
                                             setSizeSamples(1024);
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000019, 1024), fflush(stdout);
                                             write_reg(0x00000015, 1<<1);  //page mode
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000015, 1<<1), fflush(stdout);
                                             usleep(100);
                                             write_reg(0x00000027, 1);  //external enable
                                             printf("write: adr=0x%08x, val=0x%08x\n", 0x00000027, 1), fflush(stdout);
                                             break;
                                     default:
                                         break;
                                     }
                                     val=0;
                                     ret=send(ns, &val,sizeof(val), MSG_NOSIGNAL);
                                     break;
                                 case 0x12: //read status and page
                                     //нужно подправить!!!
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
                                         onceGet(&tmasFast[0],&shift[0],0,0,0);
//                                		 onceGet(&tmasFast[8192],&shift[1024],0,0,1);
                                     }else{
                                         readNPages(&tmasFast[0],shift,cmd_data[1], val*16384, 0);//8192
//                                		 readNPages(&tmasFast[??],shift,cmd_data[1], val*16384, 1);//8192
                                     }
                                     for(t=0;t<cmd_data[1];t++)
                                     {
                                         doColibrateCurgr(&tmasFast[t*8192*val],ddata,shift[t],&COEFF,1024,4,cmd_data[3],&ini);
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
                                 default:
                                     break;
                                }
                                if ( ret < 0 )
                                {
                                  printf("error!\n"), fflush(stdout);
                                }
                }

        }
        close(ns);
    //	FD_CLR(ns, &rfds);

        printf("%s: client disconnected...\n", SERVER_NAME), fflush(stdout);
//		printf("Power OFF\n"), fflush(stdout);
//        write_reg(0x00000023, 0);   	//power off
        printf("%s: listen...\n", SERVER_NAME), fflush(stdout);
    }// } //

//   free(test_data);
   deinit_mem();
//   close(fp);

   return 0;
}

