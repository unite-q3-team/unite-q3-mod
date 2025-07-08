// code/game/clcmds/testcmd.h
#include "../g_local.h"

void Cmd_NewTest_f(gentity_t *ent);
void Cmd_Plrlist_f(gentity_t *ent);
void Cmd_From_f(gentity_t *ent);
void getpos_f(gentity_t *ent);
void setpos_f(gentity_t *ent);
void hi_f(gentity_t *ent);
void checkauth_f(gentity_t *ent);
void plsauth_f(gentity_t *ent);
void deauth_f(gentity_t *ent);

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
