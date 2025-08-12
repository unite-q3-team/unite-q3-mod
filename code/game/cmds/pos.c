// code/game/cmds/pos.c
#include "cmds.h"

void getpos_f(gentity_t *ent) {
    vec3_t plr_vec;
    if (!ent->client) return;
    VectorCopy(ent->client->ps.origin, plr_vec);

    trap_SendServerCommand(ent - g_entities, va("print \"^3Your position: ^7%2.f %2.f %2.f\n\"", plr_vec[0], plr_vec[1], plr_vec[2]));
}

void setpos_f(gentity_t *ent) {
    vec3_t origin, angles;
    char buffer[MAX_TOKEN_CHARS];
    int argc, id1, id2;
    gentity_t *target1 = NULL, *target2 = NULL;
    int i;
    int zoffset = 100;

    if (!ent->client || !ent->authed) {
        return;
    }

    argc = trap_Argc();

    VectorClear(angles);

    if (argc == 2) {
        // setpos <id> — телепортируем себя к игроку
        trap_Argv(1, buffer, sizeof(buffer));
        id1 = atoi(buffer);
        if (id1 < 0 || id1 >= MAX_CLIENTS) {
            trap_SendServerCommand(ent - g_entities, "print \"^3Invalid ID\n\"");
            return;
        }

        target1 = &g_entities[id1];
        if (!target1->inuse || !target1->client) {
            trap_SendServerCommand(ent - g_entities, "print \"^3Player not found\n\"");
            return;
        }

        VectorCopy(target1->client->ps.origin, origin);
        angles[YAW] = ent->client->ps.viewangles[YAW];
        angles[PITCH] = ent->client->ps.viewangles[PITCH];
        origin[2] = origin[2] + zoffset;
        target1->speed = 0;
        TeleportPlayer(ent, origin, angles);

    } else if (argc == 3) {
        // setpos <id_src> <id_dest> — телепортируем одного игрока к другому
        trap_Argv(1, buffer, sizeof(buffer));
        id1 = atoi(buffer);
        trap_Argv(2, buffer, sizeof(buffer));
        id2 = atoi(buffer);

        if (id1 < 0 || id1 >= MAX_CLIENTS || id2 < 0 || id2 >= MAX_CLIENTS) {
            trap_SendServerCommand(ent - g_entities, "print \"^3Invalid ID\n\"");
            return;
        }

        target1 = &g_entities[id1];
        target2 = &g_entities[id2];

        if (!target1->inuse || !target1->client ||
            !target2->inuse || !target2->client) {
            trap_SendServerCommand(ent - g_entities, "print \"^3Player not found\n\"");
            return;
        }

        VectorCopy(target2->client->ps.origin, origin);
        origin[2] = origin[2] + zoffset;
        angles[PITCH] = target1->client->ps.viewangles[PITCH];
        angles[YAW] = target1->client->ps.viewangles[YAW];
        target1->speed = 0;
        TeleportPlayer(target1, origin, angles);

    } else if (argc == 4) {
        // setpos x y z — телепортируем себя по координатам
        for (i = 0; i < 3; i++) {
            trap_Argv(i + 1, buffer, sizeof(buffer));
            origin[i] = atof(buffer);
        }
        origin[2] = origin[2] + zoffset;
        angles[YAW] = ent->client->ps.viewangles[YAW];
        angles[PITCH] = ent->client->ps.viewangles[PITCH];
        target1->speed = 0;
        TeleportPlayer(ent, origin, angles);

    } else if (argc == 5) {
        // setpos <id> x y z — телепортируем указанного игрока по координатам
        trap_Argv(1, buffer, sizeof(buffer));
        id1 = atoi(buffer);
        if (id1 < 0 || id1 >= MAX_CLIENTS) {
            trap_SendServerCommand(ent - g_entities, "print \"^3Invalid ID\n\"");
            return;
        }

        target1 = &g_entities[id1];
        if (!target1->inuse || !target1->client) {
            trap_SendServerCommand(ent - g_entities, "print \"^3Player not found\n\"");
            return;
        }

        for (i = 0; i < 3; i++) {
            trap_Argv(i + 2, buffer, sizeof(buffer));
            origin[i] = atof(buffer);
        }

        angles[YAW] = target1->client->ps.viewangles[YAW];
        angles[PITCH] = target1->client->ps.viewangles[PITCH];
        target1->speed = 0;
        TeleportPlayer(target1, origin, angles);

    } else {
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage:\n\"");
        trap_SendServerCommand(ent - g_entities, "print \"^3  setpos x y z\n\"");
        trap_SendServerCommand(ent - g_entities, "print \"^3  setpos <id>\n\"");
        trap_SendServerCommand(ent - g_entities, "print \"^3  setpos <id_src> <id_dest>\n\"");
        trap_SendServerCommand(ent - g_entities, "print \"^3  setpos <id> x y z\n\"");
        return;
    }
}

/* Toggle CenterPrint coordinates display for the caller (requires auth) */
void poscp_f(gentity_t *ent) {
    if (!ent || !ent->client) return;
    if (!ent->authed) return;

    ent->client->pers.posCpEnabled = !ent->client->pers.posCpEnabled;
    if ( ent->client->pers.posCpEnabled ) {
        vec3_t p;
        /* If following someone, show their coords immediately */
        if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
            int targetClient = ent->client->sess.spectatorClient;
            gclient_t *tcl = NULL;
            if ( targetClient == FOLLOW_ACTIVE1 ) targetClient = level.follow1;
            else if ( targetClient == FOLLOW_ACTIVE2 ) targetClient = level.follow2;
            if ( (unsigned)targetClient < (unsigned)level.maxclients ) {
                tcl = &level.clients[targetClient];
            }
            if ( tcl && tcl->pers.connected == CON_CONNECTED )
                VectorCopy(tcl->ps.origin, p);
            else
                VectorCopy(ent->client->ps.origin, p);
        } else {
            VectorCopy(ent->client->ps.origin, p);
        }
        ent->client->pers.posCpNextTime = level.time; /* allow immediate push */
        trap_SendServerCommand(ent - g_entities, va("cp \"%2.f %2.f %2.f\"", p[0], p[1], p[2]));
    }
    trap_SendServerCommand(
        ent - g_entities,
        ent->client->pers.posCpEnabled
            ? "print \"^3poscp: ^2ON\n\""
            : "print \"^3poscp: ^1OFF\n\""
    );
}