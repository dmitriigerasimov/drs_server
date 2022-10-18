/*
 * memtest.h
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 */
#include <sys/stat.h>
#include <stdint.h>
#ifndef MEMTEST_H_
#define MEMTEST_H_

//SDRAM 32000000-35ffffff //64 MB
//#define SDRAM_64_BASE 0x32000000 //838860800
//#define SDRAM_64_SPAN 0x3FFFFFF
#define SDRAM_BASE_FAST 0x20000000 //536870912
#define SDRAM_SPAN_FAST 0x0FFFFFFF //268435455

//#define SDRAM_BASE_SLOW 0x36000000 //905969664
//#define SDRAM_SPAN_SLOW 0x7FFFFFF

#define SDRAM_BASE_SLOW 0x30000000
#define SDRAM_SPAN_SLOW 0x0FFFFFFF

extern void *data_map, *data_map_shift, *data_map_slow;

extern void memw(off_t byte_addr, uint32_t data);
extern uint32_t memr(off_t byte_addr);
extern int init_mem(void);
extern void write_reg(unsigned int reg_adr, unsigned int reg_data);
extern unsigned int read_reg(unsigned int reg_adr);
extern void deinit_mem(void);

#endif /* MEMTEST_H_ */
