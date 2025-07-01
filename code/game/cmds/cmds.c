// code/game/clcmds/cmds.c
#include "cmds.h"

void Cmd_NewTest_f(gentity_t *ent) {
    trap_SendServerCommand(ent - g_entities, "print \"^3[DEBUG]^7 Command executed!\n\"");
}
