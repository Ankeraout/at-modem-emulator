#ifndef __FCS_H__
#define __FCS_H__

#include <stddef.h>
#include <stdint.h>

uint16_t pppfcs16(const uint8_t *buffer, size_t bufferSize);

#endif
