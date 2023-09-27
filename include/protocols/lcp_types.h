#ifndef __INCLUDE_PROTOCOLS_LCP_TYPES_H__
#define __INCLUDE_PROTOCOLS_LCP_TYPES_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ts_lcpContext {
    bool ackSent;
    bool ackReceived;
    uint8_t identifier;
};

#endif
