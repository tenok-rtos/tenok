/**
 * @file
 * The register definitions of the part the kernel is running on. Which file
 * carries them is a thing each family names differently, so the choice is made
 * here once and the rest of the kernel asks only for the part it is on.
 */
#ifndef __ARCH_DEVICE_H__
#define __ARCH_DEVICE_H__

#include "kconfig.h"

#ifdef CONFIG_PLATFORM_NUCLEO_F746ZG
#include "stm32f7xx.h"
#else
#include "stm32f4xx.h"
#endif

#endif
