#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "memtest.h"

#define PAGE_SIZE 8192
#define HPS2FPGA_BRIDGE_BASE	0xC0000000 //данные быстрых АЦП
#define LWHPS2FPGA_BRIDGE_BASE	0xff200000 //управление

int fd;
volatile unsigned int *control_mem;
void *control_map;
void *data_map, *data_map_shift, *data_map_slow;

#define MAP_SIZE           (4096)
#define MAP_MASK           (MAP_SIZE-1)

/**
 * @brief Записать в память
 * @param byte_addr
 * @param data
 */
void memw(off_t byte_addr, uint32_t data)
{
    void *map_page_addr, *map_byte_addr;
    map_page_addr = mmap( 0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, byte_addr & ~MAP_MASK );
    if( map_page_addr == MAP_FAILED ) {
        perror( "mmap" );
        return;
    }

    map_byte_addr = map_page_addr + (byte_addr & MAP_MASK);
    *( ( uint32_t *) map_byte_addr ) = data;
    if( munmap( map_page_addr, MAP_SIZE ) ) {
        perror( "munmap" );
        return;
    }
}

/**
 * @brief Считать из памяти
 * @param byte_addr
 * @return
 */
uint32_t memr(off_t byte_addr)
{
    void *map_page_addr, *map_byte_addr;
    uint32_t data;
    map_page_addr = mmap( 0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, byte_addr & ~MAP_MASK );
    if( map_page_addr == MAP_FAILED ) {
        perror( "mmap" );
        return 0;
    }
    map_byte_addr = map_page_addr + (byte_addr & MAP_MASK);
    data = *( ( uint32_t *) map_byte_addr );
    printf( "data = 0x%08x\n", data );
    if( munmap( map_page_addr, MAP_SIZE ) ) {
        perror( "munmap" );
        return 0;
    }
    return (data);
}

/**
 * @brief init_mem
 * @return
 */
int init_mem(void)
{
	int ret = EXIT_FAILURE;
//	unsigned char value;
	off_t control_base = LWHPS2FPGA_BRIDGE_BASE;
	off_t data_base = SDRAM_BASE_FAST;
	off_t data_base_slow = SDRAM_BASE_SLOW;
	off_t data_shift = HPS2FPGA_BRIDGE_BASE;

	/* open the memory device file */
	fd = open("/dev/mem", O_RDWR|O_SYNC);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	/* map the LWHPS2FPGA bridge into process memory */
	control_map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, control_base);
	if (control_map == MAP_FAILED) {
		perror("mmap");
		goto cleanup;
	}

	data_map = mmap(NULL, SDRAM_SPAN_FAST, PROT_READ | PROT_WRITE, MAP_SHARED, fd, data_base);
	if (data_map == MAP_FAILED) {
		perror("mmap");
		goto cleanup;
	}
	ret = 0;
	
	data_map_slow = mmap(NULL, SDRAM_SPAN_SLOW, PROT_READ | PROT_WRITE, MAP_SHARED, fd, data_base_slow);
	if (data_map == MAP_FAILED) {
		perror("mmap");
		goto cleanup;
	}
	ret = 0;

	data_map_shift = mmap(NULL, 0x8000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, data_shift);
	if (data_map == MAP_FAILED) {
		perror("mmap");
		goto cleanup;
	}
	ret = 0;

cleanup:
	return ret;
}

void write_reg(unsigned int reg_adr, unsigned int reg_data)
{
	/* get the delay_ctrl peripheral's base address */
	control_mem = (unsigned int *) (control_map + reg_adr*4);
//    printf("write: adr=0x%08x, val=0x%08x\n\r", reg_adr, reg_data), fflush(stdout);

	/* write the value */
	*control_mem = reg_data;
}

unsigned int read_reg(unsigned int reg_adr)
{
	unsigned int reg_data;
	control_mem = (unsigned int *) (control_map + reg_adr*4);
    reg_data=(unsigned int)control_mem[0];
//    printf("read: adr=0x%08x, val=0x%08x\n\r", reg_adr, reg_data), fflush(stdout);
	return(reg_data);
}

void deinit_mem(void)
{
	if (munmap(control_map, PAGE_SIZE) < 0)
	{
		perror("munmap");
		goto cleanup;
	}
	if (munmap(data_map, SDRAM_SPAN_FAST) < 0)
	{
		perror("munmap");
		goto cleanup;
	}
	if (munmap(data_map_slow, SDRAM_SPAN_SLOW) < 0)
	{
		perror("munmap");
		goto cleanup;
	}
	if (munmap(data_map_shift, 0x8000) < 0)
	{
		perror("munmap");
		goto cleanup;
	}
cleanup:
	close(fd);
}
