#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "list.h"
#include "protocols/lcp.h"
#include "protocols/ppp.h"

// TODO: deallocate memory for PPP registered protocols

static struct ts_pppProtocolHandler *pppGetProtocolHandler(
    struct ts_pppContext *p_context,
    enum te_pppProtocolNumber p_protocolNumber
);

void pppInit(
    struct ts_pppContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg
) {
    p_context->linkState = E_PPPLINKSTATE_DEAD;
    p_context->sendHandler = p_sendHandler;
    p_context->sendHandlerArg = p_sendHandlerArg;
    p_context->mru = C_PPP_MAX_MRU;
    p_context->magicNumber = 0;

    struct ts_pppProtocolHandler *l_lcpHandler =
        malloc(sizeof(struct ts_pppProtocolHandler));

    if(l_lcpHandler == NULL) {
        fprintf(stderr, "Warning: failed to create LCP handler.\n");
        return;
    }

    l_lcpHandler->protocolNumber = E_PPPPROTOCOLNUMBER_LCP;
    l_lcpHandler->receiveHandler = lcpReceive;
    l_lcpHandler->up = (tf_pppUpHandler *)lcpUp;
    l_lcpHandler->down = NULL;
    l_lcpHandler->network = NULL;
    l_lcpHandler->arg = p_context;

    pppRegisterProtocol(p_context, l_lcpHandler);
}

void pppRegisterProtocol(
    struct ts_pppContext *p_context,
    struct ts_pppProtocolHandler *p_protocolHandler
) {
    struct ts_listElement *l_element = malloc(sizeof(struct ts_listElement));

    if(l_element == NULL) {
        fprintf(
            stderr,
            "Warning: failed to register protocol 0x%04x.\n",
            p_protocolHandler->protocolNumber
        );

        return;
    }

    l_element->data = p_protocolHandler;
    l_element->next = p_context->protocolHandlers;
    p_context->protocolHandlers = l_element;
}

void pppUp(struct ts_pppContext *p_context) {
    p_context->linkState = E_PPPLINKSTATE_ESTABLISH;

    struct ts_listElement *l_listElement = p_context->protocolHandlers;

    while(l_listElement != NULL) {
        struct ts_pppProtocolHandler *l_protocolHandler =
            (struct ts_pppProtocolHandler *)l_listElement->data;

        if(l_protocolHandler->up != NULL) {
            l_protocolHandler->up(l_protocolHandler->arg);
        }

        l_listElement = l_listElement->next;
    }
}

void pppDown(struct ts_pppContext *p_context) {
    p_context->linkState = E_PPPLINKSTATE_DEAD;

    struct ts_listElement *l_listElement = p_context->protocolHandlers;

    while(l_listElement != NULL) {
        struct ts_pppProtocolHandler *l_protocolHandler =
            (struct ts_pppProtocolHandler *)l_listElement->data;

        if(l_protocolHandler->down != NULL) {
            l_protocolHandler->down(l_protocolHandler->arg);
        }

        l_listElement = l_listElement->next;
    }
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
        (
            (l_context->linkState == E_PPPLINKSTATE_ESTABLISH)
            || (l_context->linkState == E_PPPLINKSTATE_DEAD)
        )
        && (l_protocolNumber != E_PPPPROTOCOLNUMBER_LCP)
    ) {
        return;
    }

    struct ts_pppProtocolHandler *l_protocolHandler =
        pppGetProtocolHandler(l_context, l_protocolNumber);

    if(l_protocolHandler == NULL) {
        lcpSendProtocolReject(l_context, p_buffer, p_size);
        return;
    }

    l_protocolHandler->receiveHandler(
        l_protocolHandler->arg,
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

void pppNetwork(struct ts_pppContext *p_context) {
    p_context->linkState = E_PPPLINKSTATE_NETWORK;

    struct ts_listElement *l_listElement = p_context->protocolHandlers;

    while(l_listElement != NULL) {
        struct ts_pppProtocolHandler *l_protocolHandler =
            (struct ts_pppProtocolHandler *)l_listElement->data;

        if(l_protocolHandler->network != NULL) {
            l_protocolHandler->network(l_protocolHandler->arg);
        }

        l_listElement = l_listElement->next;
    }
}

static struct ts_pppProtocolHandler *pppGetProtocolHandler(
    struct ts_pppContext *p_context,
    enum te_pppProtocolNumber p_protocolNumber
) {
    struct ts_listElement *l_element = p_context->protocolHandlers;

    while(l_element != NULL) {
        struct ts_pppProtocolHandler *l_protocolHandler =
            (struct ts_pppProtocolHandler *)l_element->data;

        if(l_protocolHandler->protocolNumber == p_protocolNumber) {
            return l_protocolHandler;
        }

        l_element = l_element->next;
    }

    return NULL;
}
