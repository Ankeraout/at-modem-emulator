#ifndef __PROTOCOLS_HAYES_H_INCLUDED__
#define __PROTOCOLS_HAYES_H_INCLUDED__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocols/ppp.h"

#define C_HAYES_COMMAND_BUFFER_SIZE 64

enum te_hayesState {
    E_HAYESSTATE_CMD,
    E_HAYESSTATE_CMD_A,
    E_HAYESSTATE_CMD_AT,
    E_HAYESSTATE_DATA,
    E_HAYESSTATE_DATA_PLUS1,
    E_HAYESSTATE_DATA_PLUS2
};

enum te_hayesCommandResult {
    E_HAYESCOMMANDRESULT_OK,
    E_HAYESCOMMANDRESULT_CONNECT,
    E_HAYESCOMMANDRESULT_RING,
    E_HAYESCOMMANDRESULT_NO_CARRIER,
    E_HAYESCOMMANDRESULT_ERROR,
    E_HAYESCOMMANDRESULT_NO_DIALTONE = 6,
    E_HAYESCOMMANDRESULT_BUSY,
    E_HAYESCOMMANDRESULT_NO_ANSWER
};

struct ts_hayesContext {
    enum te_hayesState state;
    uint8_t commandBuffer[C_HAYES_COMMAND_BUFFER_SIZE];
    int commandBufferSize;
    bool commandReject;
    char commandName;
    int commandParameterInt1;
    int commandParameterInt2;
    enum te_hayesCommandResult commandResult;
    const char *customCommandResult;
    bool echo;
    bool quiet;
    bool verbose;
    uint8_t regS[13];
    bool connected;
    tf_pppSendHandler *sendHandler;
    void *sendHandlerArg;
    tf_pppReceiveHandler *receiveHandler;
    void *receiveHandlerArg;
};

void hayesInit(
    struct ts_hayesContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg,
    tf_pppReceiveHandler *p_receiveHandler,
    void *p_receiveHandlerArg
);
void hayesReceive(void *p_arg, const void *p_buffer, size_t p_size);
void hayesSend(void *p_arg, const void *p_buffer, size_t p_size);

#endif
