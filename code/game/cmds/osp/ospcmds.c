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
    int base[MAX_QPATH];
    int i;
    unsigned int weaponMask;
    int w;
    int len;
    int firstW;
    int targetNum;
    int argc;
    int hasWeaponInfo;

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

    base[OSP_STATS_UNKNOWN0] = 1; // validity flag
    base[OSP_STATS_SCORE] = cl->ps.persistant[PERS_SCORE];
    base[OSP_STATS_TEAM] = cl->sess.sessionTeam;
    base[OSP_STATS_KILLS] = cl->kills;
    base[OSP_STATS_DEATHS] = cl->deaths;
    base[OSP_STATS_SUCIDES] = cl->suicides;

    if (g_freeze.integer)
    {
        base[OSP_STATS_WINS] = 0; // fixme
        base[OSP_STATS_LOSSES] = cl->sess.wins; // lol, but true
    }
    else 
    {
        base[OSP_STATS_WINS] = cl->sess.wins;
        base[OSP_STATS_LOSSES] = cl->sess.losses;
    }

    base[OSP_STATS_TEAM_KILLS] = cl->teamKills;
    base[OSP_STATS_DMG_TEAM] = cl->teamDamageGiven;
    base[OSP_STATS_DMG_GIVEN] = cl->totalDamageGiven;
    base[OSP_STATS_DMG_RCVD] = cl->totalDamageTaken;

    base[OSP_STATS_CAPS] = cl->ps.persistant[PERS_CAPTURES];
    base[OSP_STATS_ASSIST] = cl->ps.persistant[PERS_ASSIST_COUNT];
    base[OSP_STATS_DEFENCES] = cl->ps.persistant[PERS_DEFEND_COUNT];
    base[OSP_STATS_RETURNS] = 0; // flag returns (ctf)
    base[OSP_STATS_TIME] = 0; // flag time (ctf)

    base[OSP_STATS_MH] = cl->healthMegaCount;
    base[OSP_STATS_GA] = cl->armorGACount;
    base[OSP_STATS_RA] = cl->armorRACount;
    base[OSP_STATS_YA] = cl->armorYACount;

    weaponMask = 0u;
    for (w = 0; w < WP_NUM_WEAPONS; ++w) {
        int used = (cl->perWeaponShots[w] != 0) || (cl->perWeaponHits[w] != 0) ||
                  (cl->perWeaponKills[w] != 0) || (cl->perWeaponDeaths[w] != 0) ||
                  (cl->perWeaponPickups[w] != 0) || (cl->perWeaponDrops[w] != 0);
        if (used) {
            weaponMask |= (1u << w);
        }
    }
    base[OSP_STATS_WEAPON_MASK] = (int)weaponMask;

    base[OSP_STATS_UNKNOWN1] = 0;
    base[OSP_STATS_UNKNOWN2] = 0;

    len = Com_sprintf(msg, sizeof(msg), "statsinfo");
    for (i = 0; i < 21; ++i) {
        len += Com_sprintf(msg + len, sizeof(msg) - len, " %d", base[i]);
        if (len >= (int)sizeof(msg)) {
            break;
        }
    }

    len += Com_sprintf(msg + len, sizeof(msg) - len, "  ");

    hasWeaponInfo = 0;
    if (weaponMask != 0) {
        for (w = 0; w < WP_NUM_WEAPONS; ++w) {
            if (!(weaponMask & (1u << w))) continue;
            if (cl->perWeaponHits[w] || cl->perWeaponShots[w] || cl->perWeaponKills[w] || cl->perWeaponDeaths[w] || cl->perWeaponPickups /* || cl->perWeaponDrops */) {
                hasWeaponInfo = 1;
                break;
            }
        }
    }

    if (hasWeaponInfo) {
        len += Com_sprintf(msg + len, sizeof(msg) - len, " %d", base[OSP_STATS_WEAPON_MASK]);
        for (w = 0; w < WP_NUM_WEAPONS; ++w) {
            if (!(weaponMask & (1u << w))) continue;
            len += Com_sprintf(msg + len, sizeof(msg) - len, " %d %d %d %d",
                cl->perWeaponHits[w] + (65536 * cl->perWeaponDrops[w]),
                cl->perWeaponShots[w] + (65536 * cl->perWeaponPickups[w]),
                cl->perWeaponKills[w],
                cl->perWeaponDeaths[w]);
            if (len >= (int)sizeof(msg)) break;
        }
    }

    trap_SendServerCommand(ent - g_entities, msg);
}
