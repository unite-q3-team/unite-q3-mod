// code/game/cmds/serverutils.c
#include "cmds.h"

void Cmd_svfps_f(gentity_t *ent) {
    // sv_fps.integer
    trap_SendServerCommand(ent - g_entities, va("print \"^3Current server fps^1: ^2%d ^1[^2%.2f^1 ^2msec^1]\n\"", sv_fps.integer, (1000.0f / sv_fps.value)));
    trap_SendServerCommand(ent - g_entities, va("print \"^3Level time time^1: ^2%d\n\"", level.time));
}

// restart map
void map_restart_f(gentity_t *ent){
    if (!ent->client || !ent->authed) return;

    trap_SendConsoleCommand(EXEC_APPEND, "map_restart 0\n");
}

