// code/game/cmds/cmds.c
#include "cmds.h"

void Cmd_NewTest_f(gentity_t *ent) {
	int filelen;
	char path[MAX_QPATH];
	fileHandle_t filterFileHandle;
	char* fileContent;
	const char* ptr;
    const char* filename = "geoip";

    Com_sprintf(path, MAX_QPATH, "%s.dat", filename);

    filelen = trap_FS_FOpenFile(path, &filterFileHandle, FS_READ);

	if (filelen < 0 || !filterFileHandle)
	{
		Com_Printf("^1geoip file %s not found\n", path);
		return;
	}

	fileContent = G_Alloc(filelen + 1);
	// OSP_MEMORY_CHECK(fileContent);

    trap_FS_FCloseFile(filterFileHandle);

    trap_SendServerCommand(ent - g_entities, "print \"^3[DEBUG]^7 Command executed!\n\"");
}
