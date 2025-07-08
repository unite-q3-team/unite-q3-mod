// code/game/cmds/player.c
#include "cmds.h"

void AlignString(char *dest, int destSize, const char *src, int width, qboolean leftAlign) {
    int len;
    int pad;

    len = strlen(src);
    if (len > width) len = width;
    pad = width - len;

    if (leftAlign) {
        Q_strncpyz(dest, src, len + 1);
        if (pad > 0) memset(dest + len, ' ', pad);
    } else {
        if (pad > 0) memset(dest, ' ', pad);
        Q_strncpyz(dest + pad, src, len + 1);
    }
    dest[width] = '\0';
}


void Cmd_Plrlist_f(gentity_t *ent) {
    char buffer[8192];
    int i;
    gclient_t *cl;
    char userinfo[MAX_INFO_STRING];
    char name_padded[MAX_QPATH];
    char rate_buf[16];
    char snaps_buf[16];
    char cc_buf[16];
    const char *osp_str;
    const char *mod_str;
    const char *cc_str;
    char rate_aligned[7];
    char snaps_aligned[5];
    char mod_aligned[7];
    char cc_aligned[5];
    char teamChar[4];
    int activePlayers = 0;
    int nudge;

    // Инициализация буфера
    buffer[0] = '\0';
    Q_strcat(buffer, sizeof(buffer),
        va(" ^3Map^7 ^1: ^2%s\n\n  ^3ID ^1: ^3Players                          Nudge   Rate  Snaps  MOD  CC\n", g_mapname.string));
    Q_strcat(buffer, sizeof(buffer),
        "^1----------------------------------------------------------------------\n");

    // Основной цикл
    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];

        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        activePlayers++;

        // Определение команды
        switch (cl->sess.sessionTeam) {
        case TEAM_RED:
            strcpy(teamChar, "^1R");
            break;
        case TEAM_BLUE:
            strcpy(teamChar, "^4B");
            break;
        case TEAM_SPECTATOR:
            strcpy(teamChar, "^3S");
            break;
        case TEAM_FREE:
            strcpy(teamChar, "^7F");
            break;
        default:
            strcpy(teamChar, " ");
            break;
        }

        trap_GetUserinfo(i, userinfo, sizeof(userinfo));

        // Получение данных
        nudge = GetUserinfoInt(userinfo, "cl_timeNudge", 0);

        strncpy(rate_buf, GetUserinfoString(userinfo, "rate", "0"), sizeof(rate_buf) - 1);
        rate_buf[sizeof(rate_buf) - 1] = '\0';

        strncpy(snaps_buf, GetUserinfoString(userinfo, "snaps", "0"), sizeof(snaps_buf) - 1);
        snaps_buf[sizeof(snaps_buf) - 1] = '\0';

        osp_str = GetUserinfoString(userinfo, "osp_client", "");
        if (osp_str[0] != '\0') {
            mod_str = "^3OSP";
        } else {
            mod_str = "^1---";
        }

        cc_str = "^1??";

        // Выравнивание данных
        AlignString(rate_aligned, sizeof(rate_aligned), rate_buf, 6, qfalse);
        AlignString(snaps_aligned, sizeof(snaps_aligned), snaps_buf, 4, qfalse);
        AlignString(mod_aligned, sizeof(mod_aligned), mod_str, 6, qfalse);
        // AlignString(cc_aligned, sizeof(cc_aligned), cc_str, 6, qfalse);

        // Форматирование имени
        Q_FixNameWidth(cl->pers.netname, name_padded, 32);

        // Добавление строки в буфер
        Q_strcat(buffer, sizeof(buffer),
            va("%s ^7%2d ^1:^7 %s ^7%5d %6s   %4s %s %s\n",
                teamChar, i, name_padded, nudge, rate_aligned, snaps_aligned, mod_aligned, cc_str));
    }

        Q_strcat(buffer, sizeof(buffer),
            va("\n^3Total players: ^7%d\n", activePlayers));

    SendServerCommandInChunks(ent, va("\n%s\n", buffer));
}

void Cmd_From_f(gentity_t *ent){

}

void killplayer_f(gentity_t *ent){
    char buf[MAX_TOKEN_CHARS];
    gentity_t *victim;

    if (!ent->client) return;

    if (trap_Argc() != 2) {
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage: killplayer <id>\n\"");
        return;
    }

    trap_Argv(1, buf, sizeof(buf));
    victim = &g_entities[atoi(buf)];

    if (!victim->client){
        trap_SendServerCommand(ent - g_entities, "print \"^1Invalid client!\n\"");
        return;
    }

	if ((g_freeze.integer && is_spectator(victim->client)) || (!g_freeze.integer && victim->client->sess.sessionTeam == TEAM_SPECTATOR)) {
        trap_SendServerCommand(ent - g_entities, "print \"^1Can't kill a spectator!\n\"");
		return;
	}

    if (victim->health <= 0){
        trap_SendServerCommand(ent - g_entities, "print \"^1Can't kill already dead person\n\"");
        return;
    }

    victim->flags &= ~FL_GODMODE;
    victim->client->ps.stats[STAT_HEALTH] = victim->health = -999;

    player_die(victim, victim, victim, 100000, (g_freeze.integer ? MOD_BFG_SPLASH : MOD_SUICIDE));
}