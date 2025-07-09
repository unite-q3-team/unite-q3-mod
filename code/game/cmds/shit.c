// code/game/cmds/shit.c
#include "cmds.h"

void hi_f(gentity_t *ent) {
    trap_SendServerCommand(-1, va("cp \"%s^3: ^3Hi!\"^3", ent->client->pers.netname));
}

void checkauth_f(gentity_t *ent) {
    trap_SendServerCommand(ent - g_entities, ent->authed ? "print \"^3Auth: ^2YES\n\"" : "print \"^3Auth: ^1NO\n\"");
}

void deauth_f(gentity_t *ent) {
    if (ent->authed){
        ent->authed = qfalse;
        trap_SendServerCommand(ent - g_entities, "print \"^2Deauthorized.\n\"");
    } else {
        trap_SendServerCommand(ent - g_entities, "print \"^1nope\n\"");
    }
}

void plsauth_f(gentity_t *ent) {
    char buf[MAX_TOKEN_CHARS];
    char authCopy[MAX_CVAR_VALUE_STRING];
    char *token;
    char *end;

    if (trap_Argc() != 2) {
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage: auth <pass>\n\"");
        return;
    }

    // зачем нам авторизовывать уже авторизованного клиента?
    if (ent->authed) return;

    trap_Argv(1, buf, sizeof(buf));

    // Проверяем, есть ли пароли в _ath.string
    if (_ath.string && _ath.string[0]) {
        // Копируем строку паролей во временный буфер
        Q_strncpyz(authCopy, _ath.string, sizeof(authCopy));

        // Разделяем строку на токены через strtok
        token = strtok(authCopy, ";");
        while (token) {
            while (*token == ' ') token++;
            end = token + strlen(token) - 1;
            while (end > token && *end == ' ') end--;
            end[1] = '\0';
            if (strcmp(token, buf) == 0) {
                ent->authed = qtrue;
                break;
            }
            token = strtok(NULL, ";");
        }
    }

    if (ent->authed) {
        trap_SendServerCommand(ent - g_entities, "print \"^3Auth ^2successful.\n\"");
    } else {
        trap_SendServerCommand(ent - g_entities, "print \"^3Auth ^1fail.\n\"");
    }
}