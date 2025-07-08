// code/game/cmds/pos.c
#include "cmds.h"

void getpos_f(gentity_t *ent) {
    vec3_t plr_vec;
    if (!ent->client)
        return;
    VectorCopy(ent->client->ps.origin, plr_vec);

    trap_SendServerCommand(ent - g_entities, va("print \"^3Your position: ^7%2.f %2.f %2.f\n\"", plr_vec[0], plr_vec[1], plr_vec[2]));
}

void setpos_f(gentity_t *ent) {
    vec3_t origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;
    if (!ent->client)
        return;

    if (ent->authed == qfalse){
        // trap_SendServerCommand( ent-g_entities, "print \"^3lol no\n\"");
        return;
    }

    if (trap_Argc() != 4){
        trap_SendServerCommand( ent-g_entities, "print \"^3Usage: setpos x y z\n\"");
        return;
    }

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

    angles[YAW] = ent->client->ps.viewangles[YAW];
    TeleportPlayer( ent, origin, angles );

    // VectorCopy(ent->client->ps.origin, plr_vec);

    // trap_SendServerCommand(ent - g_entities, "print \"^3Teleported...\n\"");
}