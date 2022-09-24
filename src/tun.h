#ifndef __TUN_H_INCLUDED__
#define __TUN_H_INCLUDED__

extern int tunOpen(char *p_deviceName);
extern int tunClose(int p_tunFile);

#endif
