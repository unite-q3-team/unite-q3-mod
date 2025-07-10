qboolean ftmod_isSpectator(gclient_t * client);
qboolean ftmod_setSpectator(gentity_t * ent);
qboolean ftmod_setClient(gentity_t * ent);
qboolean ftmod_damageBody(gentity_t * targ, gentity_t * attacker, vec3_t dir, int mod, int knockback);
qboolean ftmod_isBody(gentity_t * ent);
qboolean ftmod_isBodyFrozen(gentity_t * ent);
qboolean ftmod_readyCheck(void);
void ftmod_respawnSpectator(gentity_t * ent);
void ftmod_persistantSpectator(gentity_t * ent, gclient_t * cl);
void ftmod_bodyFree(gentity_t * self);
void ftmod_playerFreeze(gentity_t * self, gentity_t * attacker, int mod);
void ftmod_teamWins(int team);
void ftmod_checkDelay(void);

