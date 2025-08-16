// code/game/clcmds/testcmd.h
#include "../g_local.h"

void Cmd_NewTestdsadsa_f(void);
void Cmd_svPlrlist_f(void);

/* announcer server cmds (implemented in cmds/announcer.c) */
void Svcmd_AnnouncerReload_f( void );
void Svcmd_AnnouncerList_f( void );
void Svcmd_AnnouncerEnable_f( void );

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
