#ifndef __INCLUDE_PROTOCOLS_PPP_H__
#define __INCLUDE_PROTOCOLS_PPP_H__

#include <stdbool.h>
#include <stddef.h>

#include "protocols/lcp_types.h"

#define C_PPP_MAX_MRU 1500
#define C_PPP_OVERHEAD 2
#define C_PPP_MAX_FRAME_SIZE (C_PPP_MAX_MRU + C_PPP_OVERHEAD)

typedef void tf_pppSendHandler(
    void *arg,
    const void *p_buffer,
    size_t p_size
);
typedef void tf_pppReceiveHandler(
    void *arg,
    const void *p_buffer,
    size_t p_size
);

enum te_pppLinkState {
    E_PPPLINKSTATE_DEAD,
    E_PPPLINKSTATE_ESTABLISH,
    E_PPPLINKSTATE_AUTHENTICATE,
    E_PPPLINKSTATE_NETWORK,
    E_PPPLINKSTATE_TERMINATE
};

enum te_pppProtocol {
    E_PPPPROTOCOL_LCP
};

enum te_pppProtocolNumber {
    E_PPPPROTOCOLNUMBER_LCP = 0xc021
};

struct ts_pppProtocolHandler {
    bool enabled;
    tf_pppReceiveHandler *receiveHandler;
    void *receiveHandlerArg;
};

struct ts_pppConfiguration {
    uint32_t magicNumber;
    uint16_t mru;
};

struct ts_pppContext {
    enum te_pppLinkState linkState;
    tf_pppSendHandler *sendHandler;
    void *sendHandlerArg;
    struct ts_pppProtocolHandler lcpHandler;
    uint16_t mru;
    uint32_t magicNumber;
    struct ts_lcpContext lcpContext;
};

void pppInit(
    struct ts_pppContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg
);
void pppRegisterProtocol(
    struct ts_pppContext *p_context,
    enum te_pppProtocol p_protocol,
    tf_pppReceiveHandler *p_receiveHandler,
    void *p_receiveHandlerArg
);
void pppUp(struct ts_pppContext *p_context);
void pppDown(struct ts_pppContext *p_context);
void pppReceive(void *p_arg, const void *p_buffer, size_t p_size);
void pppSend(
    struct ts_pppContext *p_context,
    enum te_pppProtocolNumber p_protocolNumber,
    const void *p_buffer,
    size_t p_size
);

#endif
