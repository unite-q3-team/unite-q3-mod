// code/game/cmds/osp/ospcmds.c
#include "ospcmds.h"

void osptest(gentity_t *ent){
    trap_SendServerCommand(ent - g_entities, "print \"^3xd\n\"");
    trap_SendServerCommand(ent - g_entities, "statsinfo 1");
}
