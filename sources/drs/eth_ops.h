/*
 * ethernet_test.h
 *
 *  Created on: 24 дек. 2014 г.
 *      Author: Zubarev
 */
#pragma once

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

#include "mem_ops.h"
#include <dap_common.h>

typedef struct 
{
//  char firmware_path[256];
  uint32_t ROFS;
  uint32_t OFS;
  float dac_gains[8];
  float dac_offsets[8];
  float adc_offsets[8];
  float adc_gains[8];
 
} DAP_ALIGN_PACKED fastadc_parameter_t;

typedef struct 
{
  float adc_offsets[8];
  float adc_gains[8];
} DAP_ALIGN_PACKED slowadc_parameter_t;

typedef struct 
{
  fastadc_parameter_t fastadc;
  slowadc_parameter_t slowadc;
} DAP_ALIGN_PACKED parameter_t;

