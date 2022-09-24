#ifndef __PROTOCOLS_HDLC_H_INCLUDED__
#define __PROTOCOLS_HDLC_H_INCLUDED__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "client.h"
#include "protocols/ppp.h"

#define C_HDLC_OVERHEAD_MAX 8
#define C_HDLC_MAX_FRAME_SIZE (C_PPP_MAX_FRAME_SIZE + C_HDLC_OVERHEAD_MAX)

struct ts_hdlcContext {
    uint8_t recvFrameBuffer[C_HDLC_MAX_FRAME_SIZE];
    uint_fast16_t recvFrameSize;
    uint8_t sendFrameBuffer[C_HDLC_MAX_FRAME_SIZE * 2];
    uint_fast16_t sendFrameSize;
    bool recvEscape;
    bool firstFrameSent;
    bool acfcEnabled;
    bool fcs32Enabled;
    uint32_t receiveFcs;
    uint32_t sendFcs;
};

struct ts_client;

void hdlcInit(struct ts_client *p_client);
void hdlcReceive(struct ts_client *p_client, uint8_t p_byte);
void hdlcSend(
    struct ts_client *p_client,
    const uint8_t *p_buffer,
    size_t p_size
);
void hdlcSetAcfcEnabled(struct ts_client *p_client, bool p_acfcEnabled);

#endif
