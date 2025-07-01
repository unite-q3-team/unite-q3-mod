// code/game/clcmds/testcmd.c
#include "testcmd.h"

void Cmd_NewTest_f(gentity_t *ent) {
    trap_SendServerCommand(ent - g_entities, "print \"^3[DEBUG]^7 Command 'damn' executed!\n\"");
}
