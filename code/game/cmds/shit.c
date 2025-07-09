// code/game/cmds/shit.c
#include "cmds.h"

void hi_f(gentity_t *ent) {
    trap_SendServerCommand(-1, va("cp \"%s^3: ^3Hi!\"^3", ent->client->pers.netname));
}

void playsound_f(gentity_t *ent){
    char snd[MAX_TOKEN_CHARS];
    int i;
    // gclient_t *cl;
    gentity_t *victim;
    gentity_t *te;

    if (!ent->authed) {return;}

    if (trap_Argc() != 2){
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage:\n^3  sndplay <wav>\n\"");
        return;
    }
    trap_Argv(1, snd, sizeof(snd));
    trap_SendServerCommand(ent - g_entities, va("print \"^3Playing: %s\n\"", snd));

    for (i=0; i < level.num_entities; i++){
        victim = &g_entities[i];
        if (!victim->client) continue;
        te = G_TempEntity(ent->r.currentOrigin, EV_GLOBAL_SOUND);
        te->s.eventParm = G_SoundIndex(snd);
    }
}
