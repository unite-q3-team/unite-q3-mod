// Copyright (C) 1999-2000 Id Software, Inc.
//
// #include "g_local.h"
#include "cmds/cmds.h"
#include "g_freeze.h"

#ifdef MISSIONPACK
#include "../../ui/menudef.h"			// for the voice chats
#endif

#define CHUNK_SIZE 1000

/* Function declarations */
qboolean SetTeamSafe( gentity_t *ent, const char *s );

/* Cached map list to avoid repeated filesystem scans */
static int s_cachedMapCount = 0;
static char s_cachedMaps[512][64];

/* Admin ban/mute system - structures defined in g_local.h */

banEntry_t s_bans[MAX_BANS];
muteEntry_t s_mutes[MAX_MUTES];
int s_banCount = 0;
int s_muteCount = 0;

/* File paths for persistent storage */
#define BANS_FILE "bans.dat"
#define MUTES_FILE "mutes.dat"

void G_EnsureMapListCache(void) {
    char listbuf[8192];
    int count, i, pos;
    if ( s_cachedMapCount > 0 ) {
        return;
    }
    s_cachedMapCount = 0;
    count = trap_FS_GetFileList( "maps", ".bsp", listbuf, sizeof(listbuf) );
    pos = 0;
    for ( i = 0; i < count && s_cachedMapCount < 512; ++i ) {
        char *nm;
        int nlen;
        if ( pos >= (int)sizeof(listbuf) ) break;
        nm = &listbuf[pos];
        nlen = (int)strlen( nm );
        if ( nlen > 0 ) {
            char tmp[64];
            Q_strncpyz( tmp, nm, sizeof(tmp) );
            nlen = (int)strlen( tmp );
            if ( nlen > 4 && !Q_stricmp( tmp + nlen - 4, ".bsp" ) ) {
                tmp[nlen - 4] = '\0';
            }
            Q_strncpyz( s_cachedMaps[s_cachedMapCount], tmp, sizeof(s_cachedMaps[0]) );
            s_cachedMapCount++;
        }
        pos += (int)strlen( nm ) + 1;
    }
}

void SendServerCommandInChunks(gentity_t *ent, const char *text) {
    int text_len;
    int sent_pos;
    int max_chunk;
    int last_newline;
    int i;
    int chunk_len;
    char sendbuf[CHUNK_SIZE + 1];

    text_len = strlen(text);
    sent_pos = 0;

    while (sent_pos < text_len) {
        max_chunk = (text_len - sent_pos > CHUNK_SIZE) ? CHUNK_SIZE : (text_len - sent_pos);
        last_newline = -1;

        for (i = 0; i < max_chunk; i++) {
            if (text[sent_pos + i] == '\n') {
                last_newline = i;
            }
        }

        if (last_newline == -1) {
            chunk_len = max_chunk;
        } else {
            chunk_len = last_newline + 1;
        }

        memcpy(sendbuf, text + sent_pos, chunk_len);
        sendbuf[chunk_len] = '\0';

        trap_SendServerCommand(ent - g_entities, va("print \"%s\"", sendbuf));

        sent_pos += chunk_len;
    }
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[256]; // enough to hold 14 integers
	char		string[MAX_STRING_CHARS-1];
	int			stringlength;
	int			i, j, ping, prefix;
	gclient_t	*cl;
	int			numSorted, scoreFlags, accuracy, perfect;

	// send the latest information on all clients
	string[0] = '\0';
	stringlength = 0;
	scoreFlags = 0;

	numSorted = level.numConnectedClients;

	// estimate prefix length to avoid oversize of final string
	prefix = BG_sprintf( entry, "scores %i %i %i", level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE], numSorted );
	
	for ( i = 0 ; i < numSorted ; i++ ) {

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}

		if( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		} else {
			accuracy = 0;
		}

		perfect = ( cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0 ) ? 1 : 0;

		//freeze
		if (g_freeze.integer)
		scoreFlags = cl->sess.wins;
		//freeze
		j = BG_sprintf( entry, " %i %i %i %i %i %i %i %i %i %i %i %i %i %i",
			level.sortedClients[i],
			cl->ps.persistant[PERS_SCORE],
			ping,
			(level.time - cl->pers.enterTime)/60000,
			scoreFlags,
			g_entities[level.sortedClients[i]].s.powerups,
			accuracy, 
			cl->ps.persistant[PERS_IMPRESSIVE_COUNT],
			cl->ps.persistant[PERS_EXCELLENT_COUNT],
			cl->ps.persistant[PERS_GAUNTLET_FRAG_COUNT], 
			cl->ps.persistant[PERS_DEFEND_COUNT], 
			cl->ps.persistant[PERS_ASSIST_COUNT], 
			perfect,
			cl->ps.persistant[PERS_CAPTURES]);

		if ( stringlength + j + prefix >= sizeof( string ) )
			break;

		strcpy( string + stringlength, entry );
		stringlength += j;
	}

	trap_SendServerCommand( ent-g_entities, va( "scores %i %i %i%s", i,
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE],
		string ) );
}

#define MAX_SPAWN_STR_LEN 1024

void SendSpawnCoordsToClient(gentity_t *ent) {
    char spawnCoordsStr[MAX_SPAWN_STR_LEN];
    int i, len = 0;
    gentity_t *spot;
    int x, y, z, written;

    if (level.numSpawnSpots == 0) {
        trap_SendServerCommand(ent - g_entities, "spawnCoords 0");
        return;
    }

    len = Q_snprintf(spawnCoordsStr, sizeof(spawnCoordsStr), "spawnCoords %d", level.numSpawnSpots);

    for (i = 0; i < level.numSpawnSpots; i++) {
        spot = level.spawnSpots[i];

        x = (int)(spot->r.currentOrigin[0]);
        y = (int)(spot->r.currentOrigin[1]);
        z = (int)(spot->r.currentOrigin[2]);

    
        written = Q_snprintf(spawnCoordsStr + len, sizeof(spawnCoordsStr) - len,
                             " %d %d %d", x, y, z);
        if (written <= 0 || written >= sizeof(spawnCoordsStr) - len) {
            break;
        }
        len += written;
    }

    trap_SendServerCommand(ent - g_entities, spawnCoordsStr);
}



/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}


/*
==================
CheatsOk
==================
*/
qboolean	CheatsOk( gentity_t *ent ) {
	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, "print \"Cheats are not enabled on this server.\n\"");
		return qfalse;
	}
	if ( ent->health <= 0 ) {
		trap_SendServerCommand( ent-g_entities, "print \"You must be alive to use this command.\n\"");
		return qfalse;
	}
	return qtrue;
}


/*
==================
ConcatArgs
==================
*/
char *ConcatArgs( int start ) {
	static char line[MAX_STRING_CHARS];
	char	arg[MAX_STRING_CHARS];
	int		i, c, tlen;
	int		len;

	len = 0;
	c = trap_Argc();
	for ( i = start ; i < c ; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );
		tlen = (int)strlen( arg );
		if ( len + tlen >= sizeof( line )-1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = '\0';

	return line;
}


/*
==================
SanitizeString

Remove case and control characters
==================
*/
void SanitizeString( const char *in, char *out ) {
	while ( *in ) {
		if ( *in == 27 ) {
			in += 2;		// skip color code
			continue;
		}
		if ( *in < ' ' ) {
			in++;
			continue;
		}
		*out = tolower( *in );
		out++;
		in++;
	}

	*out = '\0';
}


/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, char *s ) {
	gclient_t	*cl;
	int			idnum;
	char		s2[MAX_STRING_CHARS];
	char		n2[MAX_STRING_CHARS];

	// numeric values are just slot numbers
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi( s );
		if ( (unsigned) idnum >= (unsigned)level.maxclients ) {
			trap_SendServerCommand( to-g_entities, va("print \"Bad client slot: %i\n\"", idnum));
			return -1;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected != CON_CONNECTED ) {
			trap_SendServerCommand( to-g_entities, va("print \"Client %i is not active\n\"", idnum));
			return -1;
		}
		return idnum;
	}

	// check for a name match
	SanitizeString( s, s2 );
	for ( idnum=0,cl=level.clients ; idnum < level.maxclients ; idnum++,cl++ ) {
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		SanitizeString( cl->pers.netname, n2 );
		if ( !strcmp( n2, s2 ) ) {
			return idnum;
		}
	}

	trap_SendServerCommand( to-g_entities, va("print \"User %s is not on the server\n\"", s));
	return -1;
}


/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f( gentity_t *ent )
{
	char		*name;
	gitem_t		*it;
	int			i;
	qboolean	give_all;
	gentity_t	*it_ent;
	trace_t		trace;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	name = ConcatArgs( 1 );

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	if (give_all || Q_stricmp( name, "health") == 0)
	{
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_NUM_WEAPONS) - 1 - 
			( 1 << WP_GRAPPLING_HOOK ) - ( 1 << WP_NONE );
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for ( i = 0 ; i < MAX_WEAPONS ; i++ ) {
			ent->client->ps.ammo[i] = 999;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		ent->client->ps.stats[STAT_ARMOR] = 200;

		if (!give_all)
			return;
	}

	if (Q_stricmp(name, "excellent") == 0) {
		ent->client->ps.persistant[PERS_EXCELLENT_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "impressive") == 0) {
		ent->client->ps.persistant[PERS_IMPRESSIVE_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "gauntletaward") == 0) {
		ent->client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "defend") == 0) {
		ent->client->ps.persistant[PERS_DEFEND_COUNT]++;
		return;
	}
	if (Q_stricmp(name, "assist") == 0) {
		ent->client->ps.persistant[PERS_ASSIST_COUNT]++;
		return;
	}

	// spawn a specific item right on the player
	if ( !give_all ) {
		it = BG_FindItem (name);
		if (!it) {
			return;
		}

		it_ent = G_Spawn();
		VectorCopy( ent->r.currentOrigin, it_ent->s.origin );
		it_ent->classname = it->classname;
		G_SpawnItem (it_ent, it);
		FinishSpawningItem(it_ent );
		memset( &trace, 0, sizeof( trace ) );
		Touch_Item (it_ent, ent, &trace);
		if (it_ent->inuse) {
			G_FreeEntity( it_ent );
		}
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent )
{
	const char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	trap_SendServerCommand( ent-g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	const char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	trap_SendServerCommand( ent-g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	const char *msg;

	if ( !CheatsOk( ent ) ) {
		return;
	}

	if ( ent->client->noclip ) {
		msg = "noclip OFF\n";
	} else {
		msg = "noclip ON\n";
	}
	ent->client->noclip = !ent->client->noclip;

	trap_SendServerCommand( ent-g_entities, va("print \"%s\"", msg));
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent ) {
	if ( !CheatsOk( ent ) ) {
		return;
	}

	// doesn't work in single player
	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		trap_SendServerCommand( ent-g_entities, 
			"print \"Must be in g_gametype 0 for levelshot\n\"" );
		return;
	}

	if ( !ent->client->pers.localClient )
	{
		trap_SendServerCommand( ent - g_entities,
			"print \"The levelshot command must be executed by a local client\n\"" );
		return;
	}

	BeginIntermission();
	trap_SendServerCommand( ent-g_entities, "clientLevelShot" );
}


/*
==================
Cmd_TeamTask_f
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap_Argc() != 2 ) {
		return;
	}
	trap_Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap_GetUserinfo( client, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "teamtask", va( "%d", task ) );
	trap_SetUserinfo( client, userinfo );
	ClientUserinfoChanged( client );
}


/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f(gentity_t *ent) {
	if ( g_debugTrace.integer ) {
		G_Printf("[KILL] request by client=%d name=%s health=%d team=%d freeze=%d\n",
			ent ? ent->s.clientNum : -1,
			(ent && ent->client) ? ent->client->pers.netname : "<nc>",
			ent ? ent->health : -9999,
			(ent && ent->client) ? ent->client->sess.sessionTeam : -1,
			g_freeze.integer);
	}
	if ((g_freeze.integer && ftmod_isSpectator(ent->client)) ||
		(!g_freeze.integer && ent->client->sess.sessionTeam == TEAM_SPECTATOR)) {
		if ( g_debugTrace.integer ) G_Printf("[KILL] denied: spectator\n");
		return;
	}

	if (ent->health <= 0) {
		if ( g_debugTrace.integer ) G_Printf("[KILL] denied: already dead\n");
		return;
	}

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

	if ( g_debugTrace.integer ) G_Printf("[KILL] executing player_die for client %d\n", ent->s.clientNum);
	player_die(ent, ent, ent, 100000,
		(MOD_SUICIDE));
}



/*
=================
BroadcastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, team_t oldTeam )
{
	int clientNum = client - level.clients;

	if ( client->sess.sessionTeam == TEAM_RED ) {
		G_BroadcastServerCommand( clientNum, va("cp \"%s" S_COLOR_WHITE " joined the " S_COLOR_RED "red" S_COLOR_WHITE " team.\n\"",
			client->pers.netname) );
	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		G_BroadcastServerCommand( clientNum, va("cp \"%s" S_COLOR_WHITE " joined the " S_COLOR_BLUE "blue" S_COLOR_WHITE " team.\n\"",
		client->pers.netname));
	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		G_BroadcastServerCommand( clientNum, va("cp \"%s" S_COLOR_WHITE " joined the spectators.\n\"",
		client->pers.netname));
	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		G_BroadcastServerCommand( clientNum, va("cp \"%s" S_COLOR_WHITE " joined the battle.\n\"",
		client->pers.netname));
	}
}


static qboolean AllowTeamSwitch( int clientNum, team_t newTeam ) {

	if ( g_teamForceBalance.integer  ) {
		int		counts[TEAM_NUM_TEAMS];

		counts[TEAM_BLUE] = TeamCount( clientNum, TEAM_BLUE );
		counts[TEAM_RED] = TeamCount( clientNum, TEAM_RED );

		// We allow a spread of two
		if ( newTeam == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
			trap_SendServerCommand( clientNum, "cp \"Red team has too many players.\n\"" );
			return qfalse; // ignore the request
		}

		if ( newTeam == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
			trap_SendServerCommand( clientNum, "cp \"Blue team has too many players.\n\"" );
			return qfalse; // ignore the request
		}

		// It's ok, the team we are switching to has less or same number of players
	}

	return qtrue;
}


/*
=================
SetTeamSafe

Safe team change that doesn't kill alive players
=================
*/
qboolean SetTeamSafe( gentity_t *ent, const char *s ) {
	team_t				team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;
	qboolean			checkTeamLeader;
	qboolean			wasAlive;
	int					savedHealth;
	qboolean			savedFreezeState;
	int					savedArmor;

	//
	// see what change is requested
	//

	clientNum = ent - g_entities;
	client = level.clients + clientNum;

	if ( g_debugTrace.integer ) {
		G_Printf("[TEAM] safe request: client=%d name=%s from=%d arg=%s freeze=%d gt=%d\n",
			clientNum,
			client ? client->pers.netname : "<nc>",
			client ? client->sess.sessionTeam : -1,
			s ? s : "<null>",
			g_freeze.integer,
			g_gametype.integer);
	}

	// early team override
	if ( client->pers.connected == CON_CONNECTING && g_gametype.integer >= GT_TEAM ) {
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE; 
		} else {
			team = -1;
		}
		if ( team != -1 && AllowTeamSwitch( clientNum, team ) ) {
			client->sess.sessionTeam = team;
			client->pers.teamState.state = TEAM_BEGIN;
			G_WriteClientSessionData( client );
			// count current clients and rank for scoreboard
			CalculateRanks();
			if ( g_debugTrace.integer ) G_Printf("[TEAM] early override applied: to=%d\n", team);
		}
		return qfalse; // bypass flood protection
	}

	specClient = clientNum;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( g_gametype.integer >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			team = PickTeam( clientNum );
		}

		if ( !AllowTeamSwitch( clientNum, team ) ) {
			return qtrue;
		}

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	// override decision if limiting the players
	if ( (g_gametype.integer == GT_TOURNAMENT)
		&& level.numNonSpectatorClients >= 2 ) {
		team = TEAM_SPECTATOR;
	} else if ( g_maxGameClients.integer > 0 && 
		level.numNonSpectatorClients >= g_maxGameClients.integer ) {
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	oldTeam = client->sess.sessionTeam;
	if ( team == oldTeam ) {
		if ( team != TEAM_SPECTATOR )
			return qfalse;

		// do soft release if possible
		if ( ( client->ps.pm_flags & PMF_FOLLOW ) && client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent, qtrue );
			return qfalse;
		}

		// second spectator team request will move player to intermission point
		if ( client->ps.persistant[ PERS_TEAM ] == TEAM_SPECTATOR && !( client->ps.pm_flags & PMF_FOLLOW )
			&& client->sess.spectatorState == SPECTATOR_FREE ) {
			VectorCopy( level.intermission_origin, ent->s.origin );
			VectorCopy( level.intermission_origin, client->ps.origin );
			SetClientViewAngle( ent, level.intermission_angle );
			return qfalse;
		}
	}

	//
	// execute the team change
	//

	// Check if player was alive before team change
	wasAlive = (ent->health > 0);

	// Save current state for alive players
	savedHealth = ent->health;
	savedFreezeState = ent->freezeState;
	savedArmor = client->ps.stats[STAT_ARMOR];

	// if the player was dead leave the body
	if ( ent->health <= 0 ) {
		CopyToBodyQue( ent );
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;

    if ( oldTeam != TEAM_SPECTATOR ) {
		
		// revert any casted votes
		if ( oldTeam != team )
			G_RevertVote( ent->client );

		// Only kill the player if they're moving to spectator or if they were already dead
		if ( team == TEAM_SPECTATOR || !wasAlive ) {
			// Kill him (makes sure he loses flags, etc)
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			player_die (ent, ent, ent, 100000, MOD_SUICIDE);
			if ( g_debugTrace.integer ) G_Printf("[TEAM] forced suicide for team change client=%d\n", clientNum);
		}

        /* Reset extended stats on team change */
        {
            gclient_t *cl = client;
            int w;
            cl->accuracy_hits = 0;
            cl->accuracy_shots = 0;
            cl->totalDamageGiven = 0;
            cl->totalDamageTaken = 0;
            cl->kills = 0;
            cl->deaths = 0;
            cl->currentKillStreak = 0;
            cl->armorPickedTotal = 0;
            cl->healthPickedTotal = 0;
            for ( w = 0; w < WP_NUM_WEAPONS; ++w ) {
                cl->perWeaponDamageGiven[w] = 0;
                cl->perWeaponDamageTaken[w] = 0;
                cl->perWeaponShots[w] = 0;
                cl->perWeaponHits[w] = 0;
                cl->perWeaponKills[w] = 0;
                cl->perWeaponDeaths[w] = 0;
                cl->perWeaponPickups[w] = 0;
                cl->perWeaponDrops[w] = 0;
            }
        }
	}

	// they go to the end of the line for tournements
	if ( team == TEAM_SPECTATOR ) {
		client->sess.spectatorTime = 0;
	}

	client->sess.sessionTeam = team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;
	if ( g_debugTrace.integer ) G_Printf("[TEAM] applied: client=%d newTeam=%d specState=%d specClient=%d\n", clientNum, team, specState, specClient);

	checkTeamLeader = client->sess.teamLeader;
	client->sess.teamLeader = qfalse;

	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			SetLeader( team, clientNum );
		}
	}

	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		if ( checkTeamLeader ) {
			CheckTeamLeader( oldTeam );
		}
	}

	G_WriteClientSessionData( client );

    BroadcastTeamChange( client, oldTeam );

	/* Check if we need to cancel ready countdown due to human player switching to spectator */
	if ( level.readyCountdownStarted && level.warmupTime > 0 && level.time < level.warmupTime ) {
		if ( team == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR && !(ent->r.svFlags & SVF_BOT) ) {
			int totalHumans;
			int totalPlayers;
			int i;
			totalHumans = 0;
			totalPlayers = 0;
			for ( i = 0; i < level.maxclients; i++ ) {
				gentity_t *e = &g_entities[i];
				gclient_t *cl = &level.clients[i];
				if ( cl->pers.connected != CON_CONNECTED ) {
					continue;
				}
				if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
					continue;
				}
				totalPlayers++;
				if ( !(e->r.svFlags & SVF_BOT) ) {
					totalHumans++;
				}
			}
			{
				int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
				if ( requireTwo && totalHumans < 2 ) {
					level.readyCountdownStarted = qfalse;
					level.warmupTime = -1;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_BroadcastServerCommand( -1, "cp \"^3Countdown cancelled: need 2 human players\"" );
				} else if ( totalPlayers < 2 ) {
					level.readyCountdownStarted = qfalse;
					level.warmupTime = -1;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_BroadcastServerCommand( -1, "cp \"^3Countdown cancelled: need 2 players\"" );
				}
			}
		}
	}

	/* Check if we need to cancel active match due to insufficient players */
	if ( level.warmupTime == 0 && g_gametype.integer != GT_SINGLE_PLAYER ) {
		if ( team == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
			int totalHumans;
			int totalPlayers;
			int i;
			totalHumans = 0;
			totalPlayers = 0;
			for ( i = 0; i < level.maxclients; i++ ) {
				gentity_t *e = &g_entities[i];
				gclient_t *cl = &level.clients[i];
				if ( cl->pers.connected != CON_CONNECTED ) {
					continue;
				}
				if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
					continue;
				}
				totalPlayers++;
				if ( !(e->r.svFlags & SVF_BOT) ) {
					totalHumans++;
				}
			}
			{
				int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
				if ( requireTwo && totalHumans < 2 ) {
					level.warmupTime = -1;
					level.readyCountdownStarted = qfalse;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_LogPrintf( "Warmup:\n" );
					level.abortedDueToNoPlayers = 1;
					G_BroadcastServerCommand( -1, "cp \"^3Match aborted: need 2 human players\"" );
				} else if ( totalPlayers < 2 ) {
					level.warmupTime = -1;
					level.readyCountdownStarted = qfalse;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_LogPrintf( "Warmup:\n" );
					level.abortedDueToNoPlayers = 1;
					G_BroadcastServerCommand( -1, "cp \"^3Match aborted: need 2 players\"" );
				}
			}
		}
	}

	/* Reset ready status when switching to spectator */
	if ( team == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		ent->readyBegin = qfalse;
	}

	// get and distribute relevent paramters
	ClientUserinfoChanged( clientNum );

	//freeze
	ent->freezeState = qfalse;

    ClientBegin( clientNum );

	// Restore state for alive players who switched teams
	if ( wasAlive && team != TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		ent->health = savedHealth;
		client->ps.stats[STAT_HEALTH] = savedHealth;
		client->ps.stats[STAT_ARMOR] = savedArmor;
		ent->freezeState = savedFreezeState;
		if ( g_debugTrace.integer ) G_Printf("[TEAM] restored state: health=%d armor=%d freeze=%d\n", savedHealth, savedArmor, savedFreezeState);
	}

	// count current clients and rank for scoreboard
	CalculateRanks();

	return qfalse;
}

/*
=================
SetTeam

Legacy function - now uses SetTeamSafe for non-spectator changes
=================
*/
qboolean SetTeam( gentity_t *ent, const char *s ) {
	team_t				team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;
	qboolean			checkTeamLeader;

	//
	// see what change is requested
	//

	clientNum = ent - g_entities;
	client = level.clients + clientNum;

	if ( g_debugTrace.integer ) {
		G_Printf("[TEAM] request: client=%d name=%s from=%d arg=%s freeze=%d gt=%d\n",
			clientNum,
			client ? client->pers.netname : "<nc>",
			client ? client->sess.sessionTeam : -1,
			s ? s : "<null>",
			g_freeze.integer,
			g_gametype.integer);
	}

	// early team override
	if ( client->pers.connected == CON_CONNECTING && g_gametype.integer >= GT_TEAM ) {
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE; 
		} else {
			team = -1;
		}
		if ( team != -1 && AllowTeamSwitch( clientNum, team ) ) {
			client->sess.sessionTeam = team;
			client->pers.teamState.state = TEAM_BEGIN;
			G_WriteClientSessionData( client );
			// count current clients and rank for scoreboard
			CalculateRanks();
			if ( g_debugTrace.integer ) G_Printf("[TEAM] early override applied: to=%d\n", team);
		}
		return qfalse; // bypass flood protection
	}

	specClient = clientNum;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_SCOREBOARD;
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( g_gametype.integer >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			team = PickTeam( clientNum );
		}

		if ( !AllowTeamSwitch( clientNum, team ) ) {
			return qtrue;
		}

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	// override decision if limiting the players
	if ( (g_gametype.integer == GT_TOURNAMENT)
		&& level.numNonSpectatorClients >= 2 ) {
		team = TEAM_SPECTATOR;
	} else if ( g_maxGameClients.integer > 0 && 
		level.numNonSpectatorClients >= g_maxGameClients.integer ) {
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	oldTeam = client->sess.sessionTeam;
	if ( team == oldTeam ) {
		if ( team != TEAM_SPECTATOR )
			return qfalse;

		// do soft release if possible
		if ( ( client->ps.pm_flags & PMF_FOLLOW ) && client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent, qtrue );
			return qfalse;
		}

		// second spectator team request will move player to intermission point
		if ( client->ps.persistant[ PERS_TEAM ] == TEAM_SPECTATOR && !( client->ps.pm_flags & PMF_FOLLOW )
			&& client->sess.spectatorState == SPECTATOR_FREE ) {
			VectorCopy( level.intermission_origin, ent->s.origin );
			VectorCopy( level.intermission_origin, client->ps.origin );
			SetClientViewAngle( ent, level.intermission_angle );
			return qfalse;
		}
	}

	//
	// execute the team change
	//

	// if the player was dead leave the body
	if ( ent->health <= 0 ) {
		CopyToBodyQue( ent );
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;

    if ( oldTeam != TEAM_SPECTATOR ) {
		
		// revert any casted votes
		if ( oldTeam != team )
			G_RevertVote( ent->client );

		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
        player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		if ( g_debugTrace.integer ) G_Printf("[TEAM] forced suicide for team change client=%d\n", clientNum);

        /* Reset extended stats on team change */
        {
            gclient_t *cl = client;
            int w;
            cl->accuracy_hits = 0;
            cl->accuracy_shots = 0;
            cl->totalDamageGiven = 0;
            cl->totalDamageTaken = 0;
            cl->kills = 0;
            cl->deaths = 0;
            cl->currentKillStreak = 0;
            cl->armorPickedTotal = 0;
            cl->healthPickedTotal = 0;
            for ( w = 0; w < WP_NUM_WEAPONS; ++w ) {
                cl->perWeaponDamageGiven[w] = 0;
                cl->perWeaponDamageTaken[w] = 0;
                cl->perWeaponShots[w] = 0;
                cl->perWeaponHits[w] = 0;
                cl->perWeaponKills[w] = 0;
                cl->perWeaponDeaths[w] = 0;
                cl->perWeaponPickups[w] = 0;
                cl->perWeaponDrops[w] = 0;
            }
        }
	}

	// they go to the end of the line for tournements
	if ( team == TEAM_SPECTATOR ) {
		client->sess.spectatorTime = 0;
	}

	client->sess.sessionTeam = team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;
	if ( g_debugTrace.integer ) G_Printf("[TEAM] applied: client=%d newTeam=%d specState=%d specClient=%d\n", clientNum, team, specState, specClient);

	checkTeamLeader = client->sess.teamLeader;
	client->sess.teamLeader = qfalse;

	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			SetLeader( team, clientNum );
		}
	}

	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		if ( checkTeamLeader ) {
			CheckTeamLeader( oldTeam );
		}
	}

	G_WriteClientSessionData( client );

    BroadcastTeamChange( client, oldTeam );

	/* Check if we need to cancel ready countdown due to human player switching to spectator */
	if ( level.readyCountdownStarted && level.warmupTime > 0 && level.time < level.warmupTime ) {
		if ( team == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR && !(ent->r.svFlags & SVF_BOT) ) {
			int totalHumans;
			int totalPlayers;
			int i;
			totalHumans = 0;
			totalPlayers = 0;
			for ( i = 0; i < level.maxclients; i++ ) {
				gentity_t *e = &g_entities[i];
				gclient_t *cl = &level.clients[i];
				if ( cl->pers.connected != CON_CONNECTED ) {
					continue;
				}
				if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
					continue;
				}
				totalPlayers++;
				if ( !(e->r.svFlags & SVF_BOT) ) {
					totalHumans++;
				}
			}
			{
				int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
				if ( requireTwo && totalHumans < 2 ) {
					level.readyCountdownStarted = qfalse;
					level.warmupTime = -1;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_BroadcastServerCommand( -1, "cp \"^3Countdown cancelled: need 2 human players\"" );
				} else if ( totalPlayers < 2 ) {
					level.readyCountdownStarted = qfalse;
					level.warmupTime = -1;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_BroadcastServerCommand( -1, "cp \"^3Countdown cancelled: need 2 players\"" );
				}
			}
		}
	}

	/* Check if we need to cancel active match due to insufficient players */
	if ( level.warmupTime == 0 && g_gametype.integer != GT_SINGLE_PLAYER ) {
		if ( team == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
			int totalHumans;
			int totalPlayers;
			int i;
			totalHumans = 0;
			totalPlayers = 0;
			for ( i = 0; i < level.maxclients; i++ ) {
				gentity_t *e = &g_entities[i];
				gclient_t *cl = &level.clients[i];
				if ( cl->pers.connected != CON_CONNECTED ) {
					continue;
				}
				if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
					continue;
				}
				totalPlayers++;
				if ( !(e->r.svFlags & SVF_BOT) ) {
					totalHumans++;
				}
			}
			{
				int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
				if ( requireTwo && totalHumans < 2 ) {
					level.warmupTime = -1;
					level.readyCountdownStarted = qfalse;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_LogPrintf( "Warmup:\n" );
					level.abortedDueToNoPlayers = 1;
					G_BroadcastServerCommand( -1, "cp \"^3Match aborted: need 2 human players\"" );
				} else if ( totalPlayers < 2 ) {
					level.warmupTime = -1;
					level.readyCountdownStarted = qfalse;
					trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
					G_LogPrintf( "Warmup:\n" );
					level.abortedDueToNoPlayers = 1;
					G_BroadcastServerCommand( -1, "cp \"^3Match aborted: need 2 players\"" );
				}
			}
		}
	}

	/* Reset ready status when switching to spectator */
	if ( team == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		ent->readyBegin = qfalse;
	}

	// get and distribute relevent paramters
	ClientUserinfoChanged( clientNum );

	//freeze
	ent->freezeState = qfalse;

    ClientBegin( clientNum );

    /* After a successful join from spectators to a playing team, print plain 'entered the game' */
    if ( team != TEAM_SPECTATOR && oldTeam == TEAM_SPECTATOR ) {
        G_BroadcastServerCommand( -1, va("print \"%s" S_COLOR_WHITE " entered the game\n\"", client->pers.netname) );
    }

	return qtrue;
}


/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void StopFollowing( gentity_t *ent, qboolean release ) {
	gclient_t *client;

	if ( ent->r.svFlags & SVF_BOT || !ent->inuse )
		return;

	client = ent->client;

	client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;	
	client->sess.sessionTeam = TEAM_SPECTATOR;	
	if ( release ) {
		client->ps.stats[STAT_HEALTH] = ent->health = 1;
		memset( client->ps.powerups, 0, sizeof ( client->ps.powerups ) );
	}
	SetClientViewAngle( ent, client->ps.viewangles );

	client->sess.spectatorState = SPECTATOR_FREE;
	client->ps.pm_flags &= ~PMF_FOLLOW;
	//ent->r.svFlags &= ~SVF_BOT;

	client->ps.clientNum = ent - g_entities;
}

void StopFollowingNew( gentity_t *ent ) { // using for freeze only
	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;	
/*freeze
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;	
freeze*/
	SetClientViewAngle( ent, ent->client->ps.viewangles );
	ent->client->ps.stats[ STAT_HEALTH ] = ent->health = 100;
	memset( ent->client->ps.powerups, 0, sizeof ( ent->client->ps.powerups ) );
//freeze
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;
}


/*
=================
Cmd_Team_f
=================
*/
static void Cmd_Team_f( gentity_t *ent ) {
	char		s[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		switch ( ent->client->sess.sessionTeam ) {
		case TEAM_BLUE:
			trap_SendServerCommand( ent-g_entities, "print \"Blue team\n\"" );
			break;
		case TEAM_RED:
			trap_SendServerCommand( ent-g_entities, "print \"Red team\n\"" );
			break;
		case TEAM_FREE:
			trap_SendServerCommand( ent-g_entities, "print \"Free team\n\"" );
			break;
		case TEAM_SPECTATOR:
			trap_SendServerCommand( ent-g_entities, "print \"Spectator team\n\"" );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		char msg_str[128];
		float cdtime = (ent->client->switchTeamTime - level.time) / 1000.f;
		// Com_sprintf(msg_str, sizeof(msg_str), );
		trap_SendServerCommand( ent-g_entities, va("print \"^1! ^3Wait ^1%.1f ^3seconds before changing team.\n\"", cdtime));
		return;
	}

	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}
    if (g_freeze.integer) {
        if ( ent->freezeState ) {
            /* In freeze mode, do not block team changes for frozen players.
               Simply ensure we are not in follow state, then proceed. */
            if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
                StopFollowingNew( ent );
            }
        }
    }
	trap_Argv( 1, s, sizeof( s ) );

	if ( SetTeam( ent, s ) ) {
		if (g_teamChangeCooldown.integer <= 0)
			trap_Cvar_Set("sv_floodProtect_time", "5000");

		ent->client->switchTeamTime = level.time + g_teamChangeCooldown.integer;
	}
}


/*
=================
Cmd_Follow_f
=================
*/
static void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if ( trap_Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent, qtrue );
		}
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if (ent->freezeState && (g_freeze.integer ? !ftmod_isSpectator(ent->client) : ent->client->sess.sessionTeam != TEAM_SPECTATOR))
		return;

    if (g_freeze.integer) {
        if (ftmod_isSpectator(&level.clients[i])) {
            return;
        }
        /* In team modes, optionally restrict following to teammates (freeze)
           Only restrict if the follower is NOT a true spectator (spectator team) */
        if ( g_gametype.integer >= GT_TEAM && !g_teamAllowEnemySpectate.integer && ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
            if ( level.clients[i].sess.sessionTeam != ent->client->sess.sessionTeam ) {
                return;
            }
        }
    } else {
		if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR) {
			return;
		}
        /* In team modes, optionally restrict following to teammates
           Only restrict if the follower is NOT a true spectator */
        if ( g_gametype.integer >= GT_TEAM && !g_teamAllowEnemySpectate.integer && ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
			if ( level.clients[i].sess.sessionTeam != ent->client->sess.sessionTeam ) {
				return;
			}
		}
	}


	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeamSafe( ent, "spectator" );
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}


/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;
	gclient_t	*client;

	//freeze
	if (g_freeze.integer) {
		if ( ent->freezeState && !ftmod_isSpectator( ent->client ) ) return;
		if ( ftmod_setClient( ent ) ) return;
	}
	//freeze
	// if they are playing a tournement game, count as a loss
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		ent->client->sess.losses++;
	}

	client = ent->client;

	// first set them to spectator
	if ( client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeamSafe( ent, "spectator" );
	}

	if ( dir != 1 && dir != -1 ) {
		G_Error( "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = client->sess.spectatorClient;
	original = clientnum;
	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients ) {
			clientnum = 0;
		}
		if ( clientnum < 0 ) {
			clientnum = level.maxclients - 1;
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if (&level.clients[clientnum] == ent->client) {
			if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
				StopFollowingNew(ent);
				ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
				ent->client->ps.pm_time = 100;
				return;
			}
		}

		if (g_entities[clientnum].freezeState)
			continue;

        if (g_freeze.integer) {
            if (ftmod_isSpectator(&level.clients[clientnum])) {
                continue;
            }
            /* In team modes, optionally restrict following to teammates (freeze)
               Only restrict if the follower is NOT a true spectator (spectator team) */
            if ( g_gametype.integer >= GT_TEAM && !g_teamAllowEnemySpectate.integer && client->sess.sessionTeam != TEAM_SPECTATOR ) {
                if ( level.clients[clientnum].sess.sessionTeam != ent->client->sess.sessionTeam ) {
                    continue;
                }
            }
        } else {
            if (level.clients[clientnum].sess.sessionTeam == TEAM_SPECTATOR) {
                continue;
            }
            /* If in team gametype and enemy spectate disabled, skip enemies
               Only restrict if the follower is NOT a true spectator */
            if ( g_gametype.integer >= GT_TEAM && !g_teamAllowEnemySpectate.integer && client->sess.sessionTeam != TEAM_SPECTATOR ) {
                if ( level.clients[clientnum].sess.sessionTeam != ent->client->sess.sessionTeam ) {
                    continue;
                }
            }
        }

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}


/*
==================
G_Say
==================
*/
static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message ) {
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	// no chatting to players in tournements
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& other->client->sess.sessionTeam == TEAM_FREE
		&& ent->client->sess.sessionTeam != TEAM_FREE ) {
		return;
	}

	trap_SendServerCommand( other-g_entities, va( "%s \"%s%c%c%s\" %i", mode == SAY_TEAM ? "tchat" : "chat", 
		name, Q_COLOR_ESCAPE, color, message, ent - g_entities ) );
}

#define EC		"\x19"

static void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64 + 64 + 12]; // name + location + formatting
	// don't let text be too long for malicious reasons
	char		text[MAX_SAY_TEXT];
	char		location[64];

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	switch ( mode ) {
	default:
	case SAY_ALL:
		G_LogPrintf( "say: %s: %s\n", ent->client->pers.netname, chatText );
		Com_sprintf (name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		break;
	case SAY_TEAM:
		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, chatText );
		if (Team_GetLocationMsg(ent, location, sizeof(location)))
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC") (%s)"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location);
		else
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ", 
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_CYAN;
		break;
	case SAY_TELL:
		if (target && target->inuse && target->client && g_gametype.integer >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocationMsg(ent, location, sizeof(location)))
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"] (%s)"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
		else
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_MAGENTA;
		break;
	}

	Q_strncpyz( text, chatText, sizeof(text) );

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text );
		return;
	}

	// echo the text to the console
	if ( g_dedicated.integer ) {
		G_Printf( "%s%s\n", name, text);
	}

	// send it to all the apropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_SayTo( ent, other, mode, color, name, text );
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent, int mode, qboolean arg0 ) {
	char		*p;
	char		*ip;

	if ( trap_Argc () < 2 && !arg0 ) {
		return;
	}

	// Check if player is muted
	{
		char userinfo[MAX_INFO_STRING];
		trap_GetUserinfo( ent - g_entities, userinfo, sizeof( userinfo ) );
		ip = Info_ValueForKey( userinfo, "ip" );
		if ( !ip || !ip[0] ) {
			ip = "unknown";
		}
	}
	
	if ( IsMuted( ip ) ) {
		trap_SendServerCommand( ent - g_entities, "print \"^1You are muted and cannot speak.\n\"" );
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	G_Say( ent, NULL, mode, p );
}


/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	char		*p;
	char		arg[MAX_TOKEN_CHARS];
	char		*ip;

	if ( trap_Argc () < 2 ) {
		return;
	}

	// Check if player is muted
	{
		char userinfo[MAX_INFO_STRING];
		trap_GetUserinfo( ent - g_entities, userinfo, sizeof( userinfo ) );
		ip = Info_ValueForKey( userinfo, "ip" );
		if ( !ip || !ip[0] ) {
			ip = "unknown";
		}
	}
	
	if ( IsMuted( ip ) ) {
		trap_SendServerCommand( ent - g_entities, "print \"^1You are muted and cannot speak.\n\"" );
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = atoi( arg );
	if ( (unsigned)targetNum >= (unsigned)level.maxclients ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	p = ConcatArgs( 2 );

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}

#ifdef MISSIONPACK

static void G_VoiceTo( gentity_t *ent, gentity_t *other, int mode, const char *id, qboolean voiceonly ) {
	int color;
	char *cmd;

	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( mode == SAY_TEAM && !OnSameTeam(ent, other) ) {
		return;
	}
	// no chatting to players in tournements
	if ( g_gametype.integer == GT_TOURNAMENT ) {
		return;
	}

	if (mode == SAY_TEAM) {
		color = COLOR_CYAN;
		cmd = "vtchat";
	}
	else if (mode == SAY_TELL) {
		color = COLOR_MAGENTA;
		cmd = "vtell";
	}
	else {
		color = COLOR_GREEN;
		cmd = "vchat";
	}

	trap_SendServerCommand( other-g_entities, va("%s %d %d %d %s", cmd, voiceonly, ent->s.number, color, id));
}

void G_Voice( gentity_t *ent, gentity_t *target, int mode, const char *id, qboolean voiceonly ) {
	int			j;
	gentity_t	*other;

	if ( g_gametype.integer < GT_TEAM && mode == SAY_TEAM ) {
		mode = SAY_ALL;
	}

	if ( target ) {
		G_VoiceTo( ent, target, mode, id, voiceonly );
		return;
	}

	// echo the text to the console
	if ( g_dedicated.integer ) {
		G_Printf( "voice: %s %s\n", ent->client->pers.netname, id);
	}

	// send it to all the apropriate clients
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		G_VoiceTo( ent, other, mode, id, voiceonly );
	}
}

/*
==================
Cmd_Voice_f
==================
*/
static void Cmd_Voice_f( gentity_t *ent, int mode, qboolean arg0, qboolean voiceonly ) {
	char		*p;

	if ( trap_Argc () < 2 && !arg0 ) {
		return;
	}

	if (arg0)
	{
		p = ConcatArgs( 0 );
	}
	else
	{
		p = ConcatArgs( 1 );
	}

	G_Voice( ent, NULL, mode, p, voiceonly );
}

/*
==================
Cmd_VoiceTell_f
==================
*/
static void Cmd_VoiceTell_f( gentity_t *ent, qboolean voiceonly ) {
	int			targetNum;
	gentity_t	*target;
	char		*id;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap_Argc () < 2 ) {
		return;
	}

	trap_Argv( 1, arg, sizeof( arg ) );
	targetNum = atoi( arg );
	if ( targetNum < 0 || targetNum >= level.maxclients ) {
		return;
	}

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client ) {
		return;
	}

	id = ConcatArgs( 2 );

	G_LogPrintf( "vtell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, id );
	G_Voice( ent, target, SAY_TELL, id, voiceonly );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Voice( ent, ent, SAY_TELL, id, voiceonly );
	}
}


/*
==================
Cmd_VoiceTaunt_f
==================
*/
static void Cmd_VoiceTaunt_f( gentity_t *ent ) {
	gentity_t *who;
	int i;

	if (!ent->client) {
		return;
	}

	// insult someone who just killed you
	if (ent->enemy && ent->enemy->client && ent->enemy->client->lastkilled_client == ent->s.number) {
		// i am a dead corpse
		if (!(ent->enemy->r.svFlags & SVF_BOT)) {
			G_Voice( ent, ent->enemy, SAY_TELL, VOICECHAT_DEATHINSULT, qfalse );
		}
		if (!(ent->r.svFlags & SVF_BOT)) {
			G_Voice( ent, ent,        SAY_TELL, VOICECHAT_DEATHINSULT, qfalse );
		}
		ent->enemy = NULL;
		return;
	}
	// insult someone you just killed
	if (ent->client->lastkilled_client >= 0 && ent->client->lastkilled_client != ent->s.number) {
		who = g_entities + ent->client->lastkilled_client;
		if (who->client) {
			// who is the person I just killed
			if (who->client->lasthurt_mod == MOD_GAUNTLET) {
				if (!(who->r.svFlags & SVF_BOT)) {
					G_Voice( ent, who, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse );	// and I killed them with a gauntlet
				}
				if (!(ent->r.svFlags & SVF_BOT)) {
					G_Voice( ent, ent, SAY_TELL, VOICECHAT_KILLGAUNTLET, qfalse );
				}
			} else {
				if (!(who->r.svFlags & SVF_BOT)) {
					G_Voice( ent, who, SAY_TELL, VOICECHAT_KILLINSULT, qfalse );	// and I killed them with something else
				}
				if (!(ent->r.svFlags & SVF_BOT)) {
					G_Voice( ent, ent, SAY_TELL, VOICECHAT_KILLINSULT, qfalse );
				}
			}
			ent->client->lastkilled_client = -1;
			return;
		}
	}

	if (g_gametype.integer >= GT_TEAM) {
		// praise a team mate who just got a reward
		for(i = 0; i < MAX_CLIENTS; i++) {
			who = g_entities + i;
			if (who->client && who != ent && who->client->sess.sessionTeam == ent->client->sess.sessionTeam) {
				if (who->client->rewardTime > level.time) {
					if (!(who->r.svFlags & SVF_BOT)) {
						G_Voice( ent, who, SAY_TELL, VOICECHAT_PRAISE, qfalse );
					}
					if (!(ent->r.svFlags & SVF_BOT)) {
						G_Voice( ent, ent, SAY_TELL, VOICECHAT_PRAISE, qfalse );
					}
					return;
				}
			}
		}
	}

	// just say something
	G_Voice( ent, NULL, SAY_ALL, VOICECHAT_TAUNT, qfalse );
}
#endif


static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};

void Cmd_GameCommand_f( gentity_t *ent ) {
	int		player;
	int		order;
	char	str[MAX_TOKEN_CHARS];

	trap_Argv( 1, str, sizeof( str ) );
	player = atoi( str );
	trap_Argv( 2, str, sizeof( str ) );
	order = atoi( str );

	if ( (unsigned)player >= MAX_CLIENTS ) {
		return;
	}
	if ( (unsigned) order > ARRAY_LEN( gc_orders ) ) {
		return;
	}
	G_Say( ent, &g_entities[player], SAY_TELL, gc_orders[order] );
	G_Say( ent, ent, SAY_TELL, gc_orders[order] );
}


/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	trap_SendServerCommand( ent-g_entities, va("print \"%s\n\"", vtos( ent->s.origin ) ) );
}

/*
==================
Cmd_Time_f

Prints the current local time for the caller
==================
*/
static void Cmd_Time_f( gentity_t *ent ) {
    qtime_t now;
    trap_RealTime( &now );
    trap_SendServerCommand(
        ent - g_entities,
        va(
            "print \"^3Time 23:^7 %04d-%02d-%02d %02d:%02d:%02d\n\"",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday,
            now.tm_hour,
            now.tm_min,
            now.tm_sec
        )
    );
}

/* Server performance/memory info */
static void Cmd_SysInfo_f( gentity_t *ent ) {
    char buf[MAX_STRING_CHARS];
    int len = 0;
    const char *hunkmegs;
    const char *sv_fps;
    const char *com_fps;
    int uptimeMs;
    int serverTime;
    int i, inuseEnts = 0, itemEnts = 0, clientEnts = 0;
    buf[0] = '\0';

    // memory hints
    hunkmegs = va("%i", trap_Cvar_VariableIntegerValue("com_hunkMegs"));
    // tickrate
    sv_fps = va("%i", trap_Cvar_VariableIntegerValue("sv_fps"));
    com_fps = va("%i", trap_Cvar_VariableIntegerValue("com_maxfps"));

    uptimeMs = level.time - level.startTime;
    serverTime = level.time;

    for ( i = 0; i < level.num_entities; ++i ) {
        gentity_t *e = &g_entities[i];
        if ( e->inuse ) {
            inuseEnts++;
            if ( e->item ) itemEnts++;
            if ( i < MAX_CLIENTS && e->client ) clientEnts++;
        }
    }

    len += Com_sprintf( buf + len, sizeof(buf) - len, "^2Server SysInfo:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7----------------\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^3Tickrate   : ^7sv_fps=%s, com_maxfps=%s\n", sv_fps, com_fps );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^3Uptime     : ^7%dm %02ds (serverTime=%d)\n", (uptimeMs/60000), (uptimeMs/1000)%60, serverTime );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^3Entities   : ^7in use %d / %d (clients %d, items %d)\n", inuseEnts, level.num_entities, clientEnts, itemEnts );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^3Clients    : ^7max %d, connected %d, playing %d\n", level.maxclients, level.numConnectedClients, level.numPlayingClients );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^3Hunk (cfg) : ^7com_hunkMegs=%s\n", hunkmegs );
    // Note: detailed allocator stats aren't available in QVM; expose game_memory console cmd hint
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^3Memory Hint: ^7use \'game_memory\' in server console for allocation summary\n" );

    trap_SendServerCommand( ent - g_entities, va("print \"%s\n\"", buf) );
}

// Display labels for weapons (indexes follow weapon_t), with colon and padded to width ~13
static const char *const s_weaponLabel[] = {
    "",                 // WP_NONE
    "^3Gauntlet    :^7",    // WP_GAUNTLET
    "^3MachineGun  :^7",    // WP_MACHINEGUN
    "^3Shotgun     :^7",    // WP_SHOTGUN
    "^3G.Launcher  :^7",    // WP_GRENADE_LAUNCHER
    "^3R.Launcher  :^7",    // WP_ROCKET_LAUNCHER
    "^3LightningGun:^7",    // WP_LIGHTNING
    "^3Railgun     :^7",    // WP_RAILGUN
    "^3PlasmaGun   :^7",    // WP_PLASMAGUN
    "^3BFG10K      :^7",    // WP_BFG
    "^3Grapple     :^7",    // WP_GRAPPLING_HOOK
#ifdef MISSIONPACK
    "^3Nailgun     :^7",    // WP_NAILGUN
    "^3ProxLauncher:^7",    // WP_PROX_LAUNCHER
    "^3Chaingun    :^7",    // WP_CHAINGUN
#endif
};

/* Forward declarations for functions referenced in the dispatch table (C89/q3lcc) */
/* callvote moved to cmds/votesystem.c */
void Cmd_CallVote_f( gentity_t *ent );
void Cmd_Vote_f( gentity_t *ent );
static void Cmd_SetViewpos_f( gentity_t *ent );
static void G_PrintStatsForClientTo( gentity_t *ent, gclient_t *cl );
static void Cmd_Stats_f( gentity_t *ent );
static void Cmd_StatsAll_f( gentity_t *ent );
void Cmd_Topshots_f( gentity_t *ent );
void Cmd_Awards_f( gentity_t *ent );
void playsound_f( gentity_t *ent );
void map_restart_f( gentity_t *ent );
void Osp_Wstats( gentity_t *ent );
static void Cmd_Ready_f( gentity_t *ent );
static void Cmd_Unready_f( gentity_t *ent );

// lightweight wrappers for commands that need fixed params
static void Cmd_FollowNext_f( gentity_t *ent ) { Cmd_FollowCycle_f( ent, 1 ); }
static void Cmd_FollowPrev_f( gentity_t *ent ) { Cmd_FollowCycle_f( ent, -1 ); }
static void Cmd_Help_f( gentity_t *ent );
void Cmd_ScoresText_f( gentity_t *ent );
static void Cmd_MapList_f( gentity_t *ent );
static void Cmd_Rotation_f( gentity_t *ent );
void Cmd_CV_f( gentity_t *ent ); /* from cmds/votesystem.c */
static void Cmd_SpawnFrozenBody_f( gentity_t *ent );
static void Cmd_ReadyAll_f( gentity_t *ent );
static void Cmd_UnreadyAll_f( gentity_t *ent );
static void Cmd_RemoveFrozenBody_f( gentity_t *ent );
static void Cmd_AdminCP_f( gentity_t *ent );
static void Cmd_AdminSay_f( gentity_t *ent );
static void Cmd_AdminBan_f( gentity_t *ent );
static void Cmd_AdminKick_f( gentity_t *ent );
static void Cmd_AdminMute_f( gentity_t *ent );
static void Cmd_AdminBanList_f( gentity_t *ent );
static void Cmd_AdminMuteList_f( gentity_t *ent );
static void Cmd_AdminUnban_f( gentity_t *ent );
static void Cmd_AdminUnmute_f( gentity_t *ent );
static void Cmd_AdminDumpUser_f( gentity_t *ent );
/* announcer commands */
void AN_Cmd_AnnReload( gentity_t *ent );
void AN_Cmd_AnnList( gentity_t *ent );
void AN_Cmd_AnnEnable( gentity_t *ent );
void AN_Cmd_AnnForce( gentity_t *ent );

typedef void (*gameCmdHandler_t)( gentity_t *ent );
typedef struct {
    const char *name;
    gameCmdHandler_t handler;
    qboolean requiresAuth;
} gameCommandDef_t;

// Command dispatch table (simple gentity_t* signature only)
static const gameCommandDef_t gameCommandTable[] = {
    { "help",           Cmd_Help_f,          qfalse },
    { "give",            Cmd_Give_f,         qfalse },
    { "god",             Cmd_God_f,          qfalse },
    { "notarget",        Cmd_Notarget_f,     qfalse },
    { "noclip",          Cmd_Noclip_f,       qfalse },
    { "kill",            Cmd_Kill_f,         qfalse },
    { "teamtask",        Cmd_TeamTask_f,     qfalse },
    { "levelshot",       Cmd_LevelShot_f,    qfalse },
    { "follow",          Cmd_Follow_f,       qfalse },
    { "follownext",      Cmd_FollowNext_f,   qfalse },
    { "followprev",      Cmd_FollowPrev_f,   qfalse },
    { "team",            Cmd_Team_f,         qfalse },
    { "where",           Cmd_Where_f,        qfalse },
    { "time",            Cmd_Time_f,         qfalse },
    { "servertime",      Cmd_Time_f,         qfalse },
    { "sysinfo",         Cmd_SysInfo_f,      qtrue  },
    { "callvote",        Cmd_CallVote_f,     qfalse },
    { "vote",            Cmd_Vote_f,         qfalse },
    /* team votes removed from g_cmds; handled elsewhere or not supported */
    { "gc",              Cmd_GameCommand_f,  qfalse },
    { "setviewpos",      Cmd_SetViewpos_f,   qfalse },
    { "getpos",          getpos_f,           qfalse },
    { "setpos",          setpos_f,           qtrue  },
    { "poscp",           poscp_f,            qtrue  },
    { "spawns",          spawns_f,           qtrue  },
    { "spawnadd",        spawnadd_f,         qtrue  },
    { "spawnrm",         spawnrm_f,          qtrue  },
    { "spawnsave",       spawnsave_f,        qtrue  },
    { "spawnreload",     spawnreload_f,      qtrue  },
    { "spawnundo",       spawnundo_f,        qtrue  },
    { "spawnredo",       spawnredo_f,        qtrue  },
    { "spawnang",        spawnang_f,         qtrue  },
    { "spawnscp",        spawnscp_f,         qtrue  },
    { "hi",              hi_f,               qfalse },
    { "fteam",           fteam_f,            qtrue  },
    { "fukk",            fteam_f,            qtrue  },
    { "auth",            plsauth_f,          qfalse },
    { "deauth",          deauth_f,           qfalse },
    { "checkauth",       checkauth_f,        qfalse },
    { "sndplay",         playsound_f,        qtrue  },
    { "killplayer",      killplayer_f,       qtrue  },
    { "setname",         setname_f,          qtrue  },
    { "svfps",           Cmd_svfps_f,        qfalse },
    { "restart",         map_restart_f,      qtrue  },
    { "getstatsinfo",    Osp_Wstats,         qfalse },
    { "stats",           Cmd_Stats_f,        qfalse },
    { "statsall",        Cmd_StatsAll_f,     qfalse },
    { "topshots",        Cmd_Topshots_f,     qfalse },
    { "awards",          Cmd_Awards_f,       qfalse },
    { "scores",          Cmd_ScoresText_f,   qfalse },
    { "maplist",         Cmd_MapList_f,      qfalse },
    { "rotation",        Cmd_Rotation_f,     qfalse },
    { "cv",              Cmd_CV_f,           qfalse },
    { "spawnfrozenbody", Cmd_SpawnFrozenBody_f, qtrue },
    { "readyall",       Cmd_ReadyAll_f,     qtrue  },
    { "unreadyall",     Cmd_UnreadyAll_f,   qtrue  },
    { "annreload",       AN_Cmd_AnnReload,   qtrue  },
    { "annlist",         AN_Cmd_AnnList,     qtrue  },
    { "annenable",       AN_Cmd_AnnEnable,   qtrue  },
    { "annforce",        AN_Cmd_AnnForce,    qtrue  },
    { "atest",           Cmd_NewTest_f,      qfalse },
    { "listitems",       listitems_f,        qtrue  },
    { "items",           items_f,            qtrue  },
    { "irreload",        irreload_f,         qtrue  },
    { "irlist",          irlist_f,           qtrue  },
    { "irmatchcp",       irmatchcp_f,        qtrue  },
    { "iradd",           iradd_f,            qtrue  },
    { "irrm",            irrm_f,             qtrue  },
    { "irrepl",          irrepl_f,           qtrue  },
    { "irmove",          irmove_f,           qtrue  },
    { "irundo",          irundo_f,           qtrue  },
    { "irredo",          irredo_f,           qtrue  },
    { "irrespawn",       irrespawn_f,        qtrue  },
    { "irsave",          irsave_f,           qtrue  },
    { "wtreload",       wtreload_f,         qtrue  },
    { "wtlist",         wtlist_f,           qtrue  },
    { "wtsave",         wtsave_f,           qtrue  },
    { "shuffle",         shuffle_f,          qtrue  },
    { "playerlist",      Cmd_Plrlist_f,      qfalse },
    { "players",         Cmd_Plrlist_f,      qfalse },
    { "from",            Cmd_From_f,         qfalse },
    { "ready",           Cmd_Ready_f,        qfalse },
    { "unready",         Cmd_Unready_f,      qfalse },
    { "removefrozenbody", Cmd_RemoveFrozenBody_f, qtrue },
    { "acp",              Cmd_AdminCP_f,      qtrue  },
    { "asay",            Cmd_AdminSay_f,     qtrue  },
    { "aban",            Cmd_AdminBan_f,     qtrue  },
    { "akick",           Cmd_AdminKick_f,    qtrue  },
    { "amute",           Cmd_AdminMute_f,    qtrue  },
    { "abanlist",        Cmd_AdminBanList_f, qtrue  },
    { "amutelist",       Cmd_AdminMuteList_f, qtrue  },
    { "aunban",          Cmd_AdminUnban_f,   qtrue  },
    { "aunmute",         Cmd_AdminUnmute_f,  qtrue  },
    { "adumpuser",       Cmd_AdminDumpUser_f, qtrue  },
};

/* Commands handled directly in ClientCommand (not via table) */
static const char *const s_directClientCmds[] = {
    "say",
    "say_team",
    "tell",
#ifdef MISSIONPACK
    "vsay",
    "vsay_team",
    "vtell",
    "vosay",
    "vosay_team",
    "votell",
    "vtaunt",
#endif
    "score"
};

static qboolean DispatchGameCommand( const char *cmd, gentity_t *ent ) {
    int i;
    for ( i = 0; i < (int)ARRAY_LEN( gameCommandTable ); ++i ) {
        if ( Q_stricmp( cmd, gameCommandTable[i].name ) == 0 ) {
            gameCommandTable[i].handler( ent );
            return qtrue;
        }
    }
    return qfalse;
}




/* moved to cmds/votesystem.c */


/*
==================
Cmd_CallTeamVote_f
==================
*/
static void Cmd_CallTeamVote_f( gentity_t *ent ) {
	int		i, team, cs_offset;
	char	arg1[MAX_STRING_TOKENS];
	char	arg2[MAX_STRING_TOKENS];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

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

	// make sure it is a valid command to vote on
	trap_Argv( 1, arg1, sizeof( arg1 ) );
	arg2[0] = '\0';
	for ( i = 2; i < trap_Argc(); i++ ) {
		if (i > 2)
			strcat(arg2, " ");
		trap_Argv( i, &arg2[strlen(arg2)], sizeof( arg2 ) - (int)strlen(arg2) );
	}

	if( strchr( arg1, ';' ) || strchr( arg2, ';' ) || strchr( arg2, '\n' ) || strchr( arg2, '\r' ) ) {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	if ( !Q_stricmp( arg1, "leader" ) ) {
		char netname[MAX_NETNAME], leader[MAX_NETNAME];

		if ( !arg2[0] ) {
			i = ent->client->ps.clientNum;
		}
		else {
			// numeric values are just slot numbers
			for (i = 0; i < 3; i++) {
				if ( !arg2[i] || arg2[i] < '0' || arg2[i] > '9' )
					break;
			}
			if ( i >= 3 || !arg2[i]) {
				i = atoi( arg2 );
				if ( i < 0 || i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", i) );
					return;
				}

				if ( !g_entities[i].inuse ) {
					trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", i) );
					return;
				}
			}
			else {
				Q_strncpyz(leader, arg2, sizeof(leader));
				Q_CleanStr(leader);
				for ( i = 0 ; i < level.maxclients ; i++ ) {
					if ( level.clients[i].pers.connected == CON_DISCONNECTED )
						continue;
					if (level.clients[i].sess.sessionTeam != team)
						continue;
					Q_strncpyz(netname, level.clients[i].pers.netname, sizeof(netname));
					Q_CleanStr(netname);
					if ( !Q_stricmp(netname, leader) ) {
						break;
					}
				}
				if ( i >= level.maxclients ) {
					trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) );
					return;
				}
			}
		}
		Com_sprintf(arg2, sizeof(arg2), "%d", i);
	} else {
		trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
		return;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if (level.clients[i].sess.sessionTeam == team)
			trap_SendServerCommand( i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname ) );
	}

	// start the voting, the caller automatically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
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


/*
==================
Cmd_TeamVote_f
==================
*/
static void Cmd_TeamVote_f( gentity_t *ent ) {
	int			team, cs_offset;
	char		msg[64];

	team = ent->client->sess.sessionTeam;
	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap_SendServerCommand( ent-g_entities, "print \"^3No team vote in progress.\n\"" );
		return;
	}
	if ( ent->client->pers.teamVoted != 0 ) {
		trap_SendServerCommand( ent-g_entities, "print \"^3Team vote already cast.\n\"" );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent-g_entities, "print \"^3Not allowed to vote as spectator.\n\"" );
		return;
	}

	// trap_SendServerCommand( ent-g_entities, "print \"Team vote cast.\n\"" );

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

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
static void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( !g_cheats.integer ) {
		trap_SendServerCommand( ent-g_entities, "print \"Cheats are not enabled on this server.\n\"");
		return;
	}
	if ( trap_Argc() != 5 ) {
		trap_SendServerCommand( ent-g_entities, "print \"usage: setviewpos x y z yaw\n\"");
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap_Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap_Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	TeleportPlayer( ent, origin, angles );
}



/*
=================
Cmd_Stats_f
=================
*/
static void G_PrintStatsForClientTo( gentity_t *ent, gclient_t *cl ) {
    int w;
    char line[256];
    char buf[MAX_STRING_CHARS];
    int len;
    int shots, hits;
    int accInt, accFrac;

    buf[0] = '\0';
    len = 0;

    /* Helper to append and flush when needed */
    /* Note: keep some headroom to accommodate va/print wrapping */
#define FLUSH_BUF() \
    do { \
        if ( len > 0 ) { \
            trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) ); \
            buf[0] = '\0'; \
            len = 0; \
        } \
    } while (0)

#define APPEND_LINE(str) \
    do { \
        const char *s__ = (str); \
        int add__ = (int)strlen( s__ ); \
        if ( len + add__ + 32 >= (int)sizeof( buf ) ) { \
            FLUSH_BUF(); \
        } \
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", s__ ); \
    } while (0)

    Com_sprintf( line, sizeof(line), "Accuracy info for: %s^7\n\n", cl->pers.netname );
    APPEND_LINE( line );
    APPEND_LINE( "Weapon            Accrcy      Hits/Atts     Kills   Deaths Pickup Drops\n" );
    APPEND_LINE( "------------------------------------------------------------------------\n" );

    {
        qboolean any;
        any = qfalse;
        for ( w = WP_GAUNTLET; w < WP_NUM_WEAPONS; ++w ) {
        if ( cl->perWeaponShots[w] == 0 && cl->perWeaponHits[w] == 0 && cl->perWeaponDamageGiven[w] == 0 && cl->perWeaponDamageTaken[w] == 0 ) {
                continue;
        }
            any = qtrue;
        shots = cl->perWeaponShots[w];
        hits = cl->perWeaponHits[w];
        if ( w == WP_SHOTGUN && hits > 0 ) {
            hits = hits > 0 ? 1 : 0;
        }
        if ( shots > 0 ) {
                int num;
                int pct;
                num = hits * 1000;
                pct = shots ? num / shots : 0;
            accInt = pct / 10;
            accFrac = pct % 10;
        } else {
            accInt = 0; accFrac = 0;
        }
        Com_sprintf( line, sizeof(line), "%-13s %8d.%1d    %2d/%-6d  %6d  %6d %6d  %5d\n",
            (w < (int)ARRAY_LEN(s_weaponLabel) ? s_weaponLabel[w] : "Weapon:"),
            accInt, accFrac, hits, shots,
            cl->perWeaponKills[w], cl->perWeaponDeaths[w],
            cl->perWeaponPickups[w], cl->perWeaponDrops[w] );
            APPEND_LINE( line );
        }
        if ( !any ) {
            APPEND_LINE( "No weapon info available.\n" );
            FLUSH_BUF();
            return; // don't draw footer when no info
        }
    }

    APPEND_LINE( "\n" );
    Com_sprintf( line, sizeof(line), "^3Damage Given: ^7%d", cl->totalDamageGiven );
    APPEND_LINE( line );
    Com_sprintf( line, sizeof(line), "     ^2Armor Taken : ^7%d", cl->armorPickedTotal );
    if ( cl->armorRACount > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^1RA^2)", cl->armorRACount) );
    }
    if ( cl->armorYACount > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^3YA^2)", cl->armorYACount) );
    }
    if ( cl->armorShardCount > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^2SH^2)", cl->armorShardCount) );
    }
    APPEND_LINE( line );
    Com_sprintf( line, sizeof(line), "\n^3Damage Recvd: ^7%d", cl->totalDamageTaken );
    APPEND_LINE( line );
    Com_sprintf( line, sizeof(line), "     ^2Health Taken: ^7%d", cl->healthPickedTotal );
    if ( cl->healthMegaCount > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^5MEGA^2)", cl->healthMegaCount) );
    }
    if ( cl->health50Count > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^150^2)", cl->health50Count) );
    }
    if ( cl->health25Count > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^325^2)", cl->health25Count) );
    }
    if ( cl->health5Count > 0 ) {
        Q_strcat( line, sizeof(line), va(" ^2(^7%d ^25)^2", cl->health5Count) );
    }
    APPEND_LINE( line );
    if ( cl->totalDamageTaken > 0 ) {
        int ratioTimes100;
        ratioTimes100 = (cl->totalDamageGiven * 100) / cl->totalDamageTaken;
        Com_sprintf( line, sizeof(line), "\n^3Damage Ratio: ^7%d.%02d\n",
            ratioTimes100 / 100, ratioTimes100 % 100 );
    } else {
        Com_sprintf( line, sizeof(line), "\n^3Damage Ratio: ^70.00\n" );
    }
    APPEND_LINE( line );

    FLUSH_BUF();

#undef APPEND_LINE
#undef FLUSH_BUF
}

static void Cmd_Stats_f( gentity_t *ent ) {
    gclient_t *cl;
    gclient_t *targetCl;
    int targetNum = -1;
    int w;
    char line[256];
    int shots, hits;
    int accInt, accFrac;

    if ( !ent || !ent->client ) return;

    // Determine which client's stats to show
    {
        int argc = trap_Argc();
        targetCl = NULL;

        if ( argc >= 2 ) {
            char arg[MAX_TOKEN_CHARS];
            int i, isNum = 1;
            trap_Argv( 1, arg, sizeof(arg) );
            for ( i = 0; arg[i]; ++i ) {
                if ( arg[i] < '0' || arg[i] > '9' ) { isNum = 0; break; }
            }
            if ( isNum ) {
                targetNum = atoi(arg);
                if ( targetNum < 0 || targetNum >= level.maxclients ) {
                    trap_SendServerCommand( ent - g_entities, "print \"^1! ^3No such client number.\n\"" );
                    return;
                }
                if ( level.clients[targetNum].pers.connected != CON_CONNECTED ) {
                    trap_SendServerCommand( ent - g_entities, "print \"^1! ^3Client is not connected.\n\"" );
                    return;
                }
                if ( g_freeze.integer ? ftmod_isSpectator(&level.clients[targetNum]) : (level.clients[targetNum].sess.sessionTeam == TEAM_SPECTATOR) ) {
                    trap_SendServerCommand( ent - g_entities, "print \"^1! ^3Target is a spectator.\n\"" );
                    return;
                }
                targetCl = &level.clients[targetNum];
            }
        }

        if ( !targetCl ) {
            // default: current player, or followed target if spectating
            if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
                targetNum = ent->client->sess.spectatorClient;
                if ( targetNum >= 0 && targetNum < level.maxclients &&
                     level.clients[targetNum].pers.connected == CON_CONNECTED ) {
                    targetCl = &level.clients[targetNum];
                }
            }
            if ( !targetCl ) {
                if ( g_freeze.integer ? ftmod_isSpectator(ent->client) : (ent->client->sess.sessionTeam == TEAM_SPECTATOR) ) {
                    trap_SendServerCommand( ent - g_entities, "print \"^3Usage: ^7stats <clientnum>\n\"" );
                    return;
                }
                targetCl = ent->client; // self
                targetNum = ent->client - level.clients;
            }
        }
    }

    cl = targetCl;
    G_PrintStatsForClientTo( ent, cl );
}

/*
=================
Cmd_StatsAll_f
=================
*/
static void Cmd_StatsAll_f( gentity_t *ent ) {
    static int nextIdx = 0;
    int processed = 0;
    int i;
    if ( !ent || !ent->client ) {
        return;
    }
    /* process a small batch per command to avoid long frame; schedule next chunk using think */
    for ( i = 0; i < level.maxclients && processed < 4; ++i ) {
        int idx = (nextIdx + i) % level.maxclients;
        gclient_t *cl = &level.clients[idx];
        if ( cl->pers.connected != CON_CONNECTED ) {
            continue;
        }
        G_PrintStatsForClientTo( ent, cl );
        processed++;
    }
    nextIdx = (nextIdx + processed) % level.maxclients;
    if ( processed > 0 ) {
        /* reschedule self after ~30ms using a temp entity think */
        gentity_t *te = G_Spawn();
        te->classname = "statsall_sched";
        te->think = (void(*)(gentity_t*))Cmd_StatsAll_f;
        te->nextthink = level.time + 70;
        te->r.svFlags |= SVF_NOCLIENT;
    }
}

/*
=================
Cmd_Topshots_f
=================
*/
void Cmd_Topshots_f( gentity_t *ent ) {
    int i;
    int w;
    /* best per-weapon holders (for GA..BFG only) */
    int bestClient[WP_NUM_WEAPONS];
    int bestPct10[WP_NUM_WEAPONS];
    int bestHits[WP_NUM_WEAPONS];
    int bestShots[WP_NUM_WEAPONS];
    int bestKills[WP_NUM_WEAPONS];
    int bestDeaths[WP_NUM_WEAPONS];

    char buf[MAX_STRING_CHARS];
    char line[256];
    int len;

    if ( !ent || !ent->client ) {
        return;
    }

    buf[0] = '\0';
    len = 0;

#define FLUSH_BUF() \
    do { \
        if ( len > 0 ) { \
            trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) ); \
            buf[0] = '\0'; \
            len = 0; \
        } \
    } while (0)

#define APPEND_LINE(str) \
    do { \
        const char *s__ = (str); \
        int add__ = (int)strlen( s__ ); \
        if ( len + add__ + 32 >= (int)sizeof( buf ) ) { \
            FLUSH_BUF(); \
        } \
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", s__ ); \
    } while (0)

    /* init best arrays */
    for ( w = 0; w < WP_NUM_WEAPONS; ++w ) {
        bestClient[w] = -1;
        bestPct10[w] = -1;
        bestHits[w] = 0;
        bestShots[w] = 0;
        bestKills[w] = 0;
        bestDeaths[w] = 0;
    }
    for ( i = 0; i < level.maxclients; ++i ) {
        gclient_t *cl = &level.clients[i];
        if ( cl->pers.connected != CON_CONNECTED ) {
            continue;
        }
        for ( w = WP_GAUNTLET; w <= WP_BFG; ++w ) {
            int hitsNorm;
            int hits = cl->perWeaponHits[w];
            int shots = cl->perWeaponShots[w];
            if ( shots <= 0 ) {
                continue;
            }
            hitsNorm = hits;
            if ( w == WP_SHOTGUN && hitsNorm > 0 ) {
                hitsNorm = 1; /* treat any pellet hit as one for accuracy */
            }
            {
                int thisPct10 = (hitsNorm * 1000) / shots;
                if ( thisPct10 > bestPct10[w] ) {
                    bestPct10[w] = thisPct10;
                    bestClient[w] = i;
                    bestHits[w] = hits;
                    bestShots[w] = shots;
                    bestKills[w] = cl->perWeaponKills[w];
                    bestDeaths[w] = cl->perWeaponDeaths[w];
                }
            }
        }
    }

    /* Header */
    APPEND_LINE( "^2Best Match Accuracies:\n" );
    APPEND_LINE( "^3WP    Acc Hits/Atts Kills Deaths\n" );
    APPEND_LINE( "------------------------------------------------------\n" );

    /* Abbreviations for GA..BFG */
    {
        static const char *const weaponAbbrev[] = {
            "",      /* WP_NONE */
            "GA",    /* 1 Gauntlet */
            "MG",    /* 2 Machinegun */
            "SG",    /* 3 Shotgun */
            "GL",    /* 4 Grenade Launcher */
            "RL",    /* 5 Rocket Launcher */
            "LG",    /* 6 Lightning Gun */
            "RG",    /* 7 Railgun */
            "PG",    /* 8 PlasmaGun */
            "BFG",   /* 9 BFG */
        };
        for ( w = WP_GAUNTLET; w <= WP_BFG; ++w ) {
            int cnum = bestClient[w];
            if ( cnum >= 0 && bestPct10[w] >= 0 ) {
                Com_sprintf( line, sizeof(line), "^3%-3s^7 %3d.%1d    %4d/%-4d      ^2%3d      ^1%3d ^7%s\n",
                    weaponAbbrev[w],
                    bestPct10[w] / 10, bestPct10[w] % 10,
                    bestHits[w], bestShots[w],
                    bestKills[w], bestDeaths[w],
                    level.clients[cnum].pers.netname );
                APPEND_LINE( line );
            }
        }
    }

    FLUSH_BUF();

#undef APPEND_LINE
#undef FLUSH_BUF
}

/*
=================
Cmd_Help_f
=================
*/
static void Cmd_Help_f( gentity_t *ent ) {
    char buf[MAX_STRING_CHARS];
    char cell[64];
    char row[256];
    int len = 0;
    int i;
    qboolean isAuthed = (ent && ent->authed);
    /* collect names */
    const char *names[512];
    int disabled[512];
    int count;
    const char *authNames[256];
    int authDisabled[256];
    int authCount;
    int maxLen;
    int colWidth;
    int perRow;
    int idx;
    buf[0] = '\0';

    count = 0;
    authCount = 0;

    /* core commands */
    for ( i = 0; i < (int)ARRAY_LEN( s_directClientCmds ); ++i ) {
        if ( count < (int)ARRAY_LEN(names) ) {
            names[count] = s_directClientCmds[i];
            disabled[count] = DC_IsDisabled( s_directClientCmds[i] );
            count++;
        }
    }
    /* table commands */
    for ( i = 0; i < (int)ARRAY_LEN( gameCommandTable ); ++i ) {
        if ( gameCommandTable[i].requiresAuth ) {
            if ( authCount < (int)ARRAY_LEN(authNames) ) {
                authNames[authCount] = gameCommandTable[i].name;
                authDisabled[authCount] = DC_IsDisabled( gameCommandTable[i].name );
                authCount++;
            }
        } else {
            if ( count < (int)ARRAY_LEN(names) ) {
                names[count] = gameCommandTable[i].name;
                disabled[count] = DC_IsDisabled( gameCommandTable[i].name );
                count++;
            }
        }
    }

    /* print general commands in a grid */
    len += Com_sprintf( buf + len, sizeof(buf) - len, "\n^2Commands:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7---------\n" );
    maxLen = 0;
    for ( i = 0; i < count; ++i ) {
        int nlen = (int)strlen( names[i] );
        if ( nlen > maxLen ) maxLen = nlen;
    }
    if ( maxLen < 1 ) maxLen = 1;
    if ( maxLen > 14 ) maxLen = 14;
    colWidth = maxLen + 2;
    perRow = 4;
    row[0] = '\0';
    for ( idx = 0; idx < count; ++idx ) {
        const char *color = disabled[idx] ? "^1" : "^7";
        Com_sprintf( cell, sizeof(cell), "%s%-*s^7", color, colWidth, names[idx] );
        Q_strcat( row, sizeof(row), cell );
        if ( ((idx+1) % perRow) == 0 || idx+1 == count ) {
            len += Com_sprintf( buf + len, sizeof(buf) - len, "%s\n", row );
            row[0] = '\0';
            if ( len > (int)sizeof(buf) - 256 ) {
                trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
                buf[0] = '\0';
                len = 0;
            }
        } else {
            Q_strcat( row, sizeof(row), " " );
        }
    }

    /* print auth-only commands in a separate grid (only for authed users) */
    if ( isAuthed && authCount > 0 ) {
        len += Com_sprintf( buf + len, sizeof(buf) - len, "\n^2Commands (auth required):^7\n" );
        len += Com_sprintf( buf + len, sizeof(buf) - len, "^7-------------------------\n" );
        maxLen = 0;
        for ( i = 0; i < authCount; ++i ) {
            int nlen2 = (int)strlen( authNames[i] );
            if ( nlen2 > maxLen ) maxLen = nlen2;
        }
        if ( maxLen < 1 ) maxLen = 1;
        if ( maxLen > 14 ) maxLen = 14;
        colWidth = maxLen + 2;
        perRow = 4;
        row[0] = '\0';
        for ( idx = 0; idx < authCount; ++idx ) {
            const char *color2 = authDisabled[idx] ? "^1" : "^3"; /* auth ones in yellow unless disabled */
            Com_sprintf( cell, sizeof(cell), "%s%-*s^7", color2, colWidth, authNames[idx] );
            Q_strcat( row, sizeof(row), cell );
            if ( ((idx+1) % perRow) == 0 || idx+1 == authCount ) {
                len += Com_sprintf( buf + len, sizeof(buf) - len, "%s\n", row );
                row[0] = '\0';
                if ( len > (int)sizeof(buf) - 256 ) {
                    trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
                    buf[0] = '\0';
                    len = 0;
                }
            } else {
                Q_strcat( row, sizeof(row), " " );
            }
        }
    }

    if ( len > 0 ) {
        trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
    }
}

/*
=================
Cmd_ScoresText_f
=================
*/
void Cmd_ScoresText_f( gentity_t *ent ) {
    char buf[MAX_STRING_CHARS];
    char line[256];
    int len = 0;
    int i;
    buf[0] = '\0';
    Com_sprintf( line, sizeof(line), "\nMap: %s\n", g_mapname.string );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", line );
    {
        int secs = (g_timelimit.integer * 60) - (level.time / 1000);
        if ( secs < 0 ) secs = 0;
        Com_sprintf( line, sizeof(line), "Time Remaining: %d:%02d\n\n", secs/60, secs%60 );
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", line );
    }
    len += Com_sprintf( buf + len, sizeof(buf) - len, "Player          Kll Dth Sui Time  FPH Eff     DG     DR  Score\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "--------------------------------------------------------------\n" );
    for ( i = 0; i < level.maxclients; ++i ) {
        gclient_t *cl = &level.clients[i];
        int mins;
        int fph;
        int eff10;
        if ( cl->pers.connected != CON_CONNECTED ) continue;
        mins = (level.time - cl->pers.enterTime) / 60000;
        if ( mins <= 0 ) mins = 0;
        fph = mins > 0 ? (cl->kills * 60) / (mins == 0 ? 1 : mins) : (cl->kills * 60);
        eff10 = (cl->kills + cl->deaths) > 0 ? (cl->kills * 1000) / (cl->kills + cl->deaths) : 0;
        Com_sprintf( line, sizeof(line), "%-14s %3d %3d %3d %4d %4d %3d %6d %6d %6d\n",
            cl->pers.netname,
            cl->kills,
            cl->deaths,
            0, /* suicides not tracked */
            mins,
            fph,
            eff10 / 10,
            cl->totalDamageGiven,
            cl->totalDamageTaken,
            cl->ps.persistant[PERS_SCORE] );
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", line );
        if ( len > (int)sizeof(buf) - 128 ) {
            trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
            buf[0] = '\0';
            len = 0;
        }
    }
    if ( len > 0 ) {
        trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
    }
}

/*
=================
Cmd_MapList_f
=================
*/
static void Cmd_MapList_f( gentity_t *ent ) {
    char listbuf[8192];
    int count, i, pos;
    int index = 0;
    char row[256];
    char buf[MAX_STRING_CHARS];
    int len = 0;
    char *name;
    int maxLen = 0;
    int colWidth, perRow;
    int page = 1;
    int rowsPerPage = 10;
    int perPage;
    int totalPages;
    int start, end;
    int argc, ai, isNum, j;
    char argTok[MAX_TOKEN_CHARS];

    buf[0] = '\0';
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^2Available Maps:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7---------------\n" );

    G_EnsureMapListCache();
    count = s_cachedMapCount;
    /* optional page number: maplist [page] */
    argc = trap_Argc();
    for ( ai = 1; ai < argc; ++ai ) {
        trap_Argv( ai, argTok, sizeof(argTok) );
        isNum = 1;
        for ( j = 0; argTok[j]; ++j ) {
            if ( argTok[j] < '0' || argTok[j] > '9' ) { isNum = 0; break; }
        }
        if ( isNum && argTok[0] ) {
            page = atoi( argTok );
            if ( page < 1 ) page = 1;
            break;
        }
    }
    /* First pass: find maximum name length, capped */
    for ( i = 0; i < count; ++i ) {
        int nlen = (int)strlen( s_cachedMaps[i] );
        if ( nlen > maxLen ) maxLen = nlen;
    }
    if ( maxLen < 1 ) maxLen = 1;
    if ( maxLen > 16 ) maxLen = 16; /* cap to keep row short */
    colWidth = maxLen + 2;
    perRow = 3; /* fixed 3 columns */
    perPage = perRow * rowsPerPage;
    totalPages = (count + perPage - 1) / perPage;
    if ( totalPages < 1 ) totalPages = 1;
    if ( page > totalPages ) page = totalPages;
    start = (page - 1) * perPage;
    end = start + perPage;
    if ( end > count ) end = count;

    /* Second pass: output rows */
    pos = 0;
    index = 0;
    row[0] = '\0';
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7(Page %d/%d, Total %d)\n", page, totalPages, count );
    for ( i = start; i < end; ++i ) {
        const char *mapname;
        char cell[96];
        mapname = s_cachedMaps[i];
        index = i + 1;
        {
            int isCurrent = (Q_stricmp(mapname, g_mapname.string)==0);
            const char *star = isCurrent ? "^1*" : " ";
            const char *nameColor = isCurrent ? "^3" : "^2";
            Com_sprintf( cell, sizeof(cell), "^7%3d.^7 %s%s%-*s^7", index, star, nameColor, colWidth, mapname );
        }
        Q_strcat( row, sizeof(row), cell );
        if ( (index % perRow) != 0 && (i+1) < end ) {
            Q_strcat( row, sizeof(row), " " );
        }
        if ( (index % perRow) == 0 || (i+1) >= end ) {
            /* flush one row exactly */
            len += Com_sprintf( buf + len, sizeof(buf) - len, "%s\n", row );
            row[0] = '\0';
            if ( len > (int)sizeof(buf) - 256 ) {
                SendServerCommandInChunks( ent, buf );
                buf[0] = '\0';
                len = 0;
            }
        }
    }
    if ( len > 0 ) {
        SendServerCommandInChunks( ent, buf );
    }
}

/*
=================
Cmd_Rotation_f
=================
*/
static void Cmd_Rotation_f( gentity_t *ent ) {
    char bufFile[ 8192 ];
    fileHandle_t fh;
    int lenFile;
    char *s;
    char *tk;
    const int perRow = 3;
    int count = 0;
    const char *maps[256];
    char storage[ 256 ][ 64 ];
    int idx;
    char line[256];
    char out[MAX_STRING_CHARS];
    int outLen = 0;

    out[0] = '\0';
    outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "^2Current Map Rotation:^7\n" );
    outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "^7---------------------\n" );

    if ( !g_rotation.string[0] ) {
        outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "^3Rotation mode:^7 NONE\n" );
        trap_SendServerCommand( ent - g_entities, va("print \"%s\"", out) );
        return;
    }
    lenFile = trap_FS_FOpenFile( g_rotation.string, &fh, FS_READ );
    if ( fh == FS_INVALID_HANDLE ) {
        outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "^1Rotation file not found: ^7%s\n", g_rotation.string );
        trap_SendServerCommand( ent - g_entities, va("print \"%s\"", out) );
        return;
    }
    if ( lenFile >= (int)sizeof(bufFile) ) lenFile = sizeof(bufFile) - 1;
    trap_FS_Read( bufFile, lenFile, fh );
    bufFile[lenFile] = '\0';
    trap_FS_FCloseFile( fh );

    Com_InitSeparators();
    COM_BeginParseSession( g_rotation.string );
    s = bufFile;
    count = 0;
    while ( 1 ) {
        tk = COM_ParseSep( &s, qtrue );
        if ( tk[0] == '\0' ) break;
        if ( G_MapExist( tk ) ) {
            if ( count < 256 ) {
                Q_strncpyz( storage[count], tk, sizeof(storage[count]) );
                maps[count] = storage[count];
                count++;
            }
        }
    }

    for ( idx = 0; idx < count; ++idx ) {
        {
            int isCurrent = (Q_stricmp(maps[idx], g_mapname.string)==0);
            const char *star = isCurrent ? "^1*" : " ";
            const char *nameColor = isCurrent ? "^3" : "^2";
            Com_sprintf( line, sizeof(line), "^7%3d.^7 %s%s%-16s^7", idx+1, star, nameColor, maps[idx] );
        }
        outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "%s", line );
        if ( ((idx+1) % perRow) == 0 ) {
            outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "\n" );
            if ( outLen > (int)sizeof(out) - 256 ) {
                SendServerCommandInChunks( ent, out );
                out[0] = '\0';
                outLen = 0;
            }
        } else if ( idx+1 < count ) {
            outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "  " );
        }
    }
    if ( (count % perRow) != 0 ) {
        outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "\n" );
    }
    outLen += Com_sprintf( out + outLen, sizeof(out) - outLen, "^3Rotation mode:^7 %s\n", "RANDOM" );
    SendServerCommandInChunks( ent, out );
}

/* keep colored helper for listing when no args via callvote */
/* moved to cmds/votesystem.c */
/* legacy callvote helper removed; moved to cmds/votesystem.c */
/*
=================
Cmd_Awards_f
=================
*/
void Cmd_Awards_f( gentity_t *ent ) {
    int i;
    int mostKillsClient = -1, mostKills = -1;
    int bestEffClient = -1, bestEffPct10 = -1; /* efficiency in tenths percent */
    /* Placeholders if we don't track them */
    int bestStreakClient = -1, bestStreak = 0;
    int bestSkillClient = -1, bestSkill = 0;
    int mostChatFragClient = -1, mostChatFrags = -1;
    int mostSuicidesClient = -1, mostSuicides = -1;
    int mostImpressiveClient = -1, mostImpressive = -1;
    int mostExcellentClient = -1, mostExcellent = -1;
    int mostDefendClient = -1, mostDefend = -1;
    int mostAssistClient = -1, mostAssist = -1;
    int mostCapturesClient = -1, mostCaptures = -1;
    int mostGauntletClient = -1, mostGauntlet = -1;

    char buf[MAX_STRING_CHARS];
    char line[256];
    int len;

    if ( !ent || !ent->client ) {
        return;
    }

    for ( i = 0; i < level.maxclients; ++i ) {
        gclient_t *cl = &level.clients[i];
        int k, d;
        int effPct10;
        if ( cl->pers.connected != CON_CONNECTED ) {
            continue;
        }
        k = cl->kills; d = cl->deaths;
        if ( k > mostKills ) {
            mostKills = k;
            mostKillsClient = i;
        }
        if ( (k + d) > 0 ) {
            effPct10 = (k * 1000) / (k + d);
            if ( effPct10 > bestEffPct10 ) {
                bestEffPct10 = effPct10;
                bestEffClient = i;
            }
        }
        /* track best streak */
        if ( cl->bestKillStreak > bestStreak ) {
            bestStreak = cl->bestKillStreak;
            bestStreakClient = i;
        }

        /* track chat frags */
        if ( cl->chatFragCount > mostChatFrags ) {
            mostChatFrags = cl->chatFragCount;
            mostChatFragClient = i;
        }

        /* track suicides */
        if ( cl->suicides > mostSuicides ) {
            mostSuicides = cl->suicides;
            mostSuicidesClient = i;
        }

        /* track most impressive: PERS_IMPRESSIVE_COUNT */
        if ( cl->ps.persistant[PERS_IMPRESSIVE_COUNT] > mostImpressive ) {
            mostImpressive = cl->ps.persistant[PERS_IMPRESSIVE_COUNT];
            mostImpressiveClient = i;
        }

        /* track most gauntlet kills */
        if ( cl->perWeaponKills[WP_GAUNTLET] > mostGauntlet ) {
            mostGauntlet = cl->perWeaponKills[WP_GAUNTLET];
            mostGauntletClient = i;
        }

        /* track other built-in awards */
        if ( cl->ps.persistant[PERS_EXCELLENT_COUNT] > mostExcellent ) {
            mostExcellent = cl->ps.persistant[PERS_EXCELLENT_COUNT];
            mostExcellentClient = i;
        }
        if ( cl->ps.persistant[PERS_DEFEND_COUNT] > mostDefend ) {
            mostDefend = cl->ps.persistant[PERS_DEFEND_COUNT];
            mostDefendClient = i;
        }
        if ( cl->ps.persistant[PERS_ASSIST_COUNT] > mostAssist ) {
            mostAssist = cl->ps.persistant[PERS_ASSIST_COUNT];
            mostAssistClient = i;
        }
        if ( cl->ps.persistant[PERS_CAPTURES] > mostCaptures ) {
            mostCaptures = cl->ps.persistant[PERS_CAPTURES];
            mostCapturesClient = i;
        }
    }

    buf[0] = '\0';
    len = 0;

#define FLUSH_BUF() \
    do { \
        if ( len > 0 ) { \
            trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) ); \
            buf[0] = '\0'; \
            len = 0; \
        } \
    } while (0)

#define APPEND_LINE(str) \
    do { \
        const char *s__ = (str); \
        int add__ = (int)strlen( s__ ); \
        if ( len + add__ + 32 >= (int)sizeof( buf ) ) { \
            FLUSH_BUF(); \
        } \
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%s", s__ ); \
    } while (0)

    APPEND_LINE( "\n^2Awards List:\n" );
    APPEND_LINE( "^7------------------------------------------------------\n" );
    if ( mostKillsClient >= 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Kills               ^1%2d    ^7%s\n",
            mostKills, level.clients[mostKillsClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( bestEffClient >= 0 ) {
        Com_sprintf( line, sizeof(line), "^3Best Efficiency        ^1%2d.%01d    ^7%s\n",
            bestEffPct10 / 10, bestEffPct10 % 10,
            level.clients[bestEffClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( bestStreakClient >= 0 ) {
        Com_sprintf( line, sizeof(line), "^3Best Kill Streak          ^1%2d    ^7%s\n",
            bestStreak, level.clients[bestStreakClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( bestSkillClient >= 0 ) {
        Com_sprintf( line, sizeof(line), "^3Best Skill              ^1%4d    ^7%s\n",
            bestSkill, level.clients[bestSkillClient].pers.netname );
        APPEND_LINE( line );
    }

    if ( mostChatFragClient >= 0 && mostChatFrags > 0 ) {
        Com_sprintf( line, sizeof(line), "^1Most Chat Frags         ^1%2d    ^7%s\n",
            mostChatFrags, level.clients[mostChatFragClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostSuicidesClient >= 0 && mostSuicides > 0 ) {
        Com_sprintf( line, sizeof(line), "^1Most Suicides           ^1%2d    ^7%s\n",
            mostSuicides, level.clients[mostSuicidesClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostImpressiveClient >= 0 && mostImpressive > 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Impressive         ^1%2d    ^7%s\n",
            mostImpressive, level.clients[mostImpressiveClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostExcellentClient >= 0 && mostExcellent > 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Excellent          ^1%2d    ^7%s\n",
            mostExcellent, level.clients[mostExcellentClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostDefendClient >= 0 && mostDefend > 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Defends            ^1%2d    ^7%s\n",
            mostDefend, level.clients[mostDefendClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostAssistClient >= 0 && mostAssist > 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Assists            ^1%2d    ^7%s\n",
            mostAssist, level.clients[mostAssistClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostCapturesClient >= 0 && mostCaptures > 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Captures           ^1%2d    ^7%s\n",
            mostCaptures, level.clients[mostCapturesClient].pers.netname );
        APPEND_LINE( line );
    }
    if ( mostGauntletClient >= 0 && mostGauntlet > 0 ) {
        Com_sprintf( line, sizeof(line), "^3Most Gauntlet Kills     ^1%2d    ^7%s\n",
            mostGauntlet, level.clients[mostGauntletClient].pers.netname );
        APPEND_LINE( line );
    }

    FLUSH_BUF();

#undef APPEND_LINE
#undef FLUSH_BUF
}

static void Cmd_Test_f( gentity_t *ent ) {
	trap_SendServerCommand( ent-g_entities, "print \"Pohuui\n\"");
}

static void Cmd_HookOn_f( gentity_t *ent ) {
    if ( !ent || !ent->client ) return;
    if ( !g_hook.integer ) return;
    if ( ent->client->hook ) return;
    {
        vec3_t forward, right, up;
        vec3_t muzzle, origin;
        AngleVectors( ent->client->ps.viewangles, forward, right, up );
        VectorCopy( ent->client->ps.origin, origin );
        origin[2] += ent->client->ps.viewheight;
        VectorMA( origin, 14.0f, forward, muzzle );
        fire_grapple( ent, muzzle, forward );
        ent->client->fireHeld = qtrue;
    }
}

static void Cmd_HookOff_f( gentity_t *ent ) {
    if ( !ent || !ent->client ) return;
    if ( ent->client->hook ) {
        Weapon_HookFree( ent->client->hook );
    }
    ent->client->fireHeld = qfalse;
}





/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum ) {
	gentity_t *ent;
	char	cmd[MAX_TOKEN_CHARS];
	char 	args[MAX_STRING_CHARS];
	int 	i;

	ent = g_entities + clientNum;
	if ( !ent->client )
		return;

	args[0] = '\0';

    for ( i = 0; i < trap_Argc(); i++ ) {
        trap_Argv( i, cmd, sizeof( cmd ) );
        if ( i > 0 ) {
            Q_strcat( args, sizeof( args ), " " );
        }
        Q_strcat( args, sizeof( args ), cmd );
    }
		
	trap_Argv( 0, cmd, sizeof( cmd ) );

	if (!(Q_stricmp (cmd, "say") == 0 
		|| Q_stricmp (cmd, "say_team") == 0 
		|| Q_stricmp (cmd, "tell") == 0
		|| Q_stricmp (cmd, "score") == 0
	)) {
		G_LogPrintf( "@  %s%c%c[%d] cmd: %s\n", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, clientNum, args );
	}

	if ( ent->client->pers.connected != CON_CONNECTED ) {
		if ( ent->client->pers.connected == CON_CONNECTING && g_gametype.integer >= GT_TEAM ) {
			if ( Q_stricmp( cmd, "team" ) == 0 && !level.restarted ) {
				Cmd_Team_f( ent ); // early team override
			}
		}
		return;	// not fully in game yet
	}

    if (Q_stricmp (cmd, "+hook") == 0) { Cmd_HookOn_f(ent); return; }
    if (Q_stricmp (cmd, "-hook") == 0) { Cmd_HookOff_f(ent); return; }

	if (Q_stricmp (cmd, "say") == 0) {
		Cmd_Say_f (ent, SAY_ALL, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0) {
		Cmd_Say_f (ent, SAY_TEAM, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "tell") == 0) {
		Cmd_Tell_f ( ent );
		return;
	}
#ifdef MISSIONPACK
	if (Q_stricmp (cmd, "vsay") == 0) {
		Cmd_Voice_f (ent, SAY_ALL, qfalse, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "vsay_team") == 0) {
		Cmd_Voice_f (ent, SAY_TEAM, qfalse, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "vtell") == 0) {
		Cmd_VoiceTell_f ( ent, qfalse );
		return;
	}
	if (Q_stricmp (cmd, "vosay") == 0) {
		Cmd_Voice_f (ent, SAY_ALL, qfalse, qtrue);
		return;
	}
	if (Q_stricmp (cmd, "vosay_team") == 0) {
		Cmd_Voice_f (ent, SAY_TEAM, qfalse, qtrue);
		return;
	}
	if (Q_stricmp (cmd, "votell") == 0) {
		Cmd_VoiceTell_f ( ent, qtrue );
		return;
	}
	if (Q_stricmp (cmd, "vtaunt") == 0) {
		Cmd_VoiceTaunt_f ( ent );
		return;
	}
#endif
	if (Q_stricmp (cmd, "score") == 0) {
		Cmd_Score_f (ent);
		return;
	}



    /* Check disabled commands list before dispatch */
    if ( DC_IsDisabled( cmd ) ) {
        trap_SendServerCommand( clientNum, va( "print \"^1! ^3Command '^1%s^3' is disabled on this server.\n\"", cmd ) );
        return;
    }

	// ignore all other commands when at intermission
	if (level.intermissiontime) {
		if ( Q_stricmp( cmd, "stats" ) == 0 ) { Cmd_Stats_f( ent ); return; }
		if ( Q_stricmp( cmd, "statsall" ) == 0 ) { Cmd_StatsAll_f( ent ); return; }
		if ( Q_stricmp( cmd, "awards" ) == 0 ) { Cmd_Awards_f( ent ); return; }
		if ( Q_stricmp( cmd, "topshots" ) == 0 ) { Cmd_Topshots_f( ent ); return; }
		if ( Q_stricmp( cmd, "team" ) == 0 ) { Cmd_Team_f( ent ); return; }
		if ( Q_stricmp( cmd, "callvote" ) == 0 ) { 
			Cmd_CallVote_f( ent ); 
			// if vote was actually created, mark it as called during intermission
			if ( level.voteTime && level.intermissiontime ) {
				level.voteCalledDuringIntermission = qtrue;
			}
			return; 
		}
		if ( Q_stricmp( cmd, "vote" ) == 0 ) { Cmd_Vote_f( ent ); return; }
		if ( Q_stricmp( cmd, "cv" ) == 0 ) { 
			// Allow cv command during intermission for map changes
			Cmd_CV_f( ent );
			// if vote was actually created, mark it as called during intermission
			if ( level.voteTime && level.intermissiontime ) {
				level.voteCalledDuringIntermission = qtrue;
			}
			return; 
		}
		Cmd_Say_f (ent, qfalse, qtrue);
		return;
	}

    if ( DispatchGameCommand( cmd, ent ) ) {
        return;
    }
    Com_Printf("@ ^ unknown command\n");
    trap_SendServerCommand( clientNum, va( "print \"^1! ^3Command '^7%s^3' not recognized.^3\n^1! ^3Type '^7\\kill'^3 for no reason lol\n\"", cmd ) );
}

/*
==================
Cmd_Ready_f / Cmd_Unready_f

Warmup ready system: during warmup, players can type \ready or \unready.
When all non-spectator human players are ready, we announce and start a 5s countdown.
==================
*/
static void G_CheckAllReadyAndStart(void) {
    int i;
    int totalHumans;
    int readyHumans;

    if ( level.warmupTime == 0 ) {
        return;
    }

    totalHumans = 0;
    readyHumans = 0;
    for ( i = 0; i < level.maxclients; i++ ) {
        gentity_t *e = &g_entities[i];
        gclient_t *cl = &level.clients[i];
        if ( cl->pers.connected != CON_CONNECTED ) {
            continue;
        }
        if ( e->r.svFlags & SVF_BOT ) {
            continue;
        }
        if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
            continue;
        }
        totalHumans++;
        if ( e->readyBegin ) {
            readyHumans++;
        }
    }

    {
        int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
        if ( requireTwo && totalHumans < 2 ) {
            return;
        }
    }

    if ( totalHumans > 0 && readyHumans == totalHumans ) {
        /* Announce and set warmup to 5 seconds from now */
        G_BroadcastServerCommand( -1, "cp \"^3All players ready!\"" );
        level.warmupTime = level.time + 5000;
        level.readyCountdownStarted = qtrue;
        trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
    }

    /* If countdown is already running, revalidate player counts each call */
    if ( level.warmupTime > 0 && level.time < level.warmupTime ) {
        int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
        int totalPlayers = 0;
        int i2;
        for ( i2 = 0; i2 < level.maxclients; ++i2 ) {
            gentity_t *e2 = &g_entities[i2];
            gclient_t *cl2 = &level.clients[i2];
            if ( cl2->pers.connected != CON_CONNECTED ) continue;
            if ( cl2->sess.sessionTeam == TEAM_SPECTATOR ) continue;
            totalPlayers++;
        }
        if ( (requireTwo && totalHumans < 2) || totalPlayers < 2 ) {
            level.readyCountdownStarted = qfalse;
            level.warmupTime = -1;
            trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
            G_BroadcastServerCommand( -1, "cp \"^3Countdown cancelled: need 2 human players\"" );
        }
    }
}

static void Cmd_Ready_f( gentity_t *ent ) {
    if ( !ent || !ent->client ) {
        return;
    }
    if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
        return;
    }
    if ( level.warmupTime == 0 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Not in warmup.\n\"" );
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Spectators don't need to ready.\n\"" );
        return;
    }
    if ( ent->r.svFlags & SVF_BOT ) {
        return;
    }
    if ( ent->readyBegin ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3You are already ready.\n\"" );
        return;
    }
    ent->readyBegin = qtrue;
    G_BroadcastServerCommand( -1, va( "print \"%s ^3is ^2ready\n\"", ent->client->pers.netname ) );
	G_BroadcastServerCommand( -1, va( "cp \"%s ^3is ^2ready\n\"", ent->client->pers.netname ) );
    /* Update scoreboard ready mask bit */
    ftmod_checkDelay();
    G_CheckAllReadyAndStart();
}

static void Cmd_Unready_f( gentity_t *ent ) {
    if ( !ent || !ent->client ) {
        return;
    }
    if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
        return;
    }
    if ( level.warmupTime == 0 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Not in warmup.\n\"" );
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        return;
    }
    if ( ent->r.svFlags & SVF_BOT ) {
        return;
    }
    if ( !ent->readyBegin ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3You are not ready.\n\"" );
        return;
    }
    if ( level.warmupTime > 0 && level.time < level.warmupTime ) {
        /* If countdown active: permit cancellation when it would violate human/player requirement */
        int totalHumans = 0, totalPlayers = 0, i;
        for ( i = 0; i < level.maxclients; ++i ) {
            gentity_t *e = &g_entities[i];
            gclient_t *cl = &level.clients[i];
            if ( cl->pers.connected != CON_CONNECTED ) continue;
            if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) continue;
            totalPlayers++;
            if ( !(e->r.svFlags & SVF_BOT) ) totalHumans++;
        }
        if ( (trap_Cvar_VariableIntegerValue("g_requireTwoHumans") && totalHumans < 2) || totalPlayers < 2 ) {
            /* allow unready to cancel */
        } else {
            trap_SendServerCommand( ent - g_entities, "print \"^3Countdown in progress; cannot unready.\n\"" );
            return;
        }
    }

    ent->readyBegin = qfalse;
    G_BroadcastServerCommand( -1, va( "print \"%s ^3is ^1not ready\n\"", ent->client->pers.netname ) );
	G_BroadcastServerCommand( -1, va( "cp \"%s ^3is ^1not ready\n\"", ent->client->pers.netname ) );
    /* Update scoreboard ready mask bit */
    ftmod_checkDelay();
}

/*
==================
Cmd_SpawnFrozenBody_f

Spawn a frozen body at the position the player is looking at
==================
*/
static void Cmd_SpawnFrozenBody_f( gentity_t *ent ) {
    vec3_t start, end, forward, right, up;
    vec3_t origin;
    trace_t tr;
    int argc;
    char arg[MAX_TOKEN_CHARS];

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        // trap_SendServerCommand( ent - g_entities, "print \"^1You must be authenticated to use this command.\n\"" );
        return;
    }

    // Check if freeze tag is enabled
    if ( !g_freeze.integer ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Freeze tag is not enabled.\n\"" );
        return;
    }

    // Require player to be on a team (spectators cannot spawn bodies)
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Join a team to use this command.\n\"" );
        return;
    }

    // If argument "here" is provided, spawn at player's current position
    argc = trap_Argc();
    if ( argc >= 2 ) {
        trap_Argv( 1, arg, sizeof( arg ) );
        if ( Q_stricmp( arg, "here" ) == 0 ) {
            VectorCopy( ent->client->ps.origin, origin );
            origin[2] += 45; /* lift a bit to avoid ground clipping */
            ftmod_spawnFrozenBodyAtPosition( ent, origin );
            trap_SendServerCommand( ent - g_entities, "print \"^2Frozen body spawned successfully.\\n\"" );
            return;
        }
    }

    // Get player's view position and direction
    VectorCopy( ent->client->ps.origin, start );
    start[2] += ent->client->ps.viewheight;
    
    AngleVectors( ent->client->ps.viewangles, forward, right, up );
    VectorMA( start, 8192, forward, end );

    // Trace to find where player is looking
    trap_Trace( &tr, start, NULL, NULL, end, ent->s.number, MASK_SOLID );
    
    if ( tr.fraction == 1.0 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1No surface found to spawn frozen body.\n\"" );
        return;
    }

    // Calculate spawn position (slightly above the surface)
    VectorCopy( tr.endpos, origin );
    origin[2] += 45; // Lift up a bit to avoid clipping into ground

    // Create the frozen body using the freeze module function
    ftmod_spawnFrozenBodyAtPosition( ent, origin );

    trap_SendServerCommand( ent - g_entities, "print \"^2Frozen body spawned successfully.\n\"" );
}

/*
==================
Cmd_ReadyAll_f

Admin: force all eligible players to ready
==================
*/
static void Cmd_ReadyAll_f( gentity_t *ent ) {
    int i;
    if ( !ent || !ent->client || !ent->authed ) {
        return;
    }
    for ( i = 0; i < level.maxclients; ++i ) {
        gentity_t *e = &g_entities[i];
        gclient_t *cl = &level.clients[i];
        if ( cl->pers.connected != CON_CONNECTED ) continue;
        if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) continue;
        if ( e->r.svFlags & SVF_BOT ) continue;
        e->readyBegin = qtrue;
    }
    /* Update scoreboard ready mask and possibly start countdown */
    ftmod_checkDelay();
    G_CheckAllReadyAndStart();
}

/*
==================
Cmd_UnreadyAll_f

Admin: force all players to unready and cancel countdown if active
==================
*/
static void Cmd_UnreadyAll_f( gentity_t *ent ) {
    int i;
    if ( !ent || !ent->client || !ent->authed ) {
        return;
    }
    for ( i = 0; i < level.maxclients; ++i ) {
        gentity_t *e = &g_entities[i];
        gclient_t *cl = &level.clients[i];
        if ( cl->pers.connected != CON_CONNECTED ) continue;
        if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) continue;
        if ( e->r.svFlags & SVF_BOT ) continue;
        e->readyBegin = qfalse;
    }
    /* Cancel countdown if it is running */
    if ( level.warmupTime > 0 && level.time < level.warmupTime ) {
        level.readyCountdownStarted = qfalse;
        level.warmupTime = -1;
        trap_SetConfigstring( CS_WARMUP, va( "%i", level.warmupTime ) );
        G_BroadcastServerCommand( -1, "cp \"^3Countdown cancelled by admin\"" );
    }
    /* Update scoreboard ready mask */
    ftmod_checkDelay();
}

static void Cmd_RemoveFrozenBody_f( gentity_t *ent ) {
    int i;
    int removed = 0;
    gentity_t *body;

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    // Check if freeze tag is enabled
    if ( !g_freeze.integer ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Freeze tag is not enabled.\n\"" );
        return;
    }

    // Remove all frozen bodies spawned by this player
    for ( i = 0; i < level.num_entities; i++ ) {
        body = &g_entities[i];
        if ( !body->inuse ) {
            continue;
        }
        if ( !body->freezeState ) {
            continue;
        }
        if ( Q_stricmp( body->classname, "freezebody" ) != 0 ) {
            continue;
        }
        // Check if this body belongs to the player
        if ( body->target_ent == ent || body->s.otherEntityNum == ent->s.number ) {
            // Clear the player's reference to this body
            if ( ent->target_ent == body ) {
                ent->target_ent = NULL;
            }
            // Remove the body
            G_FreeEntity( body );
            removed++;
        }
    }

    if ( removed > 0 ) {
        trap_SendServerCommand( ent - g_entities, va( "print \"^2Removed %d frozen body/bodies.\n\"", removed ) );
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3No frozen bodies found to remove.\n\"" );
    }
}

/*
==================
Cmd_AdminCP_f

Admin command to send centerprint message to all players
Usage: cp <message>
==================
*/
static void Cmd_AdminCP_f( gentity_t *ent ) {
    char *message;

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: cp <message>\n\"" );
        return;
    }

    message = ConcatArgs( 1 );
    if ( !message || !message[0] ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: cp <message>\n\"" );
        return;
    }

    // Send centerprint to all connected players
    G_BroadcastServerCommand( -1, va( "cp \"%s\"", message ) );
    
    // Confirm to admin
    trap_SendServerCommand( ent - g_entities, va( "print \"^2CenterPrint sent: ^7%s\n\"", message ) );
}

/*
==================
Cmd_AdminSay_f

Admin command to send server announcement to all players
Usage: asay <message>
==================
*/
static void Cmd_AdminSay_f( gentity_t *ent ) {
    char *message;

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: asay <message>\n\"" );
        return;
    }

    message = ConcatArgs( 1 );
    if ( !message || !message[0] ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: asay <message>\n\"" );
        return;
    }

    // Send server announcement to all connected players
    G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s\n\"", message ) );
    
    // Confirm to admin
    trap_SendServerCommand( ent - g_entities, va( "print \"^2Server announcement sent: ^7%s\n\"", message ) );
}

/*
==================
Helper functions for ban/mute system
==================
*/

static void SaveBans( void );
static void SaveMutes( void );

void AddBan( const char *ip, const char *name, const char *reason, int admin, int duration ) {
    if ( s_banCount >= MAX_BANS ) {
        return;
    }
    
    Q_strncpyz( s_bans[s_banCount].ip, ip, sizeof(s_bans[s_banCount].ip) );
    Q_strncpyz( s_bans[s_banCount].name, name, sizeof(s_bans[s_banCount].name) );
    Q_strncpyz( s_bans[s_banCount].reason, reason, sizeof(s_bans[s_banCount].reason) );
    s_bans[s_banCount].time = level.time;
    s_bans[s_banCount].admin = admin;
    s_bans[s_banCount].duration = duration;
    s_banCount++;
    
    SaveBans();
}

void AddMute( const char *ip, const char *name, const char *reason, int admin, int duration ) {
    if ( s_muteCount >= MAX_MUTES ) {
        return;
    }
    
    Q_strncpyz( s_mutes[s_muteCount].ip, ip, sizeof(s_mutes[s_muteCount].ip) );
    Q_strncpyz( s_mutes[s_muteCount].name, name, sizeof(s_mutes[s_muteCount].name) );
    Q_strncpyz( s_mutes[s_muteCount].reason, reason, sizeof(s_mutes[s_muteCount].reason) );
    s_mutes[s_muteCount].time = level.time;
    s_mutes[s_muteCount].admin = admin;
    s_mutes[s_muteCount].duration = duration;
    s_muteCount++;
    
    SaveMutes();
}

qboolean IsBanned( const char *ip ) {
    int i;
    int currentTime;
    
    currentTime = level.time;
    for ( i = 0; i < s_banCount; i++ ) {
        if ( !Q_stricmp( s_bans[i].ip, ip ) ) {
            /* Check if ban has expired */
            if ( s_bans[i].duration > 0 ) {
                if ( currentTime - s_bans[i].time > s_bans[i].duration * 1000 ) {
                    /* Ban expired, remove it */
                    RemoveBan( i );
                    i--; /* Adjust index since we removed an element */
                    continue;
                }
            }
            return qtrue;
        }
    }
    return qfalse;
}

qboolean IsMuted( const char *ip ) {
    int i;
    int currentTime;
    
    currentTime = level.time;
    for ( i = 0; i < s_muteCount; i++ ) {
        if ( !Q_stricmp( s_mutes[i].ip, ip ) ) {
            /* Check if mute has expired */
            if ( s_mutes[i].duration > 0 ) {
                if ( currentTime - s_mutes[i].time > s_mutes[i].duration * 1000 ) {
                    /* Mute expired, remove it */
                    RemoveMute( i );
                    i--; /* Adjust index since we removed an element */
                    continue;
                }
            }
            return qtrue;
        }
    }
    return qfalse;
}

void RemoveBan( int index ) {
    int i;
    if ( index < 0 || index >= s_banCount ) {
        return;
    }
    
    for ( i = index; i < s_banCount - 1; i++ ) {
        s_bans[i] = s_bans[i + 1];
    }
    s_banCount--;
    
    SaveBans();
}

void RemoveMute( int index ) {
    int i;
    if ( index < 0 || index >= s_muteCount ) {
        return;
    }
    
    for ( i = index; i < s_muteCount - 1; i++ ) {
        s_mutes[i] = s_mutes[i + 1];
    }
    s_muteCount--;
    
    SaveMutes();
}

static void SaveBans( void ) {
    fileHandle_t f;
    int i;
    
    if ( trap_FS_FOpenFile( BANS_FILE, &f, FS_WRITE ) < 0 ) {
        G_Printf( "Warning: Could not save bans to %s\n", BANS_FILE );
        return;
    }
    
    trap_FS_Write( &s_banCount, sizeof( s_banCount ), f );
    for ( i = 0; i < s_banCount; i++ ) {
        trap_FS_Write( &s_bans[i], sizeof( s_bans[i] ), f );
    }
    
    trap_FS_FCloseFile( f );
}

void LoadBans( void ) {
    fileHandle_t f;
    int len;
    int i;
    
    len = trap_FS_FOpenFile( BANS_FILE, &f, FS_READ );
    if ( len < 0 ) {
        return; /* File doesn't exist yet */
    }
    
    if ( len < sizeof( s_banCount ) ) {
        trap_FS_FCloseFile( f );
        return;
    }
    
    trap_FS_Read( &s_banCount, sizeof( s_banCount ), f );
    if ( s_banCount < 0 || s_banCount > MAX_BANS ) {
        s_banCount = 0;
        trap_FS_FCloseFile( f );
        return;
    }
    
    for ( i = 0; i < s_banCount; i++ ) {
        trap_FS_Read( &s_bans[i], sizeof( s_bans[i] ), f );
    }
    
    trap_FS_FCloseFile( f );
}

static void SaveMutes( void ) {
    fileHandle_t f;
    int i;
    
    if ( trap_FS_FOpenFile( MUTES_FILE, &f, FS_WRITE ) < 0 ) {
        G_Printf( "Warning: Could not save mutes to %s\n", MUTES_FILE );
        return;
    }
    
    trap_FS_Write( &s_muteCount, sizeof( s_muteCount ), f );
    for ( i = 0; i < s_muteCount; i++ ) {
        trap_FS_Write( &s_mutes[i], sizeof( s_mutes[i] ), f );
    }
    
    trap_FS_FCloseFile( f );
}

void LoadMutes( void ) {
    fileHandle_t f;
    int len;
    int i;
    
    len = trap_FS_FOpenFile( MUTES_FILE, &f, FS_READ );
    if ( len < 0 ) {
        return; /* File doesn't exist yet */
    }
    
    if ( len < sizeof( s_muteCount ) ) {
        trap_FS_FCloseFile( f );
        return;
    }
    
    trap_FS_Read( &s_muteCount, sizeof( s_muteCount ), f );
    if ( s_muteCount < 0 || s_muteCount > MAX_MUTES ) {
        s_muteCount = 0;
        trap_FS_FCloseFile( f );
        return;
    }
    
    for ( i = 0; i < s_muteCount; i++ ) {
        trap_FS_Read( &s_mutes[i], sizeof( s_mutes[i] ), f );
    }
    
    trap_FS_FCloseFile( f );
}

void CleanupExpiredBansAndMutes( void ) {
    int i;
    int currentTime;
    qboolean changed = qfalse;
    
    currentTime = level.time;
    
    /* Clean up expired bans */
    for ( i = 0; i < s_banCount; i++ ) {
        if ( s_bans[i].duration > 0 ) {
            if ( currentTime - s_bans[i].time > s_bans[i].duration * 1000 ) {
                RemoveBan( i );
                i--; /* Adjust index since we removed an element */
                changed = qtrue;
            }
        }
    }
    
    /* Clean up expired mutes */
    for ( i = 0; i < s_muteCount; i++ ) {
        if ( s_mutes[i].duration > 0 ) {
            if ( currentTime - s_mutes[i].time > s_mutes[i].duration * 1000 ) {
                RemoveMute( i );
                i--; /* Adjust index since we removed an element */
                changed = qtrue;
            }
        }
    }
    
    if ( changed ) {
        G_Printf( "Cleaned up expired bans and mutes\n" );
    }
}

void Info_Print( const char *s ) {
    char key[256];
    char value[256];
    char *o;
    int l;

    if ( *s == '\\' )
        s++;
    while ( *s ) {
        o = key;
        while ( *s && *s != '\\' )
            *o++ = *s++;

        l = o - key;
        if ( l < 20 ) {
            memset( o, ' ', 20 - l );
            key[20] = 0;
        } else {
            *o = 0;
        }

        if ( !*s ) {
            return;
        }

        o = value;
        s++;
        while ( *s && *s != '\\' )
            *o++ = *s++;
        *o = 0;

        if ( *s )
            s++;
    }
}

/*
==================
Cmd_AdminBan_f

Admin command to ban a player
Usage: aban <clientNum> [reason]
==================
*/
static void Cmd_AdminBan_f( gentity_t *ent ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    char reason[256];
    gentity_t *target;
    char *ip;
    int duration = 0;  /* 0 = permanent */

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: aban <clientNum> [duration] [reason]\n\"" );
        trap_SendServerCommand( ent - g_entities, "print \"^3Duration: 0=permanent, or time in minutes (e.g. 30 for 30 minutes)\n\"" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid client number.\n\"" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Client not connected.\n\"" );
        return;
    }

    // Parse duration and reason
    if ( trap_Argc() > 2 ) {
        char durationArg[MAX_TOKEN_CHARS];
        trap_Argv( 2, durationArg, sizeof( durationArg ) );
        
        // Check if second argument is a number (duration)
        if ( durationArg[0] >= '0' && durationArg[0] <= '9' ) {
            duration = atoi( durationArg ) * 60; // Convert minutes to seconds
            if ( trap_Argc() > 3 ) {
                Q_strncpyz( reason, ConcatArgs( 3 ), sizeof( reason ) );
            } else {
                Q_strncpyz( reason, "No reason specified", sizeof( reason ) );
            }
        } else {
            // Second argument is reason, duration is permanent
            duration = 0;
            Q_strncpyz( reason, ConcatArgs( 2 ), sizeof( reason ) );
        }
    } else {
        Q_strncpyz( reason, "No reason specified", sizeof( reason ) );
    }

    // Get IP
    {
        char userinfo[MAX_INFO_STRING];
        trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );
        ip = Info_ValueForKey( userinfo, "ip" );
        if ( !ip || !ip[0] ) {
            ip = "unknown";
        }
    }

    // Add to ban list
    AddBan( ip, target->client->pers.netname, reason, ent - g_entities, duration );

    // Kick the player
    if ( duration > 0 ) {
        trap_DropClient( clientNum, va( "You have been banned for %d minutes: %s", duration / 60, reason ) );
    } else {
        trap_DropClient( clientNum, va( "You have been permanently banned: %s", reason ) );
    }

    // Notify admin
    if ( duration > 0 ) {
        trap_SendServerCommand( ent - g_entities, va( "print \"^2Banned ^7%s ^2for %d minutes: ^7%s\n\"", target->client->pers.netname, duration / 60, reason ) );
    } else {
        trap_SendServerCommand( ent - g_entities, va( "print \"^2Permanently banned ^7%s ^2for: ^7%s\n\"", target->client->pers.netname, reason ) );
    }

    // Notify all players
    if ( duration > 0 ) {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been banned for %d minutes by admin.\n\"", target->client->pers.netname, duration / 60 ) );
    } else {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been permanently banned by admin.\n\"", target->client->pers.netname ) );
    }
}

/*
==================
Cmd_AdminKick_f

Admin command to kick a player
Usage: akick <clientNum> [reason]
==================
*/
static void Cmd_AdminKick_f( gentity_t *ent ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    char reason[256];
    gentity_t *target;

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: akick <clientNum> [reason]\n\"" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid client number.\n\"" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Client not connected.\n\"" );
        return;
    }

    // Get reason
    if ( trap_Argc() > 2 ) {
        Q_strncpyz( reason, ConcatArgs( 2 ), sizeof( reason ) );
    } else {
        Q_strncpyz( reason, "No reason specified", sizeof( reason ) );
    }

    // Kick the player
    trap_DropClient( clientNum, va( "You have been kicked: %s", reason ) );

    // Notify admin
    trap_SendServerCommand( ent - g_entities, va( "print \"^2Kicked ^7%s ^2for: ^7%s\n\"", target->client->pers.netname, reason ) );

    // Notify all players
    G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been kicked by admin.\n\"", target->client->pers.netname ) );
}

/*
==================
Cmd_AdminMute_f

Admin command to mute a player
Usage: amute <clientNum> [reason]
==================
*/
static void Cmd_AdminMute_f( gentity_t *ent ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    char reason[256];
    gentity_t *target;
    char *ip;
    int duration = 0;  /* 0 = permanent */

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: amute <clientNum> [duration] [reason]\n\"" );
        trap_SendServerCommand( ent - g_entities, "print \"^3Duration: 0=permanent, or time in minutes (e.g. 30 for 30 minutes)\n\"" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid client number.\n\"" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Client not connected.\n\"" );
        return;
    }

    // Parse duration and reason
    if ( trap_Argc() > 2 ) {
        char durationArg[MAX_TOKEN_CHARS];
        trap_Argv( 2, durationArg, sizeof( durationArg ) );
        
        // Check if second argument is a number (duration)
        if ( durationArg[0] >= '0' && durationArg[0] <= '9' ) {
            duration = atoi( durationArg ) * 60; // Convert minutes to seconds
            if ( trap_Argc() > 3 ) {
                Q_strncpyz( reason, ConcatArgs( 3 ), sizeof( reason ) );
            } else {
                Q_strncpyz( reason, "No reason specified", sizeof( reason ) );
            }
        } else {
            // Second argument is reason, duration is permanent
            duration = 0;
            Q_strncpyz( reason, ConcatArgs( 2 ), sizeof( reason ) );
        }
    } else {
        Q_strncpyz( reason, "No reason specified", sizeof( reason ) );
    }

    // Get IP
    {
        char userinfo[MAX_INFO_STRING];
        trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );
        ip = Info_ValueForKey( userinfo, "ip" );
        if ( !ip || !ip[0] ) {
            ip = "unknown";
        }
    }

    // Add to mute list
    AddMute( ip, target->client->pers.netname, reason, ent - g_entities, duration );

    // Notify admin
    if ( duration > 0 ) {
        trap_SendServerCommand( ent - g_entities, va( "print \"^2Muted ^7%s ^2for %d minutes: ^7%s\n\"", target->client->pers.netname, duration / 60, reason ) );
    } else {
        trap_SendServerCommand( ent - g_entities, va( "print \"^2Permanently muted ^7%s ^2for: ^7%s\n\"", target->client->pers.netname, reason ) );
    }

    // Notify all players
    if ( duration > 0 ) {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been muted for %d minutes by admin.\n\"", target->client->pers.netname, duration / 60 ) );
    } else {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been permanently muted by admin.\n\"", target->client->pers.netname ) );
    }
}

/*
==================
Cmd_AdminBanList_f

Admin command to show ban list
Usage: abanlist
==================
*/
static void Cmd_AdminBanList_f( gentity_t *ent ) {
    int i;
    char timeStr[64];

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( s_banCount == 0 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3No banned players.\n\"" );
        return;
    }

    trap_SendServerCommand( ent - g_entities, "print \"^2=== Banned Players ===\n\"" );
    for ( i = 0; i < s_banCount; i++ ) {
        int minutes = ( level.time - s_bans[i].time ) / 60000;
        if ( s_bans[i].duration > 0 ) {
            int remaining = ( s_bans[i].duration - ( level.time - s_bans[i].time ) ) / 1000;
            if ( remaining > 0 ) {
                Q_strncpyz( timeStr, va( "%d min remaining", remaining / 60 ), sizeof( timeStr ) );
            } else {
                Q_strncpyz( timeStr, "EXPIRED", sizeof( timeStr ) );
            }
        } else {
            Q_strncpyz( timeStr, "PERMANENT", sizeof( timeStr ) );
        }
        trap_SendServerCommand( ent - g_entities, va( "print \"^3%d. ^7%s ^3(%s) ^1- ^7%s ^3[%s]\n\"", 
            i, s_bans[i].name, s_bans[i].ip, s_bans[i].reason, timeStr ) );
    }
}

/*
==================
Cmd_AdminMuteList_f

Admin command to show mute list
Usage: amutelist
==================
*/
static void Cmd_AdminMuteList_f( gentity_t *ent ) {
    int i;
    char timeStr[64];

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( s_muteCount == 0 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3No muted players.\n\"" );
        return;
    }

    trap_SendServerCommand( ent - g_entities, "print \"^2=== Muted Players ===\n\"" );
    for ( i = 0; i < s_muteCount; i++ ) {
        int minutes = ( level.time - s_mutes[i].time ) / 60000;
        if ( s_mutes[i].duration > 0 ) {
            int remaining = ( s_mutes[i].duration - ( level.time - s_mutes[i].time ) ) / 1000;
            if ( remaining > 0 ) {
                Q_strncpyz( timeStr, va( "%d min remaining", remaining / 60 ), sizeof( timeStr ) );
            } else {
                Q_strncpyz( timeStr, "EXPIRED", sizeof( timeStr ) );
            }
        } else {
            Q_strncpyz( timeStr, "PERMANENT", sizeof( timeStr ) );
        }
        trap_SendServerCommand( ent - g_entities, va( "print \"^3%d. ^7%s ^3(%s) ^1- ^7%s ^3[%s]\n\"", 
            i, s_mutes[i].name, s_mutes[i].ip, s_mutes[i].reason, timeStr ) );
    }
}

/*
==================
Cmd_AdminUnban_f

Admin command to unban a player
Usage: aunban <index>
==================
*/
static void Cmd_AdminUnban_f( gentity_t *ent ) {
    int index;
    char arg[MAX_TOKEN_CHARS];
    char name[64];

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: aunban <index>\n\"" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    index = atoi( arg );
    
    if ( index < 0 || index >= s_banCount ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid ban index.\n\"" );
        return;
    }

    Q_strncpyz( name, s_bans[index].name, sizeof( name ) );
    RemoveBan( index );

    // Notify admin
    trap_SendServerCommand( ent - g_entities, va( "print \"^2Unbanned ^7%s\n\"", name ) );
}

/*
==================
Cmd_AdminUnmute_f

Admin command to unmute a player
Usage: aunmute <index>
==================
*/
static void Cmd_AdminUnmute_f( gentity_t *ent ) {
    int index;
    char arg[MAX_TOKEN_CHARS];
    char name[64];

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: aunmute <index>\n\"" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    index = atoi( arg );
    
    if ( index < 0 || index >= s_muteCount ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid mute index.\n\"" );
        return;
    }

    Q_strncpyz( name, s_mutes[index].name, sizeof( name ) );
    RemoveMute( index );

    // Notify admin
    trap_SendServerCommand( ent - g_entities, va( "print \"^2Unmuted ^7%s\n\"", name ) );
}

/*
==================
Cmd_AdminDumpUser_f

Admin command to dump user info
Usage: adumpuser <clientNum>
==================
*/
static void Cmd_AdminDumpUser_f( gentity_t *ent ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    gentity_t *target;
    char userinfo[MAX_INFO_STRING];
    char *s;
    char key[256];
    char value[256];
    char *o;

    if ( !ent || !ent->client ) {
        return;
    }

    // Check if player is authenticated
    if ( !ent->authed ) {
        return;
    }

    if ( trap_Argc() < 2 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: adumpuser <clientNum>\n\"" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid client number.\n\"" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Client not connected.\n\"" );
        return;
    }

    trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

    // Send header
    trap_SendServerCommand( ent - g_entities, va( "print \"^2=== Player %d (%s) Information ===\n\"", clientNum, target->client->pers.netname ) );
    
    // Parse userinfo string and display as key: value pairs
    s = userinfo;
    if ( *s == '\\' )
        s++;
    
    while ( *s ) {
        // Extract key
        o = key;
        while ( *s && *s != '\\' && o < key + sizeof(key) - 1 )
            *o++ = *s++;
        *o = 0;

        if ( !*s ) {
            break;
        }

        // Extract value
        s++; // skip the '\'
        o = value;
        while ( *s && *s != '\\' && o < value + sizeof(value) - 1 )
            *o++ = *s++;
        *o = 0;

        // Print key: value pair
        if ( key[0] ) {
            trap_SendServerCommand( ent - g_entities, va( "print \"^7%s: ^3%s\n\"", key, value ) );
        }

        if ( *s )
            s++;
    }
    
    trap_SendServerCommand( ent - g_entities, "print \"^2=== End Player Information ===\n\"" );
}
