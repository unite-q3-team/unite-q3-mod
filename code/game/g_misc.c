// Copyright (C) 1999-2000 Id Software, Inc.
//
// g_misc.c

#include "g_local.h"
#include "g_freeze.h"


/*QUAKED func_group (0 0 0) ?
Used to group brushes together just for editor convenience.  They are turned into normal brushes by the utilities.
*/


/*QUAKED info_camp (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_camp( gentity_t *self ) {
	G_SetOrigin( self, self->s.origin );
}


/*QUAKED info_null (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for calculations in the utilities (spotlights, etc), but removed during gameplay.
*/
void SP_info_null( gentity_t *self ) {
	G_FreeEntity( self );
}


/*QUAKED info_notnull (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
target_position does the same thing
*/
void SP_info_notnull( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
}


/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) linear
Non-displayed light.
"light" overrides the default 300 intensity.
Linear checbox gives linear falloff instead of inverse square
Lights pointed at a target will be spotlights.
"radius" overrides the default 64 unit radius of a spotlight at the target point.
*/
void SP_light( gentity_t *self ) {
	G_FreeEntity( self );
}



/*
=================================================================================

TELEPORTERS

=================================================================================
*/

void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	gentity_t	*tent;
	int mode;
	qboolean skipKillbox;
	qboolean killBoth;
	vec3_t finalOrigin;
	vec3_t mins, maxs;
	int touch[MAX_GENTITIES];
	int num, i;
	gentity_t *hit;
	vec3_t dir;
	float radii[4];
	int ri, ai;
	vec3_t cand, off;
	static const float OFF8[8][2] = {
		{1.0f, 0.0f}, {0.70710678f, 0.70710678f}, {0.0f, 1.0f}, {-0.70710678f, 0.70710678f},
		{-1.0f, 0.0f}, {-0.70710678f, -0.70710678f}, {0.0f, -1.0f}, {0.70710678f, -0.70710678f}
	};
	qboolean occupied;

	/* Early check: in WAIT mode (5), if destination is occupied, do nothing (no effects) */
	mode = g_telefragMode.integer;
	if ( mode == 5 ) {
		VectorAdd( origin, player->r.mins, mins );
		VectorAdd( origin, player->r.maxs, maxs );
		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
		occupied = qfalse;
		for ( i = 0; i < num; ++i ) {
			hit = &g_entities[ touch[i] ];
			if ( hit == player ) continue;
			if ( hit->client || ( g_freeze.integer && ftmod_isBodyFrozen( hit ) ) ) { occupied = qtrue; break; }
		}
		if ( occupied ) {
			return;
		}
	}
	/* Early handling for mode 4: try to push occupant(s) away; if still blocked, abort without effects */
	if ( mode == 3 ) {
		VectorAdd( origin, player->r.mins, mins );
		VectorAdd( origin, player->r.maxs, maxs );
		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
		occupied = qfalse;
		for ( i = 0; i < num; ++i ) {
			hit = &g_entities[ touch[i] ];
			if ( hit == player ) continue;
			if ( hit->client ) {
				VectorSubtract( hit->r.currentOrigin, origin, dir );
				if ( VectorNormalize( dir ) == 0.0f ) { dir[0]=0.0f; dir[1]=0.0f; dir[2]=1.0f; }
				VectorMA( hit->client->ps.velocity, 400.0f, dir, hit->client->ps.velocity );
				occupied = qtrue;
			} else if ( g_freeze.integer && ftmod_isBodyFrozen( hit ) ) {
				VectorSubtract( hit->r.currentOrigin, origin, dir );
				if ( VectorNormalize( dir ) == 0.0f ) { dir[0]=0.0f; dir[1]=0.0f; dir[2]=1.0f; }
				VectorMA( hit->s.pos.trDelta, 200.0f, dir, hit->s.pos.trDelta );
				hit->s.pos.trType = TR_GRAVITY;
				hit->s.pos.trTime = level.time;
				occupied = qtrue;
			}
		}
		if ( occupied ) {
			/* after impulse they won't move immediately; treat as WAIT (5) for this frame */
			return;
		}
	}

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
		if (
		(g_freeze.integer && !ftmod_isSpectator(player->client)) ||
		(!g_freeze.integer && player->client->sess.sessionTeam != TEAM_SPECTATOR)
	) {
		tent = G_TempEntity( player->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity( origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = player->s.clientNum;
	}

	/* telefrag mode handling */
	mode = g_telefragMode.integer;
	skipKillbox = qfalse;
	killBoth = qfalse;
	VectorCopy( origin, finalOrigin );
	if ( mode != 0 ) {
		/* occupancy at target */
		VectorAdd( origin, player->r.mins, mins );
		VectorAdd( origin, player->r.maxs, maxs );
		num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
		if ( mode == 5 ) {
			occupied = qfalse;
			for ( i = 0; i < num; ++i ) { hit = &g_entities[ touch[i] ]; if ( hit == player ) continue; if ( hit->client || ( g_freeze.integer && ftmod_isBodyFrozen( hit ) ) ) { occupied = qtrue; break; } }
			if ( occupied ) { return; }
			skipKillbox = qtrue;
		} else if ( mode == 4 ) {
			vec3_t angs;
			/* teleport to random spawnpoint */
			SelectSpawnPoint( player, origin, finalOrigin, angs );
			if ( angles ) { VectorCopy( angs, angles ); }
			skipKillbox = qtrue;
		} else if ( mode == 3 ) {
			/* no-op here; handled early above */
		} else if ( mode == 2 ) {
			radii[0]=16.0f; radii[1]=32.0f; radii[2]=48.0f; radii[3]=64.0f;
			for ( ri = 0; ri < 4; ++ri ) {
				for ( ai = 0; ai < 8; ++ai ) {
					off[0] = OFF8[ai][0] * radii[ri];
					off[1] = OFF8[ai][1] * radii[ri];
					off[2] = 0.0f;
					VectorAdd( origin, off, cand );
					VectorAdd( cand, player->r.mins, mins );
					VectorAdd( cand, player->r.maxs, maxs );
					num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
					if ( num == 0 ) { VectorCopy( cand, finalOrigin ); skipKillbox = qtrue; ri = 4; break; }
				}
			}
		} else if ( mode == 1 ) {
			skipKillbox = qtrue;
		} else if ( mode == 6 ) {
			occupied = qfalse;
			for ( i = 0; i < num; ++i ) { hit = &g_entities[ touch[i] ]; if ( hit == player ) continue; if ( hit->client || ( g_freeze.integer && ftmod_isBodyFrozen( hit ) ) ) { occupied = qtrue; break; } }
			if ( occupied ) { killBoth = qtrue; }
			skipKillbox = qfalse;
		}
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap_UnlinkEntity( player );

	VectorCopy( finalOrigin, player->client->ps.origin );
	player->client->ps.origin[2] += 1.0f;

	// spit the player out
	if ( angles )
		AngleVectors( angles, player->client->ps.velocity, NULL, NULL );

	if ( player->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		VectorScale( player->client->ps.velocity, 1.25f, player->client->ps.velocity );
	} else {
		VectorScale( player->client->ps.velocity, g_speed.value * 1.25f, player->client->ps.velocity );
	}

	player->client->ps.pm_time = 160; // hold time
	player->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// set angles
	if ( angles )
		SetClientViewAngle( player, angles );

	// unlagged
	G_ResetHistory( player );

	// handle telefrag per mode
	if (
		(g_freeze.integer && !ftmod_isSpectator( player->client )) ||
		(!g_freeze.integer && player->client->sess.sessionTeam != TEAM_SPECTATOR)
	) {
		if ( !skipKillbox ) {
			G_KillBox( player );
			/* For frozen bodies: mode 0 and 6 thaw bodies at destination */
			if ( g_freeze.integer && (mode == 0 || mode == 6) ) {
				VectorAdd( finalOrigin, player->r.mins, mins );
				VectorAdd( finalOrigin, player->r.maxs, maxs );
				num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );
				for ( i = 0; i < num; ++i ) {
					hit = &g_entities[ touch[i] ];
					if ( ftmod_isBodyFrozen( hit ) ) {
						if ( hit->target_ent && hit->target_ent->client ) {
							ftmod_bodyFree( hit );
						}
					}
				}
			}
		}
		if ( killBoth ) {
			G_Damage( player, player, player, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
		}
	}

	// save results of pmove
	BG_PlayerStateToEntityState( &player->client->ps, &player->s, qtrue );

	/* drop offhand hook on teleport if enabled */
	if ( g_hook.integer && trap_Cvar_VariableIntegerValue( "g_hook_resetOnTeleport" ) ) {
		if ( player->client->hook ) {
			Weapon_HookFree( player->client->hook );
			player->client->fireHeld = qfalse;
		}
	}

	// use the precise origin for linking
	VectorCopy( player->client->ps.origin, player->r.currentOrigin );

	if (
		(g_freeze.integer && !ftmod_isSpectator( player->client )) ||
		(!g_freeze.integer && player->client->sess.sessionTeam != TEAM_SPECTATOR)
	) {
		trap_LinkEntity( player );
	}

}


/*QUAKED misc_teleporter_dest (1 0 0) (-32 -32 -24) (32 32 -16)
Point teleporters at these.
Now that we don't have teleport destination pads, this is just
an info_notnull
*/
void SP_misc_teleporter_dest( gentity_t *ent ) {
}


//===========================================================

/*QUAKED misc_model (1 0 0) (-16 -16 -16) (16 16 16)
"model"		arbitrary .md3 file to display
*/
void SP_misc_model( gentity_t *ent ) {

#if 0
	ent->s.modelindex = G_ModelIndex( ent->model );
	VectorSet (ent->mins, -16, -16, -16);
	VectorSet (ent->maxs, 16, 16, 16);
	trap_LinkEntity (ent);

	G_SetOrigin( ent, ent->s.origin );
	VectorCopy( ent->s.angles, ent->s.apos.trBase );
#else
	G_FreeEntity( ent );
#endif
}

//===========================================================

void locateCamera( gentity_t *ent ) {
	vec3_t		dir;
	gentity_t	*target;
	gentity_t	*owner;

	owner = G_PickTarget( ent->target );
	if ( !owner ) {
		G_Printf( "Couldn't find target for misc_partal_surface\n" );
		G_FreeEntity( ent );
		return;
	}
	ent->r.ownerNum = owner->s.number;

	// frame holds the rotate speed
	if ( owner->spawnflags & 1 ) {
		ent->s.frame = 25;
	} else if ( owner->spawnflags & 2 ) {
		ent->s.frame = 75;
	}

	// swing camera ?
	if ( owner->spawnflags & 4 ) {
		// set to 0 for no rotation at all
		ent->s.powerups = 0;
	}
	else {
		ent->s.powerups = 1;
	}

	// clientNum holds the rotate offset
	ent->s.clientNum = owner->s.clientNum;

	VectorCopy( owner->s.origin, ent->s.origin2 );

	// see if the portal_camera has a target
	target = G_PickTarget( owner->target );
	if ( target ) {
		VectorSubtract( target->s.origin, owner->s.origin, dir );
		VectorNormalize( dir );
	} else {
		G_SetMovedir( owner->s.angles, dir );
	}

	ent->s.eventParm = DirToByte( dir );
}

/*QUAKED misc_portal_surface (0 0 1) (-8 -8 -8) (8 8 8)
The portal surface nearest this entity will show a view from the targeted misc_portal_camera, or a mirror view if untargeted.
This must be within 64 world units of the surface!
*/
void SP_misc_portal_surface(gentity_t *ent) {
	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	trap_LinkEntity (ent);

	ent->r.svFlags = SVF_PORTAL;
	ent->s.eType = ET_PORTAL;

	if ( !ent->target ) {
		VectorCopy( ent->s.origin, ent->s.origin2 );
	} else {
		ent->think = locateCamera;
		ent->nextthink = level.time + 100;
	}
}

/*QUAKED misc_portal_camera (0 0 1) (-8 -8 -8) (8 8 8) slowrotate fastrotate noswing
The target for a misc_portal_director.  You can set either angles or target another entity to determine the direction of view.
"roll" an angle modifier to orient the camera around the target vector;
*/
void SP_misc_portal_camera(gentity_t *ent) {
	float	roll;

	VectorClear( ent->r.mins );
	VectorClear( ent->r.maxs );
	trap_LinkEntity (ent);

	G_SpawnFloat( "roll", "0", &roll );

	ent->s.clientNum = roll/360.0 * 256;
}

/*
======================================================================

  SHOOTERS

======================================================================
*/

void Use_Shooter( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	vec3_t		dir;
	float		deg;
	vec3_t		up, right;

	// see if we have a target
	if ( ent->enemy ) {
		VectorSubtract( ent->enemy->r.currentOrigin, ent->s.origin, dir );
		VectorNormalize( dir );
	} else {
		VectorCopy( ent->movedir, dir );
	}

	// randomize a bit
	PerpendicularVector( up, dir );
	CrossProduct( up, dir, right );

	deg = crandom() * ent->random;
	VectorMA( dir, deg, up, dir );

	deg = crandom() * ent->random;
	VectorMA( dir, deg, right, dir );

	VectorNormalize( dir );

	switch ( ent->s.weapon ) {
	case WP_GRENADE_LAUNCHER:
		fire_grenade( ent, ent->s.origin, dir );
		break;
	case WP_ROCKET_LAUNCHER:
		fire_rocket( ent, ent->s.origin, dir );
		break;
	case WP_PLASMAGUN:
		fire_plasma( ent, ent->s.origin, dir );
		break;
	}

	G_AddEvent( ent, EV_FIRE_WEAPON, 0 );
}


static void InitShooter_Finish( gentity_t *ent ) {
	ent->enemy = G_PickTarget( ent->target );
	ent->think = 0;
	ent->nextthink = 0;
}

void InitShooter( gentity_t *ent, int weapon ) {
	ent->use = Use_Shooter;
	ent->s.weapon = weapon;

	RegisterItem( BG_FindItemForWeapon( weapon ) );

	G_SetMovedir( ent->s.angles, ent->movedir );

	if ( !ent->random ) {
		ent->random = 1.0;
	}
	ent->random = sin( M_PI * ent->random / 180 );
	// target might be a moving object, so we can't set movedir for it
	if ( ent->target ) {
		ent->think = InitShooter_Finish;
		ent->nextthink = level.time + 500;
	}
	trap_LinkEntity( ent );
}

/*QUAKED shooter_rocket (1 0 0) (-16 -16 -16) (16 16 16)
Fires at either the target or the current direction.
"random" the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_rocket( gentity_t *ent ) {
	InitShooter( ent, WP_ROCKET_LAUNCHER );
}

/*QUAKED shooter_plasma (1 0 0) (-16 -16 -16) (16 16 16)
Fires at either the target or the current direction.
"random" is the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_plasma( gentity_t *ent ) {
	InitShooter( ent, WP_PLASMAGUN);
}

/*QUAKED shooter_grenade (1 0 0) (-16 -16 -16) (16 16 16)
Fires at either the target or the current direction.
"random" is the number of degrees of deviance from the taget. (1.0 default)
*/
void SP_shooter_grenade( gentity_t *ent ) {
	InitShooter( ent, WP_GRENADE_LAUNCHER);
}


#ifdef MISSIONPACK
static void PortalDie (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod) {
	G_FreeEntity( self );
	//FIXME do something more interesting
}


void DropPortalDestination( gentity_t *player ) {
	gentity_t	*ent;
	vec3_t		snapped;

	// create the portal destination
	ent = G_Spawn();
	ent->s.modelindex = G_ModelIndex( "models/powerups/teleporter/tele_exit.md3" );

	VectorCopy( player->s.pos.trBase, snapped );
	SnapVector( snapped );
	G_SetOrigin( ent, snapped );
	VectorCopy( player->r.mins, ent->r.mins );
	VectorCopy( player->r.maxs, ent->r.maxs );

	ent->classname = "hi_portal destination";
	ent->s.pos.trType = TR_STATIONARY;

	ent->r.contents = CONTENTS_CORPSE;
	ent->takedamage = qtrue;
	ent->health = 200;
	ent->die = PortalDie;

	VectorCopy( player->s.apos.trBase, ent->s.angles );

	ent->think = G_FreeEntity;
	ent->nextthink = level.time + 2 * 60 * 1000;

	trap_LinkEntity( ent );

	player->client->portalID = ++level.portalSequence;
	ent->count = player->client->portalID;

	// give the item back so they can drop the source now
	player->client->ps.stats[STAT_HOLDABLE_ITEM] = BG_FindItem( "Portal" ) - bg_itemlist;
}


static void PortalTouch( gentity_t *self, gentity_t *other, trace_t *trace) {
	gentity_t	*destination;

	// see if we will even let other try to use it
	if( other->health <= 0 ) {
		return;
	}
	if( !other->client ) {
		return;
	}
//	if( other->client->ps.persistant[PERS_TEAM] != self->spawnflags ) {
//		return;
//	}

	if ( other->client->ps.powerups[PW_NEUTRALFLAG] ) {		// only happens in One Flag CTF
		Drop_Item( other, BG_FindItemForPowerup( PW_NEUTRALFLAG ), 0 );
		other->client->ps.powerups[PW_NEUTRALFLAG] = 0;
	}
	else if ( other->client->ps.powerups[PW_REDFLAG] ) {		// only happens in standard CTF
		Drop_Item( other, BG_FindItemForPowerup( PW_REDFLAG ), 0 );
		other->client->ps.powerups[PW_REDFLAG] = 0;
	}
	else if ( other->client->ps.powerups[PW_BLUEFLAG] ) {	// only happens in standard CTF
		Drop_Item( other, BG_FindItemForPowerup( PW_BLUEFLAG ), 0 );
		other->client->ps.powerups[PW_BLUEFLAG] = 0;
	}

	// find the destination
	destination = NULL;
	while( (destination = G_Find(destination, FOFS(classname), "hi_portal destination")) != NULL ) {
		if( destination->count == self->count ) {
			break;
		}
	}

	// if there is not one, die!
	if( !destination ) {
		if( self->pos1[0] || self->pos1[1] || self->pos1[2] ) {
			TeleportPlayer( other, self->pos1, self->s.angles );
		}
		G_Damage( other, other, other, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG );
		return;
	}

	TeleportPlayer( other, destination->s.pos.trBase, destination->s.angles );
}


static void PortalEnable( gentity_t *self ) {
	self->touch = PortalTouch;
	self->think = G_FreeEntity;
	self->nextthink = level.time + 2 * 60 * 1000;
}


void DropPortalSource( gentity_t *player ) {
	gentity_t	*ent;
	gentity_t	*destination;
	vec3_t		snapped;

	// create the portal source
	ent = G_Spawn();
	ent->s.modelindex = G_ModelIndex( "models/powerups/teleporter/tele_enter.md3" );

	VectorCopy( player->s.pos.trBase, snapped );
	SnapVector( snapped );
	G_SetOrigin( ent, snapped );
	VectorCopy( player->r.mins, ent->r.mins );
	VectorCopy( player->r.maxs, ent->r.maxs );

	ent->classname = "hi_portal source";
	ent->s.pos.trType = TR_STATIONARY;

	ent->r.contents = CONTENTS_CORPSE | CONTENTS_TRIGGER;
	ent->takedamage = qtrue;
	ent->health = 200;
	ent->die = PortalDie;

	trap_LinkEntity( ent );

	ent->count = player->client->portalID;
	player->client->portalID = 0;

//	ent->spawnflags = player->client->ps.persistant[PERS_TEAM];

	ent->nextthink = level.time + 1000;
	ent->think = PortalEnable;

	// find the destination
	destination = NULL;
	while( (destination = G_Find(destination, FOFS(classname), "hi_portal destination")) != NULL ) {
		if( destination->count == ent->count ) {
			VectorCopy( destination->s.pos.trBase, ent->pos1 );
			break;
		}
	}

}
#endif
