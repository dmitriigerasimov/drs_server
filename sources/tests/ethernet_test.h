/*
 * ethernet_test.h
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 */
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include "memtest.h"
#ifndef ETHERNET_TEST_H_
#define ETHERNET_TEST_H_

#pragma pack (push, 1)
typedef struct 
{
//  char firmware_path[256];
  uint32_t ROFS;
  uint32_t OFS;
  float dac_gains[8];
  float dac_offsets[8];
  float adc_offsets[8];
  float adc_gains[8];
 
} fastadc_parameter_t;
typedef struct 
{
  float adc_offsets[8];
  float adc_gains[8];
} slowadc_parameter_t;

typedef struct 
{
  fastadc_parameter_t fastadc;
  slowadc_parameter_t slowadc;
} parameter_t;
#pragma pack ( pop)

#endif /* ETHERNET_TEST_H_ */
