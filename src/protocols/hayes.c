#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "client.h"
#include "protocols/hdlc.h"

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

static void hayesProcessCommand(struct ts_client *p_client);
static void hayesProcessSubcommand(struct ts_client *p_client);
static void hayesSendCommandResult(
    struct ts_client *p_client,
    enum te_hayesCommandResult p_commandResult
);

static void hayesHandleUnknown(struct ts_client *p_client);
static void hayesHandleA(struct ts_client *p_client);
static void hayesHandleD(struct ts_client *p_client);
static void hayesHandleE(struct ts_client *p_client);
static void hayesHandleH(struct ts_client *p_client);
static void hayesHandleI(struct ts_client *p_client);
static void hayesHandleL(struct ts_client *p_client);
static void hayesHandleM(struct ts_client *p_client);
static void hayesHandleO(struct ts_client *p_client);
static void hayesHandleQ(struct ts_client *p_client);
static void hayesHandleSGet(struct ts_client *p_client);
static void hayesHandleSSet(struct ts_client *p_client);
static void hayesHandleV(struct ts_client *p_client);
static void hayesHandleX(struct ts_client *p_client);
static void hayesHandleZ(struct ts_client *p_client);

typedef void (*t_hayesCommandHandler)(struct ts_client *p_client);

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

void hayesInit(struct ts_client *p_client) {
    hayesHandleZ(p_client);
    p_client->hayesContext.state = E_HAYESSTATE_CMD;
    p_client->hayesContext.commandBufferSize = 0;
    p_client->hayesContext.commandReject = false;
}

void hayesReceive(struct ts_client *p_client, uint8_t p_byte) {
    switch(p_client->hayesContext.state) {
        case E_HAYESSTATE_CMD:
            if(p_byte == 'A' || p_byte == 'a') {
                p_client->hayesContext.state = E_HAYESSTATE_CMD_A;
            }

            break;

        case E_HAYESSTATE_CMD_A:
            if(p_byte == 'T' || p_byte == 't') {
                p_client->hayesContext.state = E_HAYESSTATE_CMD_AT;
                p_client->hayesContext.commandBufferSize = 0;
                p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_OK;
            } else {
                p_client->hayesContext.state = E_HAYESSTATE_CMD;
            }

            break;

        case E_HAYESSTATE_CMD_AT:
            if(p_byte == '\r') {
                if(!p_client->hayesContext.commandReject) {
                    hayesProcessCommand(p_client);

                    if(p_client->hayesContext.state == E_HAYESSTATE_CMD_AT) {
                        p_client->hayesContext.state = E_HAYESSTATE_CMD;
                    }
                }
            } else if(
                p_client->hayesContext.commandBufferSize
                < C_HAYES_COMMAND_BUFFER_SIZE
            ) {
                p_client->hayesContext.commandBuffer[
                    p_client->hayesContext.commandBufferSize++
                ] = p_byte;
            } else {
                p_client->hayesContext.commandReject = true;
            }

            break;

        case E_HAYESSTATE_DATA:
            if(p_byte == '+') {
                p_client->hayesContext.state = E_HAYESSTATE_DATA_PLUS1;
            } else {
                hdlcReceive(p_client, p_byte);
            }

            break;

        case E_HAYESSTATE_DATA_PLUS1:
            if(p_byte == '+') {
                p_client->hayesContext.state = E_HAYESSTATE_DATA_PLUS2;
            } else {
                hdlcReceive(p_client, '+');
                hdlcReceive(p_client, p_byte);
                p_client->hayesContext.state = E_HAYESSTATE_DATA;
            }

            break;

        case E_HAYESSTATE_DATA_PLUS2:
            if(p_byte == '+') {
                p_client->hayesContext.state = E_HAYESSTATE_CMD;
            } else {
                hdlcReceive(p_client, '+');
                hdlcReceive(p_client, '+');
                hdlcReceive(p_client, p_byte);
                p_client->hayesContext.state = E_HAYESSTATE_DATA;
            }

            break;
    }
}

void hayesSend(struct ts_client *p_client, uint8_t *p_buffer, size_t p_size) {

}

static void hayesSendCommandResult(
    struct ts_client *p_client,
    enum te_hayesCommandResult p_commandResult
) {
    char l_buffer[64];

    if(!p_client->hayesContext.quiet) {
        if(p_client->hayesContext.verbose) {
            sprintf(l_buffer, "\r\n%s\r\n", s_resultStrings[p_commandResult]);
        } else {
            sprintf(l_buffer, "%d\r", p_commandResult);
        }
    }

    clientWriteString(p_client, l_buffer);
}

static void hayesProcessSubcommand(struct ts_client *p_client) {
    if(
        p_client->hayesContext.commandName >= 'a'
        && p_client->hayesContext.commandName <= 'z'
    ) {
        s_hayesCommandHandlers[p_client->hayesContext.commandName - 'a' + 26](
            p_client
        );
    } else {
        s_hayesCommandHandlers[p_client->hayesContext.commandName - 'A'](
            p_client
        );
    }
}

static void hayesProcessCommand(struct ts_client *p_client) {
    char l_commandBuffer[100];
    memcpy(l_commandBuffer, p_client->hayesContext.commandBuffer, p_client->hayesContext.commandBufferSize);
    l_commandBuffer[p_client->hayesContext.commandBufferSize] = '\0';

    enum te_hayesCommandParserState l_parserState =
        E_HAYESCOMMANDPARSERSTATE_COMMAND;

    for(
        int l_stringIndex = 0;
        l_stringIndex < p_client->hayesContext.commandBufferSize;
        l_stringIndex++
    ) {
        char l_c = p_client->hayesContext.commandBuffer[l_stringIndex];

        if(p_client->hayesContext.commandResult == E_HAYESCOMMANDRESULT_ERROR) {
            break;
        }

        switch(l_parserState) {
            case E_HAYESCOMMANDPARSERSTATE_COMMAND:
                if(l_c == 'D' || l_c == 'd') {
                    p_client->hayesContext.commandName = 'D';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_D;
                } else if(l_c == 'S' || l_c == 's') {
                    p_client->hayesContext.commandName = 's';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S;
                    p_client->hayesContext.commandParameterInt1 = 0;
                } else if(
                    (l_c >= 'A' && l_c <= 'Z')
                    || (l_c >= 'a' && l_c <= 'z')
                ) {
                    p_client->hayesContext.commandName = l_c;

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_NUMBER;
                    p_client->hayesContext.commandParameterInt1 = 0;
                } else {
                    p_client->hayesContext.commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_NUMBER:
                if(l_c >= '0' && l_c <= '9') {
                    p_client->hayesContext.commandParameterInt1 *= 10;
                    p_client->hayesContext.commandParameterInt1 += l_c - '0';
                } else if(l_c == 'D' || l_c == 'd') {
                    hayesProcessSubcommand(p_client);

                    p_client->hayesContext.commandName = 'D';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_D;
                } else if(l_c == 'S' || l_c == 's') {
                    hayesProcessSubcommand(p_client);

                    p_client->hayesContext.commandName = 's';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S;
                    p_client->hayesContext.commandParameterInt1 = 0;
                } else if(
                    (l_c >= 'A' && l_c <= 'Z')
                    || (l_c >= 'a' && l_c <= 'z')
                ) {
                    hayesProcessSubcommand(p_client);

                    p_client->hayesContext.commandName = l_c;

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_NUMBER;
                    p_client->hayesContext.commandParameterInt1 = 0;
                } else {
                    p_client->hayesContext.commandResult =
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
                    hayesProcessSubcommand(p_client);
                    l_parserState = E_HAYESCOMMANDPARSERSTATE_COMMAND;
                } else {
                    p_client->hayesContext.commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_S:
                if(l_c >= '0' && l_c <= '9') {
                    p_client->hayesContext.commandParameterInt1 *= 10;
                    p_client->hayesContext.commandParameterInt1 += l_c - '0';
                } else if(l_c == '=') {
                    p_client->hayesContext.commandName = 'S';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S_EQ;
                    p_client->hayesContext.commandParameterInt2 = 0;
                } else if(l_c == '?') {
                    hayesProcessSubcommand(p_client);
                    l_parserState = E_HAYESCOMMANDPARSERSTATE_COMMAND;
                } else {
                    p_client->hayesContext.commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;

            case E_HAYESCOMMANDPARSERSTATE_S_EQ:
                if(l_c >= '0' && l_c <= '9') {
                    p_client->hayesContext.commandParameterInt2 *= 10;
                    p_client->hayesContext.commandParameterInt2 += l_c;
                } else if(l_c == 'D' || l_c == 'd') {
                    hayesProcessSubcommand(p_client);

                    p_client->hayesContext.commandName = 'D';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_D;
                } else if(l_c == 'S' || l_c == 's') {
                    hayesProcessSubcommand(p_client);

                    p_client->hayesContext.commandName = 's';

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_S;
                    p_client->hayesContext.commandParameterInt1 = 0;
                } else if(
                    (l_c >= 'A' && l_c <= 'Z')
                    || (l_c >= 'a' && l_c <= 'z')
                ) {
                    hayesProcessSubcommand(p_client);

                    p_client->hayesContext.commandName = l_c;

                    l_parserState = E_HAYESCOMMANDPARSERSTATE_NUMBER;
                    p_client->hayesContext.commandParameterInt1 = 0;
                } else {
                    p_client->hayesContext.commandResult =
                        E_HAYESCOMMANDRESULT_ERROR;
                }

                break;
        }
    }

    if(l_parserState != E_HAYESCOMMANDPARSERSTATE_COMMAND) {
        hayesProcessSubcommand(p_client);
    }

    hayesSendCommandResult(p_client, p_client->hayesContext.commandResult);
}

static void hayesHandleUnknown(struct ts_client *p_client) {
    p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleA(struct ts_client *p_client) {
    p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleD(struct ts_client *p_client) {
    p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_CONNECT;
    p_client->hayesContext.state = E_HAYESSTATE_DATA;
    p_client->hayesContext.connected = true;
    hdlcInit(p_client);
}

static void hayesHandleE(struct ts_client *p_client) {
    int l_parameterValue = p_client->hayesContext.commandParameterInt1;

    if(l_parameterValue == 0) {
        p_client->hayesContext.echo = false;
    } else if(l_parameterValue == 1) {
        p_client->hayesContext.echo = true;
    } else {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleH(struct ts_client *p_client) {
    p_client->hayesContext.connected = false;
}

static void hayesHandleI(struct ts_client *p_client) {
    p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleL(struct ts_client *p_client) {
    int l_parameterValue = p_client->hayesContext.commandParameterInt1;

    if(l_parameterValue >= 0 && l_parameterValue <= 3) {
        // Do nothing
    } else {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleM(struct ts_client *p_client) {
    int l_parameterValue = p_client->hayesContext.commandParameterInt1;

    if(l_parameterValue >= 0 && l_parameterValue <= 2) {
        // Do nothing
    } else {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleO(struct ts_client *p_client) {
    if(p_client->hayesContext.connected) {
        p_client->hayesContext.state = E_HAYESSTATE_DATA;
    } else {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleSGet(struct ts_client *p_client) {
    int l_registerNumber = p_client->hayesContext.commandParameterInt1;

    if(l_registerNumber > 12) {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
        return;
    }

    char l_buffer[4];

    sprintf(l_buffer, "%d", p_client->hayesContext.regS[l_registerNumber]);
    clientWriteString(p_client, l_buffer);
}

static void hayesHandleSSet(struct ts_client *p_client) {
    int l_registerNumber = p_client->hayesContext.commandParameterInt1;
    int l_registerValue = p_client->hayesContext.commandParameterInt2;

    if(l_registerNumber > 12) {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
        return;
    }

    if(l_registerValue > 255) {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }

    p_client->hayesContext.regS[l_registerNumber] = l_registerValue;
}

static void hayesHandleQ(struct ts_client *p_client) {
    int l_parameterValue = p_client->hayesContext.commandParameterInt1;

    if(l_parameterValue == 0) {
        p_client->hayesContext.quiet = false;
    } else if(l_parameterValue == 1) {
        p_client->hayesContext.quiet = true;
    } else {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleV(struct ts_client *p_client) {
    int l_parameterValue = p_client->hayesContext.commandParameterInt1;

    if(l_parameterValue == 0) {
        p_client->hayesContext.verbose = false;
    } else if(l_parameterValue == 1) {
        p_client->hayesContext.verbose = true;
    } else {
        p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
    }
}

static void hayesHandleX(struct ts_client *p_client) {
    p_client->hayesContext.commandResult = E_HAYESCOMMANDRESULT_ERROR;
}

static void hayesHandleZ(struct ts_client *p_client) {
    p_client->hayesContext.quiet = false;
    p_client->hayesContext.echo = true;
    p_client->hayesContext.verbose = true;
    p_client->hayesContext.connected = false;
    p_client->hayesContext.regS[0] = 0;
    p_client->hayesContext.regS[1] = 0;
    p_client->hayesContext.regS[2] = '+';
    p_client->hayesContext.regS[3] = '\r';
    p_client->hayesContext.regS[4] = '\n';
    p_client->hayesContext.regS[5] = '\b';
    p_client->hayesContext.regS[6] = 2;
    p_client->hayesContext.regS[7] = 50;
    p_client->hayesContext.regS[8] = 2;
    p_client->hayesContext.regS[9] = 6;
    p_client->hayesContext.regS[10] = 14;
    p_client->hayesContext.regS[11] = 95;
    p_client->hayesContext.regS[12] = 50;
}
