// code/game/cmds/votesystem.c
#include "cmds.h"

/* Local map cache for vote helpers */
static int vs_cachedMapCount = 0;
static char vs_cachedMaps[512][64];

static void VS_EnsureMapListCache(void) {
    char listbuf[8192];
    int count;
    int i;
    int pos;
    if ( vs_cachedMapCount > 0 ) {
        return;
    }
    vs_cachedMapCount = 0;
    count = trap_FS_GetFileList( "maps", ".bsp", listbuf, sizeof(listbuf) );
    pos = 0;
    for ( i = 0; i < count && vs_cachedMapCount < 512; ++i ) {
        char *nm;
        int nlen;
        if ( pos >= (int)sizeof(listbuf) ) {
            break;
        }
        nm = &listbuf[pos];
        nlen = (int)strlen( nm );
        if ( nlen > 0 ) {
            char tmp[64];
            Q_strncpyz( tmp, nm, sizeof(tmp) );
            nlen = (int)strlen( tmp );
            if ( nlen > 4 && !Q_stricmp( tmp + nlen - 4, ".bsp" ) ) {
                tmp[nlen - 4] = '\0';
            }
            Q_strncpyz( vs_cachedMaps[vs_cachedMapCount], tmp, sizeof(vs_cachedMaps[0]) );
            vs_cachedMapCount++;
        }
        pos += (int)strlen( nm ) + 1;
    }
}

static qboolean VS_GetMapNameByIndex( int indexOneBased, char *out, int outSize ) {
    VS_EnsureMapListCache();
    if ( indexOneBased <= 0 || indexOneBased > vs_cachedMapCount ) {
        return qfalse;
    }
    Q_strncpyz( out, vs_cachedMaps[indexOneBased - 1], outSize );
    return qtrue;
}

static void VS_PrintMapList( gentity_t *ent ) {
    char row[256];
    char buf[MAX_STRING_CHARS];
    int len;
    int perRow;
    int i;
    VS_EnsureMapListCache();
    buf[0] = '\0';
    len = 0;
    perRow = 3;
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^2Available Maps:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7---------------\n" );
    row[0] = '\0';
    for ( i = 0; i < vs_cachedMapCount; ++i ) {
        char cell[96];
        Com_sprintf( cell, sizeof(cell), "^7%3d.^7 ^2%-16s^7", i + 1, vs_cachedMaps[i] );
        Q_strcat( row, sizeof(row), cell );
        if ( ((i + 1) % perRow) == 0 || i + 1 == vs_cachedMapCount ) {
            len += Com_sprintf( buf + len, sizeof(buf) - len, "%s\n", row );
            row[0] = '\0';
            if ( len > (int)sizeof(buf) - 256 ) {
                trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
                buf[0] = '\0';
                len = 0;
            }
        } else {
            Q_strcat( row, sizeof(row), "  " );
        }
    }
    if ( len > 0 ) {
        trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
    }
}

static const char *voteCommands[] = {
    "map_restart",
    "map",
    "rotate",
    "nextmap",
    "kick",
    "clientkick",
    "g_gametype",
    "g_freeze",
    "instagib",
    "quad",
    "timelimit",
    "fraglimit",
    "capturelimit"
};

static void Cmd_CV_HelpList( gentity_t *ent ) {
    char buf[MAX_STRING_CHARS];
    int len;
    int gt;
    char gtLine[256];
    buf[0] = '\0';
    len = 0;
    len += Com_sprintf( buf + len, sizeof(buf) - len, "\n^2Callvote Commands:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7------------------\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5map^7                  [%s]\n", g_mapname.string );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5map_restart^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5nextmap^7\n" );
    gt = g_gametype.integer;
    {
        const char *ffa;
        const char *duel;
        const char *tdm;
        const char *ctf;
        ffa = (gt == GT_FFA) ? "^1ffa(0)^7"  : "ffa(0)";
        duel= (gt == GT_TOURNAMENT) ? "^1duel(1)^7" : "duel(1)";
        tdm = (gt == GT_TEAM) ? "^1tdm(3)^7"  : "tdm(3)";
        ctf = (gt == GT_CTF) ? "^1ctf(4)^7"  : "ctf(4)";
        Com_sprintf( gtLine, sizeof(gtLine), "^5g_gametype^7           [%s|%s|%s|%s|#]\n", ffa, duel, tdm, ctf );
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", gtLine );
    }
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5instagib^7             [%d]\n", g_instagib.integer );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5g_freeze^7             [%d]\n", g_freeze.integer );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5quad^7                 [%d]\n", trap_Cvar_VariableIntegerValue("disable_item_quad") ? 0 : 1 );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5timelimit^7            [%d]\n", g_timelimit.integer );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5fraglimit^7            [%d]\n", g_fraglimit.integer );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^5capturelimit^7         [%d]\n\n", g_capturelimit.integer );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7Usage: ^3\\callvote <command> [arg]^7\n" );
    trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
}

static qboolean ValidVoteCommand( int clientNum, char *command ) {
    char buf[MAX_CVAR_VALUE_STRING];
    char *base;
    char *s;
    int i;
    if ( strchr( command, ';' ) || strchr( command, '\n' ) || strchr( command, '\r' ) ) {
        trap_SendServerCommand( clientNum, "print \"Invalid vote command.\n\"" );
        return qfalse;
    }
    base = command;
    s = buf;
    while ( *command != '\0' && *command != ' ' ) {
        *s = *command; s++; command++;
    }
    *s = '\0';
    while ( *command == ' ' || *command == '\t' ) {
        command++;
    }
    for ( i = 0; i < (int)ARRAY_LEN( voteCommands ); i++ ) {
        if ( !Q_stricmp( buf, voteCommands[i] ) ) {
            break;
        }
    }
    if ( i == (int)ARRAY_LEN( voteCommands ) ) {
        Cmd_CV_HelpList( g_entities + clientNum );
        return qfalse;
    }
    if ( Q_stricmp( buf, "g_gametype" ) == 0 ) {
        int gt;
        gt = -1;
        if ( command[0] == '\0' ) {
            trap_SendServerCommand( clientNum, va( "print \"Usage: g_gametype <ffa|duel|tdm|ctf|#> (current %d)\n\"", g_gametype.integer ) );
            return qfalse;
        }
        if ( !Q_stricmp( command, "ffa" ) ) gt = GT_FFA;
        else if ( !Q_stricmp( command, "duel" ) ) gt = GT_TOURNAMENT;
        else if ( !Q_stricmp( command, "tdm" ) ) gt = GT_TEAM;
        else if ( !Q_stricmp( command, "ctf" ) ) gt = GT_CTF;
        else gt = atoi( command );
        if ( gt == GT_SINGLE_PLAYER || gt < GT_FFA || gt >= GT_MAX_GAME_TYPE ) {
            trap_SendServerCommand( clientNum, va( "print \"Invalid gametype %i.\n\"", gt ) );
            return qfalse;
        }
        BG_sprintf( base, "g_gametype %i; map_restart", gt );
        return qtrue;
    }
    if ( Q_stricmp( buf, "map" ) == 0 ) {
        int isNum;
        int j;
        isNum = 1;
        for ( j = 0; command[j]; ++j ) {
            if ( command[j] < '0' || command[j] > '9' ) {
                isNum = 0; break;
            }
        }
        if ( isNum && command[0] != '\0' ) {
            int want;
            char mapname[64];
            want = atoi( command );
            if ( VS_GetMapNameByIndex( want, mapname, sizeof(mapname) ) ) {
                BG_sprintf( base, "map %s", mapname );
                return qtrue;
            } else {
                trap_SendServerCommand( clientNum, va( "print \"No such map index: %s.\n\"", command ) );
                return qfalse;
            }
        }
        if ( !G_MapExist( command ) ) {
            trap_SendServerCommand( clientNum, va( "print \"No such map on server: %s.\n\"", command ) );
            return qfalse;
        }
        return qtrue;
    }
    if ( Q_stricmp( buf, "instagib" ) == 0 ) {
        if ( command[0] == '\0' ) {
            trap_SendServerCommand( clientNum, va("print \"Current instagib: %d\n\"", g_instagib.integer) );
            return qfalse;
        }
        if ( !(command[0] == '0' || command[0] == '1') || command[1] != '\0' ) {
            trap_SendServerCommand( clientNum, "print \"Usage: instagib <0|1>\n\"" );
            return qfalse;
        }
        {
            int v;
            v = (command[0] == '1') ? 1 : 0;
            BG_sprintf( base, "g_instagib %d; map_restart", v );
        }
        return qtrue;
    }
    if ( Q_stricmp( buf, "g_freeze" ) == 0 ) {
        int v;
        if ( command[0] == '\0' ) {
            v = g_freeze.integer ? 0 : 1;
        } else {
            if ( !(command[0] == '0' || command[0] == '1') || command[1] != '\0' ) {
                trap_SendServerCommand( clientNum, "print \"Usage: g_freeze <0|1> (or no arg to toggle)\n\"" );
                return qfalse;
            }
            v = (command[0] == '1') ? 1 : 0;
        }
        BG_sprintf( base, "g_freeze %d; map_restart", v );
        return qtrue;
    }
    if ( Q_stricmp( buf, "quad" ) == 0 ) {
        if ( command[0] == '\0' ) {
            int enabled;
            enabled = trap_Cvar_VariableIntegerValue( "disable_item_quad" ) ? 0 : 1;
            trap_SendServerCommand( clientNum, va("print \"Current quad: %d\n\"", enabled) );
            return qfalse;
        }
        if ( !(command[0] == '0' || command[0] == '1') || command[1] != '\0' ) {
            trap_SendServerCommand( clientNum, "print \"Usage: quad <0|1>\\n\"" );
            return qfalse;
        }
        {
            int wantEnable;
            int disableVal;
            wantEnable = (command[0] == '1') ? 1 : 0;
            disableVal = wantEnable ? 0 : 1;
            BG_sprintf( base, "set disable_item_quad %d; map_restart", disableVal );
        }
        return qtrue;
    }
    if ( Q_stricmp( buf, "nextmap" ) == 0 ) {
        strcpy( base, "rotate" );
    }
    return qtrue;
}

void Cmd_CallVote_f( gentity_t *ent ) {
    int i;
    int n;
    char arg[MAX_STRING_TOKENS];
    char *argn[4];
    char cmd[MAX_STRING_TOKENS];
    char *s;
    if ( trap_Argc() == 1 ) {
        Cmd_CV_HelpList( ent );
        return;
    } else if ( trap_Argc() == 2 ) {
        trap_Argv( 1, cmd, sizeof( cmd ) );
        if ( Q_stricmp( cmd, "map" ) == 0 ) {
            VS_PrintMapList( ent );
            return;
        }
    }
    if ( !g_allowVote.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here.\n\"" );
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent-g_entities, "print \"^1! ^3Spectators cannot call votes.\n\"" );
        return;
    }
    if ( level.voteTime ) {
        trap_SendServerCommand( ent-g_entities, "print \"A vote is already in progress.\n\"" );
        return;
    }
    if ( level.voteExecuteTime || level.restarted ) {
        trap_SendServerCommand( ent-g_entities, "print \"Previous vote command is waiting execution^1.^7\n\"" );
        return;
    }
    if ( ent->client->pers.voteCount >= g_voteLimit.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"You have called the maximum number of votes.\n\"" );
        return;
    }
    arg[0] = '\0';
    s = arg;
    for ( i = 1; i < trap_Argc(); i++ ) {
        if ( arg[0] ) {
            s = Q_stradd( s, " " );
        }
        trap_Argv( i, cmd, sizeof( cmd ) );
        s = Q_stradd( s, cmd );
    }
    n = Com_Split( arg, argn, ARRAY_LEN( argn ), ';' );
    if ( n == 0 || *argn[0] == '\0' ) {
        return;
    }
    for ( i = 0; i < n; i++ ) {
        if ( !ValidVoteCommand( ent - g_entities, argn[i] ) ) {
            return;
        }
    }
    cmd[0] = '\0';
    for ( s = cmd, i = 0; i < n; i++ ) {
        if ( cmd[0] ) {
            s = Q_stradd( s, ";" );
        }
        s = Q_stradd( s, argn[i] );
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ), cmd );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
    trap_SendServerCommand( -1, va( "print \"%s called a vote(%s).\n\"", ent->client->pers.netname, cmd ) );
    level.voteTime = level.time;
    level.voteYes = 1;
    level.voteNo = 0;
    for ( i = 0 ; i < level.maxclients ; i++ ) {
        level.clients[i].ps.eFlags &= ~EF_VOTED;
        level.clients[i].pers.voted = 0;
    }
    ent->client->ps.eFlags |= EF_VOTED;
    ent->client->pers.voted = 1;
    ent->client->pers.voteCount++;
    trap_SetConfigstring( CS_VOTE_TIME, va("%i", level.voteTime ) );
    trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
    trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
    trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );
}

void Cmd_Vote_f( gentity_t *ent ) {
    char msg[64];
    if ( !level.voteTime ) {
        trap_SendServerCommand( ent-g_entities, "print \"No vote in progress.\n\"" );
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" );
        return;
    }

    trap_Argv( 1, msg, sizeof( msg ) );
    if ( msg[0] != 'y' && msg[0] != 'Y' && msg[0] != '1' && msg[0] != 'n' && msg[0] != 'N' && msg[0] != '0' ) {
        trap_SendServerCommand( ent-g_entities, "print \"Usage: vote <yes|no>\n\"" );
        return;
    }

    /* handle vote (and possibly change) */
    if ( ent->client->pers.voted != 0 ) {
        if ( !g_allowVoteChange.integer ) {
            trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
            return;
        }
        /* revert previous vote */
        if ( ent->client->pers.voted == 1 ) {
            if ( msg[0] == 'y' || msg[0] == 'Y' || msg[0] == '1' ) {
                trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
                return;
            }
            if ( level.voteYes > 0 ) level.voteYes--;
        } else if ( ent->client->pers.voted == -1 ) {
            if ( msg[0] == 'n' || msg[0] == 'N' || msg[0] == '0' ) {
                trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
                return;
            }
            if ( level.voteNo > 0 ) level.voteNo--;
        }
    } else {
        /* first time voting */
        trap_SendServerCommand( ent-g_entities, "print \"Vote cast.\n\"" );
        ent->client->ps.eFlags |= EF_VOTED;
    }

    if ( msg[0] == 'y' || msg[0] == 'Y' || msg[0] == '1' ) {
        level.voteYes++;
        ent->client->pers.voted = 1;
        trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
        trap_SendServerCommand( ent-g_entities, "print \"Your vote: ^2YES\n\"" );
    } else {
        level.voteNo++;
        ent->client->pers.voted = -1;
        trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
        trap_SendServerCommand( ent-g_entities, "print \"Your vote: ^1NO\n\"" );
    }
}

void Cmd_CV_f( gentity_t *ent ) {
    if ( trap_Argc() == 1 ) {
        Cmd_CV_HelpList( ent );
        return;
    } else if ( trap_Argc() == 2 ) {
        char sub[MAX_TOKEN_CHARS];
        trap_Argv( 1, sub, sizeof(sub) );
        if ( Q_stricmp( sub, "map" ) == 0 ) {
            VS_PrintMapList( ent );
            return;
        }
    }
    Cmd_CallVote_f( ent );
}

/* Move from g_cmds.c: revert player and team votes on leave/team change */
void G_RevertVote( gclient_t *client ) {
    if ( level.voteTime ) {
        if ( client->pers.voted == 1 ) {
            level.voteYes--;
            client->pers.voted = 0;
            client->ps.eFlags &= ~EF_VOTED;
            trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
        } else if ( client->pers.voted == -1 ) {
            level.voteNo--;
            client->pers.voted = 0;
            client->ps.eFlags &= ~EF_VOTED;
            trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
        }
    }
    if ( client->sess.sessionTeam == TEAM_RED || client->sess.sessionTeam == TEAM_BLUE ) {
        int cs_offset;
        if ( client->sess.sessionTeam == TEAM_RED ) cs_offset = 0; else cs_offset = 1;
        if ( client->pers.teamVoted == 1 ) {
            level.teamVoteYes[cs_offset]--;
            client->pers.teamVoted = 0;
            client->ps.eFlags &= ~EF_TEAMVOTED;
            trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
        } else if ( client->pers.teamVoted == -1 ) {
            level.teamVoteNo[cs_offset]--;
            client->pers.teamVoted = 0;
            client->ps.eFlags &= ~EF_TEAMVOTED;
            trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
        }
    }
}

/* Move from g_cmds.c: team vote call */
void Cmd_CallTeamVote_f( gentity_t *ent ) {
    int i, team, cs_offset;
    char arg1[MAX_STRING_TOKENS];
    char arg2[MAX_STRING_TOKENS];

    team = ent->client->sess.sessionTeam;
    if ( team == TEAM_RED ) cs_offset = 0; else if ( team == TEAM_BLUE ) cs_offset = 1; else return;

    if ( !g_allowVote.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here.\n\"" );
        return;
    }
    if ( level.teamVoteTime[cs_offset] ) {
        trap_SendServerCommand( ent-g_entities, "print \"A team vote is already in progress.\n\"" );
        return;
    }
    if ( ent->client->pers.teamVoteCount >= g_teamVoteLimit.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"You have called the maximum number of team votes.\n\"" );
        return;
    }
    if ( level.voteExecuteTime || level.restarted ) {
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent-g_entities, "print \"Not allowed to call a vote as spectator.\n\"" );
        return;
    }

    trap_Argv( 1, arg1, sizeof( arg1 ) );
    arg2[0] = '\0';
    for ( i = 2; i < trap_Argc(); i++ ) {
        if ( i > 2 ) strcat( arg2, " " );
        trap_Argv( i, &arg2[strlen(arg2)], sizeof( arg2 ) - (int)strlen(arg2) );
    }
    if ( strchr( arg1, ';' ) || strchr( arg2, ';' ) || strchr( arg2, '\n' ) || strchr( arg2, '\r' ) ) {
        trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
        return;
    }

    if ( !Q_stricmp( arg1, "leader" ) ) {
        char netname[MAX_NETNAME], leader[MAX_NETNAME];
        if ( !arg2[0] ) {
            i = ent->client->ps.clientNum;
        } else {
            int k;
            for ( k = 0; k < 3; k++ ) { if ( !arg2[k] || arg2[k] < '0' || arg2[k] > '9' ) break; }
            if ( k >= 3 || !arg2[k] ) {
                i = atoi( arg2 );
                if ( i < 0 || i >= level.maxclients ) { trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", i) ); return; }
                if ( !g_entities[i].inuse ) { trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", i) ); return; }
            } else {
                Q_strncpyz( leader, arg2, sizeof(leader) ); Q_CleanStr( leader );
                for ( i = 0; i < level.maxclients; i++ ) {
                    if ( level.clients[i].pers.connected == CON_DISCONNECTED ) continue;
                    if ( level.clients[i].sess.sessionTeam != team ) continue;
                    Q_strncpyz( netname, level.clients[i].pers.netname, sizeof(netname) ); Q_CleanStr( netname );
                    if ( !Q_stricmp( netname, leader ) ) break;
                }
                if ( i >= level.maxclients ) { trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) ); return; }
            }
        }
        Com_sprintf( arg2, sizeof(arg2), "%d", i );
    } else {
        trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
        trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
        return;
    }

    Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );
    for ( i = 0; i < level.maxclients; i++ ) {
        if ( level.clients[i].pers.connected == CON_DISCONNECTED ) continue;
        if ( level.clients[i].sess.sessionTeam == team ) trap_SendServerCommand( i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname ) );
    }
    level.teamVoteTime[cs_offset] = level.time;
    level.teamVoteYes[cs_offset] = 1;
    level.teamVoteNo[cs_offset]  = 0;
    for ( i = 0; i < level.maxclients; i++ ) {
        if ( level.clients[i].sess.sessionTeam == team ) {
            level.clients[i].ps.eFlags &= ~EF_TEAMVOTED;
            level.clients[i].pers.teamVoted = 0;
        }
    }
    ent->client->ps.eFlags |= EF_TEAMVOTED;
    ent->client->pers.teamVoted = 1;
    ent->client->pers.teamVoteCount++;
    trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
    trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset] );
    trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
    trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/* Move from g_cmds.c: team vote cast */
void Cmd_TeamVote_f( gentity_t *ent ) {
    int team, cs_offset;
    char msg[64];
    team = ent->client->sess.sessionTeam;
    if ( team == TEAM_RED ) cs_offset = 0; else if ( team == TEAM_BLUE ) cs_offset = 1; else return;
    if ( !level.teamVoteTime[cs_offset] ) { trap_SendServerCommand( ent-g_entities, "print \"No team vote in progress.\n\"" ); return; }
    if ( ent->client->pers.teamVoted != 0 ) { trap_SendServerCommand( ent-g_entities, "print \"Team vote already cast.\n\"" ); return; }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) { trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" ); return; }
    trap_SendServerCommand( ent-g_entities, "print \"Team vote cast.\n\"" );
    ent->client->ps.eFlags |= EF_TEAMVOTED;
    ent->client->pers.teamVoteCount++;
    trap_Argv( 1, msg, sizeof( msg ) );
    if ( msg[0] == 'y' || msg[0] == 'Y' || msg[0] == '1' ) {
        level.teamVoteYes[cs_offset]++;
        trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
    } else {
        level.teamVoteNo[cs_offset]++;
        trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
    }
}

/* end */

