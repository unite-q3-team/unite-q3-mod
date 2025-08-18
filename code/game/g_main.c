// Copyright (C) 1999-2000 Id Software, Inc.
//

#include "g_local.h"

/* item replacement system */
extern void IR_Init(void);
extern void IR_SpawnAdds(void);

level_locals_t	level;

typedef struct {
	vmCvar_t	*vmCvar;
	const char	*cvarName;
	const char	*defaultString;
	int			cvarFlags;
	int			modificationCount;	// for tracking changes
	qboolean	trackChange;		// track this variable, and announce if changed
	qboolean	teamShader;			// track and if changed, update shader state
} cvarTable_t;

gentity_t		g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];

#define DECLARE_G_CVAR
	#include "g_cvar.h"
#undef DECLARE_G_CVAR

static cvarTable_t gameCvarTable[] = {

	// noset vars
	{ NULL, "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_ROM, 0, qfalse  },
	{ NULL, "gamedate", __DATE__ , CVAR_ROM, 0, qfalse  },

#define G_CVAR_LIST
	#include "g_cvar.h"
#undef G_CVAR_LIST

};


static void G_InitGame( int levelTime, int randomSeed, int restart );
static void G_RunFrame( int levelTime );
static void G_ShutdownGame( int restart );
static void CheckExitRules( void );
static void SendScoreboardMessageToAllClients( void );
void G_WriteMatchStatsJSON( void );

// extension interface
#ifdef Q3_VM
qboolean (*trap_GetValue)( char *value, int valueSize, const char *key );
#else
int dll_com_trapGetValue;
#endif

int	svf_self_portal2;

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
DLLEXPORT intptr_t vmMain( int command, int arg0, int arg1, int arg2 ) {
	switch ( command ) {
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2 );
		return 0;
	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;
	case GAME_CLIENT_CONNECT:
		return (intptr_t)ClientConnect( arg0, arg1, arg2 );
	case GAME_CLIENT_THINK:
		ClientThink( arg0 );
		return 0;
	case GAME_CLIENT_USERINFO_CHANGED:
		ClientUserinfoChanged( arg0 );
		return 0;
	case GAME_CLIENT_DISCONNECT:
		ClientDisconnect( arg0 );
		return 0;
	case GAME_CLIENT_BEGIN:
		ClientBegin( arg0 );
		return 0;
	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;
	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;
	case GAME_CONSOLE_COMMAND:
		return ConsoleCommand();
	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );
	}

	return -1;
}


void QDECL G_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		text[BIG_INFO_STRING];
	int			len;

	va_start( argptr, fmt );
	len = Q_vsprintf( text, fmt, argptr );
	va_end( argptr );

	text[4095] = '\0'; // truncate to 1.32b/c max print buffer size

	trap_Print( text );
}


void G_BroadcastServerCommand( int ignoreClient, const char *command ) {
	int i;
	for ( i = 0; i < level.maxclients; i++ ) {
		if ( i == ignoreClient )
			continue;
		if ( level.clients[ i ].pers.connected == CON_CONNECTED ) {
			trap_SendServerCommand( i, command );
		}
	}
}


void QDECL G_Error( const char *fmt, ... ) {
	va_list		argptr;
	char		text[1024];

	va_start( argptr, fmt );
	Q_vsprintf( text, fmt, argptr );
	va_end( argptr );

	trap_Error( text );
}

/* C89-friendly helper for appending formatted text into a buffer */
static void JSON_Append( char *buf, int *pOff, int capacity, const char *fmt, ... ) {
	va_list argptr;
	char local[1024];
	int n, off;
	if ( !buf || !pOff || capacity <= 0 ) return;
	off = *pOff;
	if ( off >= capacity - 4 ) return;
	va_start( argptr, fmt );
	n = Q_vsprintf( local, fmt, argptr );
	va_end( argptr );
	if ( n < 0 ) n = 0;
	if ( n > (int)sizeof(local) - 1 ) n = (int)sizeof(local) - 1;
	if ( off + n >= capacity - 1 ) n = (capacity - 1) - off;
	if ( n > 0 ) {
		int i;
		for ( i = 0; i < n; ++i ) buf[ off + i ] = local[ i ];
		off += n;
	}
	*pOff = off;
}

/*
XQ3E Config String
*/
void UpdateXQ3EFeaturesConfigString(void) {
	char configStr[256];
	Com_sprintf(configStr, sizeof(configStr),
		"\\xmode\\config\\x_hck_dmg_draw\\%d\\x_hck_team_unfreezing_foe\\%d\\x_hck_ps_enemy_hitbox\\%d",
		g_x_drawDamage.integer,
		g_x_unfreezeFoe.integer,
		g_x_drawHitbox.integer
	);
	trap_SetConfigstring(1000, configStr);
}

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=MAX_CLIENTS, e=g_entities+i ; i < level.num_entities ; i++,e++ ){
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

	G_Printf ("%i teams with %i entities\n", c, c2);
}


void G_RemapTeamShaders( void ) {
#ifdef MISSIONPACK
	char string[1024];
	float f = level.time * 0.001;
	Com_sprintf( string, sizeof(string), "team_icon/%s_red", g_redteam.string );
	AddRemap("textures/ctf2/redteam01", string, f); 
	AddRemap("textures/ctf2/redteam02", string, f); 
	Com_sprintf( string, sizeof(string), "team_icon/%s_blue", g_blueteam.string );
	AddRemap("textures/ctf2/blueteam01", string, f); 
	AddRemap("textures/ctf2/blueteam02", string, f); 
	trap_SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
#endif
}


/*
=================
G_RegisterCvars
=================
*/
void G_RegisterCvars( void ) {
	qboolean remapped = qfalse;
	cvarTable_t *cv;
	int i;

    for ( i = 0, cv = gameCvarTable ; i < ARRAY_LEN( gameCvarTable ) ; i++, cv++ ) {
		trap_Cvar_Register( cv->vmCvar, cv->cvarName,
			cv->defaultString, cv->cvarFlags );
		if ( cv->vmCvar )
			cv->modificationCount = cv->vmCvar->modificationCount;

		if (cv->teamShader) {
			remapped = qtrue;
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}

	// check some things
	if ( g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE ) {
		G_Printf( "g_gametype %i is out of range, defaulting to 0\n", g_gametype.integer );
		trap_Cvar_Set( "g_gametype", "0" );
		trap_Cvar_Update( &g_gametype );
	}

	level.warmupModificationCount = g_warmup.modificationCount;

    // force g_doWarmup to 1
	trap_Cvar_Register( NULL, "g_doWarmup", "1", CVAR_ROM );
	trap_Cvar_Set( "g_doWarmup", "1" );

    // register vote timing cvars (server admins can adjust at runtime)
    trap_Cvar_Register( NULL, "g_voteTime", "30", CVAR_ARCHIVE ); // seconds for vote timeout
    trap_Cvar_Register( NULL, "g_voteExecuteDelayMs", "3000", CVAR_ARCHIVE ); // delay before executing passed vote command
}

void G_UpdateRatFlags( void ) {
	int rflags = 0;

	// if (g_itemPickup.integer) {
	// 	rflags |= RAT_EASYPICKUP;
	// }

	// if (g_powerupGlows.integer) {
	// 	rflags |= RAT_POWERUPGLOWS;
	// }

	// if (g_screenShake.integer) {
	// 	rflags |= RAT_SCREENSHAKE;
	// }

	// if (g_predictMissiles.integer && g_delagMissiles.integer) {
	// 	rflags |= RAT_PREDICTMISSILES;
	// }

	// if (g_fastSwitch.integer) {
	// 	rflags |= RAT_FASTSWITCH;
	// }

	// if (g_fastWeapons.integer) {
	// 	rflags |= RAT_FASTWEAPONS;
	// }

	// // if (g_crouchSlide.integer == 1) {
	// // 	rflags |= RAT_CROUCHSLIDE;
	// // }

	// if (g_rampJump.integer) {
	// 	rflags |= RAT_RAMPJUMP;
	// }


	// if (g_allowForcedModels.integer) {
	// 	rflags |= RAT_ALLOWFORCEDMODELS;
	// }

	// if (g_friendsWallHack.integer) {
	// 	rflags |= RAT_FRIENDSWALLHACK;
	// }

	// if (g_specShowZoom.integer) {
	// 	rflags |= RAT_SPECSHOWZOOM;
	// }

	// if (g_brightPlayerShells.integer) {
	// 	rflags |= RAT_BRIGHTSHELL;
	// }

	// if (g_brightPlayerOutlines.integer) {
	// 	rflags |= RAT_BRIGHTOUTLINE;
	// }

	// if (g_brightModels.integer) {
	// 	rflags |= RAT_BRIGHTMODEL;
	// }

	// if (g_newShotgun.integer) {
	// 	rflags |= RAT_NEWSHOTGUN;
	// }

	// if (g_additiveJump.integer) {
	// 	rflags |= RAT_ADDITIVEJUMP;
	// }

	// if (!g_allowTimenudge.integer) {
	// 	rflags |= RAT_NOTIMENUDGE;
	// }

	// if (g_friendsFlagIndicator.integer) {
	// 	rflags |= RAT_FLAGINDICATOR;
	// }

	// if (g_regularFootsteps.integer) {
	// 	rflags |= RAT_REGULARFOOTSTEPS;
	// }

	// if (g_smoothStairs.integer) {
	// 	rflags |= RAT_SMOOTHSTAIRS;
	// }

	if (!g_overbounce.integer) {
		rflags |= RAT_NOOVERBOUNCE;
	}

	// if (g_passThroughInvisWalls.integer) {
	// 	rflags |= RAT_NOINVISWALLS;
	// }

	// if (!g_bobup.integer) {
	// 	rflags |= RAT_NOBOBUP;
	// }
	
	// if (g_fastSwim.integer) {
	// 	rflags |= RAT_FASTSWIM;
	// }

	// if (g_swingGrapple.integer) {
	// 	rflags |= RAT_SWINGGRAPPLE;
	// }

	if (g_freeze.integer) {
		rflags |= RAT_FREEZETAG;
	}

	// if (g_crouchSlide.integer == 1) {
	// 	rflags |= RAT_CROUCHSLIDE;
	// }

	// if (g_slideMode.integer == 1) {
	// 	rflags |= RAT_SLIDEMODE;
	// }

	// if (g_tauntForceOn.integer) {
	// 	rflags |= RAT_FORCETAUNTS;
	// }

	// XXX --> also update code where this is called!

	trap_Cvar_Set("g_altFlags",va("%i",rflags));
	trap_Cvar_Update( &g_altFlags );
}
/*
=================
G_UpdateCvars
=================
*/
static void G_UpdateCvars( void ) {
	int			i;
	cvarTable_t	*cv;
	qboolean remapped = qfalse;
	qboolean updateRatFlags = qfalse;

	for ( i = 0, cv = gameCvarTable ; i < ARRAY_LEN( gameCvarTable ) ; i++, cv++ ) {
		if ( cv->vmCvar ) {
			trap_Cvar_Update( cv->vmCvar );

			if ( cv->modificationCount != cv->vmCvar->modificationCount ) {
				cv->modificationCount = cv->vmCvar->modificationCount;

				if ( cv->trackChange ) {
					G_BroadcastServerCommand( -1, va("print \"Server: %s changed to %s\n\"", 
						cv->cvarName, cv->vmCvar->string ) );
				}

				if (cv->teamShader) {
					remapped = qtrue;
				}

				if (
						// cv->vmCvar == &g_itemPickup 
						// || cv->vmCvar == &g_powerupGlows
						// || cv->vmCvar == &g_screenShake
						// || cv->vmCvar == &g_predictMissiles
						// || cv->vmCvar == &g_fastSwitch
						// || cv->vmCvar == &g_fastWeapons
						// // || cv->vmCvar == &g_crouchSlide
						// || cv->vmCvar == &g_rampJump
						// || cv->vmCvar == &g_allowForcedModels
						// || cv->vmCvar == &g_friendsWallHack
						// || cv->vmCvar == &g_specShowZoom
						// || cv->vmCvar == &g_brightPlayerShells
						// || cv->vmCvar == &g_brightPlayerOutlines
						// || cv->vmCvar == &g_brightModels
						// || cv->vmCvar == &g_newShotgun
						// || cv->vmCvar == &g_additiveJump
						// || cv->vmCvar == &g_friendsFlagIndicator
						// || cv->vmCvar == &g_regularFootsteps
						// || cv->vmCvar == &g_smoothStairs
						cv->vmCvar == &g_overbounce
						// || cv->vmCvar == &g_passThroughInvisWalls
						// || cv->vmCvar == &g_bobup
						// || cv->vmCvar == &g_fastSwim
						// || cv->vmCvar == &g_swingGrapple
						|| cv->vmCvar == &g_freeze
						// || cv->vmCvar == &g_crouchSlide
						// || cv->vmCvar == &g_slideMode
						// || cv->vmCvar == &g_tauntForceOn
						) {
					updateRatFlags = qtrue;
				}
			}
		}
	}

	if (remapped) {
		G_RemapTeamShaders();
	}

	if (updateRatFlags) {
		G_UpdateRatFlags();
	}
}


static void G_LocateSpawnSpots( void ) 
{
	gentity_t			*ent;
	int i, n;

	level.spawnSpots[ SPAWN_SPOT_INTERMISSION ] = NULL;

	// locate all spawn spots
	n = 0;
	ent = g_entities + MAX_CLIENTS;
	for ( i = MAX_CLIENTS; i < MAX_GENTITIES; i++, ent++ ) {
		
		if ( !ent->inuse || !ent->classname )
			continue;

		// intermission/ffa spots
		if ( !Q_stricmpn( ent->classname, "info_player_", 12 ) ) {
			if ( !Q_stricmp( ent->classname+12, "intermission" ) ) {
				if ( level.spawnSpots[ SPAWN_SPOT_INTERMISSION ] == NULL ) {
					level.spawnSpots[ SPAWN_SPOT_INTERMISSION ] = ent; // put in the last slot
					ent->fteam = TEAM_FREE;
				}
				continue;
			}
			if ( !Q_stricmp( ent->classname+12, "deathmatch" ) ) {
				level.spawnSpots[n] = ent; n++;
				level.numSpawnSpotsFFA++;
				ent->fteam = TEAM_FREE;
				ent->count = 1;
				continue;
			}
			continue;
		}

		// team spawn spots
		if ( !Q_stricmpn( ent->classname, "team_CTF_", 9 ) ) {
			if ( !Q_stricmp( ent->classname+9, "redspawn" ) ) {
				level.spawnSpots[n] = ent; n++;
				level.numSpawnSpotsTeam++;
				ent->fteam = TEAM_RED;
				ent->count = 1; // means its not initial spawn point
				continue;
			}
			if ( !Q_stricmp( ent->classname+9, "bluespawn" ) ) {
				level.spawnSpots[n] = ent; n++;
				level.numSpawnSpotsTeam++;
				ent->fteam = TEAM_BLUE;
				ent->count = 1;
				continue;
			}
			// base spawn spots
			if ( !Q_stricmp( ent->classname+9, "redplayer" ) ) {
				level.spawnSpots[n] = ent; n++;
				level.numSpawnSpotsTeam++;
				ent->fteam = TEAM_RED;
				ent->count = 0;
				continue;
			}
			if ( !Q_stricmp( ent->classname+9, "blueplayer" ) ) {
				level.spawnSpots[n] = ent; n++;
				level.numSpawnSpotsTeam++;
				ent->fteam = TEAM_BLUE;
				ent->count = 0;
				continue;
			}
		}
	}
	level.numSpawnSpots = n;
}

/* Load per-map custom spawns from spawns.txt, overriding defaults */
void G_LoadCustomSpawns( void ) {
    fileHandle_t f;
    int flen;
    char *buf;
    char *p;
    int n;
    if ( (flen = trap_FS_FOpenFile("spawns.txt", &f, FS_READ)) <= 0 ) {
        return;
    }
    if ( flen > 64 * 1024 ) flen = 64 * 1024;
    buf = (char*)G_Alloc( flen + 1 );
    if ( !buf ) { trap_FS_FCloseFile(f); return; }
    trap_FS_Read( buf, flen, f );
    trap_FS_FCloseFile( f );
    buf[flen] = '\0';

    /* Parse lines: spawn.<map>.<idx> = TEAM BASE X Y Z */
    /* If any line for current map is found, we will rebuild spawns using only those */
    n = 0;
    {
        char *scan;
        int foundAny;
        foundAny = 0;
        scan = buf;
        while ( *scan ) {
            char line[256];
            char *nl;
            int linelen;
            nl = strchr( scan, '\n' );
            if ( nl ) linelen = (int)(nl - scan); else linelen = (int)strlen(scan);
            if ( linelen > (int)sizeof(line) - 1 ) linelen = (int)sizeof(line) - 1;
            Q_strncpyz( line, scan, linelen + 1 );
            scan = nl ? nl + 1 : scan + linelen;

            /* trim spaces */
            {
                int i = (int)strlen(line);
                while ( i > 0 && (line[i-1]=='\r' || line[i-1]==' ' || line[i-1]=='\t') ) { line[i-1] = '\0'; i--; }
            }
            if ( line[0] == '\0' || line[0] == '#' || (line[0]=='/' && line[1]=='/') ) continue;

            /* split key = value */
            {
                char *eq = strchr( line, '=' );
                if ( !eq ) continue;
                *eq = '\0';
                {
                    char *key = line;
                    char *val = eq + 1;
                    /* trim key spaces */
                    while ( *key == ' ' || *key == '\t' ) key++;
                    {
                        int i = (int)strlen(key);
                        while ( i > 0 && (key[i-1]==' '||key[i-1]=='\t') ) { key[i-1]='\0'; i--; }
                    }
                    while ( *val == ' ' || *val == '\t' ) val++;
                    {
                        int i = (int)strlen(val);
                        while ( i > 0 && (val[i-1]==' '||val[i-1]=='\t') ) { val[i-1]='\0'; i--; }
                    }
                    /* expect key like spawn.<map>.<idx> */
                    if ( !Q_strncmp( key, "spawn.", 6 ) ) {
                        const char *map = key + 6;
                        const char *dot = strchr( map, '.' );
                        if ( dot ) {
                            int mapLen = (int)(dot - map);
                            char mapKey[64];
                            if ( mapLen > (int)sizeof(mapKey)-1 ) mapLen = (int)sizeof(mapKey)-1;
                            Q_strncpyz( mapKey, map, mapLen + 1 );
                            if ( !Q_stricmp( mapKey, g_mapname.string ) ) {
                                foundAny = 1;
                            }
                        }
                    }
                }
            }
        }
        if ( !foundAny ) {
            return;
        }
    }

    /* Reset arrays; keep intermission spot pointer as-is */
    level.numSpawnSpots = 0;
    level.numSpawnSpotsFFA = 0;
    level.numSpawnSpotsTeam = 0;

    p = buf;
    while ( *p ) {
        char line[256];
        char *nl;
        int linelen;
        nl = strchr( p, '\n' );
        if ( nl ) linelen = (int)(nl - p); else linelen = (int)strlen(p);
        if ( linelen > (int)sizeof(line) - 1 ) linelen = (int)sizeof(line) - 1;
        Q_strncpyz( line, p, linelen + 1 );
        p = nl ? nl + 1 : p + linelen;

        /* trim */
        {
            int i = (int)strlen(line);
            while ( i > 0 && (line[i-1]=='\r'||line[i-1]==' '||line[i-1]=='\t') ) { line[i-1] = '\0'; i--; }
        }
        if ( line[0] == '\0' || line[0] == '#' || (line[0]=='/'&&line[1]=='/') ) continue;
        {
            char *eq = strchr( line, '=' );
            if ( !eq ) continue;
            *eq = '\0';
            {
                char *key = line;
                char *val = eq + 1;
                while ( *key==' '||*key=='\t' ) key++;
                {
                    int i=(int)strlen(key);
                    while ( i>0 && (key[i-1]==' '||key[i-1]=='\t') ) { key[i-1]='\0'; i--; }
                }
                while ( *val==' '||*val=='\t' ) val++;

                if ( !Q_strncmp( key, "spawn.", 6 ) ) {
                    const char *map = key + 6;
                    const char *dot = strchr( map, '.' );
                    if ( dot ) {
                        int mapLen = (int)(dot - map);
                        char mapKey[64];
                        char valcpy[160];
                        char teamStr[16];
                        int isBase;
                        int x, y, z;
                        int haveAngles = 0;
                        int pitchi = 0, yawi = 0, rolli = 0;
                        gentity_t *spot;
                        if ( mapLen > (int)sizeof(mapKey)-1 ) mapLen = (int)sizeof(mapKey)-1;
                        Q_strncpyz( mapKey, map, mapLen + 1 );
                        if ( Q_stricmp( mapKey, g_mapname.string ) ) continue;
                        /* parse val without sscanf (not available in QVM) */
                        Q_strncpyz( valcpy, val, sizeof(valcpy) );
                        {
                            char *tokv[9];
                            int parts;
                            parts = Com_Split( valcpy, tokv, 9, ' ' );
                            if ( parts < 5 ) continue; /* require at least TEAM BASE X Y Z */
                            Q_strncpyz( teamStr, tokv[0], sizeof(teamStr) );
                            isBase = atoi( tokv[1] );
                            x = atoi( tokv[2] );
                            y = atoi( tokv[3] );
                            z = atoi( tokv[4] );
                            /* optional angles P Y R at [5..7] */
                            if ( parts >= 8 ) {
                                pitchi = atoi( tokv[5] );
                                yawi   = atoi( tokv[6] );
                                rolli  = atoi( tokv[7] );
                                haveAngles = 1;
                            }
                        }
                        spot = G_Spawn();
                        if ( !Q_stricmp( teamStr, "RED" ) ) { spot->fteam = TEAM_RED; }
                        else if ( !Q_stricmp( teamStr, "BLUE" ) ) { spot->fteam = TEAM_BLUE; }
                        else { spot->fteam = TEAM_FREE; }
                        /* base flag: 0 for base player spawn, 1 for regular */
                        spot->count = (isBase ? 0 : 1);
                        if ( spot->fteam == TEAM_FREE ) {
                            spot->classname = "info_player_deathmatch";
                            level.numSpawnSpotsFFA++;
                        } else if ( spot->fteam == TEAM_RED ) {
                            spot->classname = isBase ? "team_CTF_redplayer" : "team_CTF_redspawn";
                            level.numSpawnSpotsTeam++;
                        } else if ( spot->fteam == TEAM_BLUE ) {
                            spot->classname = isBase ? "team_CTF_blueplayer" : "team_CTF_bluespawn";
                            level.numSpawnSpotsTeam++;
                        }
                        {
                            vec3_t org;
                            org[0] = (float)x;
                            org[1] = (float)y;
                            org[2] = (float)z;
                            G_SetOrigin( spot, org );
                            /* many selection paths read spot->s.origin directly */
                            VectorCopy( org, spot->s.origin );
                            trap_LinkEntity( spot );
                        }
                        if ( haveAngles ) {
                            spot->s.angles[PITCH] = (float)pitchi;
                            spot->s.angles[YAW]   = (float)yawi;
                            spot->s.angles[ROLL]  = (float)rolli;
                        }
                        if ( level.numSpawnSpots < NUM_SPAWN_SPOTS - 1 ) {
                            level.spawnSpots[ level.numSpawnSpots++ ] = spot;
                        }
                    }
                }
            }
        }
    }
}

extern pmoveConfig_t pmoveConfig;

// Надо улучшать. Требуется переезд cvar на новую систему (как в осп2 или omega-mod)
// Сейчас требуется рестарт сервера для применения изменений, т.к. инит при старте карты
// Сейчас нет возможности сделать аналог CG_LocalEventCvarChanged (ОСП2)
void G_InitFireRatios(void) {
    pmoveConfig.fireRatios[WP_GAUNTLET]         = g_gauntlet_fireRatio.integer;
    pmoveConfig.fireRatios[WP_LIGHTNING]        = g_lg_fireRatio.integer;
    pmoveConfig.fireRatios[WP_SHOTGUN]          = g_sg_fireRatio.integer;
    pmoveConfig.fireRatios[WP_MACHINEGUN]       = g_mg_fireRatio.integer;
    pmoveConfig.fireRatios[WP_GRENADE_LAUNCHER] = g_gl_fireRatio.integer;
    pmoveConfig.fireRatios[WP_ROCKET_LAUNCHER]  = g_rl_fireRatio.integer;
    pmoveConfig.fireRatios[WP_PLASMAGUN]        = g_pg_fireRatio.integer;
    pmoveConfig.fireRatios[WP_RAILGUN]          = g_rg_fireRatio.integer;
    pmoveConfig.fireRatios[WP_BFG]              = g_bfg_fireRatio.integer;
}

/*
============
G_InitGame

============
*/
static void G_InitGame( int levelTime, int randomSeed, int restart ) {
	char value[ MAX_CVAR_VALUE_STRING ];
	int	i;

	G_Printf ("------- Game Initialization -------\n");
	G_Printf ("gamename: %s\n", GAMEVERSION);
	G_Printf ("gamedate: %s\n", __DATE__);

	// extension interface
	trap_Cvar_VariableStringBuffer( "//trap_GetValue", value, sizeof( value ) );
	if ( value[0] ) {
#ifdef Q3_VM
		trap_GetValue = (void*)~atoi( value );
#else
		dll_com_trapGetValue = atoi( value );
#endif
		if ( trap_GetValue( value, sizeof( value ), "SVF_SELF_PORTAL2_Q3E" ) ) {
			svf_self_portal2 = atoi( value );
		} else {
			svf_self_portal2 = 0;
		}
	}

	srand( randomSeed );

    G_RegisterCvars();
    /* initialize disabled commands list */
    DC_Init();

    G_InitFireRatios();
    /* load weapons config overrides */
    Weapons_Init();

    /* initialize vote system rules file early so default gets created if missing */
    VS_Init();
    /* initialize announcer */
    AN_Init();

    /* build cached map list once at startup */
    G_EnsureMapListCache();

	G_ProcessIPBans();

	G_InitMemory();
    /* initialize item replacement system (after memory init) */
    IR_Init();

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;

	level.startTime = levelTime;

	level.previousTime = levelTime;
	level.msec = FRAMETIME;

	/* clear abort flag for this match */
	level.abortedDueToNoPlayers = 0;
	level.statsWritten = 0;
	level.statsShown = 0;
	level.voteCalledDuringIntermission = qfalse;

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	if ( g_gametype.integer != GT_SINGLE_PLAYER && g_log.string[0] ) {
		if ( g_logSync.integer ) {
			trap_FS_FOpenFile( g_log.string, &level.logFile, FS_APPEND_SYNC );
		} else {
			trap_FS_FOpenFile( g_log.string, &level.logFile, FS_APPEND );
		}
		if ( level.logFile == FS_INVALID_HANDLE ) {
			G_Printf( "WARNING: Couldn't open logfile: %s\n", g_log.string );
		} else {
			char	serverinfo[MAX_INFO_STRING];

			trap_GetServerinfo( serverinfo, sizeof( serverinfo ) );

			G_LogPrintf("------------------------------------------------------------\n" );
			G_LogPrintf("InitGame: %s\n", serverinfo );
		}
	} else {
		G_Printf( "Not logging to disk.\n" );
	}

	G_InitWorldSession();

	// initialize all entities for this game
	memset( g_entities, 0, MAX_GENTITIES * sizeof(g_entities[0]) );
	level.gentities = g_entities;

	// initialize all clients for this game
	level.maxclients = g_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]) );
	level.clients = g_clients;

	// set client fields on player ents
	for ( i=0 ; i<level.maxclients ; i++ ) {
		g_entities[i].client = level.clients + i;
	}

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		g_entities[ i ].classname = "clientslot";
	}

	// let the server system know where the entites are
	trap_LocateGameData( level.gentities, level.num_entities, sizeof( gentity_t ), 
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	/* optionally force precache of all start/grantable weapons to avoid client hitches */
	if ( g_precacheStartWeapons.integer || g_precacheAllWeapons.integer ) {
		int w;
		for ( w = 1; w < WP_NUM_WEAPONS; ++w ) {
			gitem_t *it = BG_FindItemForWeapon( w );
			if ( it ) RegisterItem( it );
		}
	}

	/* optionally force precache of all items (armor/health/ammo/powerups/holdables) */
	if ( g_precacheAllItems.integer ) {
		int i;
		for ( i = 0; i < bg_numItems; ++i ) {
			gitem_t *it = &bg_itemlist[i];
			if ( it && it->classname && it->giType != IT_BAD ) {
				RegisterItem( it );
			}
		}
	}

    // parse the key/value pairs and spawn gentities
    G_SpawnEntitiesFromString();

    // spawn persistent item additions after map entities are spawned
    if ( g_itemReplace.integer ) {
        IR_SpawnAdds();
    }

	// general initialization
	G_FindTeams();

	// make sure we have flags for CTF, etc
	if( g_gametype.integer >= GT_TEAM ) {
		G_CheckTeamItems();
	}

	SaveRegisteredItems();

	G_LocateSpawnSpots();

    /* Override with custom spawns if present */
    if ( g_customSpawns.integer ) {
        G_LoadCustomSpawns();
    }

	G_Printf ("-----------------------------------\n");

	if( g_gametype.integer == GT_SINGLE_PLAYER || trap_Cvar_VariableIntegerValue( "com_buildScript" ) ) {
		G_ModelIndex( SP_PODIUM_MODEL );
	}

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( restart );
	}

	G_RemapTeamShaders();

	// don't forget to reset times
	trap_SetConfigstring( CS_INTERMISSION, "" );

	if (g_freeze.integer) {
	trap_SetConfigstring( CS_OSP_FREEZE_GAME_TYPE, "1");
	}
	trap_SetConfigstring( CS_OSP_CUSTOM_CLIENT2, "1");
	UpdateXQ3EFeaturesConfigString();

	if ( g_gametype.integer != GT_SINGLE_PLAYER ) {
		// launch rotation system on first map load
		if ( trap_Cvar_VariableIntegerValue( SV_ROTATION ) == 0 ) {
			trap_Cvar_Set( SV_ROTATION, "1" );
			level.denyMapRestart = qtrue;
			ParseMapRotation();
		}
	}
}


/*
=================
G_ShutdownGame
=================
*/
static void G_ShutdownGame( int restart ) 
{
	G_Printf ("==== ShutdownGame ====\n");

	/* Ensure match stats saved on direct map changes (vote/console) */
	G_WriteMatchStatsJSON();

	if ( level.logFile != FS_INVALID_HANDLE ) {
		G_LogPrintf("ShutdownGame:\n" );
		G_LogPrintf("------------------------------------------------------------\n" );
		trap_FS_FCloseFile( level.logFile );
		level.logFile = FS_INVALID_HANDLE;
	}

	// write all the client session data so we can get it back
	G_WriteSessionData();

	if ( trap_Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}
}



//===================================================================

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link

void QDECL Com_Error( int level, const char *fmt, ... ) {
	va_list		argptr;
	char		text[4096];

	va_start( argptr, fmt );
	Q_vsprintf( text, fmt, argptr );
	va_end( argptr );

	trap_Error( text );
}


void QDECL Com_Printf( const char *fmt, ... ) {
	va_list		argptr;
	char		text[4096];

	va_start( argptr, fmt );
	Q_vsprintf( text, fmt, argptr );
	va_end( argptr );

	trap_Print( text );
}

#endif

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 2 ) {
		return;
	}

	// never change during intermission
	if ( level.intermissiontime ) {
		return;
	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD || 
			client->sess.spectatorClient < 0  ) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorTime > nextInLine->sess.spectatorTime ) {
			nextInLine = client;
		}
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );
}


/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[1];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}


/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[0];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}


/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			clientNum;

	clientNum = level.sortedClients[0];
	if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
		level.clients[ clientNum ].sess.wins++;
		ClientUserinfoChanged( clientNum );
	}

	clientNum = level.sortedClients[1];
	if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
		level.clients[ clientNum ].sess.losses++;
		ClientUserinfoChanged( clientNum );
	}

}


/*
=============
SortRanks
=============
*/
static int QDECL SortRanks( const void *a, const void *b ) {
	gclient_t	*ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	// sort special clients last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0  ) {
		return -1;
	}

	// then connecting clients
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}

	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorTime > cb->sess.spectatorTime ) {
			return -1;
		}
		if ( ca->sess.spectatorTime < cb->sess.spectatorTime ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}


/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
	gclient_t	*cl;

	if ( level.restarted )
		return;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedClients = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients = 0;
	level.numVotingClients = 0;		// don't count bots
	for (i = 0; i < ARRAY_LEN(level.numteamVotingClients); i++) {
		level.numteamVotingClients[i] = 0;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR ) {
				level.numNonSpectatorClients++;
			
				// decide if this should be auto-followed
				if ( level.clients[i].pers.connected == CON_CONNECTED ) {
					level.numPlayingClients++;
					if ( !(g_entities[i].r.svFlags & SVF_BOT) ) {
						level.numVotingClients++;
						if ( level.clients[i].sess.sessionTeam == TEAM_RED )
							level.numteamVotingClients[0]++;
						else if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							level.numteamVotingClients[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	qsort( level.sortedClients, level.numConnectedClients, 
		sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ( g_gametype.integer >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {	
		rank = -1;
		score = MAX_QINT;
		for ( i = 0;  i < level.numPlayingClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next client is checked
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank;
			} else {
				// we are tied with the previous client
				level.clients[ level.sortedClients[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( g_gametype.integer == GT_SINGLE_PLAYER && level.numPlayingClients == 1 ) {
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
		if ( g_gametype.integer >= GT_TEAM ) {
		if ( g_freeze.integer ) {
			int redAlive, blueAlive;
			ftmod_countAlive(&redAlive, &blueAlive);
			trap_SetConfigstring( CS_SCORES1, va("%i %i", level.teamScores[TEAM_RED], redAlive ) );
			trap_SetConfigstring( CS_SCORES2, va("%i %i", level.teamScores[TEAM_BLUE], blueAlive ) );
		} else {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
		}
	} else {
		if ( level.numConnectedClients == 0 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedClients == 1 ) {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap_SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap_SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
		}
	}

	// see if it is time to end the level
	if (!g_freeze.integer)
	CheckExitRules();

	// if we are at the intermission, send the new info to everyone
	if ( level.intermissiontime ) {
		SendScoreboardMessageToAllClients();
	}
}


/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
static void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[ i ].pers.connected == CON_CONNECTED ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}


/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
void MoveClientToIntermission( gentity_t *ent ) {
	
	gclient_t * client;
	
	client = ent->client;
	
	// take out of follow mode if needed
	if ( client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent, qtrue );
	}

	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, client->ps.origin );
	SetClientViewAngle( ent, level.intermission_angle );
	client->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( client->ps.powerups, 0, sizeof( client->ps.powerups ) );

	client->ps.eFlags = ( client->ps.eFlags & ~EF_PERSISTANT ) | ( client->ps.eFlags & EF_PERSISTANT );

	ent->s.eFlags = client->ps.eFlags;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.event = 0;
	ent->r.contents = 0;

	ent->s.legsAnim = LEGS_IDLE;
	ent->s.torsoAnim = TORSO_STAND;
}


/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
void FindIntermissionPoint( void ) {
	gentity_t	*ent, *target;
	vec3_t		dir;

	if ( level.intermission_spot ) // search only once
		return;

	// find the intermission spot
	ent = level.spawnSpots[ SPAWN_SPOT_INTERMISSION ];

	if ( !ent ) { // the map creator forgot to put in an intermission point...
		SelectSpawnPoint( NULL, vec3_origin, level.intermission_origin, level.intermission_angle );
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}

	level.intermission_spot = qtrue;
}


/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void ) {
	int			i;
	gentity_t	*client;

	if ( level.intermissiontime ) {
		return;	// already active
	}

	// if in tournement mode, change the wins / losses
	if ( g_gametype.integer == GT_TOURNAMENT ) {
		AdjustTournamentScores();
	}

	level.intermissiontime = level.time;
	FindIntermissionPoint();

	// move all clients to the intermission point
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = g_entities + i;
		if ( !client->inuse )
			continue;

		// respawn if dead
		if ( client->health <= 0 ) {
			ClientRespawn( client );
		}

		MoveClientToIntermission( client );
	}

#ifdef MISSIONPACK
	if (g_singlePlayer.integer) {
		trap_Cvar_Set("ui_singlePlayerActive", "0");
		UpdateTournamentInfo();
	}
#else
	// if single player game
	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		UpdateTournamentInfo();
		SpawnModelsOnVictoryPads();
	}
#endif

	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();
}


/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar 
=============
*/
void ExitLevel( void ) {
	int		i;
	gclient_t *cl;

	//bot interbreeding
	BotInterbreedEndMatch();

	// if we are running a tournement map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( g_gametype.integer == GT_TOURNAMENT  ) {
		if ( !level.restarted ) {
			RemoveTournamentLoser();
			trap_SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			level.intermissiontime = 0;
		}
		return;	
	}

	level.intermissiontime = 0;

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before changing to CON_CONNECTING
	G_WriteSessionData();

	// capture player counts for rotation decisions before we reset states
	level.rotationTotalPlayers = 0;
	level.rotationRedPlayers = 0;
	level.rotationBluePlayers = 0;
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( cl->sess.sessionTeam != TEAM_SPECTATOR ) {
			level.rotationTotalPlayers++;
			if ( cl->sess.sessionTeam == TEAM_RED ) level.rotationRedPlayers++;
			else if ( cl->sess.sessionTeam == TEAM_BLUE ) level.rotationBluePlayers++;
		}
	}

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}

	/* revert temporary vote-overridden cvars on map change */
	{
		qboolean applied = qfalse;
		if ( level.voteTmpRevertOnMapChange ) {
			if ( level.voteTmpRevertCvar[0] && level.voteTmpPrevValue[0] ) {
				trap_Cvar_Set( level.voteTmpRevertCvar, level.voteTmpPrevValue );
				applied = qtrue;
			}
			level.voteTmpRevertOnMapChange = 0;
			level.voteTmpRevertCvar[0] = '\0';
			level.voteTmpPrevValue[0] = '\0';
		}
		/* also check persisted flags (survive map_restart) */
		if ( trap_Cvar_VariableIntegerValue( "g_voteRevertPending" ) ) {
			char cvarName[64];
			char prevVal[64];
			trap_Cvar_VariableStringBuffer( "g_voteRevertCvar", cvarName, sizeof(cvarName) );
			trap_Cvar_VariableStringBuffer( "g_voteRevertValue", prevVal, sizeof(prevVal) );
			if ( cvarName[0] && prevVal[0] ) {
				trap_Cvar_Set( cvarName, prevVal );
				applied = qtrue;
			}
			trap_Cvar_Set( "g_voteRevertPending", "0" );
			trap_Cvar_Set( "g_voteRevertCvar", "" );
			trap_Cvar_Set( "g_voteRevertValue", "" );
		}
		(void)applied;
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


/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[BIG_INFO_STRING];
	int			min, tsec, sec, len, n;

	tsec = level.time / 100;
	sec = tsec / 10;
	tsec %= 10;
	min = sec / 60;
	sec -= min * 60;

	len = Com_sprintf( string, sizeof( string ), "%3i:%02i.%i ", min, sec, tsec );

	va_start( argptr, fmt );
	Q_vsprintf( string + len, fmt,argptr );
	va_end( argptr );

	n = (int)strlen( string );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + len );
	}

	if ( level.logFile == FS_INVALID_HANDLE ) {
		return;
	}

	trap_FS_Write( string, n, level.logFile );
}


/* Helper: write JSON match stats once per match */
void G_WriteMatchStatsJSON( void ) {
    fileHandle_t f;
    char filename[128];
    char *buf;
    int i;
    int first = 1;
    char serverinfo[MAX_INFO_STRING];
    char hostname[128];
    int epoch;

    /* do not write during warmup (only after warmup fully ended) */
    if ( g_gametype.integer != GT_SINGLE_PLAYER && level.warmupTime != 0 ) {
        return;
    }

    /* suppress write if match was aborted due to insufficient players */
    if ( level.abortedDueToNoPlayers ) {
        return;
    }

    if ( level.statsWritten ) {
        return;
    }

    /* allocate a temporary buffer (released when level frees) */
    buf = (char*)G_Alloc( 64 * 1024 );
    if ( !buf ) {
        return;
    }

    {
        int off = 0;
        const int cap = 64 * 1024;
        trap_GetServerinfo( serverinfo, sizeof(serverinfo) );
        {
            const char *h = Info_ValueForKey( serverinfo, "sv_hostname" );
            Q_strncpyz( hostname, (h && *h) ? h : "", sizeof(hostname) );
        }

        epoch = trap_RealTime( NULL );
        Com_sprintf( filename, sizeof(filename), "unite-stats/match_%d_%s.json",
            epoch, g_mapname.string[0] ? g_mapname.string : "unknown" );

        /* build JSON using C89-safe appender */
        JSON_Append( buf, &off, cap, "{\n\t\"timestamp\": %i,\n", level.time );
        JSON_Append( buf, &off, cap, "\t\"map\": \"%s\",\n", g_mapname.string );
        JSON_Append( buf, &off, cap, "\t\"gametype\": %i,\n", g_gametype.integer );
        JSON_Append( buf, &off, cap, "\t\"hostname\": \"%s\",\n", hostname );
        JSON_Append( buf, &off, cap, "\t\"players\": [" );

        for ( i = 0 ; i < level.maxclients ; i++ ) {
            gclient_t *cl = &level.clients[i];
            gentity_t *ent = &g_entities[i];
            if ( cl->pers.connected != CON_CONNECTED ) continue;
            if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) continue;
            if ( ent->r.svFlags & SVF_BOT ) continue; /* skip bots */

            if ( !first ) { JSON_Append( buf, &off, cap, "," ); }
            first = 0;

            {
                char nameEsc[128];
                int j, k; char c;
                for ( j = 0, k = 0; j < (int)sizeof(cl->pers.netname) && cl->pers.netname[j] && k < (int)sizeof(nameEsc) - 2; j++ ) {
                    c = cl->pers.netname[j];
                    if ( c == '\\' || c == '\"' ) { nameEsc[k++] = '\\'; nameEsc[k++] = c; }
                    else if ( (unsigned char)c < 32 ) { nameEsc[k++] = ' '; }
                    else { nameEsc[k++] = c; }
                }
                nameEsc[k] = '\0';
                JSON_Append( buf, &off, cap, "\n\t\t{\"name\": \"%s\", ", nameEsc );
            }

            JSON_Append( buf, &off, cap, "\"team\": %d, ", cl->sess.sessionTeam );
            JSON_Append( buf, &off, cap, "\"score\": %d, ", cl->ps.persistant[PERS_SCORE] );
            JSON_Append( buf, &off, cap, "\"kills\": %d, ", cl->kills );
            JSON_Append( buf, &off, cap, "\"deaths\": %d, ", cl->deaths );
            JSON_Append( buf, &off, cap, "\"suicides\": %d, ", cl->suicides );
            JSON_Append( buf, &off, cap, "\"bestKillStreak\": %d, ", cl->bestKillStreak );
            JSON_Append( buf, &off, cap, "\"hits\": %d, ", cl->accuracy_hits );
            JSON_Append( buf, &off, cap, "\"shots\": %d, ", cl->accuracy_shots );
            JSON_Append( buf, &off, cap, "\"damageGiven\": %d, ", cl->totalDamageGiven );
            JSON_Append( buf, &off, cap, "\"damageTaken\": %d, ", cl->totalDamageTaken );
            JSON_Append( buf, &off, cap, "\"ping\": %d, ", (cl->ps.ping < 999 ? cl->ps.ping : 999) );

            /* per-weapon stats */
            {
                int w;
                int wfirst = 1;
                JSON_Append( buf, &off, cap, "\"weapons\": [" );
                for ( w = 0 ; w < WP_NUM_WEAPONS ; ++w ) {
                    int shots = cl->perWeaponShots[w];
                    int hits = cl->perWeaponHits[w];
                    int kls = cl->perWeaponKills[w];
                    int dths = cl->perWeaponDeaths[w];
                    int dmgG = cl->perWeaponDamageGiven[w];
                    int dmgT = cl->perWeaponDamageTaken[w];
                    int pcks = cl->perWeaponPickups[w];
                    int drps = cl->perWeaponDrops[w];
                    if ( shots || hits || kls || dths || dmgG || dmgT || pcks || drps ) {
                        if ( !wfirst ) { JSON_Append( buf, &off, cap, "," ); }
                        wfirst = 0;
                        JSON_Append( buf, &off, cap, "{\"id\": %d, \"shots\": %d, \"hits\": %d, \"kills\": %d, \"deaths\": %d, \"dmgGiven\": %d, \"dmgTaken\": %d, \"pickups\": %d, \"drops\": %d}",
                            w, shots, hits, kls, dths, dmgG, dmgT, pcks, drps );
                    }
                }
                JSON_Append( buf, &off, cap, "]" );
            }

            JSON_Append( buf, &off, cap, "}" );
        }

        JSON_Append( buf, &off, cap, "\n\t]\n}" );
        if ( off < cap ) buf[off] = '\0';

        if ( trap_FS_FOpenFile( filename, &f, FS_WRITE ) >= 0 && f != FS_INVALID_HANDLE ) {
            trap_FS_Write( buf, (int)strlen(buf), f );
            trap_FS_FCloseFile( f );
            level.statsWritten = 1;
        } else {
            G_Printf("WARNING: could not open %s for writing match stats. Ensure folder exists under fs_homepath.\n", filename);
        }
    }
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;
#ifdef MISSIONPACK
	qboolean won = qtrue;
#endif
	G_LogPrintf( "Exit: %s\n", string );
    /* write JSON match stats for site ingestion (non-bot humans only) */
    {
        fileHandle_t f;
        char filename[128];
        char *buf;
        int i;
        int first = 1;
        char serverinfo[MAX_INFO_STRING];
        char hostname[128];
        int epoch;

        /* allocate a temporary buffer (released when level frees) */
        buf = (char*)G_Alloc( 64 * 1024 );
        if ( buf ) {
            int off = 0;
            const int cap = 64 * 1024;
            trap_GetServerinfo( serverinfo, sizeof(serverinfo) );
            {
                const char *h = Info_ValueForKey( serverinfo, "sv_hostname" );
                Q_strncpyz( hostname, (h && *h) ? h : "", sizeof(hostname) );
            }

            epoch = trap_RealTime( NULL );
            Com_sprintf( filename, sizeof(filename), "unite-stats/match_%d_%s.json",
                epoch, g_mapname.string[0] ? g_mapname.string : "unknown" );

            /* build JSON using C89-safe appender */
            JSON_Append( buf, &off, cap, "{\n\t\"timestamp\": %i,\n", level.time );
            JSON_Append( buf, &off, cap, "\t\"map\": \"%s\",\n", g_mapname.string );
            JSON_Append( buf, &off, cap, "\t\"gametype\": %i,\n", g_gametype.integer );
            JSON_Append( buf, &off, cap, "\t\"hostname\": \"%s\",\n", hostname );
            JSON_Append( buf, &off, cap, "\t\"players\": [" );

            for ( i = 0 ; i < level.maxclients ; i++ ) {
                gclient_t *cl = &level.clients[i];
                gentity_t *ent = &g_entities[i];
                if ( cl->pers.connected != CON_CONNECTED ) continue;
                if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) continue;
                if ( ent->r.svFlags & SVF_BOT ) continue; /* skip bots */

                if ( !first ) { JSON_Append( buf, &off, cap, "," ); }
                first = 0;

                {
                    char nameEsc[128];
                    int j, k; char c;
                    for ( j = 0, k = 0; j < (int)sizeof(cl->pers.netname) && cl->pers.netname[j] && k < (int)sizeof(nameEsc) - 2; j++ ) {
                        c = cl->pers.netname[j];
                        if ( c == '\\' || c == '\"' ) { nameEsc[k++] = '\\'; nameEsc[k++] = c; }
                        else if ( (unsigned char)c < 32 ) { nameEsc[k++] = ' '; }
                        else { nameEsc[k++] = c; }
                    }
                    nameEsc[k] = '\0';
                    JSON_Append( buf, &off, cap, "\n\t\t{\"name\": \"%s\", ", nameEsc );
                }

                JSON_Append( buf, &off, cap, "\"team\": %d, ", cl->sess.sessionTeam );
                JSON_Append( buf, &off, cap, "\"score\": %d, ", cl->ps.persistant[PERS_SCORE] );
                JSON_Append( buf, &off, cap, "\"kills\": %d, ", cl->kills );
                JSON_Append( buf, &off, cap, "\"deaths\": %d, ", cl->deaths );
                JSON_Append( buf, &off, cap, "\"suicides\": %d, ", cl->suicides );
                JSON_Append( buf, &off, cap, "\"bestKillStreak\": %d, ", cl->bestKillStreak );
                JSON_Append( buf, &off, cap, "\"hits\": %d, ", cl->accuracy_hits );
                JSON_Append( buf, &off, cap, "\"shots\": %d, ", cl->accuracy_shots );
                JSON_Append( buf, &off, cap, "\"damageGiven\": %d, ", cl->totalDamageGiven );
                JSON_Append( buf, &off, cap, "\"damageTaken\": %d, ", cl->totalDamageTaken );
                JSON_Append( buf, &off, cap, "\"ping\": %d, ", (cl->ps.ping < 999 ? cl->ps.ping : 999) );

                /* per-weapon stats */
                {
                    int w;
                    int wfirst = 1;
                    JSON_Append( buf, &off, cap, "\"weapons\": [" );
                    for ( w = 0 ; w < WP_NUM_WEAPONS ; ++w ) {
                        int shots = cl->perWeaponShots[w];
                        int hits = cl->perWeaponHits[w];
                        int kls = cl->perWeaponKills[w];
                        int dths = cl->perWeaponDeaths[w];
                        int dmgG = cl->perWeaponDamageGiven[w];
                        int dmgT = cl->perWeaponDamageTaken[w];
                        int pcks = cl->perWeaponPickups[w];
                        int drps = cl->perWeaponDrops[w];
                        if ( shots || hits || kls || dths || dmgG || dmgT || pcks || drps ) {
                            if ( !wfirst ) { JSON_Append( buf, &off, cap, "," ); }
                            wfirst = 0;
                            JSON_Append( buf, &off, cap, "{\"id\": %d, \"shots\": %d, \"hits\": %d, \"kills\": %d, \"deaths\": %d, \"dmgGiven\": %d, \"dmgTaken\": %d, \"pickups\": %d, \"drops\": %d}",
                                w, shots, hits, kls, dths, dmgG, dmgT, pcks, drps );
                        }
                    }
                    JSON_Append( buf, &off, cap, "]" );
                }

                JSON_Append( buf, &off, cap, "}" );
            }

            JSON_Append( buf, &off, cap, "\n\t]\n}" );
            if ( off < cap ) buf[off] = '\0';

            if ( trap_FS_FOpenFile( filename, &f, FS_WRITE ) >= 0 && f != FS_INVALID_HANDLE ) {
                trap_FS_Write( buf, (int)strlen(buf), f );
                trap_FS_FCloseFile( f );
                level.statsWritten = 1;
            } else {
                G_Printf("WARNING: could not open %s for writing match stats. Ensure folder exists under fs_homepath.\n", filename);
            }

            /* no explicit free; G_Alloc memory is cleared on level change */
        }
    }
	level.intermissionQueued = level.time;

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n", cl->ps.persistant[PERS_SCORE], ping, level.sortedClients[i],	cl->pers.netname );
#ifdef MISSIONPACK
		if (g_singlePlayer.integer && g_gametype.integer == GT_TOURNAMENT) {
			if (g_entities[cl - level.clients].r.svFlags & SVF_BOT && cl->ps.persistant[PERS_RANK] == 0) {
				won = qfalse;
			}
		}
#endif

	}

#ifdef MISSIONPACK
	if (g_singlePlayer.integer) {
		if (g_gametype.integer >= GT_CTF) {
			won = level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE];
		}
		trap_SendConsoleCommand( EXEC_APPEND, (won) ? "spWin\n" : "spLose\n" );
	}
#endif

	// /* Show detailed statistics to all players */
	// {
	// 	int i;
	// 	for ( i = 0; i < level.maxclients; i++ ) {
	// 		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
	// 			/* Send scores, awards, and topshots to each connected player */
	// 			trap_SendServerCommand( i, "scores" );
	// 			trap_SendServerCommand( i, "awards" );
	// 			trap_SendServerCommand( i, "topshots" );
	// 		}
	// 	}
	// }

	trap_SendServerCommand( -1, "scores" );
	trap_SendServerCommand( -1, "awards" );
	trap_SendServerCommand( -1, "topshots" );

	/* Show detailed statistics to all players */
	{
		int i;
		for ( i = 0; i < level.maxclients; i++ ) {
			if ( level.clients[i].pers.connected == CON_CONNECTED ) {
				/* Call statistics functions for each connected player */
				Cmd_ScoresText_f( &g_entities[i] );
				Cmd_Awards_f( &g_entities[i] );
				Cmd_Topshots_f( &g_entities[i] );
			}
		}
	}

	/* Show detailed statistics to all players once at match end */
	{
		int ci;
		for ( ci = 0; ci < level.maxclients; ci++ ) {
			if ( level.clients[ci].pers.connected == CON_CONNECTED ) {
				Cmd_ScoresText_f( &g_entities[ci] );
				Cmd_Awards_f( &g_entities[ci] );
				Cmd_Topshots_f( &g_entities[ci] );
			}
		}
		level.statsShown = 1;
	}
}


/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady;
	int			i;
	gclient_t	*cl;
	int			readyMask;

	if ( g_gametype.integer == GT_SINGLE_PLAYER )
		return;

	// see which players are ready (for display only)
	ready = 0;
	notReady = 0;
	readyMask = 0;
	for ( i = 0 ; i < level.maxclients ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			cl->readyToExit = qtrue;
		} 

		if ( cl->readyToExit ) {
			ready++;
			if ( i < 16 ) {
				readyMask |= 1 << i;
			}
		} else {
			notReady++;
		}
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
	for ( i = 0 ; i < level.maxclients ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	}

	// exit after the configured time from intermission start
	{
		int timeout = g_intermissionTime.integer;
		if ( timeout < 1 ) timeout = 1; // minimum 1 second
		if ( level.time < level.intermissiontime + (timeout * 1000) ) {
			return;
		}
	}

	// defer exit while a vote is pending or executing
	if ( level.voteTime || level.voteExecuteTime ) {
		return;
	}

	ExitLevel();
}


/*
=============
ScoreIsTied
=============
*/
static qboolean ScoreIsTied( void ) {
	int		a, b;

	if ( level.numPlayingClients < 2 ) {
		return qfalse;
	}
	
	if ( g_gametype.integer >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
	b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

	return a == b;
}


/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
static void CheckExitRules( void ) {
 	int			i;
	gclient_t	*cl;

	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit();
		return;
	}

	if ( level.intermissionQueued ) {
#ifdef MISSIONPACK
		int time = (g_singlePlayer.integer) ? SP_INTERMISSION_DELAY_TIME : INTERMISSION_DELAY_TIME;
		if ( level.time - level.intermissionQueued >= time ) {
			/* defer intermission while a vote is pending or executing */
			if ( level.voteTime || level.voteExecuteTime ) {
				return;
			}
			level.intermissionQueued = 0;
			BeginIntermission();
		}
#else
		if ( level.time - level.intermissionQueued >= INTERMISSION_DELAY_TIME ) {
			/* defer intermission while a vote is pending or executing */
			if ( level.voteTime || level.voteExecuteTime ) {
				return;
			}
			level.intermissionQueued = 0;
			BeginIntermission();
		}
#endif
		return;
	}

	//freeze
	ftmod_checkDelay();
	//freeze

	// check for sudden death
	if ( ScoreIsTied() ) {
		// always wait for sudden death
		return;
	}

	if ( g_timelimit.integer && !level.warmupTime ) {
		if ( level.time - level.startTime >= g_timelimit.integer*60000 ) {
			G_BroadcastServerCommand( -1, "print \"Timelimit hit.\n\"" );
			LogExit( "Timelimit hit." );
			return;
		}
	}

	if ( level.numPlayingClients < 2 ) {
		/* If match already started (warmup ended), but players left → abort back to warmup without stats */
		if ( g_gametype.integer != GT_SINGLE_PLAYER && level.warmupTime == 0 ) {
			level.warmupTime = -1;
			level.readyCountdownStarted = qfalse;
			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			G_LogPrintf( "Warmup:\n" );
			level.abortedDueToNoPlayers = 1;
		}
		return;
	}

	if ( g_gametype.integer < GT_CTF && g_fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= g_fraglimit.integer ) {
			G_BroadcastServerCommand( -1, "print \"Red hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= g_fraglimit.integer ) {
			G_BroadcastServerCommand( -1, "print \"Blue hit the fraglimit.\n\"" );
			LogExit( "Fraglimit hit." );
			return;
		}

		for ( i = 0 ; i < level.maxclients ; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

			if ( cl->ps.persistant[PERS_SCORE] >= g_fraglimit.integer ) {
				LogExit( "Fraglimit hit." );
				G_BroadcastServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the fraglimit.\n\"",
					cl->pers.netname ) );
				return;
			}
		}
	}
	if (g_capturelimit.integer) {
		if (g_freeze.integer) {
			if (g_gametype.integer >= GT_TEAM) {
				if (level.teamScores[TEAM_RED] >= g_capturelimit.integer) {
					trap_SendServerCommand(-1, "print \"Red hit the capturelimit.\n\"");
					LogExit("Capturelimit hit.");
					return;
				}
				if (level.teamScores[TEAM_BLUE] >= g_capturelimit.integer) {
					trap_SendServerCommand(-1, "print \"Blue hit the capturelimit.\n\"");
					LogExit("Capturelimit hit.");
					return;
				}
			}
		} else {
			if (g_gametype.integer >= GT_CTF) {
				if (level.teamScores[TEAM_RED] >= g_capturelimit.integer) {
					trap_SendServerCommand(-1, "print \"Red hit the capturelimit.\n\"");
					LogExit("Capturelimit hit.");
					return;
				}
				if (level.teamScores[TEAM_BLUE] >= g_capturelimit.integer) {
					trap_SendServerCommand(-1, "print \"Blue hit the capturelimit.\n\"");
					LogExit("Capturelimit hit.");
					return;
				}
			}
		}
	}
}



static void ClearBodyQue( void ) {
	int	i;
	gentity_t	*ent;

	for ( i = 0 ; i < BODY_QUEUE_SIZE ; i++ ) {
		ent = level.bodyQue[ i ];
		if ( ent->r.linked || ent->physicsObject ) {
			trap_UnlinkEntity( ent );
			ent->physicsObject = qfalse;
		}
	}
}


static void G_WarmupEnd( void ) 
{
	gclient_t *client;
	gentity_t *ent;
	int i, t;

	/* if a vote is active, do not start the match; revert to waiting */
	if ( level.voteTime || level.voteExecuteTime ) {
		level.warmupTime = -1;
		level.readyCountdownStarted = qfalse;
		trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
		G_LogPrintf( "Warmup:\n" );
		return;
	}

	// remove corpses
	ClearBodyQue();

	// return flags
	Team_ResetFlags();

	memset( level.teamScores, 0, sizeof( level.teamScores ) );

    level.warmupTime = 0;
    level.readyCountdownStarted = qfalse;
	level.startTime = level.time;
	if ( g_freeze.integer ) {
    	level.freezeRoundStartTime = level.time;
	}
	trap_SetConfigstring( CS_SCORES1, "0" );
	trap_SetConfigstring( CS_SCORES2, "0" );
	trap_SetConfigstring( CS_WARMUP, "" );
	trap_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );

	
	client = level.clients;
	for ( i = 0; i < level.maxclients; i++, client++ ) {
		
		if ( client->pers.connected != CON_CONNECTED )
			continue;

		// reset player awards
		client->ps.persistant[PERS_IMPRESSIVE_COUNT] = 0;
		client->ps.persistant[PERS_EXCELLENT_COUNT] = 0;
		client->ps.persistant[PERS_DEFEND_COUNT] = 0;
		client->ps.persistant[PERS_ASSIST_COUNT] = 0;
		client->ps.persistant[PERS_GAUNTLET_FRAG_COUNT] = 0;

		client->ps.persistant[PERS_SCORE] = 0;
		client->ps.persistant[PERS_CAPTURES] = 0;

		client->ps.persistant[PERS_ATTACKER] = ENTITYNUM_NONE;
		client->ps.persistant[PERS_ATTACKEE_ARMOR] = 0;
		client->damage.enemy = client->damage.team = 0;

		// reset accuracy and extended per-life stats at warmup end
		client->accuracy_hits = 0;
		client->accuracy_shots = 0;
		client->totalDamageGiven = 0;
		client->totalDamageTaken = 0;
		client->kills = 0;
		client->deaths = 0;
		client->suicides = 0;
		client->currentKillStreak = 0;
		client->bestKillStreak = 0;
		client->armorPickedTotal = 0;
		client->armorYACount = 0;
		client->armorRACount = 0;
		client->armorShardCount = 0;
		client->healthPickedTotal = 0;
		client->healthMegaCount = 0;
		client->health50Count = 0;
		client->health25Count = 0;
		client->health5Count = 0;
		{
			int w;
			for ( w = 0 ; w < WP_NUM_WEAPONS ; ++w ) {
				client->perWeaponDamageGiven[w] = 0;
				client->perWeaponDamageTaken[w] = 0;
				client->perWeaponShots[w] = 0;
				client->perWeaponHits[w] = 0;
				client->perWeaponKills[w] = 0;
				client->perWeaponDeaths[w] = 0;
				client->perWeaponPickups[w] = 0;
				client->perWeaponDrops[w] = 0;
			}
		}

		client->ps.stats[STAT_CLIENTS_READY] = 0;
		client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

		memset( &client->ps.powerups, 0, sizeof( client->ps.powerups ) );

		ClientUserinfoChanged( i ); // set max.health etc.

		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			ClientSpawn( level.gentities + i );
		}

		trap_SendServerCommand( i, "map_restart" );
	}

	// respawn items, remove projectiles, etc.
	ent = level.gentities + MAX_CLIENTS;
	for ( i = MAX_CLIENTS; i < level.num_entities ; i++, ent++ ) {

		if ( !ent->inuse || ent->freeAfterEvent )
			continue;

		if ( ent->tag == TAG_DONTSPAWN ) {
			ent->nextthink = 0;
			continue;
		}

		if ( ent->s.eType == ET_ITEM && ent->item ) {

			// already processed in Team_ResetFlags()
			if ( ent->item->giTag == PW_NEUTRALFLAG || ent->item->giTag == PW_REDFLAG || ent->item->giTag == PW_BLUEFLAG )
				continue;

			// remove dropped items
			if ( ent->flags & FL_DROPPED_ITEM ) {
				ent->nextthink = level.time;
				continue;
			}

			// respawn picked up items
			t = SpawnTime( ent, qtrue );
			if ( t != 0 ) {
				// hide items with defined spawn time
				ent->s.eFlags |= EF_NODRAW;
				ent->r.svFlags |= SVF_NOCLIENT;
				ent->r.contents = 0;
				ent->activator = NULL;
				ent->think = RespawnItem;
			} else {
				t = FRAMETIME;
				if ( ent->activator ) {
					ent->activator = NULL;
					ent->think = RespawnItem;
				}
			}
			if ( ent->random ) {
				t += (crandom() * ent->random) * 1000;
				if ( t < FRAMETIME ) {
					t = FRAMETIME;
				}
			}
			ent->nextthink = level.time + t;

		} else if ( ent->s.eType == ET_MISSILE ) {
			// remove all launched missiles
			G_FreeEntity( ent );
		}
	}

	/* Final guard: if countdown expired but players/humans are insufficient, cancel back to warmup */
	if ( g_gametype.integer != GT_SINGLE_PLAYER ) {
		int totalPlayers = 0;
		int totalHumans = 0;
		int requireTwo = trap_Cvar_VariableIntegerValue( "g_requireTwoHumans" );
		for ( i = 0; i < level.maxclients; ++i ) {
			if ( level.clients[i].pers.connected != CON_CONNECTED ) continue;
			if ( level.clients[i].sess.sessionTeam == TEAM_SPECTATOR ) continue;
			totalPlayers++;
			if ( !(g_entities[i].r.svFlags & SVF_BOT) ) totalHumans++;
		}
		if ( (requireTwo && totalHumans < 2) || totalPlayers < 2 ) {
			level.warmupTime = -1;
			level.readyCountdownStarted = qfalse;
			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			G_LogPrintf( "Warmup:\n" );
			return;
		}
	}
}


/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/


/*
=============
CheckTournament

Once a frame, check for changes in tournement player state
=============
*/
static void CheckTournament( void ) {

	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
	if ( level.numPlayingClients == 0 ) {
		return;
	}

	if ( g_gametype.integer == GT_TOURNAMENT ) {

		// pull in a spectator if needed
		if ( level.numPlayingClients < 2 ) {
			AddTournamentPlayer();
		}

		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingClients != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingClients == 2 ) {
				if ( g_warmup.integer > 0 ) {
					level.warmupTime = level.time + g_warmup.integer * 1000;
				} else {
					level.warmupTime = 0;
				}

				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		// cancel countdown if a vote is active
		if ( level.warmupTime > 0 && ( level.voteTime || level.voteExecuteTime ) ) {
			level.warmupTime = -1;
			level.readyCountdownStarted = qfalse;
			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			G_LogPrintf( "Warmup:\n" );
			return;
		}
		if ( level.time > level.warmupTime ) {
			G_WarmupEnd();
			return;
		}
	} else if ( g_gametype.integer != GT_SINGLE_PLAYER && level.warmupTime != 0 ) {
		int		counts[TEAM_NUM_TEAMS];
		qboolean	notEnough = qfalse;

		if ( g_gametype.integer >= GT_TEAM ) {
			counts[TEAM_BLUE] = TeamConnectedCount( -1, TEAM_BLUE );
			counts[TEAM_RED] = TeamConnectedCount( -1, TEAM_RED );

			if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1) {
				notEnough = qtrue;
			}
		} else if ( level.numPlayingClients < 2 ) {
			notEnough = qtrue;
		}
		//freeze
		if (g_freeze.integer)
		{
			if ( !notEnough ) {
				notEnough = ftmod_readyCheck();
			}
		}
		//freeze
		if ( notEnough ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				level.readyCountdownStarted = qfalse;
				trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return; // still waiting for team members
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			/* pause countdown while a vote is active */
			if ( level.voteTime || level.voteExecuteTime ) {
				return;
			}
			if ( g_warmup.integer > 0 ) {
				level.warmupTime = level.time + g_warmup.integer * 1000;
			} else {
				level.warmupTime = 0;
			}

			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			return;
		}

		// if the warmup time has counted down, restart
		// cancel countdown if a vote is active
		if ( level.warmupTime > 0 && ( level.voteTime || level.voteExecuteTime ) ) {
			level.warmupTime = -1;
			level.readyCountdownStarted = qfalse;
			trap_SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			G_LogPrintf( "Warmup:\n" );
			return;
		}
		if ( level.time > level.warmupTime ) {
			G_WarmupEnd();
			return;
		}
	}
}


/*
==================
CheckVote
==================
*/
static void CheckVote( void ) {
	
	if ( level.voteExecuteTime ) {
		 if ( level.voteExecuteTime < level.time ) {
			level.voteExecuteTime = 0;
			trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );
		 }
		 return;
	}

	if ( !level.voteTime ) {
		return;
	}

    if ( level.time - level.voteTime >= (int)(trap_Cvar_VariableIntegerValue("g_voteTime") > 0 ? trap_Cvar_VariableIntegerValue("g_voteTime") * 1000 : VOTE_TIME) ) {
		G_BroadcastServerCommand( -1, "print \"Vote failed.\n\"" );
	} else {
		// ATVI Q3 1.32 Patch #9, WNF
		if ( level.voteYes > level.numVotingClients/2 ) {
			// execute the command, then remove the vote
            G_BroadcastServerCommand( -1, "print \"Vote passed.\n\"" );
            {
                int execDelayMs = (int)trap_Cvar_VariableIntegerValue("g_voteExecuteDelayMs");
                if ( execDelayMs < 0 ) execDelayMs = 0;
                level.voteExecuteTime = level.time + (execDelayMs > 0 ? execDelayMs : 3000);
            }
		} else if ( level.voteNo >= level.numVotingClients/2 ) {
			// same behavior as a timeout
			G_BroadcastServerCommand( -1, "print \"Vote failed.\n\"" );
		} else {
			// still waiting for a majority
			return;
		}
	}

	level.voteTime = 0;
	trap_SetConfigstring( CS_VOTE_TIME, "" );
	
	// if vote was called during intermission, accelerate exit
	if ( level.voteCalledDuringIntermission && level.intermissiontime ) {
		level.voteCalledDuringIntermission = qfalse;
		// force exit after 2 seconds instead of waiting for players to ready
		// (this is faster than the normal intermission timeout)
		level.exitTime = level.time + 2000;
		level.readyToExit = qtrue;
	}
}


/*
==================
PrintTeam
==================
*/
static void PrintTeam( team_t team, const char *message ) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].sess.sessionTeam != team )
			continue;
		if ( level.clients[i].pers.connected != CON_CONNECTED )
			continue;
		trap_SendServerCommand( i, message );
	}
}


/*
==================
SetLeader
==================
*/
void SetLeader( team_t team, int client ) {
	int i;

	if ( level.clients[client].pers.connected == CON_DISCONNECTED ) {
		PrintTeam( team, va("print \"%s "S_COLOR_STRIP"is not connected\n\"", level.clients[client].pers.netname) );
		return;
	}
	if (level.clients[client].sess.sessionTeam != team) {
		PrintTeam( team, va("print \"%s "S_COLOR_STRIP"is not on the team anymore\n\"", level.clients[client].pers.netname) );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader) {
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged( i );
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );
	PrintTeam( team, va("print \"%s is the new team leader\n\"", level.clients[client].pers.netname) );
}


/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( team_t team ) {
	int i;
	int	max_score, max_id;
	int	max_bot_score, max_bot_id;

	for ( i = 0 ; i < level.maxclients ; i++ ) {

		if ( level.clients[i].sess.sessionTeam != team || level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;

		if ( level.clients[i].sess.teamLeader )
			return;
	}

	// no leaders? find player with highest score
	max_score = SHRT_MIN;
	max_id = -1;
	max_bot_score = SHRT_MIN;
	max_bot_id = -1;

	for ( i = 0 ; i < level.maxclients ; i++ ) {

		if ( level.clients[i].sess.sessionTeam != team )
			continue;

		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			if ( level.clients[i].ps.persistant[PERS_SCORE] > max_bot_score ) {
				max_bot_score = level.clients[i].ps.persistant[PERS_SCORE];
				max_bot_id = i;
			}
		} else {
			if ( level.clients[i].ps.persistant[PERS_SCORE] > max_score ) {
				max_score = level.clients[i].ps.persistant[PERS_SCORE];
				max_id = i;
			}
		}
	}

	if ( max_id != -1 ) {
		SetLeader( team, max_id ); 
		return;
	}

	if ( max_bot_id != -1 ) {
		SetLeader( team, max_bot_id );
		return;
	}
}


/*
==================
CheckTeamVote
==================
*/
static void CheckTeamVote( team_t team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}
	if ( level.time - level.teamVoteTime[cs_offset] >= VOTE_TIME ) {
		G_BroadcastServerCommand( -1, "print \"Team vote failed.\n\"" );
	} else {
		if ( level.teamVoteYes[cs_offset] > level.numteamVotingClients[cs_offset]/2 ) {
			// execute the command, then remove the vote
			G_BroadcastServerCommand( -1, "print \"Team vote passed.\n\"" );
			//
			if ( !Q_strncmp( "leader", level.teamVoteString[cs_offset], 6) ) {
				//set the team leader
				SetLeader(team, atoi(level.teamVoteString[cs_offset] + 7));
			}
			else {
				trap_SendConsoleCommand( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ) );
			}
		} else if ( level.teamVoteNo[cs_offset] >= level.numteamVotingClients[cs_offset]/2 ) {
			// same behavior as a timeout
			G_BroadcastServerCommand( -1, "print \"Team vote failed.\n\"" );
		} else {
			// still waiting for a majority
			return;
		}
	}
	level.teamVoteTime[cs_offset] = 0;
	trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );

}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;

	if ( lastMod != g_password.modificationCount ) {
		lastMod = g_password.modificationCount;
		if ( g_password.string[0] && Q_stricmp( g_password.string, "none" ) != 0 ) {
			trap_Cvar_Set( "g_needpass", "1" );
		} else {
			trap_Cvar_Set( "g_needpass", "0" );
		}
	}
}


/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink( gentity_t *ent ) {
	int	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		return;
	}
	if (thinktime > level.time) {
		return;
	}
	
	ent->nextthink = 0;
	if ( !ent->think ) {
		G_Error ( "NULL ent->think");
	} else {
		ent->think (ent);
	}
}


/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
static void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;
	gclient_t	*client;
	static	gentity_t *missiles[ MAX_GENTITIES - MAX_CLIENTS ];
	int		numMissiles;
	
	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;
	level.msec = level.time - level.previousTime;

	// get any cvar changes
	G_UpdateCvars();

	G_UpdateRatFlags();

	numMissiles = 0;

	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	// &= EV_EVENT_BITS;
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
					// predicted events should never be set to zero
					//ent->client->ps.events[0] = 0;
					//ent->client->ps.events[1] = 0;
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				G_FreeEntity( ent );
				continue;
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap_UnlinkEntity( ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
			// queue for unlagged pass
			missiles[ numMissiles ] = ent;
			numMissiles++;
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
			G_RunItem( ent );
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		if ( i < MAX_CLIENTS ) {
			client = ent->client;
			client->sess.spectatorTime += level.msec; 
			if ( client->pers.connected == CON_CONNECTED )
				G_RunClient( ent );
			continue;
		}

		G_RunThink( ent );
	}

	if ( numMissiles ) {
		// unlagged
		G_TimeShiftAllClients( level.previousTime, NULL );
		// run missiles
		for ( i = 0; i < numMissiles; i++ )
			G_RunMissile( missiles[ i ] );
		// unlagged
		G_UnTimeShiftAllClients( NULL );
	}

	// perform final fixups on the players
	ent = &g_entities[0];
	for (i = 0 ; i < level.maxclients ; i++, ent++ ) {
		if ( ent->inuse ) {
			ClientEndFrame( ent );
		}
	}

	CleanupPendingSpawns();

	// see if it is time to do a tournement restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

	// cancel vote if timed out
	CheckVote();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	/* run announcer */
	AN_RunFrame();

	// for tracking changes
	CheckCvars();

	if (g_listEntity.integer) {
		for (i = 0; i < MAX_GENTITIES; i++) {
			G_Printf("%4i: %s\n", i, g_entities[i].classname);
		}
		trap_Cvar_Set("g_listEntity", "0");
	}

	// unlagged
	level.frameStartTime = trap_Milliseconds();

	/* Show detailed statistics to all players */
	/* Removed: do not spam every frame */

	/* Removed duplicate stats-once block: handled inside LogExit */
}
