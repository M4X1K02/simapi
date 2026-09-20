#ifndef _LOOPDATA_H
#define _LOOPDATA_H

#include <netinet/in.h>
#include <stdint.h>

#include <uv.h>
#include <simdata.h>
#include <simmapper.h>

#define UDP_FORWARD_MAX 8
#define UDP_FORWARD_IP_LEN 16

typedef struct
{
    char ip[UDP_FORWARD_IP_LEN];
    uint16_t port;
    struct sockaddr_in dest;
}
UdpForwardTarget;

typedef struct
{
    char* Name;
    int GameId;
    bool UseUDP;
    char* LaunchExe;
    char* LiveExe;
    int BridgeDelay;
    int simapi;
}
GameCompatInfo;

typedef struct
{
    bool poke;
    bool force_udp;
    bool auto_memmap;
    bool auto_bridge;
    bool daemon;
    bool notify;
    bool bridge_path_set;
    char* home_dir;
    char* configfile;
    char* pokesetting;
    char* targetvalue;
    UdpForwardTarget udp_forward[UDP_FORWARD_MAX];
    int udp_forward_count;
}
SimdSettings;

typedef struct
{
    uv_work_t req;
    SimulatorAPI sim;
    bool use_udp;
    bool simstate;
    bool uion;
    bool releasing;
    SimdSettings simds;
    int compat_info_size;
    pid_t bridge_pid;
    pid_t game_pid;
    GameCompatInfo* game_compat_info;
    SimData* simdata;
}
LoopData;

#endif
