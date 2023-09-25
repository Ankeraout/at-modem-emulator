#ifndef __INCLUDE_PROTOCOLS_PPP_H__
#define __INCLUDE_PROTOCOLS_PPP_H__

#include <stdbool.h>
#include <stddef.h>

#include "protocols/lcp_types.h"
#include "list.h"

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
typedef void tf_pppUpHandler(void *p_arg);
typedef void tf_pppDownHandler(void *p_arg);
typedef void tf_pppNetworkHandler(void *p_arg);

enum te_pppLinkState {
    E_PPPLINKSTATE_DEAD,
    E_PPPLINKSTATE_ESTABLISH,
    E_PPPLINKSTATE_AUTHENTICATE,
    E_PPPLINKSTATE_NETWORK,
    E_PPPLINKSTATE_TERMINATE
};

enum te_pppProtocolNumber {
    E_PPPPROTOCOLNUMBER_IP = 0x0021,
    E_PPPPROTOCOLNUMBER_IPV6 = 0x0057,
    E_PPPPROTOCOLNUMBER_IPCP = 0x8021,
    E_PPPPROTOCOLNUMBER_IPV6CP = 0x8057,
    E_PPPPROTOCOLNUMBER_LCP = 0xc021
};

struct ts_pppProtocolHandler {
    enum te_pppProtocolNumber protocolNumber;
    tf_pppReceiveHandler *receiveHandler;
    tf_pppUpHandler *up;
    tf_pppDownHandler *down;
    tf_pppNetworkHandler *network;
    void *arg;
};

struct ts_pppConfiguration {
    uint32_t magicNumber;
    uint16_t mru;
};

struct ts_pppContext {
    // PPP attributes
    enum te_pppLinkState linkState;
    tf_pppSendHandler *sendHandler;
    void *sendHandlerArg;
    uint16_t mru;
    uint32_t magicNumber;

    // Protocol contexts
    struct ts_lcpContext lcpContext;

    // Protocol handlers
    struct ts_listElement *protocolHandlers;
};

void pppInit(
    struct ts_pppContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg
);
void pppRegisterProtocol(
    struct ts_pppContext *p_context,
    struct ts_pppProtocolHandler *p_protocolHandler
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
void pppNetwork(struct ts_pppContext *p_context);

#endif
