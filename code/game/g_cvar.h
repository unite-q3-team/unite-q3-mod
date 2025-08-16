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
G_CVAR( g_debugFreeze, "g_debugFreeze", "262142", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_motd, "g_motd", "", 0, 0, qfalse, qfalse )
G_CVAR( g_blood, "com_blood", "1", 0, 0, qfalse, qfalse )

G_CVAR( g_podiumDist, "g_podiumDist", "80", 0, 0, qfalse, qfalse )
G_CVAR( g_podiumDrop, "g_podiumDrop", "70", 0, 0, qfalse, qfalse )

G_CVAR( g_allowVote, "g_allowVote", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* allow changing an already cast vote (0 = off, 1 = on) */
G_CVAR( g_allowVoteChange, "g_allowVoteChange", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* maximum number of votes a player can call in a map */
G_CVAR( g_voteLimit, "g_voteLimit", "3", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* maximum number of team votes a player can call in a map */
G_CVAR( g_teamVoteLimit, "g_teamVoteLimit", "3", CVAR_ARCHIVE, 0, qfalse, qfalse )

/* enable loading custom spawns from spawns.txt for current map */
G_CVAR( g_customSpawns, "g_customSpawns", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )

/* enable item replacement rules from itemreplace.txt */
G_CVAR( g_itemReplace, "g_itemReplace", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_listEntity, "g_listEntity", "0", 0, 0, qfalse, qfalse )

G_CVAR( g_unlagged, "g_unlagged", "1", CVAR_SERVERINFO | CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_unlagged_ShiftClientNew, "g_unlagged_ShiftClientNew", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
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
G_CVAR( g_enableBreath, "g_enableBreath", "1", 0, 0, qtrue, qfalse ) // Хуета не работает, на клиенте ни в какую не рисуются breath puffs lol
G_CVAR( g_smoothClients, "g_smoothClients", "1", 0, 0, qfalse, qfalse )
G_CVAR( pmove_fixed, "pmove_fixed", "0", CVAR_SYSTEMINFO, 0, qfalse, qfalse )
G_CVAR( pmove_msec, "pmove_msec", "8", CVAR_SYSTEMINFO, 0, qfalse, qfalse )
G_CVAR( sv_pmove_fixed, "sv_pmove_fixed", "0",0 , 0, qfalse, qfalse )
G_CVAR( sv_pmove_msec, "sv_pmove_msec", "8", 0, 0, qfalse, qfalse )
/* spectators free-fly noclip through world geometry (0=off,1=on) */
G_CVAR( g_specNoclip, "g_specNoclip", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_rotation, "g_rotation", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_selfDamage,     "g_selfDamage",     "1", 0, 0, qfalse, qfalse )
G_CVAR( g_fallDamage,     "g_fallDamage",     "1", 0, 0, qfalse, qfalse )

G_CVAR( g_startWeapon,     "g_startWeapon",     "5", 0, 0, qfalse, qfalse )
G_CVAR( g_startWeapons,     "g_startWeapons",     "511", 0, 0, qfalse, qfalse )

G_CVAR( g_mg_start_ammo,          "g_mg_start_ammo",          "200",  0, 0, qfalse, qfalse )
G_CVAR( g_sg_start_ammo,     "g_sg_start_ammo",     "25",  0, 0, qfalse, qfalse )
G_CVAR( g_gl_start_ammo,     "g_gl_start_ammo",     "15",  0, 0, qfalse, qfalse )
G_CVAR( g_rl_start_ammo,      "g_rl_start_ammo",      "50",  0, 0, qfalse, qfalse )
G_CVAR( g_lg_start_ammo,   "g_lg_start_ammo",   "200",  0, 0, qfalse, qfalse )
G_CVAR( g_rg_start_ammo,     "g_rg_start_ammo",     "25",  0, 0, qfalse, qfalse )
G_CVAR( g_pg_start_ammo,   "g_pg_start_ammo",   "200",  0, 0, qfalse, qfalse )
G_CVAR( g_bfg_start_ammo,         "g_bfg_start_ammo",         "20",  0, 0, qfalse, qfalse )
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

G_CVAR( g_rl_projectileSpeed,      "g_rl_projectileSpeed",      "900",  0, 0, qfalse, qfalse )
G_CVAR( g_pg_projectileSpeed,      "g_pg_projectileSpeed",      "2000",  0, 0, qfalse, qfalse )
G_CVAR( g_bfg_projectileSpeed,      "g_bfg_projectileSpeed",      "2000",  0, 0, qfalse, qfalse )

/* Fun homing toggles per projectile (0/1) */
G_CVAR( g_homing_rl,  "g_homing_rl",  "0", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_pg,  "g_homing_pg",  "0", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_bfg, "g_homing_bfg", "0", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_gl,  "g_homing_gl",  "0", 0, 0, qtrue, qfalse )
/* homing radius (units) */
G_CVAR( g_homing_rl_radius,  "g_homing_rl_radius",  "480", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_pg_radius,  "g_homing_pg_radius",  "480", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_bfg_radius, "g_homing_bfg_radius", "480", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_gl_radius,  "g_homing_gl_radius",  "480", 0, 0, qtrue, qfalse )
/* smart mode: 0 naive, 1 require LoS, 2 simple detour */
G_CVAR( g_homing_rl_smart,  "g_homing_rl_smart",  "1", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_pg_smart,  "g_homing_pg_smart",  "0", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_bfg_smart, "g_homing_bfg_smart", "0", 0, 0, qtrue, qfalse )
G_CVAR( g_homing_gl_smart,  "g_homing_gl_smart",  "0", 0, 0, qtrue, qfalse )

/* ricochet counts per-weapon (number of bounces off world geometry) */
// hitscan ricochet controls removed
// G_CVAR( g_ricochet_mg,  "g_ricochet_mg",  "0", 0, 0, qtrue, qfalse )
// G_CVAR( g_ricochet_sg,  "g_ricochet_sg",  "0", 0, 0, qtrue, qfalse )
// G_CVAR( g_ricochet_lg,  "g_ricochet_lg",  "0", 0, 0, qtrue, qfalse )
// G_CVAR( g_ricochet_rg,  "g_ricochet_rg",  "0", 0, 0, qtrue, qfalse )
/* -1 = unlimited (preserve classic grenade bouncing), 0 = no extra ricochets, N > 0 = limited */
G_CVAR( g_ricochet_gl,  "g_ricochet_gl",  "-1", 0, 0, qtrue, qfalse )
G_CVAR( g_ricochet_rl,  "g_ricochet_rl",  "0", 0, 0, qtrue, qfalse )
G_CVAR( g_ricochet_pg,  "g_ricochet_pg",  "0", 0, 0, qtrue, qfalse )
G_CVAR( g_ricochet_bfg, "g_ricochet_bfg", "0", 0, 0, qtrue, qfalse )

G_CVAR( g_gauntlet_fireRatio,    "g_gauntlet_fireRatio",    "400",  0, 0, qfalse, qfalse )
G_CVAR( g_lg_fireRatio,   "g_lg_fireRatio",   "50",   0, 0, qfalse, qfalse )
G_CVAR( g_sg_fireRatio,     "g_sg_fireRatio",     "1000", 0, 0, qfalse, qfalse )
G_CVAR( g_mg_fireRatio,           "g_mg_fireRatio",          "100",  0, 0, qfalse, qfalse )
G_CVAR( g_gl_fireRatio,           "g_gl_fireRatio",          "800",  0, 0, qfalse, qfalse )
G_CVAR( g_rl_fireRatio,           "g_rl_fireRatio",          "800",  0, 0, qfalse, qfalse )
G_CVAR( g_pg_fireRatio,           "g_pg_fireRatio",          "100",  0, 0, qfalse, qfalse )
G_CVAR( g_rg_fireRatio,           "g_rg_fireRatio",          "1500", 0, 0, qfalse, qfalse )
G_CVAR( g_bfg_fireRatio,           "g_bfg_fireRatio",          "200", 0, 0, qfalse, qfalse )

G_CVAR( g_vampire, "g_vampire", "0.0", CVAR_NORESTART, 0, qtrue, qfalse )
G_CVAR( g_vampireMaxHealth, "g_vampire_max_health", "500", CVAR_NORESTART, 0, qtrue, qfalse )

G_CVAR( g_instagib, "g_instagib", "0", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_railJump, 			"g_railJump", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )

G_CVAR( g_items_deathDrop, "g_items_deathDrop", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_dropWeaponOnDeath, "g_dropWeaponOnDeath", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_dropPowerupsOnDeath, "g_dropPowerupsOnDeath", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_spawn_items, "g_spawn_items", "63", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_spawn_quad, "g_spawn_quad", "1", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_spawn_weapons, "g_spawn_weapons", "1023", CVAR_SERVERINFO | CVAR_LATCH, 0, qtrue, qfalse )

G_CVAR( g_spawnProtect, "g_spawnProtect", "1", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_spawnProtectUseBS, "g_spawnProtectUseBS", "1", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_spawnProtectTime, "g_spawnProtectTime", "1000", CVAR_ARCHIVE, 0, qfalse, qfalse )

/* vote revert persistence across map_restart */
G_CVAR( g_voteRevertPending, "g_voteRevertPending", "0", 0, 0, qfalse, qfalse )
G_CVAR( g_voteRevertCvar, "g_voteRevertCvar", "", 0, 0, qfalse, qfalse )
G_CVAR( g_voteRevertValue, "g_voteRevertValue", "", 0, 0, qfalse, qfalse )

/* client precache helpers to avoid hitches when granting weapons on spawn */
G_CVAR( g_precacheStartWeapons, "g_precacheStartWeapons", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_precacheAllWeapons,   "g_precacheAllWeapons",   "0", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_precacheAllItems,     "g_precacheAllItems",     "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_damageThroughWalls, "g_damageThroughWalls", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )
//ratmod
G_CVAR( g_altFlags, "g_altFlags", "0", CVAR_SERVERINFO, 0, qfalse, qfalse )

G_CVAR( g_overbounce,  "g_overbounce", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

//Devotion
G_CVAR( pmove_autohop, "pmove_autohop", "0", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( pmove_accurate, "pmove_accurate", "0", CVAR_SYSTEMINFO | CVAR_ARCHIVE, 0, qtrue, qfalse )
// G_CVAR( g_vulnerableMissiles, "g_vulnerableMissiles", "0", CVAR_SERVERINFO | CVAR_ARCHIVE| CVAR_NORESTART, 0, qfalse, qfalse )

/* Server-side free grappling hook controls */
G_CVAR( g_hook, "g_hook", "1", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_hook_speed, "g_hook_speed", "800", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_hook_pullSpeed, "g_hook_pullSpeed", "800", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_hook_maxDist, "g_hook_maxDist", "1200", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_hook_visiblePull, "g_hook_visiblePull", "1", CVAR_ARCHIVE, 0, qtrue, qfalse )
G_CVAR( g_hook_allowPlayers, "g_hook_allowPlayers", "1", CVAR_ARCHIVE, 0, qtrue, qfalse )

//freeze
G_CVAR( g_freeze, "g_freeze", "0", CVAR_SERVERINFO | CVAR_USERINFO | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_thawTime, "g_thawTime", "2", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_thawTimeAutoRevive, "g_thawTimeAutoRevive", "90", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_thawTimeAuto_lava, "g_thawTimeAuto_lava", "10", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_thawTimeAuto_bounds, "g_thawTimeAuto_bounds", "10", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_thawTimeAuto_tp, "g_thawTimeAuto_tp", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_thawRadius, "g_thawRadius", "100", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_freeze_forceRevive, "g_freeze_forceRevive", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_freeze_beginFrozen, "g_freeze_beginFrozen", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_freeze_beginFrozenDelay, "g_freeze_beginFrozenDelay", "3", CVAR_ARCHIVE, 0, qfalse, qfalse )

// Spectating behavior in team modes
// 1 = allow following enemies (default, current behavior), 0 = restrict follow to teammates
G_CVAR( g_teamAllowEnemySpectate, "g_teamAllowEnemySpectate", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
// 1 = automatically follow a teammate upon death (freeze-style spectator), 0 = remain in free spectator
G_CVAR( g_autoFollowTeammate, "g_autoFollowTeammate", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )
// 1 = disallow free-fly spectator movement for dead players in team modes; only follow allowed
G_CVAR( g_teamNoFreeSpectate, "g_teamNoFreeSpectate", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )

// G_CVAR( g_freezeRespawnInplace, "g_freezeRespawnInplace", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
// G_CVAR( g_freezeHealth, "g_freezeHealth", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )
// G_CVAR( g_freezeKnockback, "g_freezeKnockback", "1000", CVAR_ARCHIVE, 0, qfalse, qfalse )
// G_CVAR( g_freezeBounce, "g_freezeBounce", "0.4", CVAR_ARCHIVE, 0, qfalse, qfalse )
// G_CVAR( g_thawTimeDestroyedRemnant, "g_thawTimeDestroyedRemnant", "2", CVAR_ARCHIVE, 0, qfalse, qfalse )
// G_CVAR( g_thawTimeDied, "g_thawTimeDied", "60", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_newSpawns, "g_newSpawns", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )

G_CVAR( g_doReady, "g_doReady", "0", CVAR_ARCHIVE, 0, qtrue, qfalse )

G_CVAR( _ath, "_ath", "", CVAR_ARCHIVE, 0, qfalse, qfalse )

//XQ3E
G_CVAR( g_x_drawDamage, "g_x_drawDamage", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_x_unfreezeFoe, "g_x_unfreezeFoe", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue, qfalse )
G_CVAR( g_x_drawHitbox, "g_x_drawHitbox", "1", CVAR_ARCHIVE | CVAR_LATCH, 0, qtrue, qfalse )

/* Bot behavior toggles for FreezeTag support */
G_CVAR( g_botRescueFrozen, "g_botRescueFrozen", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_botShoveBodies,  "g_botShoveBodies",  "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* 0 = off, 1 = high priority (current), 2 = override everything (suicidal rescue) */
G_CVAR( g_botRescuePriority, "g_botRescuePriority", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )

/* announcer */
G_CVAR( g_announcer, "g_announcer", "1", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* seconds between announcements */
G_CVAR( g_announcer_interval, "g_announcer_interval", "180", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* 0 = sequential order, 1 = random */
G_CVAR( g_announcer_order, "g_announcer_order", "0", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* file name for announcements (optional) */
G_CVAR( g_announcer_file, "g_announcer_file", "announcements.txt", CVAR_ARCHIVE, 0, qfalse, qfalse )
/* join texts */
G_CVAR( g_joinMessage, "g_joinMessage", "", CVAR_ARCHIVE, 0, qfalse, qfalse )
G_CVAR( g_joinCenter, "g_joinCenter", "", CVAR_ARCHIVE, 0, qfalse, qfalse )


#undef G_CVAR
