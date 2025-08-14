// code/game/cmds/osp/ospcmds.c
#include "ospcmds.h"

/* OSP stats indices to match CG_OSPShowStatsInfo (C89) */
#define OSP_STATS_UNKNOWN0   0
#define OSP_STATS_SCORE      1
#define OSP_STATS_TEAM       2
#define OSP_STATS_KILLS      3
#define OSP_STATS_DEATHS     4
#define OSP_STATS_SUCIDES    5
#define OSP_STATS_TEAM_KILLS 6
#define OSP_STATS_DMG_TEAM   7
#define OSP_STATS_DMG_GIVEN  8
#define OSP_STATS_DMG_RCVD   9
#define OSP_STATS_WINS       10
#define OSP_STATS_LOSSES     11
#define OSP_STATS_CAPS       12
#define OSP_STATS_ASSIST     13
#define OSP_STATS_DEFENCES   14
#define OSP_STATS_RETURNS    15
#define OSP_STATS_TIME       16
#define OSP_STATS_MH         17
#define OSP_STATS_GA         18
#define OSP_STATS_RA         19
#define OSP_STATS_YA         20
#define OSP_STATS_WEAPON_MASK 21
#define OSP_STATS_UNKNOWN1   22 /* first weapon: (drops<<16)|hits */
#define OSP_STATS_UNKNOWN2   23 /* first weapon: (pickups<<16)|attacks */

/* Build and send an OSP-compatible statsinfo line for the current (or targeted) client */
void Osp_Wstats(gentity_t *ent) {
    gclient_t *cl;
    gclient_t *targetCl;
    char msg[1024];
    int base[24];
    int i;
    unsigned int weaponMask;
    int w;
    int len;
    int firstW;
    int targetNum;
    int argc;

    if (!ent || !ent->client) {
        return;
    }

    /* Determine which client's stats to show (mirror g_cmds.c Cmd_Stats_f) */
    targetCl = NULL;
    argc = trap_Argc();
    if (argc >= 2) {
        char arg[MAX_TOKEN_CHARS];
        int isNum;
        int j;
        trap_Argv(1, arg, sizeof(arg));
        isNum = 1;
        for (j = 0; arg[j]; ++j) {
            if (arg[j] < '0' || arg[j] > '9') { isNum = 0; break; }
        }
        if (isNum) {
            targetNum = atoi(arg);
            if (targetNum >= 0 && targetNum < level.maxclients) {
                if (level.clients[targetNum].pers.connected == CON_CONNECTED) {
                    targetCl = &level.clients[targetNum];
                }
            }
        }
    }
    if (!targetCl) {
        if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
            targetNum = ent->client->sess.spectatorClient;
            if (targetNum >= 0 && targetNum < level.maxclients &&
                level.clients[targetNum].pers.connected == CON_CONNECTED) {
                targetCl = &level.clients[targetNum];
            }
        }
        if (!targetCl) {
            targetCl = ent->client;
        }
    }
    cl = targetCl;

    for (i = 0; i < 24; ++i) {
        base[i] = 0;
    }

    /* Base indices */
    base[OSP_STATS_UNKNOWN0] = 1; /* validity flag */
    base[OSP_STATS_SCORE] = cl->ps.persistant[PERS_SCORE];
    base[OSP_STATS_TEAM] = cl->sess.sessionTeam;
    base[OSP_STATS_KILLS] = cl->kills;
    base[OSP_STATS_DEATHS] = cl->deaths;
    base[OSP_STATS_SUCIDES] = 0; /* not tracked */

    /* Pack wins/losses (low16) with armor/health taken (high16) */
    base[OSP_STATS_WINS] = (cl->sess.wins & 0xFFFF) | ((cl->armorPickedTotal & 0xFFFF) << 16);
    base[OSP_STATS_LOSSES] = (cl->sess.losses & 0xFFFF) | ((cl->healthPickedTotal & 0xFFFF) << 16);

    base[OSP_STATS_TEAM_KILLS] = 0; /* not tracked */
    base[OSP_STATS_DMG_TEAM] = 0;   /* not tracked */
    base[OSP_STATS_DMG_GIVEN] = cl->totalDamageGiven;
    base[OSP_STATS_DMG_RCVD] = cl->totalDamageTaken;

    base[OSP_STATS_CAPS] = 0;
    base[OSP_STATS_ASSIST] = 0;
    base[OSP_STATS_DEFENCES] = 0;
    base[OSP_STATS_RETURNS] = 0;
    base[OSP_STATS_TIME] = 0;

    base[OSP_STATS_MH] = cl->healthMegaCount;
    base[OSP_STATS_GA] = cl->armorShardCount; /* best approximation */
    base[OSP_STATS_RA] = cl->armorRACount;
    base[OSP_STATS_YA] = cl->armorYACount;

    /* Weapon mask 1..9; pick the first included weapon for special packing */
    weaponMask = 0u;
    firstW = -1;
    for (w = WP_GAUNTLET; w <= WP_BFG; ++w) {
        int used;
        used = (cl->perWeaponShots[w] != 0) || (cl->perWeaponHits[w] != 0) ||
               (cl->perWeaponKills[w] != 0) || (cl->perWeaponDeaths[w] != 0) ||
               (cl->perWeaponPickups[w] != 0) || (cl->perWeaponDrops[w] != 0);
        if (used && w <= 9) {
            weaponMask |= (1u << w);
            if (firstW == -1) {
                firstW = w;
            }
        }
    }
    base[OSP_STATS_WEAPON_MASK] = (int)weaponMask;

    /* Fill packed hits/attacks for the first weapon into base[22]/[23] */
    if (firstW != -1) {
        base[OSP_STATS_UNKNOWN1] = ((cl->perWeaponDrops[firstW] & 0xFFFF) << 16) | (cl->perWeaponHits[firstW] & 0xFFFF);
        base[OSP_STATS_UNKNOWN2] = ((cl->perWeaponPickups[firstW] & 0xFFFF) << 16) | (cl->perWeaponShots[firstW] & 0xFFFF);
    }

    /* Build the command string */
    len = Com_sprintf(msg, sizeof(msg), "statsinfo");
    for (i = 0; i < 24; ++i) {
        len += Com_sprintf(msg + len, sizeof(msg) - len, " %d", base[i]);
        if (len >= (int)sizeof(msg)) {
            break;
        }
    }

    if (firstW != -1) {
        /* Append kills/deaths for the first weapon */
        len += Com_sprintf(msg + len, sizeof(msg) - len, " %u %u",
                           (unsigned)(cl->perWeaponKills[firstW] & 0xFFFF),
                           (unsigned)(cl->perWeaponDeaths[firstW] & 0xFFFF));

        /* Append other weapons in ascending order: hitsPacked attsPacked kills deaths */
        for (w = 1; w <= 9; ++w) {
            unsigned int hitsPacked;
            unsigned int attsPacked;
            unsigned int kills;
            unsigned int deaths;
            if (!(weaponMask & (1u << w))) {
                continue;
            }
            if (w == firstW) {
                continue;
            }
            hitsPacked = ((cl->perWeaponDrops[w] & 0xFFFF) << 16) | (cl->perWeaponHits[w] & 0xFFFF);
            attsPacked = ((cl->perWeaponPickups[w] & 0xFFFF) << 16) | (cl->perWeaponShots[w] & 0xFFFF);
            kills = (cl->perWeaponKills[w] & 0xFFFF);
            deaths = (cl->perWeaponDeaths[w] & 0xFFFF);
            len += Com_sprintf(msg + len, sizeof(msg) - len, " %u %u %u %u",
                               hitsPacked, attsPacked, kills, deaths);
            if (len >= (int)sizeof(msg)) {
                break;
            }
        }
    }

    trap_SendServerCommand(ent - g_entities, msg);
}
