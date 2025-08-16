// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"

// g_client.c -- client functions that don't happen every frame

const vec3_t	playerMins = {-15, -15, -24};
const vec3_t	playerMaxs = { 15,  15,  32};

static char	ban_reason[MAX_CVAR_VALUE_STRING];

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32) initial
potential spawning position for deathmatch games.
The first time a player enters the game, they will be at an 'initial' spot.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
*/
void SP_info_player_deathmatch( gentity_t *ent ) {
	int		i;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
equivelant to info_player_deathmatch
*/
void SP_info_player_start(gentity_t *ent) {
	ent->classname = "info_player_deathmatch";
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_player_intermission( gentity_t *ent ) {

}

void SetPlayerBox( gentity_t *ent ) {
    VectorSet( ent->r.mins, -15, -15, -24 );
    VectorSet( ent->r.maxs, 15, 15, 32 );
}


/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag( gentity_t *spot ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( spot->s.origin, playerMins, mins );
	VectorAdd( spot->s.origin, playerMaxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		//if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
		if ( hit->client) {
			return qtrue;
		}

	}

	return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectNearestDeathmatchSpawnPoint( vec3_t from ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist, nearestDist;
	gentity_t	*nearestSpot;

	nearestDist = 999999;
	nearestSpot = NULL;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {

		VectorSubtract( spot->s.origin, from, delta );
		dist = VectorLength( delta );
		if ( dist < nearestDist ) {
			nearestDist = dist;
			nearestSpot = spot;
		}
	}

	return nearestSpot;
}


/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
gentity_t *SelectRandomDeathmatchSpawnPoint( void ) {
	gentity_t	*spot;
	int			count;
	int			selection;
	gentity_t	*spots[MAX_SPAWN_POINTS];

	count = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}
		spots[ count ] = spot;
		count++;
	}

	if ( !count ) {	// no spots that won't telefrag
		return G_Find( NULL, FOFS(classname), "info_player_deathmatch");
	}

	selection = rand() % count;
	return spots[ selection ];
}

/*
===========
SelectRandomFurthestSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
static gentity_t *SelectRandomFurthestSpawnPoint( const gentity_t *ent, vec3_t avoidPoint, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist;
	float		list_dist[MAX_SPAWN_POINTS];
	gentity_t	*list_spot[MAX_SPAWN_POINTS];
	int			numSpots, i, j, n;
	int			selection;
	int			checkTelefrag;
	int			checkType;
	int			checkMask;
	qboolean	isBot;

	checkType = qtrue;
	checkTelefrag = qtrue;

	if ( ent )
		isBot = ((ent->r.svFlags & SVF_BOT) == SVF_BOT); 
	else
		isBot = qfalse;

	checkMask = 3;

__search:

	checkTelefrag = checkMask & 1;
	checkType = checkMask & 2;

	numSpots = 0;
	for ( n = 0 ; n < level.numSpawnSpots ; n++ ) {
		spot = level.spawnSpots[n];

		if ( spot->fteam != TEAM_FREE && level.numSpawnSpotsFFA > 0 )
			continue;

		if ( checkTelefrag && SpotWouldTelefrag( spot ) )
			continue;

		if ( checkType ) 
		{
			if ( (spot->flags & FL_NO_BOTS) && isBot )
				continue;
			if ( (spot->flags & FL_NO_HUMANS) && !isBot )
				continue;
		}

		VectorSubtract( spot->s.origin, avoidPoint, delta );
		dist = VectorLength( delta );

		for ( i = 0; i < numSpots; i++ )
		{
			if( dist > list_dist[i] )
			{
				if (numSpots >= MAX_SPAWN_POINTS)
					numSpots = MAX_SPAWN_POINTS - 1;

				for( j = numSpots; j > i; j-- )
				{
					list_dist[j] = list_dist[j-1];
					list_spot[j] = list_spot[j-1];
				}

				list_dist[i] = dist;
				list_spot[i] = spot;

				numSpots++;
				break;
			}
		}

		if(i >= numSpots && numSpots < MAX_SPAWN_POINTS)
		{
			list_dist[numSpots] = dist;
			list_spot[numSpots] = spot;
			numSpots++;
		}
	}

	if ( !numSpots ) {
		if ( checkMask <= 0 ) {
			G_Error( "Couldn't find a spawn point" );
			return NULL;
		}
		checkMask--;
		goto __search; // next attempt with different flags
	}

	// select a random spot from the spawn points furthest away
	selection = random() * (numSpots / 2);
	spot = list_spot[ selection ];

	VectorCopy( spot->s.angles, angles );
	VectorCopy( spot->s.origin, origin );
	origin[2] += 9.0f;

	return spot;
}

// TODO - структурировать, возможно перенести в другой файл.
typedef struct {
    vec3_t origin;
    int team;
    int expireTime;
} PendingSpawn;

#define MAX_PENDING_SPAWNS 64
#define PENDING_SPAWN_DURATION 1000
#define PENDING_DIST_MIN     256.0f
static PendingSpawn pendingSpawns[MAX_PENDING_SPAWNS];
static int pendingSpawnCount = 0;
#define PENDING_EXPIRE_DELAY 50
#define MAX_CANDIDATES      MAX_SPAWN_POINTS
#define ENEMY_DIST_THRESH   1024.0f
#define ALLY_DIST_THRESH     512.0f
#define HUGE_DIST        9999999.0f

static qboolean IsSpotBlockedByPendingSpawns(const vec3_t spotOrigin, int team) {
    int i;
    vec3_t delta;
    for (i = 0; i < pendingSpawnCount; i++) {
        if (pendingSpawns[i].expireTime <= level.time) {
            continue;
        }
        VectorSubtract(spotOrigin, pendingSpawns[i].origin, delta);
        if (VectorLength(delta) < PENDING_DIST_MIN) {
            if (pendingSpawns[i].team != team) {
                return qtrue;
            }
        }
    }
    return qfalse;
}

void AddPendingSpawn(vec3_t origin, int team) {
    if (pendingSpawnCount < MAX_PENDING_SPAWNS) {
        VectorCopy(origin, pendingSpawns[pendingSpawnCount].origin);
        pendingSpawns[pendingSpawnCount].team = team;
        pendingSpawns[pendingSpawnCount].expireTime = level.time + PENDING_SPAWN_DURATION;
        pendingSpawnCount++;
    }
}

void RemovePendingSpawn(vec3_t origin) {
    int i, j;

    for (i = 0; i < pendingSpawnCount; i++) {
        if (VectorCompare(pendingSpawns[i].origin, origin)) {
            for (j = i; j < pendingSpawnCount - 1; j++) {
                pendingSpawns[j] = pendingSpawns[j + 1];
            }
            pendingSpawnCount--;
            break;
        }
    }
}

void CleanupPendingSpawns(void) {
    int i = 0;
    while (i < pendingSpawnCount) {
        if (pendingSpawns[i].expireTime <= level.time) {
            RemovePendingSpawn(pendingSpawns[i].origin);
        } else {
            i++;
        }
    }
}

static void SetupSpawn(gentity_t *spot, vec3_t origin, vec3_t angles, int team)
{
    VectorCopy(spot->s.origin, origin);
    origin[2] += 9.0f;
    VectorCopy(spot->s.angles, angles);
    AddPendingSpawn(origin, team);
}

static qboolean IsValidSpot(gentity_t *spot, qboolean isBot) {
    if (SpotWouldTelefrag(spot)) return qfalse;
    if ((spot->flags & FL_NO_BOTS) && isBot) return qfalse;
    if ((spot->flags & FL_NO_HUMANS) && !isBot) return qfalse;
    return qtrue;
}

static gentity_t *PickRandomSpot(void) {
    if (level.numSpawnSpots <= 0) return NULL;
    return level.spawnSpots[rand() % level.numSpawnSpots];
}

static gentity_t *SelectFurthestSpawnFromEnemies(const gentity_t *ent, vec3_t origin, vec3_t angles)
{
    qboolean isBot;
    int i, j, team;
    gentity_t *spot;
    gentity_t *safeSpots[MAX_CANDIDATES];
    gentity_t *fallbackSpots[MAX_CANDIDATES];
    gentity_t *otherSpots[MAX_CANDIDATES];
    int safeCount = 0;
    int fallbackCount = 0;
    int otherCount = 0;
    vec3_t delta;
    float minEnemyDist, minAllyDist, dist;
    int livingAllies = 0;
    int livingEnemies = 0;

    isBot = (ent && ent->client && (ent->r.svFlags & SVF_BOT));
    team = (ent && ent->client) ? ent->client->sess.sessionTeam : TEAM_FREE;

    if (!ent || !ent->client) {
        spot = PickRandomSpot();
        if (spot) {
            SetupSpawn(spot, origin, angles, TEAM_FREE);
        }
        return spot;
    }

    for (i = 0; i < level.maxclients; i++) {
        gentity_t *enemy = &g_entities[i];
        if (!enemy->inuse || !enemy->client || enemy == ent) continue;
        if (g_gametype.integer > GT_SINGLE_PLAYER &&
            enemy->client->sess.sessionTeam == team) continue; // союзники — пропускаем
        if (enemy->client->ps.stats[STAT_HEALTH] > 0) livingEnemies++;
    }

    if (livingEnemies == 0) {
        // Если врагов нет, то просто выбираем случайную точку для команды
        spot = PickRandomSpot();
        if (spot) {
            SetupSpawn(spot, origin, angles, team);
        }
        return spot;
    }

    for (i = 0; i < level.maxclients; i++) {
        gentity_t *ally = &g_entities[i];
        if (!ally->inuse || !ally->client || ally == ent) continue;
        if (ally->client->sess.sessionTeam != team) continue;
        if (ally->client->ps.stats[STAT_HEALTH] > 0) livingAllies++;
    }

    for (i = 0; i < level.numSpawnSpots; i++) {
        spot = level.spawnSpots[i];
        if (!IsValidSpot(spot, isBot)) {
            continue;
        }

        if (IsSpotBlockedByPendingSpawns(spot->s.origin, team)) {
            continue;
        }

        minEnemyDist = HUGE_DIST;
        minAllyDist = HUGE_DIST;

        for (j = 0; j < level.maxclients; j++) {
            gentity_t *other = &g_entities[j];
            if (!other->inuse || !other->client || other == ent) continue;

            VectorSubtract(spot->s.origin, other->r.currentOrigin, delta);
            dist = VectorLength(delta);

            if (g_gametype.integer > GT_SINGLE_PLAYER &&
                other->client->sess.sessionTeam == team) {
                if (dist < minAllyDist) minAllyDist = dist;
            } else if (other->client->ps.stats[STAT_HEALTH] > 0) {
                if (dist < minEnemyDist) minEnemyDist = dist;
            }
        }

        if (minEnemyDist >= ENEMY_DIST_THRESH) {
            if (safeCount < MAX_CANDIDATES) {
                safeSpots[safeCount++] = spot;
            }
        } else if (minAllyDist <= ALLY_DIST_THRESH) {
            if (fallbackCount < MAX_CANDIDATES) {
                fallbackSpots[fallbackCount++] = spot;
            }
        } else {
            if (otherCount < MAX_CANDIDATES) {
                otherSpots[otherCount++] = spot;
            }
        }
    }

    if (otherCount > 0) {
        if (random() < 0.15f) {
            int choice = Q_irand(0, otherCount - 1);
            spot = otherSpots[choice];
            SetupSpawn(spot, origin, angles, team);
            return spot;
        }
    }

    if (safeCount > 0) {
        int choice = Q_irand(0, safeCount - 1);
        spot = safeSpots[choice];
        SetupSpawn(spot, origin, angles, team);
        return spot;
    }

    if (fallbackCount > 0) {
        int choice = Q_irand(0, fallbackCount - 1);
        spot = fallbackSpots[choice];
        SetupSpawn(spot, origin, angles, team);
        return spot;
    }

    if (otherCount > 0) {
        int choice = Q_irand(0, otherCount - 1);
        spot = otherSpots[choice];
        SetupSpawn(spot, origin, angles, team);
        return spot;
    }

    spot = PickRandomSpot();
    if (spot) {
        SetupSpawn(spot, origin, angles, team);
    }
    return spot;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *SelectSpawnPoint( gentity_t *ent, vec3_t avoidPoint, vec3_t origin, vec3_t angles ) {
	return SelectRandomFurthestSpawnPoint( ent, avoidPoint, origin, angles );
}

gentity_t *SelectTeamSpawnPoint( gentity_t *ent, vec3_t avoidPoint, vec3_t origin, vec3_t angles ) {
	if (g_newSpawns.integer)
	return SelectFurthestSpawnFromEnemies( ent, origin, angles );
	else
	return SelectRandomFurthestSpawnPoint( ent, avoidPoint, origin, angles );
}

/*
===========
SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
gentity_t *SelectInitialSpawnPoint( gentity_t *ent, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;
	int n;

	spot = NULL;

	for ( n = 0; n < level.numSpawnSpotsFFA; n++ ) {
		spot = level.spawnSpots[ n ];
		if ( spot->fteam != TEAM_FREE )
			continue;
		if ( spot->spawnflags & 1 )
			break;
		else
			spot = NULL;
	}

	if ( !spot || SpotWouldTelefrag( spot ) ) {
		return SelectSpawnPoint( ent, vec3_origin, origin, angles );
	}

	VectorCopy( spot->s.angles, angles );
	VectorCopy( spot->s.origin, origin );
	origin[2] += 9.0f;

	return spot;
}


/*
===========
SelectSpectatorSpawnPoint

============
*/
gentity_t *SelectSpectatorSpawnPoint( vec3_t origin, vec3_t angles ) {
	FindIntermissionPoint();

	VectorCopy( level.intermission_origin, origin );
	VectorCopy( level.intermission_angle, angles );

	return level.spawnSpots[ SPAWN_SPOT_INTERMISSION ]; // was NULL
}


/*
=======================================================================

BODYQUE

=======================================================================
*/

/*
===============
InitBodyQue
===============
*/
void InitBodyQue (void) {
	int		i;
	gentity_t	*ent;

	level.bodyQueIndex = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++) {
		ent = G_Spawn();
		ent->classname = "bodyque";
		ent->neverFree = qtrue;
		level.bodyQue[i] = ent;
	}
}

/*
=============
BodySink

After sitting around for five seconds, fall into the ground and dissapear
=============
*/
void BodySink( gentity_t *ent ) {
	if ( level.time - ent->timestamp > 6500 ) {
		// the body ques are never actually freed, they are just unlinked
		trap_UnlinkEntity( ent );
		ent->physicsObject = qfalse;
		return;	
	}
	ent->nextthink = level.time + FRAMETIME;
	ent->s.pos.trBase[2] -= 1;
}


/*
=============
CopyToBodyQue

A player is respawning, so make an entity that looks
just like the existing corpse to leave behind.
=============
*/
void CopyToBodyQue( gentity_t *ent ) {
#ifdef MISSIONPACK
	gentity_t	*e;
	int i;
#endif
	gentity_t		*body;
	int			contents;

	trap_UnlinkEntity (ent);

	// if client is in a nodrop area, don't leave the body
	contents = trap_PointContents( ent->s.origin, -1 );
	if ( contents & CONTENTS_NODROP ) {
		return;
	}

	// grab a body que and cycle to the next one
	body = level.bodyQue[ level.bodyQueIndex ];
	level.bodyQueIndex = (level.bodyQueIndex + 1) % BODY_QUEUE_SIZE;

	trap_UnlinkEntity (body);

	body->s = ent->s;
	body->s.eFlags = EF_DEAD;		// clear EF_TALK, etc
#ifdef MISSIONPACK
	if ( ent->s.eFlags & EF_KAMIKAZE ) {
		body->s.eFlags |= EF_KAMIKAZE;

		// check if there is a kamikaze timer around for this owner
		for (i = 0; i < level.num_entities; i++) {
			e = &g_entities[i];
			if (!e->inuse)
				continue;
			if (e->activator != ent)
				continue;
			if (strcmp(e->classname, "kamikaze timer"))
				continue;
			e->activator = body;
			break;
		}
	}
#endif
	body->s.powerups = 0;	// clear powerups
	body->s.loopSound = 0;	// clear lava burning
	body->s.number = body - g_entities;
	body->timestamp = level.time;
	body->physicsObject = qtrue;
	body->physicsBounce = 0;		// don't bounce
	if ( body->s.groundEntityNum == ENTITYNUM_NONE ) {
		body->s.pos.trType = TR_GRAVITY;
		body->s.pos.trTime = level.time;
		VectorCopy( ent->client->ps.velocity, body->s.pos.trDelta );
	} else {
		body->s.pos.trType = TR_STATIONARY;
	}
	body->s.event = 0;

	// change the animation to the last-frame only, so the sequence
	// doesn't repeat anew for the body
	switch ( body->s.legsAnim & ~ANIM_TOGGLEBIT ) {
	case BOTH_DEATH1:
	case BOTH_DEAD1:
		body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD1;
		break;
	case BOTH_DEATH2:
	case BOTH_DEAD2:
		body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD2;
		break;
	case BOTH_DEATH3:
	case BOTH_DEAD3:
	default:
		body->s.torsoAnim = body->s.legsAnim = BOTH_DEAD3;
		break;
	}

	body->r.svFlags = ent->r.svFlags;
	VectorCopy (ent->r.mins, body->r.mins);
	VectorCopy (ent->r.maxs, body->r.maxs);
	VectorCopy (ent->r.absmin, body->r.absmin);
	VectorCopy (ent->r.absmax, body->r.absmax);

	body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	body->r.contents = CONTENTS_CORPSE;
	body->r.ownerNum = ent->s.number;

	body->nextthink = level.time + 5000;
	body->think = BodySink;

	body->die = body_die;

	// don't take more damage if already gibbed
	if ( ent->health <= GIB_HEALTH ) {
		body->takedamage = qfalse;
	} else {
		body->takedamage = qtrue;
	}

	VectorCopy ( body->s.pos.trBase, body->r.currentOrigin );
	trap_LinkEntity( body );
}


//======================================================================

/*
==================
SetClientViewAngle
==================
*/
void SetClientViewAngle( gentity_t *ent, vec3_t angle ) {
	int	i, cmdAngle;
	gclient_t	*client;

	client = ent->client;

	// set the delta angle
	for (i = 0 ; i < 3 ; i++) {
		cmdAngle = ANGLE2SHORT(angle[i]);
		client->ps.delta_angles[i] = cmdAngle - client->pers.cmd.angles[i];
	}
	VectorCopy( angle, ent->s.angles );
	VectorCopy( ent->s.angles, client->ps.viewangles );
}


/*
================
StarWeapons
================
*/

void AssignStartingWeapons(gclient_t *client) {
	int i;
    client->ps.stats[STAT_WEAPONS] = 0;

    if (g_instagib.integer) {
        client->ps.stats[STAT_WEAPONS] = (1 << WP_RAILGUN);
        client->ps.ammo[WP_RAILGUN] = 999;
        if (g_instagib.integer & 2) {
            client->ps.stats[STAT_WEAPONS] |= (1 << WP_GAUNTLET);
            client->ps.ammo[WP_GAUNTLET] = -1;
        }
        return;
    }

    for (i = 1; i < WP_NUM_WEAPONS; i++) {
        if (g_startWeapons.integer & (1 << i)) {
            client->ps.stats[STAT_WEAPONS] |= (1 << i);
            switch (i) {
                case WP_GAUNTLET:
                    client->ps.ammo[WP_GAUNTLET] = -1;
                    break;
                case WP_MACHINEGUN:
                    client->ps.ammo[i] = (g_gametype.integer == GT_TEAM) ? g_mg_start_ammo.integer / 2 : g_mg_start_ammo.integer;
                    break;
                case WP_SHOTGUN:
                    client->ps.ammo[i] = g_sg_start_ammo.integer;
                    break;
                case WP_GRENADE_LAUNCHER:
                    client->ps.ammo[i] = g_gl_start_ammo.integer;
                    break;
                case WP_ROCKET_LAUNCHER:
                    client->ps.ammo[i] = g_rl_start_ammo.integer;
                    break;
                case WP_LIGHTNING:
                    client->ps.ammo[i] = g_lg_start_ammo.integer;
                    break;
                case WP_RAILGUN:
                    client->ps.ammo[i] = g_rg_start_ammo.integer;
                    break;
                case WP_PLASMAGUN:
                    client->ps.ammo[i] = g_pg_start_ammo.integer;
                    break;
                case WP_BFG:
                    client->ps.ammo[i] = g_bfg_start_ammo.integer;
                    break;
#ifdef MISSIONPACK
                case WP_NAILGUN:
                    client->ps.ammo[i] = g_start_ammo_nailgun.integer;
                    break;
                case WP_PROX_LAUNCHER:
                    client->ps.ammo[i] = g_start_ammo_proxlauncher.integer;
                    break;
                case WP_CHAINGUN:
                    client->ps.ammo[i] = g_start_ammo_chaingun.integer;
                    break;
#endif
                default:
                    client->ps.ammo[i] = 100;
                    break;
            }
        }
    }
}


void SetInitialWeapon(gclient_t *client) {
	int i;

	client->ps.weapon = WP_NONE;
	client->pers.cmd.weapon = WP_NONE;
	client->ps.weaponstate = WEAPON_READY;
	client->ps.weaponTime = 0;

    if (g_startWeapon.integer >= 0 && g_startWeapon.integer < WP_NUM_WEAPONS &&
        (client->ps.stats[STAT_WEAPONS] & (1 << g_startWeapon.integer))) {
        client->ps.weapon = g_startWeapon.integer;
    } else {
        for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
            if (client->ps.stats[STAT_WEAPONS] & (1 << i)) {
                client->ps.weapon = i;
                break;
            }
        }
    }

    if (client->ps.ammo[client->ps.weapon] <= 0 && client->ps.weapon != WP_GAUNTLET) {
        for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
            if (client->ps.stats[STAT_WEAPONS] & (1 << i) && client->ps.ammo[i] > 0) {
                client->ps.weapon = i;
                break;
            }
        }
    }

    client->ps.weaponstate = WEAPON_READY;
    client->ps.weaponTime = 0;
}



/*
================
ClientRespawn
================
*/
void ClientRespawn( gentity_t *ent ) {
	gentity_t	*tent;


	if ( ftmod_setSpectator( ent ) ) 
		return;

	if ( ent->health <= 0 )
		CopyToBodyQue( ent );

	ClientSpawn( ent );
	// bots doesn't need to see any effects
	if ( level.intermissiontime && ent->r.svFlags & SVF_BOT )
		return;

	// add a teleportation effect
	tent = G_TempEntity( ent->client->ps.origin, EV_PLAYER_TELEPORT_IN );
	tent->s.clientNum = ent->s.clientNum;

	// optimize bandwidth
	if ( level.intermissiontime ) {
		tent->r.svFlags = SVF_SINGLECLIENT;
		tent->r.singleClient = ent->s.clientNum;
	}
}


/*
================
TeamCount

Returns number of players on a team
================
*/
int TeamCount( int ignoreClientNum, team_t team ) {
	int		i;
	int		count = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( i == ignoreClientNum ) {
			continue;
		}
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			count++;
		}
	}

	return count;
}


/*
================
TeamConnectedCount

Returns number of active players on a team
================
*/
int TeamConnectedCount( int ignoreClientNum, team_t team ) {
	int		i;
	int		count = 0;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( i == ignoreClientNum ) {
			continue;
		}
		if ( level.clients[i].pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			count++;
		}
	}

	return count;
}


/*
================
TeamLeader

Returns the client number of the team leader
================
*/
int TeamLeader( team_t team ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( level.clients[i].sess.sessionTeam == team ) {
			if ( level.clients[i].sess.teamLeader )
				return i;
		}
	}

	return -1;
}


/*
================
PickTeam
================
*/
team_t PickTeam( int ignoreClientNum ) {
	int		counts[TEAM_NUM_TEAMS];

	counts[TEAM_BLUE] = TeamCount( ignoreClientNum, TEAM_BLUE );
	counts[TEAM_RED] = TeamCount( ignoreClientNum, TEAM_RED );

	if ( counts[TEAM_BLUE] > counts[TEAM_RED] ) {
		return TEAM_RED;
	}
	if ( counts[TEAM_RED] > counts[TEAM_BLUE] ) {
		return TEAM_BLUE;
	}
	// equal team count, so join the team with the lowest score
	if ( level.teamScores[TEAM_BLUE] > level.teamScores[TEAM_RED] ) {
		return TEAM_RED;
	}
	return TEAM_BLUE;
}


/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call trap_SetUserinfo
if desired.

returns qfalse in case of invalid userinfo
============
*/
qboolean ClientUserinfoChanged( int clientNum ) {
	gentity_t *ent;
	int		teamTask, teamLeader, health;
	char	*s;
	char	model[MAX_QPATH];
	char	headModel[MAX_QPATH];
	char	oldname[MAX_NETNAME];
	gclient_t	*client;
	char	c1[8];
	char	c2[8];
	char	userinfo[MAX_INFO_STRING];

	ent = g_entities + clientNum;
	client = ent->client;

	trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

	// check for malformed or illegal info strings
	if ( !Info_Validate( userinfo ) ) {
		Q_strcpy( ban_reason, "bad userinfo" );
		if ( client && client->pers.connected != CON_DISCONNECTED )
			trap_DropClient( clientNum, ban_reason );
		return qfalse;
	}

	if ( client->pers.connected == CON_DISCONNECTED ) {
		// we just checked if connecting player can join server
		// so quit now as some important data like player team is still not set
		return qtrue;
	}

	// check for local client
	s = Info_ValueForKey( userinfo, "ip" );
	if ( !strcmp( s, "localhost" ) ) {
		client->pers.localClient = qtrue;
	} else {
		client->pers.localClient = qfalse;
	}

	// check the item prediction
	s = Info_ValueForKey( userinfo, "cg_predictItems" );
	if ( !atoi( s ) ) {
		client->pers.predictItemPickup = qfalse;
	} else {
		client->pers.predictItemPickup = qtrue;
	}

	// set name
	Q_strncpyz( oldname, client->pers.netname, sizeof( oldname ) );
	s = Info_ValueForKey( userinfo, "name" );
	BG_CleanName( s, client->pers.netname, sizeof( client->pers.netname ), "UnnamedPlayer" );

	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			Q_strncpyz( client->pers.netname, "scoreboard", sizeof(client->pers.netname) );
		}
	}

	if ( client->pers.connected == CON_CONNECTED ) {
		if ( strcmp( oldname, client->pers.netname ) ) {
			G_BroadcastServerCommand( -1, va("print \"%s" S_COLOR_WHITE " renamed to %s\n\"", oldname, client->pers.netname) );
		}
	}

	// set max health
#ifdef MISSIONPACK
	if (client->ps.powerups[PW_GUARD]) {
		client->pers.maxHealth = HEALTH_SOFT_LIMIT*2;
	} else {
		health = atoi( Info_ValueForKey( userinfo, "handicap" ) );
		client->pers.maxHealth = health;
		if ( client->pers.maxHealth < 1 || client->pers.maxHealth > HEALTH_SOFT_LIMIT ) {
			client->pers.maxHealth = HEALTH_SOFT_LIMIT;
		}
	}
#else
	health = atoi( Info_ValueForKey( userinfo, "handicap" ) );
	client->pers.maxHealth = health;
	if ( client->pers.maxHealth < 1 || client->pers.maxHealth > HEALTH_SOFT_LIMIT ) {
		client->pers.maxHealth = HEALTH_SOFT_LIMIT;
	}
#endif
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

#ifdef MISSIONPACK
	if (g_gametype.integer >= GT_TEAM) {
		client->pers.teamInfo = qtrue;
	} else {
		s = Info_ValueForKey( userinfo, "teamoverlay" );
		if ( ! *s || atoi( s ) != 0 ) {
			client->pers.teamInfo = qtrue;
		} else {
			client->pers.teamInfo = qfalse;
		}
	}
#else
	// teamInfo
	s = Info_ValueForKey( userinfo, "teamoverlay" );
	if ( ! *s || atoi( s ) != 0 ) {
		client->pers.teamInfo = qtrue;
	} else {
		client->pers.teamInfo = qfalse;
	}
#endif

	// set model
	Q_strncpyz( model, Info_ValueForKey( userinfo, "model" ), sizeof( model ) );
	Q_strncpyz( headModel, Info_ValueForKey( userinfo, "headmodel" ), sizeof( headModel ) );

	// team task (0 = none, 1 = offence, 2 = defence)
	teamTask = atoi(Info_ValueForKey(userinfo, "teamtask"));
	// team Leader (1 = leader, 0 is normal player)
	teamLeader = client->sess.teamLeader;

	// colors
	Q_strncpyz( c1, Info_ValueForKey( userinfo, "color1" ), sizeof( c1 ) );
	Q_strncpyz( c2, Info_ValueForKey( userinfo, "color2" ), sizeof( c2 ) );

	// send over a subset of the userinfo keys so other clients can
	// print scoreboards, display models, and play custom sounds
	if ( ent->r.svFlags & SVF_BOT ) {
		s = va("n\\%s\\t\\%i\\model\\%s\\hmodel\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\skill\\%s\\tt\\%d\\tl\\%d",
			client->pers.netname, client->sess.sessionTeam, model, headModel, c1, c2,
			client->pers.maxHealth, client->sess.wins, client->sess.losses,
			Info_ValueForKey( userinfo, "skill" ), teamTask, teamLeader );
	} else {
		s = va("n\\%s\\t\\%i\\model\\%s\\hmodel\\%s\\c1\\%s\\c2\\%s\\hc\\%i\\w\\%i\\l\\%i\\tt\\%d\\tl\\%d",
			client->pers.netname, client->sess.sessionTeam, model, headModel, c1, c2, 
			client->pers.maxHealth, client->sess.wins, client->sess.losses, teamTask, teamLeader );
	}

	trap_SetConfigstring( CS_PLAYERS+clientNum, s );
	
	// this is not the userinfo, more like the configstring actually
	G_LogPrintf( "ClientUserinfoChanged: %i %s\n", clientNum, s );

	return qtrue;
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
const char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot ) {
	char		*value;
//	char		*areabits;
	gclient_t	*client;
	char		userinfo[MAX_INFO_STRING];
	gentity_t	*ent;
	qboolean	isAdmin;

	if ( clientNum >= level.maxclients ) {
		return "Bad connection slot.";
	}

	ent = &g_entities[ clientNum ];
	ent->client = level.clients + clientNum;

	if ( firstTime ) {
		// cleanup previous data manually
		// because client may silently (re)connect without ClientDisconnect in case of crash for example
		if ( level.clients[ clientNum ].pers.connected != CON_DISCONNECTED )
			ClientDisconnect( clientNum );

		// remove old entity from the world
		trap_UnlinkEntity( ent );
		ent->r.contents = 0;
		ent->s.eType = ET_INVISIBLE;
		ent->s.eFlags = 0;
		ent->s.modelindex = 0;
		ent->s.clientNum = clientNum;
		ent->s.number = clientNum;
		ent->takedamage = qfalse;
	}

	ent->r.svFlags &= ~SVF_BOT;
	ent->inuse = qfalse;

	trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

 	// IP filtering
 	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=500
 	// recommanding PB based IP / GUID banning, the builtin system is pretty limited
 	// check to see if they are on the banned IP list
	value = Info_ValueForKey( userinfo, "ip" );

	if ( !strcmp( value, "localhost" ) && !isBot )
		isAdmin = qtrue;
	else
		isAdmin = qfalse;

	if ( !isAdmin && G_FilterPacket( value ) ) {
		return "You are banned from this server.";
	}

	// we don't check password for bots and local client
	// NOTE: local client <-> "ip" "localhost"
	// this means this client is not running in our current process
	if ( !isBot && !isAdmin ) {
		// check for a password
		if ( g_password.string[0] && Q_stricmp( g_password.string, "none" ) ) {
			value = Info_ValueForKey( userinfo, "password" );
			if ( strcmp( g_password.string, value ) )
				return "Invalid password";
		}
	}

	// they can connect
	ent->client = level.clients + clientNum;
	client = ent->client;

//	areabits = client->areabits;
	memset( client, 0, sizeof( *client ) );

	client->ps.clientNum = clientNum;

	if ( !ClientUserinfoChanged( clientNum ) ) {
		return ban_reason;
	}

	// read or initialize the session data
	if ( firstTime || level.newSession ) {
		value = Info_ValueForKey( userinfo, "team" );
		G_InitSessionData( client, value, isBot );
		G_WriteClientSessionData( client );
	}

	G_ReadClientSessionData( client );

	//freeze
	if ( g_gametype.integer != GT_TOURNAMENT && g_freeze.integer ) {
		client->sess.wins = 0;
	}
	ent->freezeState = qfalse;
	ent->readyBegin = qfalse;
	//freeze

    if( isBot ) {
		if( !G_BotConnect( clientNum, !firstTime ) ) {
			return "BotConnectfailed";
		}
		ent->r.svFlags |= SVF_BOT;
		client->sess.spectatorClient = clientNum;
	}
    ent->inuse = qtrue;
    /* restore auth state from session */
    ent->authed = client->sess.authed;

	// get and distribute relevant paramters
	G_LogPrintf( "ClientConnect: %i\n", clientNum );

	client->pers.connected = CON_CONNECTING;

	ClientUserinfoChanged( clientNum );

	// don't do the "xxx connected" messages if they were caried over from previous level
    if ( firstTime ) {
        {
            char userinfo_loc[MAX_INFO_STRING];
            const char *ipval;
            char cc[8];
            char cname[64];
            cc[0] = '\0'; cname[0] = '\0';
            trap_GetUserinfo( clientNum, userinfo_loc, sizeof( userinfo_loc ) );
            ipval = Info_ValueForKey( userinfo_loc, "ip" );
            if (ipval && ipval[0]) {
                extern qboolean GeoIP_LookupCountryForIP(const char*,int*,char*,int,char*,int,char*,int);
                int idx_dummy; char dummyCity[2];
                GeoIP_LookupCountryForIP(ipval, &idx_dummy, cc, sizeof(cc), cname, sizeof(cname), dummyCity, sizeof(dummyCity));
            }
            if (cname[0]) {
                G_BroadcastServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " connected ^1[ ^6%s^3, %s ^1]\n\"", client->pers.netname, cname, (cc[0]?cc:"--") ) );
            } else {
                G_BroadcastServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " connected\n\"", client->pers.netname ) );
            }
        }

		// mute all prints until completely in game
		client->pers.inGame = qfalse;
	} else {
		client->pers.inGame = qtrue; // FIXME: read from session data?
	}

	// count current clients and rank for scoreboard
	CalculateRanks();

	// for statistics
//	client->areabits = areabits;
//	if ( !client->areabits )
//		client->areabits = G_Alloc( (trap_AAS_PointReachabilityAreaIndex( NULL ) + 7) / 8 );

	return NULL;
}


/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
void ClientBegin( int clientNum ) {
	gentity_t	*ent;
	gclient_t	*client;
	gentity_t	*tent;
	int			flags;
	int			spawns;

	ent = g_entities + clientNum;

	client = level.clients + clientNum;

	if ( ent->r.linked ) {
		trap_UnlinkEntity( ent );
	}

	G_InitGentity( ent );
	ent->touch = 0;
	ent->pain = 0;
	ent->client = client;

	if ( client->pers.connected == CON_DISCONNECTED )
		return;

	client->pers.connected = CON_CONNECTED;
	client->pers.enterTime = level.time;
	client->pers.teamState.state = TEAM_BEGIN;
	spawns = client->ps.persistant[PERS_SPAWN_COUNT];

	// save eflags around this, because changing teams will
	// cause this to happen with a valid entity, and we
	// want to make sure the teleport bit is set right
	// so the viewpoint doesn't interpolate through the
	// world to the new position
	flags = client->ps.eFlags;
	memset( &client->ps, 0, sizeof( client->ps ) );
	client->ps.eFlags = flags;
	client->ps.persistant[PERS_SPAWN_COUNT] = spawns;

	// ClientSpawn( ent );

	if (g_freeze.integer && g_freeze_beginFrozen.integer)
		ftmod_spawnFrozenPlayer(ent, client);
	else 
		ClientSpawn(ent);
	


	if ( !client->pers.inGame ) {
		BroadcastTeamChange( client, -1 );
		if ( client->sess.sessionTeam == TEAM_RED || client->sess.sessionTeam == TEAM_BLUE )
			CheckTeamLeader( client->sess.sessionTeam );
	}

    /* send join messages only once when first entering game, not on team switch */
    if ( !client->pers.inGame && client->pers.connected == CON_CONNECTED ) {
        if ( g_joinMessage.string[0] ) {
            /* expand \n escapes */
            char msgbuf[MAX_STRING_CHARS];
            const char *s = g_joinMessage.string; int i = 0, j = 0;
            while ( s[i] && j < (int)sizeof(msgbuf) - 2 ) { if ( s[i] == '\\' && s[i+1] == 'n' ) { msgbuf[j++]='\n'; i+=2; } else { msgbuf[j++]=s[i++]; } }
            /* ensure trailing newline for chat formatting */
            if ( j == 0 || msgbuf[j-1] != '\n' ) { msgbuf[j++] = '\n'; }
            msgbuf[j] = '\0';
            trap_SendServerCommand( clientNum, va("print \"%s\"", msgbuf) );
        }
        if ( g_joinCenter.string[0] ) {
            char cpbuf[MAX_STRING_CHARS];
            const char *s2 = g_joinCenter.string; int i2 = 0, j2 = 0;
            while ( s2[i2] && j2 < (int)sizeof(cpbuf) - 1 ) { if ( s2[i2] == '\\' && s2[i2+1] == 'n' ) { cpbuf[j2++]='\n'; i2+=2; } else { cpbuf[j2++]=s2[i2++]; } }
            cpbuf[j2] = '\0';
            trap_SendServerCommand( clientNum, va("cp \"%s\"", cpbuf) );
        }
    }

	if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
		// send event
		tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = ent->s.clientNum;

		client->sess.spectatorTime = 0;

        if ( g_gametype.integer != GT_TOURNAMENT && !client->pers.inGame ) {
            G_BroadcastServerCommand( -1, va("print \"%s" S_COLOR_WHITE " entered the game\n\"", client->pers.netname) );
        }
	}
	
	client->pers.inGame = qtrue;

	G_LogPrintf( "ClientBegin: %i\n", clientNum );

	// count current clients and rank for scoreboard
	CalculateRanks();
}

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
void ClientSpawn(gentity_t *ent) {
	int		index;
	vec3_t	spawn_origin, spawn_angles;
	gclient_t	*client;
	int		i;
	clientPersistant_t	saved;
	clientSession_t		savedSess;
	int		persistant[MAX_PERSISTANT];
	gentity_t	*spawnPoint;
	int		flags;
	int		savedPing;
//	char	*savedAreaBits;
	int		accuracy_hits, accuracy_shots;
	int		eventSequence;
	char	userinfo[MAX_INFO_STRING];
	qboolean isSpectator;
	// backups to preserve extended stats across respawns
	qboolean resetExtStats;
	int		totalDamageGiven, totalDamageTaken;
	int		perWeaponDamageGiven[WP_NUM_WEAPONS];
	int		perWeaponDamageTaken[WP_NUM_WEAPONS];
	int		perWeaponShots[WP_NUM_WEAPONS];
	int		perWeaponHits[WP_NUM_WEAPONS];
	int		perWeaponKills[WP_NUM_WEAPONS];
	int		perWeaponDeaths[WP_NUM_WEAPONS];
	int		perWeaponPickups[WP_NUM_WEAPONS];
	int		perWeaponDrops[WP_NUM_WEAPONS];
	int		kills, deaths;
	int		suicides;
	int		armorPickedTotal, armorYACount, armorRACount, armorShardCount;
	int		healthPickedTotal, healthMegaCount, health50Count, health25Count, health5Count;

	index = ent - g_entities;
	client = ent->client;

	trap_UnlinkEntity( ent );

	isSpectator = client->sess.sessionTeam == TEAM_SPECTATOR;
	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	if ( isSpectator ) {
		spawnPoint = SelectSpectatorSpawnPoint( spawn_origin, spawn_angles );
	} else if (g_gametype.integer >= GT_CTF ) {
		// all base oriented team games use the CTF spawn points
		spawnPoint = SelectCTFSpawnPoint( ent, client->sess.sessionTeam, client->pers.teamState.state, spawn_origin, spawn_angles );
	} else if (g_gametype.integer == GT_TEAM) {
		spawnPoint = SelectTeamSpawnPoint(ent, client->ps.origin, spawn_origin, spawn_angles);
		
		if (g_newSpawns.integer && spawnPoint) 
			AddPendingSpawn(spawn_origin, ent->client->sess.sessionTeam);
	}
 	else {
		do {
			// the first spawn should be at a good looking spot
			if ( !client->pers.initialSpawn && client->pers.localClient ) {
				client->pers.initialSpawn = qtrue;
				spawnPoint = SelectInitialSpawnPoint( ent, spawn_origin, spawn_angles );
			} else {
				// don't spawn near existing origin if possible
				spawnPoint = SelectSpawnPoint( ent, client->ps.origin, spawn_origin, spawn_angles );
			}

			// Tim needs to prevent bots from spawning at the initial point
			// on q3dm0...
			if ( ( spawnPoint->flags & FL_NO_BOTS ) && ( ent->r.svFlags & SVF_BOT ) ) {
				continue;	// try again
			}
			// just to be symetric, we have a nohumans option...
			if ( ( spawnPoint->flags & FL_NO_HUMANS ) && !( ent->r.svFlags & SVF_BOT ) ) {
				continue;	// try again
			}

			break;

		} while ( 1 );
	}
	client->pers.teamState.state = TEAM_ACTIVE;

#ifdef MISSIONPACK
	// always clear the kamikaze flag
	ent->s.eFlags &= ~EF_KAMIKAZE;
#endif

	// toggle the teleport bit so the client knows to not lerp
	// and never clear the voted flag
	flags = client->ps.eFlags & (EF_TELEPORT_BIT | EF_VOTED | EF_TEAMVOTED);
	flags ^= EF_TELEPORT_BIT;

	// unlagged
	G_ResetHistory( ent );
	client->saved.leveltime = 0;

	// clear everything but the persistant data

	saved = client->pers;
	savedSess = client->sess;
    savedPing = client->ps.ping;
// savedAreaBits = client->areabits;
	accuracy_hits = client->accuracy_hits;
	accuracy_shots = client->accuracy_shots;
	// decide whether to reset extended stats (only on first spawn when entering game)
	resetExtStats = (client->ps.persistant[PERS_SPAWN_COUNT] == 0 && client->pers.inGame == qfalse);
	// backup extended stats to restore after memset if not first spawn
	totalDamageGiven = client->totalDamageGiven;
	totalDamageTaken = client->totalDamageTaken;
	for ( i = 0 ; i < WP_NUM_WEAPONS ; ++i ) {
		perWeaponDamageGiven[i] = client->perWeaponDamageGiven[i];
		perWeaponDamageTaken[i] = client->perWeaponDamageTaken[i];
		perWeaponShots[i]       = client->perWeaponShots[i];
		perWeaponHits[i]        = client->perWeaponHits[i];
		perWeaponKills[i]       = client->perWeaponKills[i];
		perWeaponDeaths[i]      = client->perWeaponDeaths[i];
		perWeaponPickups[i]     = client->perWeaponPickups[i];
		perWeaponDrops[i]       = client->perWeaponDrops[i];
	}
	kills = client->kills;
	deaths = client->deaths;
	suicides = client->suicides;
	armorPickedTotal = client->armorPickedTotal;
	armorYACount = client->armorYACount;
	armorRACount = client->armorRACount;
	armorShardCount = client->armorShardCount;
	healthPickedTotal = client->healthPickedTotal;
	healthMegaCount = client->healthMegaCount;
	health50Count = client->health50Count;
	health25Count = client->health25Count;
	health5Count = client->health5Count;
	for ( i = 0 ; i < MAX_PERSISTANT ; i++ ) {
		persistant[i] = client->ps.persistant[i];
	}
	eventSequence = client->ps.eventSequence;

	Com_Memset (client, 0, sizeof(*client));

	client->pers = saved;
	client->sess = savedSess;
	client->ps.ping = savedPing;
	// client->areabits = savedAreaBits;
	if ( !resetExtStats ) {
		// restore extended stats across respawn
		client->accuracy_hits = accuracy_hits;
		client->accuracy_shots = accuracy_shots;
		client->totalDamageGiven = totalDamageGiven;
		client->totalDamageTaken = totalDamageTaken;
		for ( i = 0 ; i < WP_NUM_WEAPONS ; ++i ) {
			client->perWeaponDamageGiven[i] = perWeaponDamageGiven[i];
			client->perWeaponDamageTaken[i] = perWeaponDamageTaken[i];
			client->perWeaponShots[i]       = perWeaponShots[i];
			client->perWeaponHits[i]        = perWeaponHits[i];
			client->perWeaponKills[i]       = perWeaponKills[i];
			client->perWeaponDeaths[i]      = perWeaponDeaths[i];
			client->perWeaponPickups[i]     = perWeaponPickups[i];
			client->perWeaponDrops[i]       = perWeaponDrops[i];
		}
		client->kills = kills;
		client->deaths = deaths;
		client->suicides = suicides;
		client->armorPickedTotal = armorPickedTotal;
		client->armorYACount = armorYACount;
		client->armorRACount = armorRACount;
		client->armorShardCount = armorShardCount;
		client->healthPickedTotal = healthPickedTotal;
		client->healthMegaCount = healthMegaCount;
		client->health50Count = health50Count;
		client->health25Count = health25Count;
		client->health5Count = health5Count;
	} else {
		// first spawn after connect: start fresh
		client->accuracy_hits = 0;
		client->accuracy_shots = 0;
	}
	/* on every fresh spawn, reset current streak; keep best across life */
	client->currentKillStreak = 0;
	client->lastkilled_client = -1;

	for ( i = 0 ; i < MAX_PERSISTANT ; i++ ) {
		client->ps.persistant[i] = persistant[i];
	}
	client->ps.eventSequence = eventSequence;
	// increment the spawncount so the client will detect the respawn
	client->ps.persistant[PERS_SPAWN_COUNT]++;
	client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;

	client->airOutTime = level.time + 12000;

	trap_GetUserinfo( index, userinfo, sizeof(userinfo) );
	// set max health
	client->pers.maxHealth = atoi( Info_ValueForKey( userinfo, "handicap" ) );
	if ( client->pers.maxHealth < 1 || client->pers.maxHealth > HEALTH_SOFT_LIMIT ) {
		client->pers.maxHealth = HEALTH_SOFT_LIMIT;
	}
	// clear entity values
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
	client->ps.eFlags = flags;

	ent->s.groundEntityNum = ENTITYNUM_NONE;
	ent->client = &level.clients[index];
	ent->inuse = qtrue;
	ent->classname = "player";
	if ( isSpectator ) {
		ent->takedamage = qfalse;
		ent->r.contents = 0;
		ent->clipmask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
		client->ps.pm_type = PM_SPECTATOR;
	} else {
		ent->takedamage = qtrue;
		ent->r.contents = CONTENTS_BODY;
		ent->clipmask = MASK_PLAYERSOLID;
	}
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags = 0;
	
	VectorCopy (playerMins, ent->r.mins);
	VectorCopy (playerMaxs, ent->r.maxs);

	client->ps.clientNum = index;

	AssignStartingWeapons(client);

	// client->ps.ammo[WP_GRAPPLING_HOOK] = -1;

	ent->health = client->ps.stats[STAT_HEALTH] = g_start_health.integer;
	client->ps.stats[STAT_ARMOR] = g_start_armor.integer;

	G_SetOrigin( ent, spawn_origin );
	VectorCopy( spawn_origin, client->ps.origin );

	// the respawned flag will be cleared after the attack and jump keys come up
	client->ps.pm_flags |= PMF_RESPAWNED;

	trap_GetUsercmd( client - level.clients, &ent->client->pers.cmd );
	SetClientViewAngle( ent, spawn_angles );

	// freeze
	if (g_freeze.integer) {
		if (ftmod_isSpectator(client)) {

		} else {
			G_KillBox(ent);
			trap_LinkEntity(ent);

		}
	} else {
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR) {

		} else {
			G_KillBox(ent);
			trap_LinkEntity(ent);

		}
	}


	// Устанавливаем оружие, форсим если можно, иначе лучшее
	client->ps.weapon = WP_NONE;
	SetInitialWeapon(client);

	// don't allow full run speed for a bit
	client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	client->ps.pm_time = 100;

	/* Extend spawn protection window to 3s by pushing respawnTime forward by 2000ms */
    client->respawnTime = level.time;
	client->inactivityTime = level.time + g_inactivity.integer * 1000;
	client->latched_buttons = 0;

	/* Spawn protection: optionally disable entirely and/or choose whether to use BS effect */
	if ( g_spawnProtect.integer && g_spawnProtectTime.integer > 0 ) {
		if ( g_spawnProtectUseBS.integer ) {
			client->ps.powerups[ PW_BATTLESUIT ] = level.time + g_spawnProtectTime.integer;
		} else {
			/* no BS effect: emulate immunity by delaying damage window */
			client->respawnTime = level.time + g_spawnProtectTime.integer;
			client->ps.powerups[ PW_BATTLESUIT ] = 0;
		}
	} else {
		client->ps.powerups[ PW_BATTLESUIT ] = 0;
	}

	// set default animations
	client->ps.torsoAnim = TORSO_STAND;
	client->ps.legsAnim = LEGS_IDLE;

	if ( level.intermissiontime ) {
		MoveClientToIntermission( ent );
	} else {
		if ( !isSpectator )
			trap_LinkEntity( ent );
		// fire the targets of the spawn point
		G_UseTargets( spawnPoint, ent );

		if (g_startWeapon.integer >= 0 && g_startWeapon.integer < WP_NUM_WEAPONS
			&& (client->ps.stats[STAT_WEAPONS] & (1 << g_startWeapon.integer)) )
		{
			client->ps.weapon = g_startWeapon.integer;
		}
		else
		{
			// Иначе ставим лучшее из разрешённых
			client->ps.weapon = 1; // или 0, если есть WP_NONE
			for (i = WP_NUM_WEAPONS - 1; i > 0; i--) {
				if (client->ps.stats[STAT_WEAPONS] & (1 << i)) {
					client->ps.weapon = i;
					break;
				}
			}
		}
	}

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	client->ps.commandTime = level.time - 100;
	client->pers.cmd.serverTime = level.time;
	ClientThink( ent-g_entities );

	// positively link the client, even if the command times are weird
	if (g_freeze.integer) {
		if (!ftmod_isSpectator(client)) {
			BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
			VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);
			trap_LinkEntity(ent);
		}
	} else {
		if (ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
			BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);
			VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);
			// trap_LinkEntity(ent); // Скорее-всего лишнее
		}
	}
// run the presend to set anything else
ClientEndFrame(ent);

// clear entity state values
BG_PlayerStateToEntityState(&client->ps, &ent->s, qtrue);

}



/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call trap_DropClient(), which will call this and do
server system housekeeping.
============
*/
void ClientDisconnect( int clientNum ) {
	gentity_t	*ent;
	gentity_t	*tent;
	int			i;

	// cleanup if we are kicking a bot that
	// hasn't spawned yet
	G_RemoveQueuedBotBegin( clientNum );

	ent = g_entities + clientNum;
	if (!ent->client || ent->client->pers.connected == CON_DISCONNECTED) {
		return;
	}

	// stop any following clients
	for (i = 0; i < level.maxclients; i++) {
		if (g_freeze.integer) {
			if (ftmod_isSpectator(&level.clients[i]) &&
				level.clients[i].sess.spectatorState == SPECTATOR_FOLLOW &&
				level.clients[i].sess.spectatorClient == clientNum) {
				StopFollowingNew(&g_entities[i]);
			}
		} else {
			if (level.clients[i].sess.sessionTeam == TEAM_SPECTATOR &&
				level.clients[i].sess.spectatorState == SPECTATOR_FOLLOW &&
				level.clients[i].sess.spectatorClient == clientNum) {
				StopFollowing(&g_entities[i], qtrue);
			}
		}
	}


	// send effect if they were completely connected
	if (ent->client->pers.connected == CON_CONNECTED) {
	if ((g_freeze.integer && !ftmod_isSpectator(ent->client)) ||
		(!g_freeze.integer && ent->client->sess.sessionTeam != TEAM_SPECTATOR)) {

		tent = G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_OUT);
		tent->s.clientNum = ent->s.clientNum;

		// They don't get to take powerups with them!
		// Especially important for stuff like CTF flags
		TossClientItems(ent);
	}
}
#ifdef MISSIONPACK
		TossClientPersistantPowerups( ent );
		if( g_gametype.integer == GT_HARVESTER ) {
			TossClientCubes( ent );
		}
#endif

	

	G_RevertVote( ent->client );

	G_LogPrintf( "ClientDisconnect: %i\n", clientNum );
	ent->authed = qfalse;

	// if we are playing in tourney mode and losing, give a win to the other player
	if ( (g_gametype.integer == GT_TOURNAMENT )
		&& !level.intermissiontime
		&& !level.warmupTime && level.sortedClients[1] == clientNum ) {
		level.clients[ level.sortedClients[0] ].sess.wins++;
		ClientUserinfoChanged( level.sortedClients[0] );
	}

	trap_UnlinkEntity( ent );
	ent->s.modelindex = 0;
	ent->inuse = qfalse;
	ent->classname = "disconnected";
	ent->client->pers.connected = CON_DISCONNECTED;
	ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
	ent->client->sess.sessionTeam = TEAM_FREE;

	trap_SetConfigstring( CS_PLAYERS + clientNum, "" );

	G_ClearClientSessionData( ent->client );

	CalculateRanks();

	if ( ent->r.svFlags & SVF_BOT ) {
		BotAIShutdownClient( clientNum, qfalse );
	}
}
