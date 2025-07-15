#include "q_shared.h"

#include "g_local.h"

int check_time;
static vec3_t redflag;
static vec3_t blueflag;

qboolean ftmod_isBody(gentity_t *ent) {
    if (!ent || !ent->inuse)
        return qfalse;
    return (ent->classname && !Q_stricmp(ent->classname, "freezebody"));
}

qboolean ftmod_isBodyFrozen(gentity_t *ent) {
    if (ftmod_isBody(ent)) {
        return ent->freezeState;
    }
    return qfalse;
}

qboolean ftmod_isSpectator(gclient_t *client) {
    qboolean result = qfalse;

    if (client == NULL) {
        result = qfalse;
    }
    else if (client->sess.sessionTeam == TEAM_SPECTATOR ||
             client->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ||
             client->sess.spectatorState == SPECTATOR_FOLLOW) {
        result = qtrue;
    }

    return result;
}

qboolean ftmod_setSpectator(gentity_t *ent) {
    vec3_t origin, angles;
    qboolean result = qfalse;

    if (level.intermissiontime || !ent->freezeState) {
        result = qfalse;
    }
    else
    {
        if (ent->r.svFlags & SVF_BOT)
        {
            ent->client->respawnTime = INT_MAX;
        }
        else if (!ftmod_isSpectator(ent->client))
        {
            VectorCopy(ent->r.currentOrigin, origin);
            angles[YAW] = ent->client->ps.stats[STAT_DEAD_YAW];
            angles[PITCH] = 0;
            angles[ROLL] = 0;
            ClientSpawn(ent);
            VectorCopy(origin, ent->client->ps.origin);
            SetClientViewAngle(ent, angles);

            ent->client->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
            ent->client->sess.spectatorTime = level.time;
            ent->client->sess.spectatorState = SPECTATOR_FREE;
            ent->client->sess.spectatorClient = 0;

            trap_UnlinkEntity(ent);
        }
        result = qtrue;
    }

    return result;
}

qboolean ftmod_setClient(gentity_t *ent) {
    gclient_t *cl = ent->client;
    gentity_t *tent;
    qboolean result = qfalse;

    if (cl->ps.pm_type != PM_SPECTATOR ||
        cl->sess.sessionTeam == TEAM_SPECTATOR || ent->freezeState) {
        result = qfalse;
    }
    else
    {
        cl->sess.spectatorState = SPECTATOR_NOT;
        cl->sess.spectatorClient = 0;
        ClientSpawn(ent);

        tent = G_TempEntity(cl->ps.origin, EV_PLAYER_TELEPORT_IN);
        tent->s.clientNum = ent->s.clientNum;

        result = qtrue;
    }

    return result;
}

void ftmod_respawnSpectator(gentity_t *ent) {
    gclient_t *client = ent->client;

    if (ent->freezeState)
        return;
    if (client->sess.sessionTeam == TEAM_SPECTATOR)
        return;

    if (level.time > client->respawnTime) {
        if (g_forcerespawn.integer > 0 &&
            level.time - client->respawnTime > g_forcerespawn.integer * 1000)
        {
            Cmd_FollowCycle_f(ent, 1);
        }
    }
}

void ftmod_persistantSpectator(gentity_t *ent, gclient_t *cl) {
    int i;
    int persistant[MAX_PERSISTANT];
    int savedPing;

    savedPing = ent->client->ps.ping;
    for (i = 0; i < MAX_PERSISTANT; i++) {
        persistant[i] = ent->client->ps.persistant[i];
    }
    ent->client->ps = cl->ps;
    ent->client->ps.ping = savedPing;
    for (i = 0; i < MAX_PERSISTANT; i++) {
        switch (i)
        {
        case PERS_HITS:
        case PERS_TEAM:
        case PERS_ATTACKER:
            continue;
        }
        ent->client->ps.persistant[i] = persistant[i];
    }
}

static void ftmod_followClient(gentity_t *ent, gentity_t *other) {
    if (ent->target_ent == other)
        return;
    if (ftmod_isSpectator(ent->target_ent->client)) {
        ent->target_ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
        ent->target_ent->client->sess.spectatorClient = other->s.number;
    }
}

static void ftmod_playerFree(gentity_t *ent) {
    if (!ent || !ent->inuse || !ent->freezeState)
        return;

    ent->freezeState = qfalse;
    ent->client->respawnTime = level.time + 1700;

    if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
        StopFollowingNew(ent);
        ent->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
        ent->client->ps.pm_time = 100;
    }
    ent->client->inactivityTime = level.time + g_inactivity.integer * 1000;

    if (g_freeze_forceRevive.integer) {
        ClientSpawn(ent);
        G_TempEntity(ent->client->ps.origin, EV_PLAYER_TELEPORT_IN);
    }

    CalculateRanks();
}

void ftmod_bodyFree(gentity_t *self) {
    int i;
    gentity_t *ent;
    if (self->freezeState) {
        ftmod_playerFree(self->target_ent);
    }

    self->s.powerups = 0;
    G_FreeEntity(self);
}

static void ftmod_bodyExplode(gentity_t *self) {
    int i;
    gentity_t *e, *tent;
    vec3_t point;

    for (i = 0; i < g_maxclients.integer; i++) {
        e = g_entities + i;
        if (!e->inuse || e->health < 1)
            continue;
        if (e->client->sess.sessionTeam != self->spawnflags)
            continue;
        VectorSubtract(self->s.pos.trBase, e->s.pos.trBase, point);
        if (VectorLength(point) > 100)
            continue;
        if (ftmod_isSpectator(e->client))
            continue;

        if (!self->count)
        {
            self->count = level.time + (g_thawTime.value * 1000);
            G_Sound(self, CHAN_AUTO, self->noise_index);
            self->activator = e;
        }
        else if (self->count < level.time)
        {
            // simplified case fallback
            tent = G_TempEntity(self->target_ent->r.currentOrigin, EV_OBITUARY);
            tent->s.eventParm = MOD_UNKNOWN;
            tent->s.otherEntityNum = self->target_ent->s.number;
            tent->s.otherEntityNum2 = e->s.number;
            tent->r.svFlags = SVF_BROADCAST;

            G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n", e->s.number,
                        self->target_ent->s.number, MOD_UNKNOWN,
                        e->client->pers.netname,
                        self->target_ent->client->pers.netname, "MOD_UNKNOWN");

            AddScore(e, self->s.pos.trBase, 1);
            e->client->sess.wins++;
            e->client->ps.persistant[PERS_ASSIST_COUNT]++; // assist award
            G_Damage(self, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION,
                     MOD_TELEFRAG);
        }
        return;
    }
    self->count = 0;
}

static void ftmod_bodyWorldEffects(gentity_t *self) {
    vec3_t point;
    int contents;
    vec3_t mins, maxs;
    int previous_waterlevel;
    int i, num;
    int touch[MAX_GENTITIES];
    gentity_t *hit;

    VectorCopy(self->r.currentOrigin, point);
    point[2] -= 23;
    contents = trap_PointContents(point, -1);

    if ((contents & (CONTENTS_LAVA | CONTENTS_SLIME)) &&
        (level.time - self->timestamp > (g_thawTimeAuto_lava.value * 1000))) {
        G_Damage(self, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION,
                 MOD_TELEFRAG);
        return;
    }

    if (self->s.pos.trType == TR_STATIONARY && (contents & CONTENTS_NODROP) &&
        (level.time - self->timestamp > (g_thawTimeAuto_bounds.value * 1000))) {
        ftmod_bodyFree(self);
        return;
    }

    // Water level transition
    previous_waterlevel = self->waterlevel;
    self->waterlevel = (contents & MASK_WATER) ? 3 : 0;
    self->watertype = contents;

    if (previous_waterlevel != self->waterlevel) {
        G_AddEvent(self, (self->waterlevel ? EV_WATER_TOUCH : EV_WATER_LEAVE), 0);
    }

    VectorAdd(self->r.currentOrigin, self->r.mins, mins);
    VectorAdd(self->r.currentOrigin, self->r.maxs, maxs);
    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];
        if (!hit->touch)
            continue;

        switch (hit->s.eType)
        {
        case ET_PUSH_TRIGGER:
            if (self->s.pos.trDelta[2] < 100)
            {
                G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/world/jumppad.wav"));
            }
            VectorCopy(hit->s.origin2, self->s.pos.trDelta);
            self->s.pos.trType = TR_GRAVITY;
            self->s.pos.trTime = level.time;

            break;

        case ET_TELEPORT_TRIGGER:
            if (!(hit->spawnflags & 1))
            {
                if (g_thawTimeAuto_tp.value)
                {
                    ftmod_playerFree(self->target_ent);
                    ftmod_setClient(self->target_ent);
                }
                return;
            }
            break;
        }
    }
}

static void ftmod_tossBody(gentity_t *self) {
    int anim;
    static int n;

    self->timestamp = level.time;
    self->nextthink = level.time + 5000;
    self->s.eFlags |= EF_DEAD;
    self->s.powerups = 0;
    self->r.maxs[2] = -8;
    self->r.contents = CONTENTS_CORPSE;
    self->freezeState = qfalse;
    self->s.weapon = WP_NONE;

    switch (n) {
    case 0:
        anim = BOTH_DEATH1;
        break;
    case 1:
        anim = BOTH_DEATH2;
        break;
    case 2:
    default:
        anim = BOTH_DEATH3;
        break;
    }
    n = (n + 1) % 3;

    self->s.torsoAnim = self->s.legsAnim = anim;

    if (!g_blood.integer) {
        self->takedamage = qfalse;
    }

    trap_LinkEntity(self);
}

static void ftmod_bodyThink(gentity_t *self) {
    self->nextthink = level.time + FRAMETIME;

    if (!self->target_ent || !self->target_ent->client ||
        !self->target_ent->inuse) {
        ftmod_bodyFree(self);
        return;
    }
    if (self->s.otherEntityNum != self->target_ent->s.number) {
        ftmod_bodyFree(self);
        return;
    }
    if (level.intermissiontime || level.intermissionQueued) {
        return;
    }

    if (g_thawTimeAutoRevive.integer > 0 &&
        level.time - self->timestamp > (g_thawTimeAutoRevive.value * 1000)) {
        ftmod_playerFree(self->target_ent);
        ftmod_tossBody(self);
        return;
    }

    if (self->freezeState) {
        if (!self->target_ent->freezeState)
        {
            ftmod_tossBody(self);
            return;
        }
        ftmod_bodyExplode(self);
        if (self->last_move_time < level.time - 1000)
        {
            ftmod_bodyWorldEffects(self);
            self->last_move_time = level.time;
        }
        return;
    }

    if (level.time - self->timestamp > 6500) {
        ftmod_bodyFree(self);
    }
    else
    {
        self->s.pos.trBase[2] -= 1;
    }
}

static void ftmod_bodyDie(gentity_t *self, gentity_t *inflictor,
                          gentity_t *attacker, int damage, int mod) {
    gentity_t *tent;

    if (self->health > GIB_HEALTH) {
        return;
    }

    if (self->freezeState && !g_blood.integer) {
        ftmod_playerFree(self->target_ent);
        ftmod_tossBody(self);
        return;
    }

    tent = G_TempEntity(self->r.currentOrigin, EV_GIB_PLAYER);
    if (self->freezeState) {
        tent->s.eventParm = 255;
    }

    ftmod_bodyFree(self);
}

qboolean ftmod_damageBody(gentity_t *targ, gentity_t *attacker, vec3_t dir,
                          int mod, int knockback) {
    static float mass;
    vec3_t kvel;

    if (!mass) {
        char info[1024];
        static char mapname[128];

        trap_GetServerinfo(info, sizeof(info));
        strncpy(mapname, Info_ValueForKey(info, "mapname"), sizeof(mapname) - 1);
        mapname[sizeof(mapname) - 1] = '\0';

        if (!Q_stricmp(mapname, "q3tourney3") || !Q_stricmp(mapname, "q3dm16") ||
            !Q_stricmp(mapname, "q3dm17") || !Q_stricmp(mapname, "q3dm18") ||
            !Q_stricmp(mapname, "q3dm19") || !Q_stricmp(mapname, "q3tourney6") ||
            !Q_stricmp(mapname, "q3ctf4") || !Q_stricmp(mapname, "mpq3ctf4") ||
            !Q_stricmp(mapname, "mpq3tourney6") || !Q_stricmp(mapname, "mpteam6"))
        {
            mass = 300;
        }
        else
        {
            mass = 200;
        }
        if (g_dmflags.integer & 1024)
            mass = 300;
    }

    if (attacker->client && targ->freezeState) {
        if (knockback)
        {
            VectorScale(dir, g_knockback.value * (float)knockback / mass, kvel);
            if (mass == 200)
                kvel[2] += 24;
            VectorAdd(targ->s.pos.trDelta, kvel, targ->s.pos.trDelta);

            targ->s.pos.trType = TR_GRAVITY;
            targ->s.pos.trTime = level.time;

            targ->pain_debounce_time = level.time;
        }
        return qtrue;
    }
    return qfalse;
}

void ftmod_copyToBody(gentity_t *ent) {
    gentity_t *body;

    body = G_Spawn();
    body->classname = "freezebody";
    body->s = ent->s;
    body->s.powerups = 1 << PW_BATTLESUIT;
    body->s.number = body - g_entities;
    body->s.weapon = WP_NONE;
    body->s.eFlags = 0;
    body->s.loopSound = 0;
    body->timestamp = level.time;
    body->physicsObject = qtrue;

    G_SetOrigin(body, ent->r.currentOrigin);
    body->s.pos.trType = TR_GRAVITY;
    body->s.pos.trTime = level.time;
    VectorCopy(ent->client->ps.velocity, body->s.pos.trDelta);
    body->s.event = 0;

    switch (body->s.legsAnim & ~ANIM_TOGGLEBIT) {
    case LEGS_WALKCR:
    case LEGS_WALK:
    case LEGS_RUN:
    case LEGS_BACK:
    case LEGS_SWIM:
    case LEGS_IDLE:
    case LEGS_IDLECR:
    case LEGS_TURN:
    case LEGS_BACKCR:
    case LEGS_BACKWALK:
        switch (rand() % 4)
        {
        case 0:
            body->s.legsAnim = LEGS_JUMP;
            break;
        case 1:
            body->s.legsAnim = LEGS_LAND;
            break;
        case 2:
            body->s.legsAnim = LEGS_JUMPB;
            break;
        case 3:
            body->s.legsAnim = LEGS_LANDB;
            break;
        }
    }

    body->r.svFlags = ent->r.svFlags;
    VectorCopy(ent->r.mins, body->r.mins);
    VectorCopy(ent->r.maxs, body->r.maxs);
    VectorCopy(ent->r.absmin, body->r.absmin);
    VectorCopy(ent->r.absmax, body->r.absmax);

    body->clipmask = MASK_PLAYERSOLID;
    body->r.contents = CONTENTS_BODY;

    body->think = ftmod_bodyThink;
    body->nextthink = level.time + FRAMETIME;

    body->die = ftmod_bodyDie;
    body->takedamage = qtrue;

    body->target_ent = ent;
    ent->target_ent = body;
    body->s.otherEntityNum = ent->s.number;
    body->noise_index = G_SoundIndex("sound/player/tankjr/jump1.wav");
    body->freezeState = qtrue;
    body->spawnflags = ent->client->sess.sessionTeam;
    body->waterlevel = ent->waterlevel;
    body->count = 0;

    trap_LinkEntity(body);
}

static qboolean ftmod_nearbyBody(gentity_t *targ) {
    gentity_t *spot;
    vec3_t delta;

    if (g_gametype.integer == GT_CTF) {
        return qfalse;
    }

    spot = NULL;
    while ((spot = G_Find(spot, FOFS(classname), "freezebody")) != NULL) {
        if (!spot->freezeState)
            continue;
        if (spot->spawnflags != targ->client->sess.sessionTeam)
            continue;

        VectorSubtract(spot->s.pos.trBase, targ->s.pos.trBase, delta);
        if (VectorLength(delta) > g_thawRadius.integer)
            continue;

        if (level.time - spot->timestamp > 400)
        {
            return qtrue;
        }
    }

    return qfalse;
}

void ftmod_playerFreeze(gentity_t *self, gentity_t *attacker, int mod) {
    if (level.warmupTime) {
        return;
    }
    if (g_gametype.integer != GT_TEAM && g_gametype.integer != GT_CTF) {
        return;
    }
    if (self != attacker &&
        OnSameTeam(self, attacker)) { // don't freeze when teamkilled (disabled)
      // return;
    }
    if (self != attacker && g_gametype.integer == GT_CTF && redflag && blueflag) {
        vec3_t dist1, dist2;
        VectorSubtract(redflag, self->s.pos.trBase, dist1);
        VectorSubtract(blueflag, self->s.pos.trBase, dist2);

        if (self->client->sess.sessionTeam == TEAM_RED)
        {
            if (VectorLength(dist1) < VectorLength(dist2))
            {
                return;
            }
        }
        else if (self->client->sess.sessionTeam == TEAM_BLUE)
        {
            if (VectorLength(dist2) < VectorLength(dist1))
            {
                return;
            }
        }
    }

    switch (mod) {
    case MOD_UNKNOWN:
    case MOD_CRUSH:
    // Надо убрать стак в моделях перед тем как запрещать это
    case MOD_TELEFRAG:
    case MOD_TARGET_LASER:
    case MOD_GRAPPLE:
        return;
    }

    ftmod_copyToBody(self);
    self->r.maxs[2] = -8;
    self->freezeState = qtrue;
    check_time = (level.time - 3000) + 200;

    self->takedamage = qfalse;
    self->s.eType = ET_INVISIBLE;
    self->r.contents = 0;
    self->health = GIB_HEALTH;

    if (attacker->client && self != attacker && ftmod_nearbyBody(self)) {
        attacker->client->ps.persistant[PERS_DEFEND_COUNT]++;
        attacker->client->ps.eFlags &=
            ~(EF_AWARD_IMPRESSIVE | EF_AWARD_EXCELLENT | EF_AWARD_GAUNTLET |
              EF_AWARD_ASSIST | EF_AWARD_DEFEND | EF_AWARD_CAP);
        attacker->client->ps.eFlags |= EF_AWARD_DEFEND;
        attacker->client->rewardTime = level.time + REWARD_SPRITE_TIME;
    }
    CalculateRanks();
}

qboolean ftmod_readyCheck(void) {
    int i;
    gentity_t *e;

    if (level.warmupTime == 0)
        return qfalse;
    if (!g_doReady.integer)
        return qfalse;

    for (i = 0; i < g_maxclients.integer; i++) {
        e = g_entities + i;
        if (!e->inuse)
            continue;
        if (e->r.svFlags & SVF_BOT)
            continue;
        if (e->client->pers.connected == CON_DISCONNECTED)
            continue;
        if (e->client->sess.sessionTeam == TEAM_SPECTATOR)
            continue;
        if (!e->readyBegin)
            return qtrue;
    }
    return qfalse;
}

gentity_t *SelectRandomDeathmatchSpawnPoint(void);

void Team_ForceGesture(int team);

// end of round->start new round
void ftmod_teamWins(int team) {
    int i;
    gentity_t *e;
    char *teamstr;
    gentity_t *spawnPoint;
    int j;
    int flight;
    gclient_t *cl;
    gentity_t *te;

    spawnPoint = SelectRandomDeathmatchSpawnPoint();
    for (i = 0; i < g_maxclients.integer; i++) {
        e = g_entities + i;
        cl = e->client;
        if (!e->inuse)
            continue;

        if (e->freezeState)
        {
            if (!(g_dmflags.integer & 128) || cl->sess.sessionTeam != team)
            {
                ftmod_playerFree(e);
            }
            continue;
        }
        if (e->health < 1)
            continue;
        if (ftmod_isSpectator(cl))
            continue;
        if (g_dmflags.integer & 64)
            continue;

        if (e->health < cl->ps.stats[STAT_MAX_HEALTH])
        {
            e->health = cl->ps.stats[STAT_MAX_HEALTH];
        }

        memset(cl->ps.ammo, 0, sizeof(cl->ps.ammo));

        AssignStartingWeapons(cl);
        SetInitialWeapon(cl);

        flight = cl->ps.powerups[PW_FLIGHT];
        if (cl->ps.powerups[PW_REDFLAG])
        {
            memset(cl->ps.powerups, 0, sizeof(cl->ps.powerups));
            cl->ps.powerups[PW_REDFLAG] = INT_MAX;
        }
        else if (cl->ps.powerups[PW_BLUEFLAG])
        {
            memset(cl->ps.powerups, 0, sizeof(cl->ps.powerups));
            cl->ps.powerups[PW_BLUEFLAG] = INT_MAX;
        }
        else if (cl->ps.powerups[PW_NEUTRALFLAG])
        {
            memset(cl->ps.powerups, 0, sizeof(cl->ps.powerups));
            cl->ps.powerups[PW_NEUTRALFLAG] = INT_MAX;
        }
        else
        {
            memset(cl->ps.powerups, 0, sizeof(cl->ps.powerups));
        }
        cl->ps.powerups[PW_FLIGHT] = flight;

        e->health = cl->ps.stats[STAT_HEALTH] = g_start_health.integer;
        cl->ps.stats[STAT_ARMOR] = g_start_armor.integer;
    }

    // ignore ctf, no scores from here
    if (level.numPlayingClients < 2 || g_gametype.integer == GT_CTF) {
        return;
    }

    te = G_TempEntity(vec3_origin, EV_GLOBAL_TEAM_SOUND);

if (g_gametype.integer == GT_TEAM) {
    if (team == TEAM_RED) {
        teamstr = "^1RED";
        te->s.eventParm = GTS_REDTEAM_SCORED;
    } else {
        teamstr = "^4BLUE";
        te->s.eventParm = GTS_BLUETEAM_SCORED;
    }
} else {
    // Для других геймтипов оставить текущую логику capture
    if (team == TEAM_RED) {
        teamstr = "^1RED";
        te->s.eventParm = GTS_BLUE_CAPTURE;
    } else {
        teamstr = "^4BLUE";
        te->s.eventParm = GTS_RED_CAPTURE;
    }
}

te->r.svFlags |= SVF_BROADCAST;


    trap_SendServerCommand(
        -1, va("cp \"" S_COLOR_MAGENTA "%s " S_COLOR_WHITE "^3Team scores!\n\"",
               teamstr));
    trap_SendServerCommand(-1, va("print \"^3***%s ^3Team scores!\n\"", teamstr));

    AddTeamScore(vec3_origin, team, 1);
    Team_ForceGesture(team);

    CalculateRanks();
    if ( g_freeze.integer ) {
        level.freezeRoundStartTime = level.time;
    }
}

static qboolean ftmod_calculateScores(int team) {
    int i;
    gentity_t *e;
    qboolean modified = qfalse;

    for (i = 0; i < g_maxclients.integer; i++) {
        e = g_entities + i;
        if (!e->inuse)
            continue;
        if (e->client->sess.sessionTeam != team)
            continue;
        if (e->freezeState)
        {
            modified = qtrue;
            continue;
        }
        if (e->client->pers.connected == CON_CONNECTING)
            continue;
        if ((e->health < 1 || ftmod_isSpectator(e->client)) &&
            level.time > e->client->respawnTime)
            continue;
        return qfalse;
    }
    if (modified) {
        ftmod_teamWins(OtherTeam(team));
    }
    return modified;
}

void ftmod_checkDelay(void) {
    int i;
    gentity_t *e;
    int readyMask;

    readyMask = 0;
    for (i = 0; i < g_maxclients.integer; i++) {
        e = g_entities + i;
        if (!e->inuse)
            continue;
        if (level.warmupTime != 0 && !e->readyBegin)
            continue;
        if (level.warmupTime == 0 && !e->freezeState)
            continue;
        if (i < 16)
        {
            readyMask |= 1 << i;
        }
    }
    for (i = 0; i < g_maxclients.integer; i++) {
        e = g_entities + i;
        if (!e->inuse)
            continue;
        e->client->ps.stats[STAT_CLIENTS_READY] = readyMask;
    }

    if (check_time > level.time - 3000) {
        return;
    }
    check_time = level.time;

    if (!ftmod_calculateScores(TEAM_RED)) {
        ftmod_calculateScores(TEAM_BLUE);
    }
}

void ftmod_countAlive( int *red, int *blue ) {
	int i;
	*red = *blue = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		gclient_t *cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED )
			continue;
		if ( cl->sess.sessionTeam == TEAM_RED && g_entities[i].inuse && g_entities[i].freezeState != qtrue )
			(*red)++;
		else if ( cl->sess.sessionTeam == TEAM_BLUE && g_entities[i].inuse && g_entities[i].freezeState != qtrue )
			(*blue)++;
	}
}

void ftmod_spawnFrozenPlayer(gentity_t *ent, gclient_t *client) {
    int delay = g_freeze_beginFrozenDelay.integer * 1000;
    int timeSinceRoundStart = level.time - level.freezeRoundStartTime;

    if (g_freeze_beginFrozen.integer && (level.freezeRoundStartTime > 0
        && timeSinceRoundStart >= delay
        && client->sess.sessionTeam != TEAM_SPECTATOR)) {

        ClientSpawn(ent);

        ftmod_copyToBody(ent);

        ent->r.contents = 0;
        ent->s.eFlags |= EF_NODRAW;
        ent->s.powerups = 0;
        ent->takedamage = qfalse;
        ent->r.linked = qfalse;
        trap_UnlinkEntity(ent);

        ent->freezeState = qtrue;
        client->ps.pm_type = PM_FREEZE;
        client->ps.weapon = WP_NONE;
        client->ps.stats[STAT_HEALTH] = ent->health = 0;
    } else {
        ClientSpawn(ent);
    }
}

