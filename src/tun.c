#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int tunOpen(char *p_deviceName) {
    struct ifreq l_ifreq;

    int l_tunFile = open("/dev/net/tun", O_RDWR);

    if(l_tunFile < 0) {
        return l_tunFile;
    }

    memset(&l_ifreq, 0, sizeof(l_ifreq));

    l_ifreq.ifr_flags = IFF_TUN;

    if(*p_deviceName) {
        strncpy(l_ifreq.ifr_name, p_deviceName, IFNAMSIZ - 1);
    }

    int err = ioctl(l_tunFile, TUNSETIFF, &l_ifreq);

    if(err) {
        close(l_tunFile);
        return err;
    }

    strcpy(p_deviceName, l_ifreq.ifr_name);

    return l_tunFile;
}

int tunClose(int p_tunFile) {
    return close(p_tunFile);
}
