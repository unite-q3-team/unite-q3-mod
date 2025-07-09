// code/game/svcmds/player.c
#include "svcmds.h"

void svAlignString(char *dest, int destSize, const char *src, int width, qboolean leftAlign) {
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

void Cmd_svPlrlist_f(void) {
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
        va(" Map: %s\n\n  ID : Players                          Nudge   Rate  Snaps  MOD  CC\n", g_mapname.string));
    Q_strcat(buffer, sizeof(buffer),
        "----------------------------------------------------------------------\n");

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
            strcpy(teamChar, "R");
            break;
        case TEAM_BLUE:
            strcpy(teamChar, "B");
            break;
        case TEAM_SPECTATOR:
            strcpy(teamChar, "S");
            break;
        case TEAM_FREE:
            strcpy(teamChar, "F");
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
            mod_str = "OSP";
        } else {
            mod_str = "---";
        }

        cc_str = "??";

        // Выравнивание данных
        svAlignString(rate_aligned, sizeof(rate_aligned), rate_buf, 6, qfalse);
        svAlignString(snaps_aligned, sizeof(snaps_aligned), snaps_buf, 4, qfalse);
        svAlignString(mod_aligned, sizeof(mod_aligned), mod_str, 6, qfalse);
        // AlignString(cc_aligned, sizeof(cc_aligned), cc_str, 6, qfalse);

        // Форматирование имени
        Q_FixNameWidth(cl->pers.netname, name_padded, 32);

        // Добавление строки в буфер
        Q_strcat(buffer, sizeof(buffer),
            va("%s %2d : %s %5d %6s   %4s %s %s\n",
                teamChar, i, name_padded, nudge, rate_aligned, snaps_aligned, mod_aligned, cc_str));
    }

        Q_strcat(buffer, sizeof(buffer),
            va("\nTotal players: %d\n", activePlayers));

    // SendServerCommandInChunks(ent, va("\n%s\n", buffer));
    trap_Print( va("\n%s\n", buffer));
}