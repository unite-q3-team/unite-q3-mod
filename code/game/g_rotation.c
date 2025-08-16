// Public Domain

#include "g_local.h"

qboolean G_MapExist( const char *map ) 
{
	fileHandle_t fh;
	int len;

	if ( !map || !*map )
		return qfalse;

	len = trap_FS_FOpenFile( va( "maps/%s.bsp", map ), &fh, FS_READ );

	if ( len < 0 )
		return qfalse;

	trap_FS_FCloseFile( fh );

	return ( len >= 144 ) ? qtrue : qfalse ;
}


void G_LoadMap( const char *map ) 
{
	char cmd[ MAX_CVAR_VALUE_STRING ];
	char ver[ 16 ];
	int version;

	trap_Cvar_VariableStringBuffer( "version", ver, sizeof( ver ) );
	if ( !Q_strncmp( ver, "Q3 1.32 ", 8 ) || !Q_strncmp( ver, "Q3 1.32b ", 9 ) ||
		!Q_strncmp( ver, "Q3 1.32c ", 9 ) ) 
		version = 0; // buggy vanilla binaries
	else
		version = 1;

	if ( !map || !*map || !G_MapExist( map ) || !Q_stricmp( map, g_mapname.string ) ) {
		if ( level.time > 12*60*60*1000 || version == 0 || level.denyMapRestart )
			BG_sprintf( cmd, "map \"%s\"\n", g_mapname.string );
		else
			Q_strcpy( cmd, "map_restart 0\n" );
	} else {
		if ( !G_MapExist( map ) ) // required map doesn't exists, reload existing
			BG_sprintf( cmd, "map \"%s\"\n", g_mapname.string );
		else
			BG_sprintf( cmd, "map \"%s\"\n", map );
	}

	trap_SendConsoleCommand( EXEC_APPEND, cmd );
	level.restarted = qtrue;
}


qboolean ParseMapRotation( void ) 
{
	char buf[ 4096 ];
	char cvar[ 256 ];
	char map[ 256 ];
	char *s;
	fileHandle_t fh;
	int	len;
	char *tk;
	int reqIndex; 
	int curIndex = 0;
	int scopeLevel = 0;

	if ( g_gametype.integer == GT_SINGLE_PLAYER || !g_rotation.string[0] )
		return qfalse;

	len = trap_FS_FOpenFile( g_rotation.string, &fh, FS_READ );
	if ( fh == FS_INVALID_HANDLE ) 
	{
		Com_Printf( S_COLOR_YELLOW "%s: map rotation file doesn't exists.\n", g_rotation.string );
		return qfalse;
	}
	if ( len >= sizeof( buf ) ) 
	{
		Com_Printf( S_COLOR_YELLOW "%s: map rotation file is too big.\n", g_rotation.string );
		len = sizeof( buf ) - 1;
	}
	trap_FS_Read( buf, len, fh );
	buf[ len ] = '\0';
	trap_FS_FCloseFile( fh );
	
	Com_InitSeparators(); // needed for COM_ParseSep()

	/* Count total maps in rotation */
	COM_BeginParseSession( g_rotation.string );
	s = buf;
	curIndex = 0;
	while ( 1 ) {
		tk = COM_ParseSep( &s, qtrue );
		if ( tk[0] == '\0' ) break;
		if ( G_MapExist( tk ) ) {
			curIndex++;
		}
	}
	if ( curIndex == 0 ) {
		Com_Printf( S_COLOR_YELLOW "%s: no maps in rotation file.\n", g_rotation.string );
		trap_Cvar_Set( SV_ROTATION, "1" );
		return qfalse;
	}

	{
		int totalMaps = curIndex;
		int startIndex, tryIndex, attempts;
		char chosenMap[256];
		int chosenIndex = 0;

		startIndex = trap_Cvar_VariableIntegerValue( SV_ROTATION );
		if ( startIndex <= 0 ) startIndex = 1;
		if ( startIndex > totalMaps ) startIndex = 1;

		tryIndex = startIndex;
		for ( attempts = 0; attempts < totalMaps; attempts++ ) {
			int count = 0;
			qboolean found = qfalse;
			char *p;

			/* locate map name at tryIndex */
			COM_BeginParseSession( g_rotation.string );
			p = buf;
			while ( 1 ) {
				tk = COM_ParseSep( &p, qtrue );
				if ( tk[0] == '\0' ) break;
				if ( G_MapExist( tk ) ) {
					count++;
					if ( count == tryIndex ) {
						Q_strncpyz( chosenMap, tk, sizeof( chosenMap ) );
						chosenIndex = tryIndex;
						found = qtrue;
						break;
					}
				}
			}
			if ( !found ) {
				tryIndex = 1;
				continue;
			}

			/* parse constraints for chosenMap */
			{
				int minPlayers = 0;
				int maxPlayers = 9999;
				int minTeam = 0;
				int maxTeam = 9999;
				int depth = 0;
				qboolean afterMap = qfalse;
				qboolean inBlock = qfalse;
				char *q;

				COM_BeginParseSession( g_rotation.string );
				q = buf;
				while ( 1 ) {
					tk = COM_ParseSep( &q, qtrue );
					if ( tk[0] == '\0' ) break;

					if ( !inBlock && G_MapExist( tk ) && !Q_stricmp( tk, chosenMap ) ) {
						afterMap = qtrue;
						continue;
					}

					if ( tk[0] == '{' && tk[1] == '\0' ) {
						depth++;
						if ( depth == 1 && afterMap ) inBlock = qtrue;
						continue;
					}
					if ( tk[0] == '}' && tk[1] == '\0' ) {
						if ( depth > 0 ) depth--;
						if ( inBlock && depth == 0 ) { inBlock = qfalse; afterMap = qfalse; }
						continue;
					}

					if ( inBlock && tk[0] == '@' && tk[1] != '\0' ) {
						char key[64];
						int val;
						Q_strncpyz( key, tk + 1, sizeof( key ) );
						tk = COM_ParseSep( &q, qfalse );
						if ( tk[0] == '=' && tk[1] == '\0' ) {
							tk = COM_ParseSep( &q, qtrue );
							val = atoi( tk );
							if ( !Q_stricmp( key, "minPlayers" ) ) minPlayers = val;
							else if ( !Q_stricmp( key, "maxPlayers" ) ) maxPlayers = val;
							else if ( !Q_stricmp( key, "minTeam" ) ) minTeam = val;
							else if ( !Q_stricmp( key, "maxTeam" ) ) maxTeam = val;
							SkipTillSeparators( &q );
							continue;
						} else {
							COM_ParseWarning( S_COLOR_YELLOW "missing '=' after '@%s'", key );
							SkipRestOfLine( &q );
							continue;
						}
					}
				}

				/* evaluate constraints */
				{
					int total = level.rotationTotalPlayers;
					int red = level.rotationRedPlayers;
					int blue = level.rotationBluePlayers;
					qboolean ok = qtrue;

					if ( total < minPlayers ) ok = qfalse;
					if ( total > maxPlayers ) ok = qfalse;
					if ( g_gametype.integer >= GT_TEAM ) {
						if ( red < minTeam || blue < minTeam ) ok = qfalse;
						if ( red > maxTeam || blue > maxTeam ) ok = qfalse;
					}

					if ( ok ) {
						/* advance rotation index */
						{
							int next = chosenIndex + 1;
							if ( next > totalMaps ) next = 1;
							trap_Cvar_Set( SV_ROTATION, va( "%i", next ) );
						}

						/* apply cvars and load chosen map */
						{
							int depth2 = 0;
							qboolean afterMap2 = qfalse;
							qboolean inBlock2 = qfalse;
							char *r;

							COM_BeginParseSession( g_rotation.string );
							r = buf;
							while ( 1 ) {
								tk = COM_ParseSep( &r, qtrue );
								if ( tk[0] == '\0' ) break;

								if ( G_MapExist( tk ) ) {
									afterMap2 = ( Q_stricmp( tk, chosenMap ) == 0 );
									continue;
								}
								if ( tk[0] == '{' && tk[1] == '\0' ) {
									depth2++;
									if ( depth2 == 1 && afterMap2 ) inBlock2 = qtrue;
									continue;
								}
								if ( tk[0] == '}' && tk[1] == '\0' ) {
									if ( depth2 > 0 ) depth2--;
									if ( inBlock2 && depth2 == 0 ) inBlock2 = qfalse;
									continue;
								}
								if ( tk[0] == '$' && tk[1] != '\0' ) {
									Q_strncpyz( cvar, tk + 1, sizeof( cvar ) );
									tk = COM_ParseSep( &r, qfalse );
									if ( tk[0] == '=' && tk[1] == '\0' ) {
										tk = COM_ParseSep( &r, qtrue );
										if ( depth2 == 0 || inBlock2 ) {
											trap_Cvar_Set( cvar, tk );
										}
										SkipTillSeparators( &r );
										continue;
									} else {
										COM_ParseWarning( S_COLOR_YELLOW "missing '=' after '%s'", cvar );
										SkipRestOfLine( &r );
										continue;
									}
								}
							}

							G_LoadMap( chosenMap );
							return qtrue;
						}
					}
				}
			}

			/* next candidate */
			tryIndex++;
			if ( tryIndex > totalMaps ) tryIndex = 1;
		}

		/* No suitable map found: stay on current map */
		Com_Printf( S_COLOR_YELLOW "Rotation: no suitable map for %i players (R:%i B:%i). Staying on current map.\n",
			level.rotationTotalPlayers, level.rotationRedPlayers, level.rotationBluePlayers );
		/* keep rotation index as-is */
		G_LoadMap( NULL );
		return qtrue;
	}
}
