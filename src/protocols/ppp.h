#ifndef __PROTOCOLS_PPP_H_INCLUDED__
#define __PROTOCOLS_PPP_H_INCLUDED__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "client.h"

#define C_PPP_MRU_MIN 1500
#define C_PPP_MRU_MAX 1500
#define C_PPP_MRU_DEFAULT C_PPP_MRU_MAX
#define C_PPP_OVERHEAD_MAX 2
#define C_PPP_MAX_FRAME_SIZE (C_PPP_MRU_MAX + C_PPP_OVERHEAD_MAX)

#define C_PPP_PROTOCOLNUMBER_LCP 0xc021

enum te_pppLinkState {
    E_PPPLINKSTATE_DEAD,
    E_PPPLINKSTATE_ESTABLISH,
    E_PPPLINKSTATE_AUTHENTICATE,
    E_PPPLINKSTATE_NETWORK,
    E_PPPLINKSTATE_TERMINATE
};

struct ts_pppContext {
    uint_fast16_t mru;
    bool pfcEnabled;
    uint8_t magicNumber[4];
};

struct ts_client;

void pppInit(struct ts_client *p_client);
void pppReceive(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
);
void pppSend(
    struct ts_client *p_client,
    uint16_t p_protocolNumber,
    const void *p_buffer,
    size_t p_size
);

#endif
