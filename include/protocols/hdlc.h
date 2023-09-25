#ifndef __PROTOCOLS_HDLC_H_INCLUDED__
#define __PROTOCOLS_HDLC_H_INCLUDED__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocols/ppp.h"

// HDLC overhead:
// - 1 start flag (0x7e)
// - 1 address byte (0xff)
// - 1 command byte (0x03)
// - 2 checksum bytes
// - 1 end flag (0x7e)
// Total: 6 bytes
#define C_HDLC_OVERHEAD 6
#define C_HDLC_MAX_FRAME_SIZE (C_PPP_MAX_FRAME_SIZE + C_HDLC_OVERHEAD)

struct ts_hdlcContext {
    uint8_t receiveBuffer[C_HDLC_MAX_FRAME_SIZE];
    unsigned int receiveBufferIndex;
    unsigned int sendBufferIndex;
    tf_pppSendHandler *sendHandler;
    void *sendHandlerArg;
    tf_pppReceiveHandler *receiveHandler;
    void *receiveHandlerArg;
    bool receiveEscape;
    uint32_t accm;
};

void hdlcInit(
    struct ts_hdlcContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg,
    tf_pppReceiveHandler *p_receiveHandler,
    void *p_receiveHandlerArg
);
void hdlcReceive(
    void *p_arg,
    const void *p_buffer,
    size_t p_size
);
void hdlcSend(
    void *p_arg,
    const void *p_buffer,
    size_t p_size
);

#endif
