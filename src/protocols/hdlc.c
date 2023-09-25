#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "protocols/hdlc.h"

#define C_PPP_FCS16_INIT 0xffff
#define C_PPP_FCS16_GOOD 0xf0b8

static inline void hdlcReceiveByte(
    struct ts_hdlcContext *p_context,
    uint8_t p_byte
);
static inline void hdlcReceiveFrame(struct ts_hdlcContext *p_context);
static inline void hdlcDiscardFrame(struct ts_hdlcContext *p_context);
static uint16_t hdlcComputeFcs16(const uint8_t *p_buffer, size_t p_size);

static const uint16_t s_fcs16Table[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

void hdlcInit(
    struct ts_hdlcContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg,
    tf_pppReceiveHandler *p_receiveHandler,
    void *p_receiveHandlerArg
) {
    p_context->receiveBufferIndex = 0;
    p_context->sendBufferIndex = 0;
    p_context->sendHandler = p_sendHandler;
    p_context->sendHandlerArg = p_sendHandlerArg;
    p_context->receiveHandler = p_receiveHandler;
    p_context->receiveHandlerArg = p_receiveHandlerArg;
    p_context->receiveEscape = false;
    p_context->accm = 0xffffffff;
}

void hdlcReceive(
    void *p_arg,
    const void *p_buffer,
    size_t p_size
) {
    struct ts_hdlcContext *l_context = (struct ts_hdlcContext *)p_arg;
    const uint8_t *l_buffer = (const uint8_t *)p_buffer;

    for(size_t l_index = 0; l_index < p_size; l_index++) {
        hdlcReceiveByte(l_context, l_buffer[l_index]);
    }
}

void hdlcSend(
    void *p_arg,
    const void *p_buffer,
    size_t p_size
) {
    struct ts_hdlcContext *l_context = (struct ts_hdlcContext *)p_arg;
    const uint8_t *l_inputBuffer = (const uint8_t *)p_buffer;

    // Create packet
    uint8_t l_buffer[p_size + C_HDLC_OVERHEAD];

    l_buffer[0] = 0x7e;
    l_buffer[1] = 0xff;
    l_buffer[2] = 0x03;

    memcpy(&l_buffer[3], p_buffer, p_size);

    const uint16_t l_fcs = hdlcComputeFcs16(&l_buffer[1], p_size + 2);

    l_buffer[p_size + 3] = l_fcs;
    l_buffer[p_size + 4] = l_fcs >> 8;
    l_buffer[p_size + 5] = 0x7e;

    // Convert packet to serial form
    uint8_t l_outputBuffer[(p_size + C_HDLC_OVERHEAD) * 2];
    size_t l_outputBufferIndex = 1;

    l_outputBuffer[0] = 0x7e;

    for(size_t l_bufferIndex = 1; l_bufferIndex < p_size + 5; l_bufferIndex++) {
        const uint8_t l_byte = l_buffer[l_bufferIndex];

        if(
            ((l_byte < 0x20) && ((l_context->accm & (1 << l_byte)) != 0))
            || (l_byte == 0x7d)
            || (l_byte == 0x7e)
        ) {
            l_outputBuffer[l_outputBufferIndex++] = 0x7d;
            l_outputBuffer[l_outputBufferIndex++] = l_byte ^ 0x20;
        } else {
            l_outputBuffer[l_outputBufferIndex++] = l_byte;
        }
    }

    l_outputBuffer[l_outputBufferIndex++] = 0x7e;

    l_context->sendHandler(
        l_context->sendHandlerArg,
        l_outputBuffer,
        l_outputBufferIndex
    );
}

static inline void hdlcReceiveByte(
    struct ts_hdlcContext *p_context,
    uint8_t p_byte
) {
    if(p_byte == 0x7e) {
        if(p_context->receiveBufferIndex == 0) {
            p_context->receiveBuffer[0] = 0x7e;
            p_context->receiveBufferIndex = 1;
        } else if(p_context->receiveBufferIndex != 1) {
            p_context->receiveBuffer[p_context->receiveBufferIndex++] = 0x7e;
            hdlcReceiveFrame(p_context);
        }
    } else if(p_byte == 0x7d) {
        p_context->receiveEscape = true;
    } else {
        uint8_t l_byte;

        if(p_context->receiveEscape) {
            p_context->receiveEscape = false;
            l_byte = p_byte ^ 0x20;
        } else {
            l_byte = p_byte;
        }

        if(p_context->receiveBufferIndex == C_HDLC_MAX_FRAME_SIZE - 1) {
            return;
        }

        p_context->receiveBuffer[p_context->receiveBufferIndex++] = l_byte;
    }
}

static inline void hdlcReceiveFrame(struct ts_hdlcContext *p_context) {
    // Check address and command fields
    if(
        (p_context->receiveBuffer[1] != 0xff)
        || (p_context->receiveBuffer[2] != 0x03)
    ) {
        hdlcDiscardFrame(p_context);
        return;
    }

    // Check FCS
    const uint16_t l_actualFcs =
        (p_context->receiveBuffer[p_context->receiveBufferIndex - 2] << 8)
        | p_context->receiveBuffer[p_context->receiveBufferIndex - 3];

    const uint16_t l_computedFcs = hdlcComputeFcs16(
        &p_context->receiveBuffer[1],
        p_context->receiveBufferIndex - 4
    );

    if(l_actualFcs != l_computedFcs) {
        hdlcDiscardFrame(p_context);
        return;
    }

    // Call received frame handler
    p_context->receiveHandler(
        p_context->receiveHandlerArg,
        &p_context->receiveBuffer[3],
        p_context->receiveBufferIndex - C_HDLC_OVERHEAD
    );

    // Reset the state of the buffer
    hdlcDiscardFrame(p_context);
}

static inline void hdlcDiscardFrame(struct ts_hdlcContext *p_context) {
    p_context->receiveBufferIndex = 1;
    p_context->receiveEscape = false;
}

static uint16_t hdlcComputeFcs16(const uint8_t *p_buffer, size_t p_size) {
    uint16_t l_fcs = C_PPP_FCS16_INIT;

    for(size_t l_bufferIndex = 0; l_bufferIndex < p_size; l_bufferIndex++) {
        l_fcs = (l_fcs >> 8)
            ^ s_fcs16Table[(l_fcs ^ p_buffer[l_bufferIndex]) & 0xff];
    }

    return ~l_fcs;
}
