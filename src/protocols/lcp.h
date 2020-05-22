#ifndef __LCP_H__
#define __LCP_H__

#include <stddef.h>
#include <stdint.h>

#include <client.h>

void ppp_lcpSendFrame(client_t *client, uint8_t code, uint8_t identifier, uint8_t *buffer, size_t bufferSize);
void ppp_lcp_configurationRequestFrameReceived(client_t *client, uint8_t identifier, uint8_t *payloadBuffer, size_t payloadSize);
void ppp_lcpFrameReceived(client_t *client, uint8_t *lcpFrameBuffer, size_t lcpFrameSize);

#endif
