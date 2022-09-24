#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "client.h"
#include "protocols/ppp.h"

static void pppDiscardFrame(struct ts_client *p_client);
static void pppReceiveFrame(struct ts_client *p_client);

void pppInit(struct ts_client *p_client) {
    p_client->pppContext.mru = C_PPP_MRU_DEFAULT;
}

void pppReceive(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size) {

}

void pppSend(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size) {

}
