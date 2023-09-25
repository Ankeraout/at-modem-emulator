#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocols/hayes.h"

enum te_hayesCommandParserState {
    E_HAYESCOMMANDPARSERSTATE_COMMAND,
    E_HAYESCOMMANDPARSERSTATE_NUMBER,
    E_HAYESCOMMANDPARSERSTATE_D,
    E_HAYESCOMMANDPARSERSTATE_S,
    E_HAYESCOMMANDPARSERSTATE_S_EQ,
};

static const char *s_resultStrings[] = {
    "OK",
    "CONNECT",
    "RING",
    "NO CARRIER",
    "ERROR",
    "<undefined>",
    "NO DIALTONE",
    "BUSY",
    "NO ANSWER"
};

static inline void hayesReceiveByte(
    struct ts_hayesContext *p_context,
    uint8_t p_byte
);
static void hayesProcessCommand(struct ts_hayesContext *p_context);
static void hayesProcessSubcommand(struct ts_hayesContext *p_context);
static void hayesSendCommandResult(
    struct ts_hayesContext *p_context,
    enum te_hayesCommandResult p_commandResult
);
static void hayesSendInformation(
    struct ts_hayesContext *p_context,
    const char *p_information
);

static void hayesHandleUnknown(struct ts_hayesContext *p_context);
static void hayesHandleA(struct ts_hayesContext *p_context);
static void hayesHandleD(struct ts_hayesContext *p_context);
static void hayesHandleE(struct ts_hayesContext *p_context);
static void hayesHandleH(struct ts_hayesContext *p_context);
static void hayesHandleI(struct ts_hayesContext *p_context);
static void hayesHandleL(struct ts_hayesContext *p_context);
static void hayesHandleM(struct ts_hayesContext *p_context);
static void hayesHandleO(struct ts_hayesContext *p_context);
static void hayesHandleQ(struct ts_hayesContext *p_context);
static void hayesHandleSGet(struct ts_hayesContext *p_context);
static void hayesHandleSSet(struct ts_hayesContext *p_context);
static void hayesHandleV(struct ts_hayesContext *p_context);
static void hayesHandleX(struct ts_hayesContext *p_context);
static void hayesHandleZ(struct ts_hayesContext *p_context);

typedef void (*t_hayesCommandHandler)(struct ts_hayesContext *p_context);

static t_hayesCommandHandler s_hayesCommandHandlers[52] = {
    hayesHandleA,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleD,
    hayesHandleE,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleH,
    hayesHandleI,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleL,
    hayesHandleM,
    hayesHandleUnknown,
    hayesHandleO,
    hayesHandleUnknown,
    hayesHandleQ,
    hayesHandleUnknown,
    hayesHandleSSet,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleV,
    hayesHandleUnknown,
    hayesHandleX,
    hayesHandleUnknown,
    hayesHandleZ,
    hayesHandleA,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleD,
    hayesHandleE,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleH,
    hayesHandleI,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleL,
    hayesHandleM,
    hayesHandleUnknown,
    hayesHandleO,
    hayesHandleUnknown,
    hayesHandleQ,
    hayesHandleUnknown,
    hayesHandleSGet,
    hayesHandleUnknown,
    hayesHandleUnknown,
    hayesHandleV,
    hayesHandleUnknown,
    hayesHandleX,
    hayesHandleUnknown,
    hayesHandleZ,
};

void hayesInit(
    struct ts_hayesContext *p_context,
    tf_pppSendHandler *p_sendHandler,
    void *p_sendHandlerArg,
    tf_pppReceiveHandler *p_receiveHandler,
    void *p_receiveHandlerArg
) {
    hayesHandleZ(p_context);
    p_context->state = E_HAYESSTATE_CMD;
    p_context->commandBufferSize = 0;
    p_context->commandReject = false;
    p_context->sendHandler = p_sendHandler;
    p_context->sendHandlerArg = p_sendHandlerArg;
    p_context->receiveHandler = p_receiveHandler;
    p_context->receiveHandlerArg = p_receiveHandlerArg;
}

void hayesReceive(void *p_arg, const void *p_buffer, size_t p_size) {
    struct ts_hayesContext *l_context = (struct ts_hayesContext *)p_arg;
    const uint8_t *l_buffer = (const uint8_t *)p_buffer;

    for(size_t l_index = 0; l_index < p_size; l_index++) {
        hayesReceiveByte(l_context, l_buffer[l_index]);
    }
}

void hayesSend(
    void *p_arg,
    const void *p_buffer,
    size_t p_size
) {
    struct ts_hayesContext *l_context = (struct ts_hayesContext *)p_arg;

    l_context->sendHandler(l_context->sendHandlerArg, p_buffer, p_size);
}

static inline void hayesReceiveByte(
    struct ts_hayesContext *p_context,
    uint8_t p_byte
) {
    switch(p_context->state) {
        case E_HAYESSTATE_CMD:
            if(p_byte == 'A' || p_byte == 'a') {
                p_context->state = E_HAYESSTATE_CMD_A;
            }

            if(p_context->echo) {
                hayesSend(p_context, &p_byte, 1);
            }

            break;

        case E_HAYESSTATE_CMD_A:
            if(p_byte == 'T' || p_byte == 't') {
                p_context->state = E_HAYESSTATE_CMD_AT;
                p_context->commandBufferSize = 0;
                p_context->commandResult = E_HAYESCOMMANDRESULT_OK;
            } else {
                p_context->state = E_HAYESSTATE_CMD;
            }

            if(p_context->echo) {
                hayesSend(p_context, &p_byte, 1);
            }

            break;

        case E_HAYESSTATE_CMD_AT:
            if(p_byte == '\r') {
                if(!p_context->commandReject) {
                    if(p_context->echo) {
                        hayesSend(p_context, &p_byte, 1);
                    }

                    hayesProcessCommand(p_context);

                    if(p_context->state == E_HAYESSTATE_CMD_AT) {
                        p_context->state = E_HAYESSTATE_CMD;
                    }
                }
            } else if(
                p_context->commandBufferSize
                < C_HAYES_COMMAND_BUFFER_SIZE
            ) {
                p_context->commandBuffer[
                    p_context->commandBufferSize++
                ] = p_byte;

                if(p_context->echo) {
                    hayesSend(p_context, &p_byte, 1);
                }
            } else {
                p_context->commandReject = true;

                if(p_context->echo) {
                    hayesSend(p_context, &p_byte, 1);
                }
            }

            break;

        case E_HAYESSTATE_DATA:
            if(p_byte == '+') {
                p_context->state = E_HAYESSTATE_DATA_PLUS1;
            } else {
                p_context->receiveHandler(
                    p_context->receiveHandlerArg,
                    &p_byte,
                    1
                );
            }

            break;

        case E_HAYESSTATE_DATA_PLUS1:
            if(p_byte == '+') {
                p_context->state = E_HAYESSTATE_DATA_PLUS2;
            } else {
                p_context->receiveHandler(
                    p_context->receiveHandlerArg,
                    "+",
                    1
                );
                p_context->receiveHandler(
                    p_context->receiveHandlerArg,
                    &p_byte,
                    1
                );
                p_context->state = E_HAYESSTATE_DATA;
            }

            break;

        case E_HAYESSTATE_DATA_PLUS2:
            if(p_byte == '+') {
                p_context->state = E_HAYESSTATE_CMD;
            } else {
                p_context->receiveHandler(
                    p_context->receiveHandlerArg,
                    "++",
                    2
                );
                p_context->receiveHandler(
                    p_context->receiveHandlerArg,
                    &p_byte,
                    1
                );
                p_context->state = E_HAYESSTATE_DATA;
            }

            break;
    }
}

static void hayesSendCommandResult(
    struct ts_hayesContext *p_context,
    enum te_hayesCommandResult p_commandResult
) {
    char l_buffer[64];
    int l_size;

    if(!p_context->quiet) {
        if(p_context->verbose) {
            l_size = sprintf(l_buffer, "\r\n%s\r\n", s_resultStrings[p_commandResult]);
        } else {
            l_size = sprintf(l_buffer, "%d\r", p_commandResult);
        }

        p_context->sendHandler(p_context->sendHandlerArg, l_buffer, l_size);
    }
}

static void hayesSendInformation(
    struct ts_hayesContext *p_context,
    const char *p_information
) {
    char l_buffer[64];
    int l_size;

    if(p_context->verbose) {
        l_size = sprintf(l_buffer, "\r\n%s\r\n", p_information);
    } else {
        l_size = sprintf(l_buffer, "%s\r\n", p_information);
    }

    p_context->sendHandler(p_context->sendHandlerArg, l_buffer, l_size);
}

static void hayesProcessSubcommand(struct ts_hayesContext *p_context) {
    if(
        p_context->commandName >= 'a'
        && p_context->commandName <= 'z'
    ) {
        s_hayesCommandHandlers[p_context->commandName - 'a' + 26](
            p_context
        );
    } else {
        s_hayesCommandHandlers[p_context->commandName - 'A'](
            p_context
        );
    }
}

static void hayesProcessCommand(struct ts_hayesContext *p_context) {
    char l_commandBuffer[100];
    memcpy(l_commandBuffer, p_context->commandBuffer, p_context->commandBufferSize);
    l_commandBuffer[p_context->commandBufferSize] = '\0';

    enum te_hayesCommandParserState l_parserState =
        E_HAYESCOMMANDPARSERSTATE_COMMAND;

    for(
        int l_stringIndex = 0;
        l_stringIndex < p_context->commandBufferSize;
        l_stringIndex++
    ) {
        char l_c = p_context->commandBuffer[l_stringIndex];

        if(p_context->commandResult == E_HAYESCOMMANDRESULT_ERROR) {
            break;
        }

        switch(l_parserState) {
            case E_HAYESCOMMANDPARSERSTATE_COMMAND:
                if(l_c == 'D' || l_c == 'd') {
                    p_context->commandName = 'D';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_D;
                } else if(l_c == 'S' || l_c == 's') {
                    p_context->commandName = 's';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S;
                    p_context->commandParameterInt1 = 0;
                } else if(
                    (l_c >= 'A' && l_c <= 'Z')
                    || (l_c >= 'a' && l_c <= 'z')
                ) {
                    p_context->commandName = l_c;

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_NUMBER;
                    p_context->commandParameterInt1 = 0;
                } else {
                    p_context->commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_NUMBER:
                if(l_c >= '0' && l_c <= '9') {
                    p_context->commandParameterInt1 *= 10;
                    p_context->commandParameterInt1 += l_c - '0';
                } else if(l_c == 'D' || l_c == 'd') {
                    hayesProcessSubcommand(p_context);

                    p_context->commandName = 'D';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_D;
                } else if(l_c == 'S' || l_c == 's') {
                    hayesProcessSubcommand(p_context);

                    p_context->commandName = 's';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S;
                    p_context->commandParameterInt1 = 0;
                } else if(
                    (l_c >= 'A' && l_c <= 'Z')
                    || (l_c >= 'a' && l_c <= 'z')
                ) {
                    hayesProcessSubcommand(p_context);

                    p_context->commandName = l_c;

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_NUMBER;
                    p_context->commandParameterInt1 = 0;
                } else {
                    p_context->commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_D:
                if(
                    (l_c >= '0' && l_c <= '9')
                    || l_c == 't'
                    || l_c == 'T'
                    || l_c == 'p'
                    || l_c == 'P'
                    || l_c == '*'
                    || l_c == '#'
                    || l_c == '+'
                    || (l_c >= 'A' && l_c <= 'D')
                    || (l_c >= 'a' && l_c <= 'd')
                    || l_c == ','
                    || l_c == '!'
                    || l_c == 'W'
                    || l_c == 'w'
                    || l_c == '@'
                ) {
                    // Do nothing
                } else if(l_c == ';') {
                    hayesProcessSubcommand(p_context);
                    l_parserState = E_HAYESCOMMANDPARSERSTATE_COMMAND;
                } else {
                    p_context->commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_S:
                if(l_c >= '0' && l_c <= '9') {
                    p_context->commandParameterInt1 *= 10;
                    p_context->commandParameterInt1 += l_c - '0';
                } else if(l_c == '=') {
                    p_context->commandName = 'S';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S_EQ;
                    p_context->commandParameterInt2 = 0;
                } else if(l_c == '?') {
                    hayesProcessSubcommand(p_context);
                    l_parserState = E_HAYESCOMMANDPARSERSTATE_COMMAND;
                } else {
                    p_context->commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_S_EQ:
                if(l_c >= '0' && l_c <= '9') {
                    p_context->commandParameterInt2 *= 10;
                    p_context->commandParameterInt2 += l_c;
                } else if(l_c == 'D' || l_c == 'd') {
                    hayesProcessSubcommand(p_context);

                    p_context->commandName = 'D';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_D;
                } else if(l_c == 'S' || l_c == 's') {
                    hayesProcessSubcommand(p_context);

                    p_context->commandName = 's';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S;
                    p_context->commandParameterInt1 = 0;
                } else if(
                    (l_c >= 'A' && l_c <= 'Z')
                    || (l_c >= 'a' && l_c <= 'z')
                ) {
                    hayesProcessSubcommand(p_context);

                    p_context->commandName = l_c;

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_NUMBER;
                    p_context->commandParameterInt1 = 0;
                } else {
                    p_context->commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;
        }
    }

    if(l_parserState != E_HAYESCOMMANDPARSERSTATE_COMMAND) {
        hayesProcessSubcommand(p_context);
    }

    hayesSendCommandResult(p_context, p_context->commandResult);
}

static void hayesHandleUnknown(struct ts_hayesContext *p_context) {
    p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleA(struct ts_hayesContext *p_context) {
    p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleD(struct ts_hayesContext *p_context) {
    p_context->commandResult = E_HAYESCOMMANDRESULT_CONNECT;
    p_context->state = E_HAYESSTATE_DATA;
    p_context->connected = true;
}

static void hayesHandleE(struct ts_hayesContext *p_context) {
    int l_parameterValue = p_context->commandParameterInt1;

    if(l_parameterValue == 0) {
        p_context->echo = false;
    } else if(l_parameterValue == 1) {
        p_context->echo = true;
    } else {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleH(struct ts_hayesContext *p_context) {
    p_context->connected = false;
}

static void hayesHandleI(struct ts_hayesContext *p_context) {
    p_context->commandResult = E_HAYESCOMMANDRESULT_OK;

    hayesSendInformation(p_context, "at-modem-emulator");
}

static void hayesHandleL(struct ts_hayesContext *p_context) {
    int l_parameterValue = p_context->commandParameterInt1;

    if(l_parameterValue >= 0 && l_parameterValue <= 3) {
        // Do nothing
    } else {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleM(struct ts_hayesContext *p_context) {
    int l_parameterValue = p_context->commandParameterInt1;

    if(l_parameterValue >= 0 && l_parameterValue <= 2) {
        // Do nothing
    } else {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleO(struct ts_hayesContext *p_context) {
    if(p_context->connected) {
        p_context->state = E_HAYESSTATE_DATA;
    } else {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleSGet(struct ts_hayesContext *p_context) {
    int l_registerNumber = p_context->commandParameterInt1;

    if(l_registerNumber > 12) {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
        return;
    }

    char l_buffer[4];

    int l_size = sprintf(l_buffer, "%d", p_context->regS[l_registerNumber]);

    p_context->sendHandler(p_context->sendHandlerArg, l_buffer, l_size);
}

static void hayesHandleSSet(struct ts_hayesContext *p_context) {
    int l_registerNumber = p_context->commandParameterInt1;
    int l_registerValue = p_context->commandParameterInt2;

    if(l_registerNumber > 12) {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
        return;
    }

    if(l_registerValue > 255) {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }

    p_context->regS[l_registerNumber] = l_registerValue;
}

static void hayesHandleQ(struct ts_hayesContext *p_context) {
    int l_parameterValue = p_context->commandParameterInt1;

    if(l_parameterValue == 0) {
        p_context->quiet = false;
    } else if(l_parameterValue == 1) {
        p_context->quiet = true;
    } else {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleV(struct ts_hayesContext *p_context) {
    int l_parameterValue = p_context->commandParameterInt1;

    if(l_parameterValue == 0) {
        p_context->verbose = false;
    } else if(l_parameterValue == 1) {
        p_context->verbose = true;
    } else {
        p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleX(struct ts_hayesContext *p_context) {
    p_context->commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleZ(struct ts_hayesContext *p_context) {
    p_context->quiet = false;
    p_context->echo = true;
    p_context->verbose = true;
    p_context->connected = false;
    p_context->regS[0] = 0;
    p_context->regS[1] = 0;
    p_context->regS[2] = '+';
    p_context->regS[3] = '\r';
    p_context->regS[4] = '\n';
    p_context->regS[5] = '\b';
    p_context->regS[6] = 2;
    p_context->regS[7] = 50;
    p_context->regS[8] = 2;
    p_context->regS[9] = 6;
    p_context->regS[10] = 14;
    p_context->regS[11] = 95;
    p_context->regS[12] = 50;
}
