#ifndef __PROTOCOLS_PPP_H_INCLUDED__
#define __PROTOCOLS_PPP_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

#include "client.h"

#define C_PPP_MRU_MAX 1500
#define C_PPP_MRU_DEFAULT C_PPP_MRU_MAX
#define C_PPP_OVERHEAD_MAX 10
#define C_PPP_MAX_FRAME_SIZE (C_PPP_MRU_MAX + C_PPP_OVERHEAD_MAX)

struct ts_pppContext {
    uint_fast16_t mru;
};

struct ts_client;

void pppInit(struct ts_client *p_client);
void pppReceive(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size);
void pppSend(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size);

#endif
