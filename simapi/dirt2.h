#ifndef _DIRT2_H
#define _DIRT2_H

#include <stdbool.h>
#include "../include/dirt2data.h"

#define DIRT_RALLY_2_EXE "dirtrally2.exe"
/* Game emits to 10001 (dirt-udp-fanout). simd listens here so Moza LEDs can keep 20777. */
#define DIRT_RALLY_2_UDP_PORT 20779

typedef struct
{
    bool has_telemetry;
    struct dirt2_udp_packet dirt2_telemetry;
    float last_velX;
    float last_velY;
    float last_velZ;
    float last_runTime;
    bool has_last_vel;
}
Dirt2Map;

#endif

