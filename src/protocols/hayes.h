#ifndef __PROTOCOLS_HAYES_H_INCLUDED__
#define __PROTOCOLS_HAYES_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

#include "client.h"

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
    bool echo;
    bool quiet;
    bool verbose;
    uint8_t regS[13];
    bool connected;
};

struct ts_client;

void hayesInit(struct ts_client *p_client);
void hayesReceive(struct ts_client *p_client, uint8_t p_byte);
void hayesSend(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size);

#endif
