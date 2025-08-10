// code/game/cmds/osp/ospcmds.c
#include "ospcmds.h"

// OSP statsinfo base indices (C89-friendly defines)
#define OSP_IDX_VALID        0
#define OSP_IDX_SCORE        1
#define OSP_IDX_KILLS        2
#define OSP_IDX_DEATHS       3
#define OSP_IDX_SUICIDES     4
#define OSP_IDX_WINS         5
#define OSP_IDX_LOSSES       6
#define OSP_IDX_TEAMKILLS    7
#define OSP_IDX_DMG_GIVEN    8
#define OSP_IDX_DMG_RCVD     9
#define OSP_IDX_DMG_TEAM     10
#define OSP_IDX_CAPS         11
#define OSP_IDX_ASSIST       12
#define OSP_IDX_DEFENDS      13
#define OSP_IDX_RETURNS      14
#define OSP_IDX_TIME_MS      15
#define OSP_IDX_MH           16
#define OSP_IDX_GA           17
#define OSP_IDX_RA           18
/* OSP client code references index 20 for YA explicitly */
#define OSP_IDX_UNUSED19     19
#define OSP_IDX_YA           20
#define OSP_IDX_TEAM         2
#define OSP_IDX_WEAPON_MASK  21
#define OSP_IDX_UNKNOWN1     22
#define OSP_IDX_UNKNOWN2     23

// Minimal OSP "wstats" (statsinfo) generator based on current server-side stats
// NOTE: Index mapping follows common OSP conventions; untracked fields are zeroed
void osptest(gentity_t *ent){
    gclient_t *cl;
    gclient_t *targetCl;
    char msg[1024];
    int base[24];
    int i;
    unsigned weaponMask = 0;
    int w;
    int len;
    int firstW;
    int targetNum;

    if (!ent || !ent->client) return;

    /* Select current player or spectated player */
    targetCl = ent->client;
    if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
        targetNum = ent->client->sess.spectatorClient;
        if ( targetNum >= 0 && targetNum < level.maxclients &&
             level.clients[targetNum].pers.connected == CON_CONNECTED ) {
            targetCl = &level.clients[targetNum];
        }
    }
    cl = targetCl;
    trap_SendServerCommand(ent - g_entities, va("print \"^3current player: %s %d^7\n\"", cl->pers.netname, cl->ps.clientNum));

    // OSP base indices (best-effort mapping)
    for (i = 0; i < 24; ++i) base[i] = 0;
    /* Fill base fields per OSP definitions */
    base[OSP_IDX_VALID] = 1;
    base[OSP_IDX_SCORE] = cl->ps.persistant[PERS_SCORE];
    base[OSP_IDX_TEAM]  = cl->sess.sessionTeam;
    base[OSP_IDX_KILLS] = cl->kills;
    base[OSP_IDX_DEATHS] = cl->deaths;
    base[OSP_IDX_SUICIDES] = 0; /* not tracked separately */

    // Pack armor/health totals into WINS/LOSSES high word per OSP convention
    base[OSP_IDX_WINS] = (cl->sess.wins & 0xFFFF) | ((cl->armorPickedTotal & 0xFFFF) << 16);
    base[OSP_IDX_LOSSES] = (cl->sess.losses & 0xFFFF) | ((cl->healthPickedTotal & 0xFFFF) << 16);

    base[OSP_IDX_TEAMKILLS] = 0; /* not tracked */
    base[OSP_IDX_DMG_GIVEN] = cl->totalDamageGiven;
    base[OSP_IDX_DMG_RCVD] = cl->totalDamageTaken;
    base[OSP_IDX_DMG_TEAM] = 0;

    base[OSP_IDX_CAPS] = 0;
    base[OSP_IDX_ASSIST] = 0;
    base[OSP_IDX_DEFENDS] = 0;
    base[OSP_IDX_RETURNS] = 0;
    base[OSP_IDX_TIME_MS] = 0;

    base[OSP_IDX_MH] = cl->healthMegaCount;
    base[OSP_IDX_GA] = cl->armorShardCount; // treat GA as shards
    base[OSP_IDX_RA] = cl->armorRACount;
    base[OSP_IDX_YA] = cl->armorYACount;
    base[OSP_IDX_TEAM] = cl->sess.sessionTeam;

    /* Compute weapon mask for WP 1..9 (excluding NONE and GRAPPLE) */
    firstW = -1;
    for (w = WP_GAUNTLET; w <= WP_BFG; ++w) {
        int used = cl->perWeaponShots[w] || cl->perWeaponHits[w] ||
                   cl->perWeaponKills[w] || cl->perWeaponDeaths[w] ||
                   cl->perWeaponPickups[w] || cl->perWeaponDrops[w];
        if (used && w <= 9) {
            weaponMask |= (1u << w);
            if (firstW == -1) firstW = w;
        }
    }
    base[OSP_IDX_WEAPON_MASK] = (int)weaponMask;

    /* Build message with special placement of first weapon hits/atts into base[22]/[23] */
    len = Q_snprintf(msg, sizeof(msg), "statsinfo");
    for (i = 0; i < 24; ++i) {
        int v = base[i];
        if (firstW != -1) {
            unsigned hitsPackedFirst = ((cl->perWeaponDrops[firstW] & 0xFFFF) << 16) | (cl->perWeaponHits[firstW] & 0xFFFF);
            unsigned attsPackedFirst = ((cl->perWeaponPickups[firstW] & 0xFFFF) << 16) | (cl->perWeaponShots[firstW] & 0xFFFF);
            if (i == OSP_IDX_UNKNOWN1) v = (int)hitsPackedFirst;     /* index 22 */
            if (i == OSP_IDX_UNKNOWN2) v = (int)attsPackedFirst;     /* index 23 */
        }
        len += Q_snprintf(msg + len, sizeof(msg) - len, " %d", v);
        if (len >= (int)sizeof(msg)) break;
    }
    if (firstW != -1) {
        /* Append kills/deaths for the first weapon */
        len += Q_snprintf(msg + len, sizeof(msg) - len, " %u %u",
                          (cl->perWeaponKills[firstW] & 0xFFFF), (cl->perWeaponDeaths[firstW] & 0xFFFF));
        /* Append remaining weapons with full 4-field blocks */
        for (w = 1; w <= 9; ++w) {
            unsigned hitsPacked, attsPacked, kills, deaths;
            if (!(weaponMask & (1u << w))) continue;
            if (w == firstW) continue;
            hitsPacked = ((cl->perWeaponDrops[w] & 0xFFFF) << 16) | (cl->perWeaponHits[w] & 0xFFFF);
            attsPacked = ((cl->perWeaponPickups[w] & 0xFFFF) << 16) | (cl->perWeaponShots[w] & 0xFFFF);
            kills = (cl->perWeaponKills[w] & 0xFFFF);
            deaths = (cl->perWeaponDeaths[w] & 0xFFFF);
            len += Q_snprintf(msg + len, sizeof(msg) - len, " %u %u %u %u",
                              hitsPacked, attsPacked, kills, deaths);
            if (len >= (int)sizeof(msg)) break;
        }
    }

    /* Debug print of statsinfo for developer diagnostics */
    Com_Printf("@ osptest statsinfo: %s\n", msg);
    trap_SendServerCommand(ent - g_entities, msg);
}
