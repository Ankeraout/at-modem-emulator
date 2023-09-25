#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

#include "protocols/lcp.h"
#include "protocols/ppp.h"

static void pppInitHandler(struct ts_pppProtocolHandler *p_protocolHandler);

void pppInit(
    struct ts_pppContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg
) {
    p_context->linkState = E_PPPLINKSTATE_DEAD;
    p_context->sendHandler = p_sendHandler;
    p_context->sendHandlerArg = p_sendHandlerArg;
    pppInitHandler(&p_context->lcpHandler);
    p_context->mru = C_PPP_MAX_MRU;
    p_context->magicNumber = 0;

    pppRegisterProtocol(p_context, E_PPPPROTOCOL_LCP, lcpReceive, p_context);
}

void pppRegisterProtocol(
    struct ts_pppContext *p_context,
    enum te_pppProtocol p_protocol,
    tf_pppReceiveHandler *p_receiveHandler,
    void *p_receiveHandlerArg
) {
    struct ts_pppProtocolHandler *l_protocolHandler = NULL;

    switch(p_protocol) {
        case E_PPPPROTOCOL_LCP:
            l_protocolHandler = &p_context->lcpHandler;
            break;

        default:
            break;
    }

    if(l_protocolHandler == NULL) {
        return;
    }

    l_protocolHandler->enabled = p_receiveHandler != NULL;
    l_protocolHandler->receiveHandler = p_receiveHandler;
    l_protocolHandler->receiveHandlerArg = p_receiveHandlerArg;
}

void pppUp(struct ts_pppContext *p_context) {
    p_context->linkState = E_PPPLINKSTATE_ESTABLISH;
    lcpUp(p_context);
}

void pppDown(struct ts_pppContext *p_context) {
    p_context->linkState = E_PPPLINKSTATE_DEAD;
}

void pppReceive(void *p_arg, const void *p_buffer, size_t p_size) {
    struct ts_pppContext *l_context = (struct ts_pppContext *)p_arg;
    const uint8_t *l_buffer = (const uint8_t *)p_buffer;

    if(l_context->linkState == E_PPPLINKSTATE_DEAD) {
        pppUp(l_context);
    }

    uint16_t l_protocolNumber = ntohs(*(uint16_t *)&l_buffer[0]);

    // If the link is not yet established (LCP negociation still occurring),
    // silently discard all non-LCP packets.
    if(
        (l_context->linkState == E_PPPLINKSTATE_ESTABLISH)
        && (l_protocolNumber != E_PPPPROTOCOLNUMBER_LCP)
    ) {
        return;
    }

    struct ts_pppProtocolHandler *l_protocolHandler = NULL;

    switch(l_protocolNumber) {
        case E_PPPPROTOCOLNUMBER_LCP:
            l_protocolHandler = &l_context->lcpHandler;
            break;

        default:
            break;
    }

    if(l_protocolHandler == NULL || !l_protocolHandler->enabled) {
        lcpSendProtocolReject(l_context, p_buffer, p_size);
        return;
    }

    l_protocolHandler->receiveHandler(
        l_protocolHandler->receiveHandlerArg,
        &l_buffer[2],
        p_size - 2
    );
}

void pppSend(
    struct ts_pppContext *p_context,
    enum te_pppProtocolNumber p_protocolNumber,
    const void *p_buffer,
    size_t p_size
) {
    uint8_t l_buffer[p_size + 2];
    *(uint16_t *)l_buffer = htons(p_protocolNumber);
    memcpy(&l_buffer[2], p_buffer, p_size);

    p_context->sendHandler(p_context->sendHandlerArg, l_buffer, p_size + 2);
}

static void pppInitHandler(struct ts_pppProtocolHandler *p_protocolHandler) {
    p_protocolHandler->enabled = false;
    p_protocolHandler->receiveHandler = NULL;
    p_protocolHandler->receiveHandlerArg = NULL;
}
