#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../../include/dirt2data.h"
#include "../simdata.h"
#include "../simmap.h"
#include "../simmapper.h"

#define DR2_RPM_SCALE 10.0f
#define DR2_MS_TO_KMH 3.6f
#define DR2_MIN_CHASSIS_SPEED_MS 0.5
#define DR2_WHEEL_COUNT 4
#define DR2_WHEEL_FL 0
#define DR2_WHEEL_FR 1
#define DR2_WHEEL_RL 2
#define DR2_WHEEL_RR 3
#define DR2_CAR_NAME "DR2"
#define DR2_CAR_NAME_SIZE 4

static int froundint(float f)
{
    return (int)trunc(nearbyint(f));
}

static double vector_dot(float ax, float ay, float az, float bx, float by, float bz)
{
    return (double)ax * (double)bx
         + (double)ay * (double)by
         + (double)az * (double)bz;
}

static double longitudinal_slip_ratio(double chassis_speed_ms, double wheel_speed_ms)
{
    if (chassis_speed_ms <= DR2_MIN_CHASSIS_SPEED_MS)
    {
        return 0.0;
    }

    return (chassis_speed_ms - wheel_speed_ms) / chassis_speed_ms;
}

/**
 * @brief Maps raw DiRT Rally 2.0 UDP telemetry to the universal SimData
 * structure.
 *
 * DiRT Rally 2.0 sends UDP packets in the "extradata=3" format. This function
 * extracts those fields and maps them to the common SimData format used by
 * SimAPI.
 *
 * Key mapping highlights:
 * - RPM: Scaled by 10.0 (internal game representation).
 * - Gear: Shifted by +1 to match SimAPI range (0=R, 1=N, 2=1st...).
 * - Euler Angles: Derived from the Forward and Right vectors provided in the
 * UDP packet.
 * - Local velocity: derived from world velocity dotted with the car axes.
 * - Wheel slip: chassis speed vs per-wheel linear speed (UDP is m/s, not rad/s).
 *
 * @param simdata The target SimData structure to populate.
 * @param simmap The simulator mapping context.
 * @param base Raw UDP packet pointer.
 */
void map_dirt_rally_2_data(SimData *simdata, SimMap *simmap, char* base)
{
    if (base != NULL)
    {
        struct dirt2_udp_packet *packet = (struct dirt2_udp_packet *)base;

        if (simmap != NULL)
        {
            memcpy(&simmap->dirt2.dirt2_telemetry, packet,
                   sizeof(struct dirt2_udp_packet));
            simmap->dirt2.has_telemetry = true;
        }

        // Basic telemetry
        simdata->rpms = froundint(packet->fields.engineRPM * DR2_RPM_SCALE);
        simdata->velocity = froundint(packet->fields.speed * DR2_MS_TO_KMH);

        // Gear mapping: game(-1..10) -> simapi(0..11)
        float raw_gear = packet->fields.gear;
        int gear_int = (int)raw_gear;

        if (gear_int < 0)
        {
            simdata->gear = 0; // Reverse
        }
        else
        {
            simdata->gear = (uint32_t)gear_int + 1;
        }

        simdata->maxrpm = froundint(packet->fields.maxRPM * DR2_RPM_SCALE);

        // Character representation of gear
        if (simdata->gear == 0)
        {
            strcpy(simdata->gearc, "R");
        }
        else
            if (simdata->gear == 1)
            {
                strcpy(simdata->gearc, "N");
            }
            else
            {
                sprintf(simdata->gearc, "%d", simdata->gear - 1);
            }

        // Input mapping (already 0.0 to 1.0)
        simdata->gas = (double)packet->fields.throttle;
        simdata->steer = (double)packet->fields.steering;
        simdata->brake = (double)packet->fields.brake;
        simdata->clutch = (double)packet->fields.clutch;
        simdata->abs = (double)packet->fields.antiLockBrakes;

        // World Velocities
        simdata->worldXvelocity = (double)packet->fields.velX;
        simdata->worldYvelocity = (double)packet->fields.velY;
        simdata->worldZvelocity = (double)packet->fields.velZ;

        // Orientation Vectors (DiRT Rally 2.0 extradata=3 provides Right and
        // Forward)
        float rx = packet->fields.rightX;
        float ry = packet->fields.rightY;
        float rz = packet->fields.rightZ;
        float fx = packet->fields.forwardX;
        float fy = packet->fields.forwardY;
        float fz = packet->fields.forwardZ;

        // Up Vector = Forward x Right (Cross Product)
        float ux = fy * rz - fz * ry;
        float uy = fz * rx - fx * rz;
        float uz = fx * ry - fy * rx;

        /* Local velocity (m/s), Assetto Corsa convention: X lateral, Y forward.
         * Leave Z at 0: the packet has no vertical g-force, and projecting
         * world-up into Zvelocity trips 1 m/s airborne gates on rally bumps.
         */
        simdata->Xvelocity = vector_dot(packet->fields.velX, packet->fields.velY,
                                        packet->fields.velZ, rx, ry, rz);
        simdata->Yvelocity = vector_dot(packet->fields.velX, packet->fields.velY,
                                        packet->fields.velZ, fx, fy, fz);
        simdata->Zvelocity = 0.0;

        // Store vectors in SimData for legacy support or debugging
        simdata->tyrecontact0[0] = (double)rx;
        simdata->tyrecontact0[1] = (double)ry;
        simdata->tyrecontact0[2] = (double)rz;
        simdata->tyrecontact1[0] = (double)ux;
        simdata->tyrecontact1[1] = (double)uy;
        simdata->tyrecontact1[2] = (double)uz;
        simdata->tyrecontact2[0] = (double)fx;
        simdata->tyrecontact2[1] = (double)fy;
        simdata->tyrecontact2[2] = (double)fz;

        // Convert vectors to Euler angles (Degrees)
        // 1. Yaw (Heading): Direction the car is facing in the XZ plane
        simdata->heading = atan2((double)fx, (double)fz) * (180.0 / M_PI);

        // 2. Pitch (Nose Up +)
        double pitch_input = fmax(-1.0f, fmin(1.0f, (double)fy));
        simdata->pitch = asin(pitch_input) * (180.0 / M_PI);

        // 3. Roll (Right side down +)
        simdata->roll = atan2((double)-ry, (double)uy) * (180.0 / M_PI);

        // Wheel linear speeds from the UDP packet are m/s, while tyreRPS is rad/s.
        // Store slip from chassis vs wheel linear speed so haptics do not need a tyre diameter.
        const double chassis_speed_ms = (double)packet->fields.speed;
        const double wheel_speed_ms[DR2_WHEEL_COUNT] = {
            [DR2_WHEEL_FL] = (double)packet->fields.wheelSpeedFL,
            [DR2_WHEEL_FR] = (double)packet->fields.wheelSpeedFR,
            [DR2_WHEEL_RL] = (double)packet->fields.wheelSpeedRL,
            [DR2_WHEEL_RR] = (double)packet->fields.wheelSpeedRR,
        };

        simdata->tyreRPS[DR2_WHEEL_FL] = 0.0;
        simdata->tyreRPS[DR2_WHEEL_FR] = 0.0;
        simdata->tyreRPS[DR2_WHEEL_RL] = 0.0;
        simdata->tyreRPS[DR2_WHEEL_RR] = 0.0;
        simdata->tyreslipratio[DR2_WHEEL_FL] =
            longitudinal_slip_ratio(chassis_speed_ms, wheel_speed_ms[DR2_WHEEL_FL]);
        simdata->tyreslipratio[DR2_WHEEL_FR] =
            longitudinal_slip_ratio(chassis_speed_ms, wheel_speed_ms[DR2_WHEEL_FR]);
        simdata->tyreslipratio[DR2_WHEEL_RL] =
            longitudinal_slip_ratio(chassis_speed_ms, wheel_speed_ms[DR2_WHEEL_RL]);
        simdata->tyreslipratio[DR2_WHEEL_RR] =
            longitudinal_slip_ratio(chassis_speed_ms, wheel_speed_ms[DR2_WHEEL_RR]);
        simdata->tyreslipangle[DR2_WHEEL_FL] = 0.0;
        simdata->tyreslipangle[DR2_WHEEL_FR] = 0.0;
        simdata->tyreslipangle[DR2_WHEEL_RL] = 0.0;
        simdata->tyreslipangle[DR2_WHEEL_RR] = 0.0;

        // Game Status
        // DiRT Rally 2.0 extradata=3 is typically only sent when the simulation is
        // active. We set status to ACTIVEPLAY even if runTime is 0 to support Rally
        // Cross countdowns where telemetry starts before the clock.
        if (packet->fields.runTime >= 0)
        {
            simdata->simstatus = SIMAPI_STATUS_ACTIVEPLAY;
        }
        else
        {
            simdata->simstatus = SIMAPI_STATUS_MENU;
        }

        // Lap and Progress
        simdata->lap = (uint32_t)packet->fields.lap;
        simdata->trackdistancearound = (double)packet->fields.trackLength;
        simdata->playerspline = (double)packet->fields.progress;

        // Position
        simdata->worldposx = (double)packet->fields.posX;
        simdata->worldposy = (double)packet->fields.posY;
        simdata->worldposz = (double)packet->fields.posZ;

        // Fluids
        simdata->fuel = (double)packet->fields.fuelInTank;

        // Brake Temperatures
        simdata->braketemp[DR2_WHEEL_FL] = (double)packet->fields.brakesTempFL;
        simdata->braketemp[DR2_WHEEL_FR] = (double)packet->fields.brakesTempFR;
        simdata->braketemp[DR2_WHEEL_RL] = (double)packet->fields.brakesTempRL;
        simdata->braketemp[DR2_WHEEL_RR] = (double)packet->fields.brakesTempRR;

        simdata->tyrepressure[DR2_WHEEL_FL] = (double)packet->fields.tyrePressureFL;
        simdata->tyrepressure[DR2_WHEEL_FR] = (double)packet->fields.tyrePressureFR;
        simdata->tyrepressure[DR2_WHEEL_RL] = (double)packet->fields.tyrePressureRL;
        simdata->tyrepressure[DR2_WHEEL_RR] = (double)packet->fields.tyrePressureRR;

        simdata->suspension[DR2_WHEEL_FL] = (double)packet->fields.suspFL;
        simdata->suspension[DR2_WHEEL_FR] = (double)packet->fields.suspFR;
        simdata->suspension[DR2_WHEEL_RL] = (double)packet->fields.suspRL;
        simdata->suspension[DR2_WHEEL_RR] = (double)packet->fields.suspRR;

        simdata->suspvelocity[DR2_WHEEL_FL] = (double)packet->fields.suspVelFL;
        simdata->suspvelocity[DR2_WHEEL_FR] = (double)packet->fields.suspVelFR;
        simdata->suspvelocity[DR2_WHEEL_RL] = (double)packet->fields.suspVelRL;
        simdata->suspvelocity[DR2_WHEEL_RR] = (double)packet->fields.suspVelRR;

        simdata->distance = (double)packet->fields.distance;
        simdata->position = (uint32_t)packet->fields.racePos;
        simdata->numlaps = (uint32_t)packet->fields.totalLaps;
        simdata->playerlaps = (uint32_t)packet->fields.lapsCompleted;
        simdata->sectorindex = (uint8_t)packet->fields.sector;
        simdata->sector1time = (double)packet->fields.sector1Time;
        simdata->sector2time = (double)packet->fields.sector2Time;
        simdata->fuelcapacity = (double)packet->fields.fuelCapacity;
        simdata->idlerpm = froundint(packet->fields.idleRPM * DR2_RPM_SCALE);
        simdata->maxgears = (uint32_t)packet->fields.maxGears;

        // Timing
        simdata->currentlapinseconds = (uint32_t)packet->fields.lapTime;
        simdata->lastlapinseconds = (uint32_t)packet->fields.lastLapTime;

        // Splitting seconds into LapTime
        simdata->currentlap.hours = (uint32_t)(packet->fields.lapTime / 3600);
        simdata->currentlap.minutes =
            (uint32_t)(((int)packet->fields.lapTime % 3600) / 60);
        simdata->currentlap.seconds = (uint32_t)((int)packet->fields.lapTime % 60);
        simdata->currentlap.fraction =
            (uint32_t)((packet->fields.lapTime - floor(packet->fields.lapTime)) *
                       1000);

        simdata->lastlap.hours = (uint32_t)(packet->fields.lastLapTime / 3600);
        simdata->lastlap.minutes =
            (uint32_t)(((int)packet->fields.lastLapTime % 3600) / 60);
        simdata->lastlap.seconds = (uint32_t)((int)packet->fields.lastLapTime % 60);
        simdata->lastlap.fraction = (uint32_t)((packet->fields.lastLapTime -
                                                floor(packet->fields.lastLapTime)) *
                                               1000);

        strncpy(simdata->car, DR2_CAR_NAME, DR2_CAR_NAME_SIZE);
    }
}
