// Copyright (C) 1999-2000 Id Software, Inc.
//

// this file holds commands that can be executed by the server console, but not remote clients

// #include "g_local.h"
#include "svcmds/svcmds.h"

/* forward decl to avoid heavy include duplication */
qboolean Shuffle_Perform( const char *mode );

/* extern declarations for ban/mute system */
extern void AddBan( const char *ip, const char *name, const char *reason, int admin, int duration );
extern void AddMute( const char *ip, const char *name, const char *reason, int admin, int duration );
extern void RemoveBan( int index );
extern void RemoveMute( int index );
extern void Info_Print( const char *s );
extern int s_banCount;
extern int s_muteCount;
extern banEntry_t s_bans[];
extern muteEntry_t s_mutes[];

/*
==============================================================================

PACKET FILTERING
 

You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and you can use '*' to match any value
so you can specify an entire class C network with "addip 192.246.40.*"

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

g_filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.

TTimo NOTE: for persistence, bans are stored in g_banIPs cvar MAX_CVAR_VALUE_STRING
The size of the cvar string buffer is limiting the banning to around 20 masks
this could be improved by putting some g_banIPs2 g_banIps3 etc. maybe
still, you should rely on PB for banning instead

==============================================================================
*/

typedef struct ipFilter_s
{
	unsigned	mask;
	unsigned	compare;
} ipFilter_t;

#define	MAX_IPFILTERS	1024

static ipFilter_t	ipFilters[MAX_IPFILTERS];
static int			numIPFilters;

/*
=================
StringToFilter
=================
*/
static qboolean StringToFilter (char *s, ipFilter_t *f)
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];
	
	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}
	
	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			if (*s == '*') // 'match any'
			{
				// b[i] and m[i] to 0
				s++;
				if (!*s)
					break;
				s++;
				continue;
			}
			G_Printf( "Bad filter address: %s\n", s );
			return qfalse;
		}
		
		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		m[i] = 255;

		if (!*s)
			break;
		s++;
	}
	
	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;
	
	return qtrue;
}

/*
=================
UpdateIPBans
=================
*/
static void UpdateIPBans (void)
{
	byte	b[4];
	byte	m[4];
	int		i,j;
	char	iplist_final[MAX_CVAR_VALUE_STRING];
	char	ip[64];

	*iplist_final = 0;
	for (i = 0 ; i < numIPFilters ; i++)
	{
		if (ipFilters[i].compare == 0xffffffff)
			continue;

		*(unsigned *)b = ipFilters[i].compare;
		*(unsigned *)m = ipFilters[i].mask;
		*ip = 0;
		for (j = 0 ; j < 4 ; j++)
		{
			if (m[j]!=255)
				Q_strcat(ip, sizeof(ip), "*");
			else
				Q_strcat(ip, sizeof(ip), va("%i", b[j]));
			Q_strcat(ip, sizeof(ip), (j<3) ? "." : " ");
		}		
		if (strlen(iplist_final)+strlen(ip) < MAX_CVAR_VALUE_STRING)
		{
			Q_strcat( iplist_final, sizeof(iplist_final), ip);
		}
		else
		{
			Com_Printf("g_banIPs overflowed at MAX_CVAR_VALUE_STRING\n");
			break;
		}
	}

	trap_Cvar_Set( "g_banIPs", iplist_final );
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterPacket (char *from)
{
	int		i;
	unsigned	in;
	byte m[4];
	char *p;

	i = 0;
	p = from;
	while (*p && i < 4) {
		m[i] = 0;
		while (*p >= '0' && *p <= '9') {
			m[i] = m[i]*10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}
	
	in = *(unsigned *)m;

	for (i=0 ; i<numIPFilters ; i++)
		if ( (in & ipFilters[i].mask) == ipFilters[i].compare)
			return g_filterBan.integer != 0;

	return g_filterBan.integer == 0;
}

/*
=================
AddIP
=================
*/
static void AddIP( char *str )
{
	int		i;

	for (i = 0 ; i < numIPFilters ; i++)
		if (ipFilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numIPFilters)
	{
		if (numIPFilters == MAX_IPFILTERS)
		{
			G_Printf ("IP filter list is full\n");
			return;
		}
		numIPFilters++;
	}
	
	if (!StringToFilter (str, &ipFilters[i]))
		ipFilters[i].compare = 0xffffffffu;

	UpdateIPBans();
}

/*
=================
G_ProcessIPBans
=================
*/
void G_ProcessIPBans(void) 
{
	char *s, *t;
	char		str[MAX_CVAR_VALUE_STRING];

	Q_strncpyz( str, g_banIPs.string, sizeof(str) );

	for (t = s = g_banIPs.string; *t; /* */ ) {
		s = strchr(s, ' ');
		if (!s)
			break;
		while (*s == ' ')
			*s++ = 0;
		if (*t)
			AddIP( t );
		t = s;
	}
}


/*
=================
Svcmd_AddIP_f
=================
*/
void Svcmd_AddIP_f (void)
{
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 2 ) {
		G_Printf("Usage:  addip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	AddIP( str );

}

/*
=================
Svcmd_RemoveIP_f
=================
*/
void Svcmd_RemoveIP_f (void)
{
	ipFilter_t	f;
	int			i;
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 2 ) {
		G_Printf("Usage:  sv removeip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	if (!StringToFilter (str, &f))
		return;

	for (i=0 ; i<numIPFilters ; i++) {
		if (ipFilters[i].mask == f.mask	&&
			ipFilters[i].compare == f.compare) {
			ipFilters[i].compare = 0xffffffffu;
			G_Printf ("Removed.\n");

			UpdateIPBans();
			return;
		}
	}

	G_Printf ( "Didn't find %s.\n", str );
}

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void) {
	int			e;
	gentity_t		*check;

	check = g_entities;
	for (e = 0; e < level.num_entities ; e++, check++) {
		if ( !check->inuse ) {
			continue;
		}
		G_Printf("%3i:", e);
		switch ( check->s.eType ) {
		case ET_GENERAL:
			G_Printf("ET_GENERAL          ");
			break;
		case ET_PLAYER:
			G_Printf("ET_PLAYER           ");
			break;
		case ET_ITEM:
			G_Printf("ET_ITEM             ");
			break;
		case ET_MISSILE:
			G_Printf("ET_MISSILE          ");
			break;
		case ET_MOVER:
			G_Printf("ET_MOVER            ");
			break;
		case ET_BEAM:
			G_Printf("ET_BEAM             ");
			break;
		case ET_PORTAL:
			G_Printf("ET_PORTAL           ");
			break;
		case ET_SPEAKER:
			G_Printf("ET_SPEAKER          ");
			break;
		case ET_PUSH_TRIGGER:
			G_Printf("ET_PUSH_TRIGGER     ");
			break;
		case ET_TELEPORT_TRIGGER:
			G_Printf("ET_TELEPORT_TRIGGER ");
			break;
		case ET_INVISIBLE:
			G_Printf("ET_INVISIBLE        ");
			break;
		case ET_GRAPPLE:
			G_Printf("ET_GRAPPLE          ");
			break;
		default:
			G_Printf("%3i                 ", check->s.eType);
			break;
		}

		if ( check->classname ) {
			G_Printf("%s", check->classname);
		}
		G_Printf("\n");
	}
}

gclient_t	*ClientForString( const char *s ) {
	gclient_t	*cl;
	int			i;
	int			idnum;

	// numeric values are just slot numbers
	if ( s[0] >= '0' && s[0] <= '9' ) {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			Com_Printf( "Bad client slot: %i\n", idnum );
			return NULL;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			G_Printf( "Client %i is not connected\n", idnum );
			return NULL;
		}
		return cl;
	}

	// check for a name match
	for ( i=0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->pers.netname, s ) ) {
			return cl;
		}
	}

	G_Printf( "User %s is not on the server\n", s );

	return NULL;
}

/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
void	Svcmd_ForceTeam_f( void ) {
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 3 ) {
		G_Printf("Usage: forceteam <player> <team>\n");
		return;
	}

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	cl = ClientForString( str );
	if ( !cl ) {
		return;
	}

	// set the team
	trap_Argv( 2, str, sizeof( str ) );
	SetTeamSafe( &g_entities[cl - level.clients], str );
}


void Svcmd_Rotate_f( void ) {
	char	str[MAX_TOKEN_CHARS];

	if ( trap_Argc() >= 2 ) {
		trap_Argv( 1, str, sizeof( str ) );
		if ( atoi( str ) > 0 ) {
			trap_Cvar_Set( SV_ROTATION, str );
		}
	}

	if ( !ParseMapRotation() ) {
		char val[ MAX_CVAR_VALUE_STRING ];

		trap_Cvar_VariableStringBuffer( "nextmap", val, sizeof( val ) );

		if ( !val[0] || !Q_stricmpn( val, "map_restart ", 12 ) )
			G_LoadMap( NULL );
		else
			trap_SendConsoleCommand( EXEC_APPEND, "vstr nextmap\n" );
	}
}


char	*ConcatArgs( int start );

/*
=================
ConsoleCommand

=================
*/
qboolean	ConsoleCommand( void ) {
	char	cmd[MAX_TOKEN_CHARS];

	trap_Argv( 0, cmd, sizeof( cmd ) );

	/* pre-hook stats write for map change commands */
	if ( Q_stricmp (cmd, "map") == 0 || Q_stricmp (cmd, "devmap") == 0 || Q_stricmp (cmd, "map_restart") == 0 ) {
		/* forward-declared in g_main.c */
		extern void G_WriteMatchStatsJSON( void );
		G_WriteMatchStatsJSON();
	}

	if ( Q_stricmp (cmd, "entitylist") == 0 ) {
		Svcmd_EntityList_f();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "forceteam") == 0 ) {
		Svcmd_ForceTeam_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "game_memory") == 0) {
		Svcmd_GameMem_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addbot") == 0) {
		Svcmd_AddBot_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "botlist") == 0) {
		Svcmd_BotList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "abort_podium") == 0) {
		Svcmd_AbortPodium_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "addip") == 0) {
		Svcmd_AddIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "removeip") == 0) {
		Svcmd_RemoveIP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "listip") == 0) {
		trap_SendConsoleCommand( EXEC_NOW, "g_banIPs\n" );
		return qtrue;
	}

	if (Q_stricmp (cmd, "rotate") == 0) {
		Svcmd_Rotate_f();
		return qtrue;
	}

	/* announcer server commands */
	if ( Q_stricmp( cmd, "ann_reload" ) == 0 ) { Svcmd_AnnouncerReload_f(); return qtrue; }
	if ( Q_stricmp( cmd, "ann_list" ) == 0 ) { Svcmd_AnnouncerList_f(); return qtrue; }
	if ( Q_stricmp( cmd, "ann_enable" ) == 0 ) { Svcmd_AnnouncerEnable_f(); return qtrue; }
	if ( Q_stricmp( cmd, "ann_force" ) == 0 ) { Svcmd_AnnouncerForce_f(); return qtrue; }

	/* apply deferred vote revert after map_restart when command buffer runs */
	if ( Q_stricmp (cmd, "map_restart") == 0 ) {
		if ( trap_Cvar_VariableIntegerValue( "g_voteRevertPending" ) ) {
			char cvarName[64];
			char prevVal[64];
			trap_Cvar_VariableStringBuffer( "g_voteRevertCvar", cvarName, sizeof(cvarName) );
			trap_Cvar_VariableStringBuffer( "g_voteRevertValue", prevVal, sizeof(prevVal) );
			if ( cvarName[0] && prevVal[0] ) {
				trap_Cvar_Set( cvarName, prevVal );
			}
			trap_Cvar_Set( "g_voteRevertPending", "0" );
			trap_Cvar_Set( "g_voteRevertCvar", "" );
			trap_Cvar_Set( "g_voteRevertValue", "" );
		}
		/* allow the real map_restart to proceed */
		return qfalse;
	}

    if (Q_stricmp (cmd, "shuffle") == 0) {
        char mode[16];
        mode[0] = '\0';
        if ( trap_Argc() >= 2 ) {
            trap_Argv( 1, mode, sizeof(mode) );
        } else {
            Q_strncpyz( mode, "random", sizeof(mode) );
        }
        if ( !Shuffle_Perform( mode ) ) {
            G_Printf("Usage: shuffle <random|score>\n");
        }
        return qtrue;
    }

    /* removed console test command */

	if (Q_stricmp (cmd, "playerlist") == 0, Q_stricmp (cmd, "players") == 0) {
		Cmd_svPlrlist_f();
		return qtrue;
	}

	/* Admin commands for console */
	if (Q_stricmp (cmd, "cp") == 0) {
		Svcmd_AdminCP_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "asay") == 0) {
		Svcmd_AdminSay_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "aban") == 0) {
		Svcmd_AdminBan_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "akick") == 0) {
		Svcmd_AdminKick_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "amute") == 0) {
		Svcmd_AdminMute_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "abanlist") == 0) {
		Svcmd_AdminBanList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "amutelist") == 0) {
		Svcmd_AdminMuteList_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "aunban") == 0) {
		Svcmd_AdminUnban_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "aunmute") == 0) {
		Svcmd_AdminUnmute_f();
		return qtrue;
	}

	if (Q_stricmp (cmd, "adumpuser") == 0) {
		Svcmd_AdminDumpUser_f();
		return qtrue;
	}

	if (g_dedicated.integer) {
		if (Q_stricmp (cmd, "say") == 0) {
			G_BroadcastServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(1) ) );
			return qtrue;
		}
		// everything else will also be printed as a say command
		G_BroadcastServerCommand( -1, va("print \"server: %s\n\"", ConcatArgs(0) ) );
		return qtrue;
	}

	return qfalse;
}

/*
==================
Console Admin Commands - Implementations
==================
*/

static void Svcmd_AdminCP_f( void ) {
    char *message;

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: cp <message>\n" );
        return;
    }

    message = ConcatArgs( 1 );
    
    // Send CenterPrint to all connected players
    G_BroadcastServerCommand( -1, va( "cp \"^1[SERVER]\n^7%s\"", message ) );
    
    // Confirm to console
    G_Printf( "CenterPrint sent to all players: %s\n", message );
}

static void Svcmd_AdminSay_f( void ) {
    char *message;

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: asay <message>\n" );
        return;
    }

    message = ConcatArgs( 1 );
    
    // Send server announcement to all connected players
    G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s\n\"", message ) );
    
    // Confirm to console
    G_Printf( "Server announcement sent: %s\n", message );
}

static void Svcmd_AdminBan_f( void ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    char reason[256];
    gentity_t *target;
    char *ip;
    int duration = 0;  /* 0 = permanent */

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: ban <clientNum> [duration] [reason]\n" );
        G_Printf( "Duration: 0=permanent, or time in minutes (e.g. 30 for 30 minutes)\n" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        G_Printf( "Invalid client number.\n" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        G_Printf( "Client not connected.\n" );
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
    AddBan( ip, target->client->pers.netname, reason, -1, duration ); // -1 = console

    // Kick the player
    if ( duration > 0 ) {
        trap_DropClient( clientNum, va( "You have been banned for %d minutes: %s", duration / 60, reason ) );
    } else {
        trap_DropClient( clientNum, va( "You have been permanently banned: %s", reason ) );
    }

    // Notify console
    if ( duration > 0 ) {
        G_Printf( "Banned %s for %d minutes: %s\n", target->client->pers.netname, duration / 60, reason );
    } else {
        G_Printf( "Permanently banned %s for: %s\n", target->client->pers.netname, reason );
    }

    // Notify all players
    if ( duration > 0 ) {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been banned for %d minutes.\n\"", target->client->pers.netname, duration / 60 ) );
    } else {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been permanently banned.\n\"", target->client->pers.netname ) );
    }
}

static void Svcmd_AdminKick_f( void ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    char reason[256];
    gentity_t *target;

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: kick <clientNum> [reason]\n" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        G_Printf( "Invalid client number.\n" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        G_Printf( "Client not connected.\n" );
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

    // Notify console
    G_Printf( "Kicked %s for: %s\n", target->client->pers.netname, reason );

    // Notify all players
    G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been kicked.\n\"", target->client->pers.netname ) );
}

static void Svcmd_AdminMute_f( void ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    char reason[256];
    gentity_t *target;
    char *ip;
    int duration = 0;  /* 0 = permanent */

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: mute <clientNum> [duration] [reason]\n" );
        G_Printf( "Duration: 0=permanent, or time in minutes (e.g. 30 for 30 minutes)\n" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        G_Printf( "Invalid client number.\n" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        G_Printf( "Client not connected.\n" );
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
    AddMute( ip, target->client->pers.netname, reason, -1, duration ); // -1 = console

    // Notify console
    if ( duration > 0 ) {
        G_Printf( "Muted %s for %d minutes: %s\n", target->client->pers.netname, duration / 60, reason );
    } else {
        G_Printf( "Permanently muted %s for: %s\n", target->client->pers.netname, reason );
    }

    // Notify all players
    if ( duration > 0 ) {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been muted for %d minutes.\n\"", target->client->pers.netname, duration / 60 ) );
    } else {
        G_BroadcastServerCommand( -1, va( "print \"^1[SERVER] ^7%s ^1has been permanently muted.\n\"", target->client->pers.netname ) );
    }
}

static void Svcmd_AdminBanList_f( void ) {
    int i;
    char timeStr[64];

    if ( s_banCount == 0 ) {
        G_Printf( "No banned players.\n" );
        return;
    }

    G_Printf( "=== Banned Players ===\n" );
    for ( i = 0; i < s_banCount; i++ ) {
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
        G_Printf( "%d. %s (%s) - %s [%s]\n", 
            i, s_bans[i].name, s_bans[i].ip, s_bans[i].reason, timeStr );
    }
}

static void Svcmd_AdminMuteList_f( void ) {
    int i;
    char timeStr[64];

    if ( s_muteCount == 0 ) {
        G_Printf( "No muted players.\n" );
        return;
    }

    G_Printf( "=== Muted Players ===\n" );
    for ( i = 0; i < s_muteCount; i++ ) {
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
        G_Printf( "%d. %s (%s) - %s [%s]\n", 
            i, s_mutes[i].name, s_mutes[i].ip, s_mutes[i].reason, timeStr );
    }
}

static void Svcmd_AdminUnban_f( void ) {
    int index;
    char arg[MAX_TOKEN_CHARS];
    char name[64];

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: unban <index>\n" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    index = atoi( arg );
    
    if ( index < 0 || index >= s_banCount ) {
        G_Printf( "Invalid ban index.\n" );
        return;
    }

    Q_strncpyz( name, s_bans[index].name, sizeof( name ) );
    RemoveBan( index );

    // Notify console
    G_Printf( "Unbanned %s\n", name );
}

static void Svcmd_AdminUnmute_f( void ) {
    int index;
    char arg[MAX_TOKEN_CHARS];
    char name[64];

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: unmute <index>\n" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    index = atoi( arg );
    
    if ( index < 0 || index >= s_muteCount ) {
        G_Printf( "Invalid mute index.\n" );
        return;
    }

    Q_strncpyz( name, s_mutes[index].name, sizeof( name ) );
    RemoveMute( index );

    // Notify console
    G_Printf( "Unmuted %s\n", name );
}

static void Svcmd_AdminDumpUser_f( void ) {
    int clientNum;
    char arg[MAX_TOKEN_CHARS];
    gentity_t *target;
    char userinfo[MAX_INFO_STRING];
    char *s;
    char key[256];
    char value[256];
    char *o;

    if ( trap_Argc() < 2 ) {
        G_Printf( "Usage: adumpuser <clientNum>\n" );
        return;
    }

    trap_Argv( 1, arg, sizeof( arg ) );
    clientNum = atoi( arg );
    
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        G_Printf( "Invalid client number.\n" );
        return;
    }

    target = &g_entities[clientNum];
    if ( !target->client || target->client->pers.connected != CON_CONNECTED ) {
        G_Printf( "Client not connected.\n" );
        return;
    }

    trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

    G_Printf( "=== Player %d (%s) Information ===\n", clientNum, target->client->pers.netname );
    
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
            G_Printf( "%s: %s\n", key, value );
        }

        if ( *s )
            s++;
    }
    
    G_Printf( "=== End Player Information ===\n" );
}

