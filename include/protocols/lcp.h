#ifndef __INCLUDE_PROTOCOLS_LCP_H__
#define __INCLUDE_PROTOCOLS_LCP_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocols/ppp.h"

void lcpReceive(void *p_arg, const void *p_buffer, size_t p_size);
void lcpUp(struct ts_pppContext *p_context);
void lcpSendProtocolReject(
    struct ts_pppContext *p_context,
    const uint8_t *p_buffer,
    size_t p_size
);

#endif
