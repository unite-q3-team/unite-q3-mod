// code/game/cmds/testcmd.h
#include "../g_local.h"

void Cmd_NewTest_f(gentity_t *ent);
void Cmd_Plrlist_f(gentity_t *ent);
void Cmd_From_f(gentity_t *ent);
void hi_f(gentity_t *ent);
void getpos_f(gentity_t *ent);
void setpos_f(gentity_t *ent);
void poscp_f(gentity_t *ent);

/* disabled commands subsystem */
void DC_Init(void);
qboolean DC_IsDisabled(const char *cmdName);
void hi_f(gentity_t *ent);
void checkauth_f(gentity_t *ent);
void plsauth_f(gentity_t *ent);
void deauth_f(gentity_t *ent);
void killplayer_f(gentity_t *ent);
void Cmd_svfps_f(gentity_t *ent);
void fteam_f(gentity_t *ent);
void shuffle_f(gentity_t *ent);
qboolean Shuffle_Perform( const char *mode );
void spawns_f(gentity_t *ent);
void spawnadd_f(gentity_t *ent);
void spawnrm_f(gentity_t *ent);
void spawnsave_f(gentity_t *ent);
void spawnreload_f(gentity_t *ent);
void spawnundo_f(gentity_t *ent);
void spawnredo_f(gentity_t *ent);
void spawnyaw_f(gentity_t *ent);
void spawnpitch_f(gentity_t *ent);
void spawnscp_f(gentity_t *ent);
void spawnang_f(gentity_t *ent);

/* from itemreplace.c */
void listitems_f(gentity_t *ent);
void items_f(gentity_t *ent);
void irreload_f(gentity_t *ent);
void irlist_f(gentity_t *ent);
void irmatchcp_f(gentity_t *ent);
void iradd_f(gentity_t *ent);
void irrm_f(gentity_t *ent);
void irrepl_f(gentity_t *ent);
void irsave_f(gentity_t *ent);

/* weapons.c */
void Weapons_Init(void);
void wtreload_f(gentity_t *ent);
void wtlist_f(gentity_t *ent);
void wtsave_f(gentity_t *ent);
void irmove_f(gentity_t *ent);
void irundo_f(gentity_t *ent);
void irredo_f(gentity_t *ent);
void irrespawn_f(gentity_t *ent);

static int GetUserinfoInt(const char *userinfo, const char *key, int defaultValue) {
    const char *val = Info_ValueForKey(userinfo, key);
    if (!val || !val[0])
        return defaultValue;
    return atoi(val);
}

static const char *GetUserinfoString(const char *userinfo, const char *key, const char *defaultValue) {
    const char *val = Info_ValueForKey(userinfo, key);
    if (!val || !val[0])
        return defaultValue;
    return val;
}
