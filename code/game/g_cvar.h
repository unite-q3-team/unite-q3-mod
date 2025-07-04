#ifdef EXTERN_G_CVAR
	#define G_CVAR( vmCvar, cvarName, defaultString, cvarFlags, modificationCount, trackChange, teamShader ) extern vmCvar_t vmCvar;
#endif

#ifdef DECLARE_G_CVAR
	#define G_CVAR( vmCvar, cvarName, defaultString, cvarFlags, modificationCount, trackChange, teamShader ) vmCvar_t vmCvar;
#endif

#ifdef G_CVAR_LIST
	#define G_CVAR( vmCvar, cvarName, defaultString, cvarFlags, modificationCount, trackChange, teamShader ) { & vmCvar, cvarName, defaultString, cvarFlags, modificationCount, trackChange, teamShader },
#endif

// don't override the cheat state set by the system
G_CVAR( g_cheats, "sv_cheats", "", 0, 0, qfalse, qfalse )

//G_CVAR( g_restarted, "g_restarted", "0", CVAR_ROM, 0, qfalse, qfalse )
G_CVAR( g_mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse, qfalse )
G_CVAR( sv_fps, "sv_fps", "30", CVAR_ARCHIVE, 0, qfalse, qfalse )

// latched vars
G_CVAR( g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH, 0, qfalse, qfalse )

G_CVAR( g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse, qfalse ) // allow this many total, including spectators
G_CVAR( g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, 0, qfalse, qfalse ) // allow this many active

// change anytime vars
G_CVAR( g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue, qfalse )
G_CVAR( g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue, qfalse )
G_CVAR( g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, 0, qtrue, qfalse )

G_CVAR( g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, 0, qfalse, qfalse )

G_CVAR( g_friendlyFire, "g_friendlyFire", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )

G_CVAR( g_autoJoin, "g_autoJoin", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_teamForceBalance, "g_teamForceBalance", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_warmup, "g_warmup", "20", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_log, "g_log", "games.log", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_logSync, "g_logSync", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_password, "g_password", "", CVAR_USERINFO, 0, qfalse, qfalse )

G_CVAR( g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, 0, qfalse, qfalse )

G_CVAR( g_dedicated, "dedicated", "0", 0, 0, qfalse, qfalse )

G_CVAR( g_speed, "g_speed", "320", 0, 0, qtrue, qfalse )
G_CVAR( g_gravity, "g_gravity", "800", 0, 0, qtrue, qfalse )
G_CVAR( g_knockback, "g_knockback", "1000", 0, 0, qtrue, qfalse )
G_CVAR( g_quadfactor, "g_quadfactor", "3", 0, 0, qtrue, qfalse )
G_CVAR( g_weaponRespawn, "g_weaponrespawn", "5", 0, 0, qtrue, qfalse )
G_CVAR( g_weaponTeamRespawn, "g_weaponTeamRespawn", "30", 0, 0, qtrue, qfalse )
G_CVAR( g_forcerespawn, "g_forcerespawn", "20", 0, 0, qtrue, qfalse )
G_CVAR( g_inactivity, "g_inactivity", "0", 0, 0, qtrue, qfalse )
G_CVAR( g_teamChangeCooldown, "g_teamChangeCooldown", "5000", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_debugMove, "g_debugMove", "0", 0, 0, qfalse, qfalse )
G_CVAR( g_debugDamage, "g_debugDamage", "0", 0, 0, qfalse, qfalse )
G_CVAR( g_debugAlloc, "g_debugAlloc", "0", 0, 0, qfalse, qfalse )
G_CVAR( g_motd, "g_motd", "", 0, 0, qfalse, qfalse )
G_CVAR( g_blood, "com_blood", "1", 0, 0, qfalse, qfalse )

G_CVAR( g_podiumDist, "g_podiumDist", "80", 0, 0, qfalse, qfalse )
G_CVAR( g_podiumDrop, "g_podiumDrop", "70", 0, 0, qfalse, qfalse )

G_CVAR( g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_listEntity, "g_listEntity", "0", 0, 0, qfalse, qfalse )

G_CVAR( g_unlagged, "g_unlagged", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_predictPVS, "g_predictPVS", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

// should ve print "Kill:" and "Item:" strings into console
G_CVAR( dbg_events, "dbg_events", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

#ifdef MISSIONPACK
G_CVAR( g_obeliskHealth, "g_obeliskHealth", "2500", 0, 0, qfalse, qfalse )
G_CVAR( g_obeliskRegenPeriod, "g_obeliskRegenPeriod", "1", 0, 0, qfalse, qfalse )
G_CVAR( g_obeliskRegenAmount, "g_obeliskRegenAmount", "15", 0, 0, qfalse, qfalse )
G_CVAR( g_obeliskRespawnDelay, "g_obeliskRespawnDelay", "10", CVAR_SERVERINFO, 0, qfalse, qfalse )

G_CVAR( g_cubeTimeout, "g_cubeTimeout", "30", 0, 0, qfalse, qfalse )
G_CVAR( g_redteam, "g_redteam", "Stroggs", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_USERINFO, 0, qtrue, qtrue )
G_CVAR( g_blueteam, "g_blueteam", "Pagans", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_USERINFO, 0, qtrue, qtrue )
G_CVAR( g_singlePlayer, "ui_singlePlayerActive", "", 0, 0, qfalse, qfalse )

G_CVAR( g_enableDust, "g_enableDust", "0", CVAR_SERVERINFO, 0, qtrue, qfalse )
G_CVAR( g_proxMineTimeout, "g_proxMineTimeout", "20000", 0, 0, qfalse, qfalse )
#endif
// G_CVAR( g_enableBreath, "g_enableBreath", "1", 0, 0, qtrue, qfalse ) // Хуета не работает, на клиенте ни в какую не рисуются breath puffs lol
G_CVAR( g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse, qfalse )
G_CVAR( pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO, 0, qfalse, qfalse )
G_CVAR( pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO, 0, qfalse, qfalse )
G_CVAR( sv_pmove_fixed, "sv_pmove_fixed", "0",0 , 0, qfalse, qfalse )
G_CVAR( sv_pmove_msec, "sv_pmove_msec", "8", 0, 0, qfalse, qfalse )

G_CVAR( g_rotation, "g_rotation", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_selfDamage,     "g_selfDamage",     "1", 0, 0, qfalse, qfalse )
G_CVAR( g_startWeapon,     "g_startWeapon",     "5", 0, 0, qfalse, qfalse )
G_CVAR( g_startWeapons,     "g_startWeapons",     "1023", 0, 0, qfalse, qfalse )


G_CVAR( g_start_ammo_mg,          "g_start_ammo_mg",          "200",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_shotgun,     "g_start_ammo_shotgun",     "25",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_grenade,     "g_start_ammo_grenade",     "15",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_rocket,      "g_start_ammo_rocket",      "50",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_lightning,   "g_start_ammo_lightning",   "400",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_railgun,     "g_start_ammo_railgun",     "25",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_plasmagun,   "g_start_ammo_plasmagun",   "200",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_bfg,         "g_start_ammo_bfg",         "20",  0, 0, qfalse, qfalse )
#ifdef MISSIONPACK
G_CVAR( g_start_ammo_nailgun,     "g_start_ammo_nailgun",     "100",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_proxlauncher,"g_start_ammo_proxlauncher","100",  0, 0, qfalse, qfalse )
G_CVAR( g_start_ammo_chaingun,    "g_start_ammo_chaingun",    "100",  0, 0, qfalse, qfalse )
#endif
G_CVAR( g_start_health,   		"g_start_health",   "125",  0, 0, qfalse, qfalse )
G_CVAR( g_start_armor,         "g_start_armor",         "100",  0, 0, qfalse, qfalse )

G_CVAR( g_gauntlet_damage,   "g_gauntlet_damage",   "50",   0, 0, qfalse, qfalse )
G_CVAR( g_mg_damage,          "g_mg_damage",          "7",  0, 0, qfalse, qfalse )
G_CVAR( g_mg_damageTeam,          "g_mg_damageTeam",          "5",  0, 0, qfalse, qfalse )
G_CVAR( g_sg_damage,     "g_sg_damage",     "10",  0, 0, qfalse, qfalse )
G_CVAR( g_gl_damage,     "g_gl_damage",     "100",  0, 0, qfalse, qfalse )
G_CVAR( g_rl_damage,      "g_rl_damage",      "100",  0, 0, qfalse, qfalse )
G_CVAR( g_lg_damage,   "g_lg_damage",   "8",  0, 0, qfalse, qfalse )
G_CVAR( g_rg_damage,     "g_rg_damage",     "100",  0, 0, qfalse, qfalse )
G_CVAR( g_pg_damage,   "g_pg_damage",   "20",  0, 0, qfalse, qfalse )
G_CVAR( g_bfg_damage,         "g_bfg_damage",         "100",  0, 0, qfalse, qfalse )

G_CVAR( g_vampire, "g_vampire", "0.0", CVAR_NORESTART, 0, qtrue, qfalse )
G_CVAR( g_vampireMaxHealth, "g_vampire_max_health", "500", CVAR_NORESTART, 0, qtrue, qfalse )

G_CVAR( g_instagib, "g_instagib", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_railJump, 			"g_railJump", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )

G_CVAR( g_items, "g_items", "0", CVAR_ARCHIVE | CVAR_LATCH, 0, qfalse, qfalse )
G_CVAR( g_spawnItems, "g_spawnItems", "1", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue, qfalse )

G_CVAR( g_damageThroughWalls, "g_damageThroughWalls", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )

// G_CVAR( g_vulnerableMissiles, "g_vulnerableMissiles", "0", CVAR_SERVERINFO | CVAR_ARCHIVE| CVAR_NORESTART, 0, qfalse, qfalse )

// ... до WP_NUM_WEAPONS - 1



#undef G_CVAR
