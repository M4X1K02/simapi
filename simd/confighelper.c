#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#include "confighelper.h"

static const char UDP_FORWARD_KEY[] = "udp_forward";
static const char UDP_FORWARD_KEY_IP[] = "ip";
static const char UDP_FORWARD_KEY_PORT[] = "port";
static const char UDP_FORWARD_DEFAULT_IP[] = "127.0.0.1";
static const int UDP_PORT_MIN = 1;
static const int UDP_PORT_MAX = 65535;


//int strcicmp(char const *a, char const *b)
//{
//    for (;; a++, b++) {
//        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
//        if (d != 0 || !*a)
//            return d;
//    }
//}


int getNumberOfConfigs(const char* config_file_str)
{
    config_t cfg;
    config_init(&cfg);
    if (!config_read_file(&cfg, config_file_str))
    {
        config_destroy(&cfg);
        return -1;
    }

    config_setting_t* config = NULL;
    config_setting_t* config_sims = NULL;

    config_sims = config_lookup(&cfg, "sims");
    int configs = config_setting_length(config_sims);

    config_destroy(&cfg);
    return configs;
}


int loadconfig(SimdSettings simds, int compat_info_size, GameCompatInfo* gc)
{
    int numdevices = 0;
    config_t cfg;
    config_init(&cfg);
    if (!config_read_file(&cfg, simds.configfile))
    {
        config_destroy(&cfg);
        return -1;
    }
    else
    {
        config_setting_t* config = NULL;
        config = config_lookup(&cfg, "sims");

        int i = 0;
        while (i<compat_info_size)
        {
            config_setting_t* config_sim = config_setting_get_elem(config, i);

            const char* sim_name = NULL;
            const char* launch_exe = NULL;
            const char* live_exe = NULL;

            // these first two are the two that are required at this point
            int launch_found = 0;
            launch_found = config_setting_lookup_string(config_sim, "launchexe", &launch_exe);

            if(launch_found == 0)
            {
                i++;
                continue;
            }


            int live_found = 0;
            live_found = config_setting_lookup_string(config_sim, "liveexe", &live_exe);

            int name_found = 0;
            name_found = config_setting_lookup_string(config_sim, "name", &sim_name);
            if(name_found == 0)
            {
                sim_name = "NoName";
            }

            int simapi_found = 0;
            int simapi = 0;
            simapi_found = config_setting_lookup_int(config_sim, "simapi", &simapi);

            int gameid_found = 0;
            int gameid = 0;
            gameid_found = config_setting_lookup_int(config_sim, "gameid", &gameid);

            int bridgedelay_found = 0;
            int bridgedelay = 5;
            bridgedelay_found = config_setting_lookup_int(config_sim, "bridgedelay", &bridgedelay);

            gc[i].LaunchExe = strdup(launch_exe);
            gc[i].LiveExe = strdup(live_exe);
            gc[i].Name = strdup(sim_name);
            gc[i].BridgeDelay = bridgedelay;
            gc[i].simapi = simapi;
            gc[i].GameId = gameid;

            i++;
        }
    }


    config_destroy(&cfg);

    return 0;
}

static int parse_forward_target(config_setting_t* item, UdpForwardTarget* out)
{
    if (item == NULL || out == NULL)
    {
        return -1;
    }
    if (!config_setting_is_group(item))
    {
        return -1;
    }

    int port = 0;
    if (config_setting_lookup_int(item, UDP_FORWARD_KEY_PORT, &port) == CONFIG_FALSE)
    {
        return -1;
    }
    if (port < UDP_PORT_MIN || port > UDP_PORT_MAX)
    {
        return -1;
    }

    const char* ip = UDP_FORWARD_DEFAULT_IP;
    config_setting_lookup_string(item, UDP_FORWARD_KEY_IP, &ip);
    if (ip == NULL || ip[0] == '\0')
    {
        return -1;
    }
    if (strlen(ip) >= UDP_FORWARD_IP_LEN)
    {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    if (inet_pton(AF_INET, ip, &out->dest.sin_addr) != 1)
    {
        return -1;
    }
    out->dest.sin_family = AF_INET;
    out->dest.sin_port = htons((uint16_t)port);
    memcpy(out->ip, ip, strlen(ip) + 1);
    out->port = (uint16_t)port;
    return 0;
}

int load_udp_forward(const char* config_file, SimdSettings* simds)
{
    if (config_file == NULL || simds == NULL)
    {
        return -1;
    }

    simds->udp_forward_count = 0;
    config_t cfg;
    config_init(&cfg);
    if (!config_read_file(&cfg, config_file))
    {
        config_destroy(&cfg);
        return -1;
    }

    config_setting_t* list = config_lookup(&cfg, UDP_FORWARD_KEY);
    if (list == NULL || !config_setting_is_list(list))
    {
        config_destroy(&cfg);
        return 0;
    }

    int count = config_setting_length(list);
    int i = 0;
    while (i < count && simds->udp_forward_count < UDP_FORWARD_MAX)
    {
        UdpForwardTarget target;
        if (parse_forward_target(config_setting_get_elem(list, i), &target) == 0)
        {
            simds->udp_forward[simds->udp_forward_count] = target;
            simds->udp_forward_count++;
        }
        i++;
    }

    config_destroy(&cfg);
    return 0;
}
