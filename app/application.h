#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef VERSION
#define VERSION "vdev"
#endif

#include <bcl.h>


typedef enum
{
    PASSWD = 0,
    SSID = 1,
    ENCRYPTION = 2
}bc_wifi_command_t;


#endif // _APPLICATION_H
