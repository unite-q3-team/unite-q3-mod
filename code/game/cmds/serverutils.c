// code/game/cmds/serverutils.c
#include "cmds.h"

void Cmd_svfps_f(gentity_t *ent) {
    // sv_fps.integer
    trap_SendServerCommand(ent - g_entities, va("print \"^3Current server fps^1: ^2%d ^1[^2N/A^1]\n\"", sv_fps.integer));
    trap_SendServerCommand(ent - g_entities, va("print \"^3Level time time^1: ^2%d\n\"", level.time));
}
