// code/game/cmds/itemreplace.c
// Runtime item replacement system (per-map), similar in spirit to votesystem
// C89-compatible

#include "cmds.h"

/* forward decls (C89): helpers used before definition */
static gitem_t *IR_FindItemByClassnameOrPickup( const char *name );

/*
Config format (itemreplace.txt): lines "key = value", comments with # or //

Two kinds of entries are supported, all scoped by map name prefix:

1) Class mapping for a map (replace all of one classname with another):
   map.<MAP>.classmap.<from> = <to>
   Example: map.q3dm6.classmap.item_quad = item_health_mega

2) Targeted rule with matching by classname and optional origin tolerance,
   and application of overrides (classname/origin/angles/spawnflags/remove):
   map.<MAP>.rule.<ID>.match.classname = item_quad
   map.<MAP>.rule.<ID>.match.origin = 848 -456 312      # optional
   map.<MAP>.rule.<ID>.match.tolerance = 48            # optional, default 32
   map.<MAP>.rule.<ID>.apply.classname = item_health_mega
   map.<MAP>.rule.<ID>.apply.origin = 860 -460 312     # optional
   map.<MAP>.rule.<ID>.apply.angles = 0 180 0          # optional
   map.<MAP>.rule.<ID>.apply.spawnflags = 1            # optional
   map.<MAP>.rule.<ID>.apply.remove = 1                # optional (remove entity)

Notes:
 - Targeted rules are evaluated before class mapping. The first matching rule
   is applied. Class mapping is then applied (if any) to the possibly-updated
   classname.
 - When replacing classname, the entity keeps all other properties unless
   overridden by apply.* keys.
*/

/* ---------------- Internal parser helpers ---------------- */

static void IR_TrimRight(char *s) {
    int n;
    n = (int)strlen(s);
    while ( n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n') ) {
        s[n-1] = '\0';
        n--;
    }
}

static void IR_TrimLeft(char *s) {
    int i, n; char *p;
    n = (int)strlen(s); i = 0;
    while ( i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') ) i++;
    if ( i > 0 ) { p = s + i; memmove(s, p, strlen(p) + 1); }
}

static void IR_Trim(char *s) { IR_TrimRight(s); IR_TrimLeft(s); }

static void IR_Unquote(char *s) {
    int n; n = (int)strlen(s);
    if ( n >= 2 && ((s[0] == '"' && s[n-1] == '"') || (s[0] == '\'' && s[n-1] == '\'')) ) {
        s[n-1] = '\0'; memmove(s, s + 1, n - 1);
    }
}

static int IR_ParseVec3(const char *s, float *out) {
    int n;
    if ( !s || !*s ) return 0;
    n = Q_sscanf( s, "%f %f %f", &out[0], &out[1], &out[2] );
    return n == 3;
}

/* ---------------- Data structures ---------------- */

#define IR_MAX_RULES 128
#define IR_MAX_CLASSMAP 128
#define IR_MAX_ORIG_ITEMS 1024

typedef struct {
    char classname[64];
    float origin[3]; int hasOrigin;
    float angles[3]; int hasAngles; /* full angles */
    float angle; int hasAngle;      /* yaw-only */
    int spawnflags; int hasSpawnflags;
    int tolerance; int hasTolerance;
} ir_match_t;

typedef struct {
    int removeEntity; /* 1 to remove */
    char classname[64]; int hasClass;
    float origin[3]; int hasOrigin;
    float angles[3]; int hasAngles;
    float angle; int hasAngle;
    int spawnflags; int hasSpawnflags;
} ir_apply_t;

typedef struct {
    char map[64];
    char id[32];
    ir_match_t match;
    ir_apply_t apply;
} ir_rule_t;

typedef struct {
    char map[64];
    char fromClass[64];
    char toClass[64];
} ir_classmap_t;

static ir_rule_t ir_rules[IR_MAX_RULES];
static int ir_ruleCount = 0;
static ir_classmap_t ir_cmaps[IR_MAX_CLASSMAP];
static int ir_cmapCount = 0;
static qboolean ir_loaded = qfalse;

typedef struct {
    char classname[64];
    float origin[3];
    float angles[3];
    int spawnflags;
    float wait;
    float random;
} ir_orig_item_t;

static ir_orig_item_t ir_orig_items[IR_MAX_ORIG_ITEMS];
static int ir_orig_itemCount = 0;

/* --- runtime edit log for saving --- */
typedef struct { char classname[64]; float origin[3]; float angles[3]; int hasAngles; } ir_addop_t;
typedef struct { char classname[64]; float origin[3]; int tolerance; } ir_rmop_t;
typedef struct { char from[64]; char to[64]; float origin[3]; float angles[3]; int hasAngles; float applyOrigin[3]; int hasApplyOrigin; } ir_replop_t;
static ir_addop_t s_irAdds[128]; static int s_irAddCount = 0;
static ir_rmop_t  s_irRms[128];  static int s_irRmCount  = 0;
static ir_replop_t s_irRepls[128]; static int s_irReplCount = 0;

/* persistent adds parsed from itemreplace.txt */
#define IR_MAX_ADDS 256
typedef struct { char map[64]; char id[32]; char classname[64]; float origin[3]; int hasOrigin; float angles[3]; int hasAngles; } ir_addrule_t;
static ir_addrule_t ir_adds[IR_MAX_ADDS];
static int ir_addCount = 0;

static ir_addrule_t *IR_FindOrCreateAddRule( const char *map, const char *id ) {
    int i;
    for ( i = 0; i < ir_addCount; ++i ) {
        if ( !Q_stricmp( ir_adds[i].map, map ) && !Q_stricmp( ir_adds[i].id, id ) ) {
            return &ir_adds[i];
        }
    }
    if ( ir_addCount >= IR_MAX_ADDS ) return NULL;
    memset( &ir_adds[ir_addCount], 0, sizeof(ir_adds[0]) );
    Q_strncpyz( ir_adds[ir_addCount].map, map, sizeof(ir_adds[0].map) );
    Q_strncpyz( ir_adds[ir_addCount].id, id, sizeof(ir_adds[0].id) );
    ir_addCount++;
    return &ir_adds[ir_addCount - 1];
}

/* ---------------- Loader ---------------- */

static const char *ir_defaultText =
    "# itemreplace.txt — per-map item replacement rules\n"
    "# Lines support key=value, comments: # or //\n"
    "# Key format: map.<map>.<section>\n"
    "# Sections:\n"
    "#   classmap.<fromClass> = <toClass>           # replace all occurrences of a class on the map\n"
    "#   rule.<id>.match.classname = <class>        # required for targeted rule\n"
    "#   rule.<id>.match.origin = X Y Z             # optional: match only near XYZ\n"
    "#   rule.<id>.match.tolerance = N              # optional: distance tolerance (default 32)\n"
    "#   rule.<id>.apply.classname = <class>        # optional: new class\n"
    "#   rule.<id>.apply.origin = X Y Z             # optional: new origin\n"
    "#   rule.<id>.apply.angles = P Y R             # optional: new angles\n"
    "#   rule.<id>.apply.angle = Y                  # optional: new yaw only\n"
    "#   rule.<id>.apply.spawnflags = N             # optional: new spawnflags\n"
    "#   rule.<id>.apply.remove = 1                 # optional: remove entity\n"
    "\n"
    "# Example: on q3dm6 replace all Quads with MegaHealth\n"
    "map.q3dm6.classmap.item_quad = item_health_mega\n"
    "\n"
    "# Example targeted override\n"
    "# map.q3dm6.rule.r1.match.classname = item_quad\n"
    "# map.q3dm6.rule.r1.match.origin = 848 -456 312\n"
    "# map.q3dm6.rule.r1.apply.classname = item_health_mega\n"
    "# map.q3dm6.rule.r1.apply.origin = 860 -460 312\n"
    "# map.q3dm6.rule.r1.apply.angles = 0 180 0\n";

static void IR_CreateDefaultFile(void) {
    fileHandle_t f; int r;
    r = trap_FS_FOpenFile("itemreplace.txt", &f, FS_WRITE);
    if ( r >= 0 ) {
        G_Printf("itemreplace: creating default itemreplace.txt\n");
        trap_FS_Write( ir_defaultText, (int)strlen(ir_defaultText), f );
        trap_FS_FCloseFile( f );
    } else {
        G_Printf("itemreplace: failed to create itemreplace.txt (FS_WRITE)\n");
    }
}

static ir_rule_t *IR_FindOrCreateRule(const char *map, const char *id) {
    int i;
    for ( i = 0; i < ir_ruleCount; i++ ) {
        if ( !Q_stricmp(ir_rules[i].map, map) && !Q_stricmp(ir_rules[i].id, id) ) {
            return &ir_rules[i];
        }
    }
    if ( ir_ruleCount >= IR_MAX_RULES ) return NULL;
    memset(&ir_rules[ir_ruleCount], 0, sizeof(ir_rules[0]));
    Q_strncpyz(ir_rules[ir_ruleCount].map, map, sizeof(ir_rules[0].map));
    Q_strncpyz(ir_rules[ir_ruleCount].id, id, sizeof(ir_rules[0].id));
    ir_rules[ir_ruleCount].match.tolerance = 32; /* default */
    ir_rules[ir_ruleCount].match.hasTolerance = 1;
    ir_ruleCount++;
    return &ir_rules[ir_ruleCount - 1];
}

static ir_classmap_t *IR_AddClassmap(const char *map, const char *from, const char *to) {
    ir_classmap_t *cm;
    if ( ir_cmapCount >= IR_MAX_CLASSMAP ) return NULL;
    cm = &ir_cmaps[ir_cmapCount++];
    memset(cm, 0, sizeof(*cm));
    Q_strncpyz(cm->map, map, sizeof(cm->map));
    Q_strncpyz(cm->fromClass, from, sizeof(cm->fromClass));
    Q_strncpyz(cm->toClass, to, sizeof(cm->toClass));
    return cm;
}

static void IR_Load(void) {
    fileHandle_t f; int flen; char *buf; char *p;
    char line[512];
    char *nl; int linelen;
    if ( ir_loaded ) return;
    ir_loaded = qtrue;
    ir_ruleCount = 0; ir_cmapCount = 0; ir_addCount = 0;

    flen = trap_FS_FOpenFile("itemreplace.txt", &f, FS_READ);
    if ( flen <= 0 ) {
        G_Printf("itemreplace: itemreplace.txt not found, creating default...\n");
        IR_CreateDefaultFile();
        flen = trap_FS_FOpenFile("itemreplace.txt", &f, FS_READ);
        if ( flen <= 0 ) {
            G_Printf("itemreplace: still cannot open itemreplace.txt after create attempt\n");
            return;
        }
    }
    if ( flen > 64 * 1024 ) flen = 64 * 1024;
    buf = (char*)G_Alloc(flen + 1);
    if ( !buf ) { trap_FS_FCloseFile(f); return; }
    trap_FS_Read(buf, flen, f); trap_FS_FCloseFile(f); buf[flen] = '\0';

    p = buf;
    while ( *p ) {
        char *comment; char *eq;
        char key[256]; char val[256];
        char map[64]; char section[64]; char rest[128];
        int i, klen, pos, dot1, dot2;

        nl = strchr(p, '\n');
        if ( nl ) linelen = (int)(nl - p); else linelen = (int)strlen(p);
        if ( linelen > (int)sizeof(line) - 1 ) linelen = (int)sizeof(line) - 1;
        Q_strncpyz(line, p, linelen + 1);
        if ( nl ) p = nl + 1; else p = p + linelen;

        comment = strstr(line, "//"); if ( comment ) *comment = '\0';
        comment = strchr(line, '#'); if ( comment ) *comment = '\0';
        IR_Trim(line); if ( !line[0] ) continue;

        eq = strchr(line, '='); if ( !eq ) continue;
        Q_strncpyz(key, line, (int)(eq - line) + 1);
        Q_strncpyz(val, eq + 1, sizeof(val)); IR_Trim(key); IR_Trim(val); IR_Unquote(val);

        /* Expect key beginning with "map." */
        if ( Q_strncmp(key, "map.", 4) != 0 ) continue;
        /* Split: map.<map>.<...> */
        pos = 4; dot1 = -1; klen = (int)strlen(key);
        for ( i = pos; i < klen; i++ ) { if ( key[i] == '.' ) { dot1 = i; break; } }
        if ( dot1 <= pos ) continue;
        Q_strncpyz(map, key + pos, dot1 - pos + 1);

        /* Remainder */
        Q_strncpyz(rest, key + dot1 + 1, sizeof(rest)); IR_Trim(rest);
        /* classmap: rest starts with "classmap." */
        if ( !Q_strncmp(rest, "classmap.", 9) ) {
            char from[64];
            Q_strncpyz(from, rest + 9, sizeof(from));
            IR_Trim(from);
            if ( from[0] && val[0] ) {
                IR_AddClassmap(map, from, val);
            }
            continue;
        }
        /* add.<id>.<...> */
        if ( !Q_strncmp(rest, "add.", 4) ) {
            int dot2;
            /* find next dot */
            dot2 = -1; klen = (int)strlen(rest);
            for ( i = 4; i < klen; i++ ) { if ( rest[i] == '.' ) { dot2 = i; break; } }
            if ( dot2 <= 4 ) continue;
            Q_strncpyz(section, rest + 4, dot2 - 4 + 1); /* ID */
            {
                ir_addrule_t *a = IR_FindOrCreateAddRule( map, section );
                const char *attr = rest + dot2 + 1;
                if ( !a ) continue;
                if ( !Q_stricmp(attr, "classname") ) {
                    Q_strncpyz(a->classname, val, sizeof(a->classname));
                } else if ( !Q_stricmp(attr, "origin") ) {
                    if ( IR_ParseVec3(val, a->origin) ) a->hasOrigin = 1;
                } else if ( !Q_stricmp(attr, "angles") ) {
                    if ( IR_ParseVec3(val, a->angles) ) a->hasAngles = 1;
                }
            }
            continue;
        }
        /* rule.<id>.<...> */
        if ( Q_strncmp(rest, "rule.", 5) != 0 ) continue;
        /* find second dot */
        dot2 = -1; klen = (int)strlen(rest);
        for ( i = 5; i < klen; i++ ) { if ( rest[i] == '.' ) { dot2 = i; break; } }
        if ( dot2 <= 5 ) continue;
        Q_strncpyz(section, rest + 5, dot2 - 5 + 1); /* ID */
        {
            ir_rule_t *r; const char *attr = rest + dot2 + 1;
            r = IR_FindOrCreateRule(map, section); if ( !r ) continue;
            if ( !Q_stricmp(attr, "match.classname") ) {
                Q_strncpyz(r->match.classname, val, sizeof(r->match.classname));
            } else if ( !Q_stricmp(attr, "match.origin") ) {
                if ( IR_ParseVec3(val, r->match.origin) ) r->match.hasOrigin = 1;
            } else if ( !Q_stricmp(attr, "match.tolerance") ) {
                r->match.tolerance = atoi(val); r->match.hasTolerance = 1;
            } else if ( !Q_stricmp(attr, "apply.classname") ) {
                Q_strncpyz(r->apply.classname, val, sizeof(r->apply.classname)); r->apply.hasClass = 1;
            } else if ( !Q_stricmp(attr, "apply.origin") ) {
                if ( IR_ParseVec3(val, r->apply.origin) ) r->apply.hasOrigin = 1;
            } else if ( !Q_stricmp(attr, "apply.angles") ) {
                if ( IR_ParseVec3(val, r->apply.angles) ) r->apply.hasAngles = 1;
            } else if ( !Q_stricmp(attr, "apply.angle") ) {
                r->apply.angle = (float)atof(val); r->apply.hasAngle = 1;
            } else if ( !Q_stricmp(attr, "apply.spawnflags") ) {
                r->apply.spawnflags = atoi(val); r->apply.hasSpawnflags = 1;
            } else if ( !Q_stricmp(attr, "apply.remove") ) {
                r->apply.removeEntity = ( atoi(val) != 0 );
            }
        }
    }
}

/* ---------------- Runtime application ---------------- */

/* Undo/Redo support */
#define IR_MAX_HISTORY 256
typedef enum { IRO_NONE=0, IRO_ADD, IRO_REMOVE, IRO_REPLACE, IRO_MOVE } ir_optype_t;
typedef struct {
    ir_optype_t type;
    char classA[64];
    char classB[64];
    float orgA[3];
    float orgB[3];
    float angA[3]; int hasAngA;
    float angB[3]; int hasAngB;
} ir_editop_t;

static ir_editop_t ir_undo[IR_MAX_HISTORY];
static int ir_undoCount = 0;
static ir_editop_t ir_redo[IR_MAX_HISTORY];
static int ir_redoCount = 0;
static qboolean ir_suppressLog = qfalse; /* suppress s_ir* logging during undo/redo */

static void IR_ClearRedo(void) { ir_redoCount = 0; }
static void IR_PushUndo(const ir_editop_t *op) { if ( ir_undoCount < IR_MAX_HISTORY ) { ir_undo[ir_undoCount++] = *op; } IR_ClearRedo(); }
static void IR_PushRedo(const ir_editop_t *op) { if ( ir_redoCount < IR_MAX_HISTORY ) { ir_redo[ir_redoCount++] = *op; } }

static gentity_t *IR_FindNearestItemByClassAt(const char *classname, const float *origin, float radius) {
    int i; gentity_t *best = NULL; float bestD2 = 9e9f;
    for ( i = MAX_CLIENTS; i < level.num_entities; ++i ) {
        gentity_t *e = &g_entities[i]; float dx,dy,dz,d2;
        if ( !e->inuse || !e->item ) continue;
        if ( classname && classname[0] ) {
            if ( !e->classname || Q_stricmp(e->classname, classname) != 0 ) continue;
        }
        dx = e->r.currentOrigin[0]-origin[0]; dy = e->r.currentOrigin[1]-origin[1]; dz = e->r.currentOrigin[2]-origin[2]; d2 = dx*dx+dy*dy+dz*dz;
        if ( d2 < bestD2 ) { bestD2 = d2; best = e; }
    }
    if ( best && radius > 0.0f ) {
        float dx = best->r.currentOrigin[0]-origin[0]; float dy = best->r.currentOrigin[1]-origin[1]; float dz = best->r.currentOrigin[2]-origin[2];
        float d2 = dx*dx+dy*dy+dz*dz; if ( d2 > radius*radius ) return NULL;
    }
    return best;
}

static gentity_t *IR_SpawnItemAt(const char *classname, const float *origin, const float *angles, qboolean hasAngles) {
    gitem_t *it = IR_FindItemByClassnameOrPickup( classname );
    gentity_t *it_ent;
    if ( !it ) return NULL;
    it_ent = G_Spawn();
    it_ent->classname = it->classname;
    VectorCopy( origin, it_ent->s.origin ); VectorCopy( origin, it_ent->r.currentOrigin ); VectorCopy( origin, it_ent->s.pos.trBase );
    if ( hasAngles && angles ) { VectorCopy( angles, it_ent->s.angles ); }
    G_SpawnItem( it_ent, it ); FinishSpawningItem( it_ent );
    /* ensure we are stationary on floor right away */
    it_ent->s.pos.trType = TR_STATIONARY;
    VectorClear( it_ent->s.pos.trDelta );
    return it_ent;
}

static void IR_MoveEntityTo(gentity_t *e, const float *origin) {
    VectorCopy( origin, e->s.origin );
    VectorCopy( origin, e->r.currentOrigin );
    VectorCopy( origin, e->s.pos.trBase );
    trap_LinkEntity( e );
    e->s.pos.trType = TR_STATIONARY;
    VectorClear( e->s.pos.trDelta );
}

static int IR_StringEquals(const char *a, const char *b) {
    if ( !a || !b ) return 0; return Q_stricmp(a, b) == 0;
}

static int IR_MapEquals(const char *map) {
    return Q_stricmp( map, g_mapname.string ) == 0;
}

static int IR_IsRuleMatch(const ir_rule_t *r, const gentity_t *ent) {
    float dx, dy, dz, dist2, tol;
    if ( r->match.classname[0] ) {
        if ( !IR_StringEquals( r->match.classname, ent->classname ) ) return 0;
    }
    if ( r->match.hasOrigin ) {
        dx = ent->s.origin[0] - r->match.origin[0];
        dy = ent->s.origin[1] - r->match.origin[1];
        dz = ent->s.origin[2] - r->match.origin[2];
        dist2 = dx*dx + dy*dy + dz*dz;
        tol = (float)( r->match.hasTolerance ? r->match.tolerance : 32 );
        if ( dist2 > tol * tol ) return 0;
    }
    return 1;
}

/* Apply a single rule to ent. Returns -1 remove, 1 applied, 0 no change. */
static int IR_ApplyRule(const ir_rule_t *r, gentity_t *ent) {
    if ( !IR_IsRuleMatch(r, ent) ) return 0;
    if ( r->apply.removeEntity ) {
        return -1;
    }
    if ( r->apply.hasClass && r->apply.classname[0] ) {
        ent->classname = G_NewString( r->apply.classname );
    }
    if ( r->apply.hasOrigin ) {
        VectorCopy( r->apply.origin, ent->s.origin );
        VectorCopy( ent->s.origin, ent->s.pos.trBase );
        VectorCopy( ent->s.origin, ent->r.currentOrigin );
    }
    if ( r->apply.hasAngles ) {
        VectorCopy( r->apply.angles, ent->s.angles );
    } else if ( r->apply.hasAngle ) {
        ent->s.angles[0] = 0.0f; ent->s.angles[1] = r->apply.angle; ent->s.angles[2] = 0.0f;
    }
    if ( r->apply.hasSpawnflags ) {
        ent->spawnflags = r->apply.spawnflags;
    }
    return 1;
}

/* Public: initialize (load config only) */
void IR_Init(void) {
    IR_Load();
}

/* Public: force reload of item rules */
void IR_Reload(void) {
    ir_loaded = qfalse;
    IR_Load();
}

/* Public: apply to entity before actual spawn. Return -1 to remove, 0/1 otherwise */
int IR_ApplyToEntity(gentity_t *ent) {
    int i, rc;
    qboolean changed = qfalse;
    const char *origClass;
    if ( !ir_loaded ) IR_Load();
    /* Ignore clients and worldspawn */
    if ( !ent || !ent->classname || !ent->classname[0] ) return 0;

    /* Targeted per-map rules first */
    for ( i = 0; i < ir_ruleCount; i++ ) {
        const ir_rule_t *r = &ir_rules[i];
        if ( !IR_MapEquals( r->map ) ) continue;
        rc = IR_ApplyRule( r, ent );
        if ( rc < 0 ) return -1;
        if ( rc > 0 ) { changed = qtrue; break; }
    }

    /* Class mapping (may still apply after targeted change) */
    origClass = ent->classname;
    for ( i = 0; i < ir_cmapCount; i++ ) {
        const ir_classmap_t *cm = &ir_cmaps[i];
        if ( !IR_MapEquals( cm->map ) ) continue;
        if ( IR_StringEquals( cm->fromClass, origClass ) ) {
            ent->classname = G_NewString( cm->toClass );
            changed = qtrue;
            break;
        }
    }

    return changed ? 1 : 0;
}

/* Original map items capture support */
static int IR_IsItemClass(const char *classname) {
    gitem_t *item;
    if ( !classname || !classname[0] ) return 0;
    for ( item = bg_itemlist + 1; item->classname; item++ ) {
        if ( !strcmp( item->classname, classname ) ) return 1;
    }
    return 0;
}

/* helper: find item by Quake classname (e.g., "item_quad"). Falls back to pickup name */
static gitem_t *IR_FindItemByClassnameOrPickup( const char *name ) {
    gitem_t *it;
    if ( !name || !name[0] ) return NULL;
    for ( it = bg_itemlist + 1; it->classname; ++it ) {
        if ( !Q_stricmp( it->classname, name ) ) return it;
    }
    /* fallback: try pickup name */
    return BG_FindItem( name );
}

void IR_ResetOriginalItems(void) {
    ir_orig_itemCount = 0;
}

void IR_RecordOriginalEntity(const gentity_t *ent) {
    ir_orig_item_t *dst;
    float waitv, randv;
    if ( !ent || !ent->classname ) return;
    if ( !IR_IsItemClass( ent->classname ) ) return;
    if ( ir_orig_itemCount >= IR_MAX_ORIG_ITEMS ) return;
    dst = &ir_orig_items[ ir_orig_itemCount++ ];
    Q_strncpyz( dst->classname, ent->classname, sizeof(dst->classname) );
    VectorCopy( ent->s.origin, dst->origin );
    VectorCopy( ent->s.angles, dst->angles );
    dst->spawnflags = ent->spawnflags;
    /* read original wait/random directly from current spawn vars */
    waitv = 0.0f; randv = 0.0f;
    G_SpawnFloat( "wait", "0", &waitv );
    G_SpawnFloat( "random", "0", &randv );
    dst->wait = waitv;
    dst->random = randv;
}

/* Utility: list all items on the current map (auth required) */
void listitems_f(gentity_t *ent) {
    int i;
    if ( !ent || !ent->client || !ent->authed ) {
        return;
    }
    trap_SendServerCommand( ent - g_entities, "print \"\n^2Items on map (original):^7\n\"" );
    trap_SendServerCommand( ent - g_entities, "print \"^7----------------------\n\"" );
    for ( i = 0; i < ir_orig_itemCount; ++i ) {
        char line[256];
        const ir_orig_item_t *it = &ir_orig_items[i];
        Com_sprintf( line, sizeof(line),
            "^5%3d^7 %-18s  org:^7 %.0f %.0f %.0f  yaw:^7 %.0f  flags:%d wait:%g rnd:%g",
            i + 1,
            it->classname,
            it->origin[0], it->origin[1], it->origin[2],
            it->angles[1],
            it->spawnflags,
            it->wait,
            it->random
        );
        trap_SendServerCommand( ent - g_entities, va("print \"%s\n\"", line) );
    }
}

/* Utility: list all available item definitions (auth required) */
void items_f(gentity_t *ent) {
    int idx;
    gitem_t *it;
    if ( !ent || !ent->client || !ent->authed ) {
        return;
    }
    trap_SendServerCommand( ent - g_entities, "print \"\n^2Available items:^7\n\"" );
    trap_SendServerCommand( ent - g_entities, "print \"^7----------------\n\"" );
    idx = 1;
    for ( it = bg_itemlist + 1; it->classname; ++it, ++idx ) {
        char line[160];
        Com_sprintf( line, sizeof(line), "^5%3d^7 %-24s tag:%d", idx, it->classname, it->giTag );
        trap_SendServerCommand( ent - g_entities, va("print \"%s\n\"", line) );
    }
}

/* Admin: reload rules (respects g_itemReplace) */
void irreload_f(gentity_t *ent) {
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( !g_itemReplace.integer ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3g_itemReplace is disabled\n\"" );
        return;
    }
    IR_Reload();
    trap_SendServerCommand( ent - g_entities, "print \"^2itemreplace: rules reloaded\n\"" );
}

/* Admin: list loaded rules and classmaps */
void irlist_f(gentity_t *ent) {
    int i;
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( !ir_loaded ) IR_Load();
    trap_SendServerCommand( ent - g_entities, "print \"\n^2ItemReplace: Rules^7\n\"" );
    trap_SendServerCommand( ent - g_entities, "print \"^7-------------------\n\"" );
    for ( i = 0; i < ir_ruleCount; ++i ) {
        char line[256];
        ir_rule_t *r = &ir_rules[i];
        Com_sprintf( line, sizeof(line), "^5%-12s ^7%-8s ^7match:%s%s%s  apply:%s%s%s%s%s",
            r->map, r->id,
            (r->match.classname[0]?r->match.classname:"*"),
            (r->match.hasOrigin?" org":""),
            (r->match.hasTolerance?" tol":""),
            (r->apply.removeEntity?"remove":""),
            (r->apply.hasClass?" class":""),
            (r->apply.hasOrigin?" org":""),
            (r->apply.hasAngles?" ang":""),
            (r->apply.hasAngle?" yaw":""),
            (r->apply.hasSpawnflags?" flags":"")
        );
        trap_SendServerCommand( ent - g_entities, va("print \"%s\n\"", line) );
    }
    trap_SendServerCommand( ent - g_entities, "print \"\n^2ItemReplace: Classmaps^7\n\"" );
    trap_SendServerCommand( ent - g_entities, "print \"^7----------------------\n\"" );
    for ( i = 0; i < ir_cmapCount; ++i ) {
        char line[160]; ir_classmap_t *cm = &ir_cmaps[i];
        Com_sprintf( line, sizeof(line), "^5%-12s ^7%s ^5-> ^7%s", cm->map, cm->fromClass, cm->toClass );
        trap_SendServerCommand( ent - g_entities, va("print \"%s\n\"", line) );
    }
}

/* Toggle real-time CenterPrint of nearest item rule match (similar to spawnscp) */
void irmatchcp_f(gentity_t *ent) {
    if ( !ent || !ent->client || !ent->authed ) return;
    ent->client->pers.itemCpEnabled = !ent->client->pers.itemCpEnabled;
    trap_SendServerCommand( ent - g_entities,
        ent->client->pers.itemCpEnabled ? "print \"^3irmatchcp: ^2ON\n\"" : "print \"^3irmatchcp: ^1OFF\n\""
    );
}

/* Admin: add an item at your feet: iradd <classname> [yaw] */
void iradd_f(gentity_t *ent) {
    char cls[64]; char yawbuf[32]; float yaw = 0.0f;
    gitem_t *it; gentity_t *it_ent; vec3_t org;
    if (!ent||!ent->client||!ent->authed) return;
    if ( trap_Argc() < 2 ) { trap_SendServerCommand(ent-g_entities, "print \"^3Usage: iradd <classname> [yaw]\n\"" ); return; }
    trap_Argv(1, cls, sizeof(cls));
    if ( trap_Argc() >= 3 ) { trap_Argv(2, yawbuf, sizeof(yawbuf)); yaw = (float)atof(yawbuf); }
    it = IR_FindItemByClassnameOrPickup( cls );
    if ( !it ) { trap_SendServerCommand(ent-g_entities, "print \"^1Unknown item classname\n\"" ); return; }
    VectorCopy( ent->client->ps.origin, org ); org[2] += 16;
    it_ent = IR_SpawnItemAt( it->classname, org, NULL, qfalse );
    if ( it_ent ) { it_ent->s.angles[0]=0.0f; it_ent->s.angles[1]=yaw; it_ent->s.angles[2]=0.0f; }
    trap_SendServerCommand(ent-g_entities, va("print \"^2Spawned %s at %2.f %2.f %2.f yaw %2.f\n\"", it->classname, org[0], org[1], org[2], yaw));
    if ( s_irAddCount < 128 && !ir_suppressLog ) {
        Q_strncpyz( s_irAdds[s_irAddCount].classname, it->classname, sizeof(s_irAdds[0].classname) );
        s_irAdds[s_irAddCount].origin[0]=org[0]; s_irAdds[s_irAddCount].origin[1]=org[1]; s_irAdds[s_irAddCount].origin[2]=org[2];
        s_irAdds[s_irAddCount].angles[0]=0.0f; s_irAdds[s_irAddCount].angles[1]=yaw; s_irAdds[s_irAddCount].angles[2]=0.0f; s_irAdds[s_irAddCount].hasAngles=1;
        s_irAddCount++;
    }
}

/* Admin: remove nearest item within radius: irrm [radius] */
void irrm_f(gentity_t *ent) {
    float radius = 128.0f; char rbuf[32];
    gentity_t *best = NULL; float bestD2 = 9e9f; vec3_t p; int i;
    if (!ent||!ent->client||!ent->authed) return;
    if ( trap_Argc() >= 2 ) { trap_Argv(1, rbuf, sizeof(rbuf)); radius = (float)atof(rbuf); if (radius <= 0) radius = 128.0f; }
    VectorCopy( ent->client->ps.origin, p );
    for ( i = 0; i < level.num_entities; ++i ) {
        gentity_t *e = &g_entities[i]; float dx, dy, dz, d2;
        if ( !e->inuse ) continue;
        if ( !e->item ) continue; /* keep only items */
        dx = e->r.currentOrigin[0]-p[0]; dy = e->r.currentOrigin[1]-p[1]; dz = e->r.currentOrigin[2]-p[2]; d2 = dx*dx+dy*dy+dz*dz;
        if ( d2 < bestD2 ) { bestD2 = d2; best = e; }
    }
    if ( best && bestD2 <= radius*radius ) {
        /* push undo: remove can be undone by add */
        {
            ir_editop_t op; memset(&op,0,sizeof(op)); op.type = IRO_REMOVE;
            Q_strncpyz(op.classA, best->classname?best->classname:"", sizeof(op.classA));
            VectorCopy( best->r.currentOrigin, op.orgA );
            VectorCopy( best->s.angles, op.angA ); op.hasAngA = 1;
            IR_PushUndo( &op );
        }
        trap_SendServerCommand( ent - g_entities, va("print \"^1Removed item %s at %2.f %2.f %2.f\n\"", best->classname?best->classname:"<unknown>", best->r.currentOrigin[0], best->r.currentOrigin[1], best->r.currentOrigin[2]) );
        if ( s_irRmCount < 128 && !ir_suppressLog ) {
            Q_strncpyz( s_irRms[s_irRmCount].classname, best->classname?best->classname:"", sizeof(s_irRms[0].classname) );
            s_irRms[s_irRmCount].origin[0]=best->r.currentOrigin[0]; s_irRms[s_irRmCount].origin[1]=best->r.currentOrigin[1]; s_irRms[s_irRmCount].origin[2]=best->r.currentOrigin[2];
            s_irRms[s_irRmCount].tolerance = 32;
            s_irRmCount++;
        }
        G_FreeEntity( best );
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3No nearby item to remove\n\"" );
    }
}

/* Admin: replace nearest item: irrepl <toClass> [radius] */
void irrepl_f(gentity_t *ent) {
    char tocls[64]; char rbuf[32]; float radius = 128.0f; gitem_t *toit; gentity_t *best=NULL; float bestD2=9e9f; vec3_t p; int i; vec3_t org; vec3_t ang;
    if (!ent||!ent->client||!ent->authed) return;
    if ( trap_Argc() < 2 ) { trap_SendServerCommand(ent-g_entities, "print \"^3Usage: irrepl <toClass> [radius]\n\"" ); return; }
    trap_Argv(1, tocls, sizeof(tocls));
    if ( trap_Argc() >= 3 ) { trap_Argv(2, rbuf, sizeof(rbuf)); radius = (float)atof(rbuf); if (radius <= 0) radius = 128.0f; }
    toit = IR_FindItemByClassnameOrPickup( tocls );
    if ( !toit ) { trap_SendServerCommand(ent-g_entities, "print \"^1Unknown item classname\n\"" ); return; }
    VectorCopy( ent->client->ps.origin, p );
    for ( i = 0; i < level.num_entities; ++i ) {
        gentity_t *e = &g_entities[i]; float dx, dy, dz, d2;
        if ( !e->inuse ) continue; if ( !e->item ) continue;
        dx = e->r.currentOrigin[0]-p[0]; dy = e->r.currentOrigin[1]-p[1]; dz = e->r.currentOrigin[2]-p[2]; d2 = dx*dx+dy*dy+dz*dz;
        if ( d2 < bestD2 ) { bestD2 = d2; best = e; }
    }
    if ( best && bestD2 <= radius*radius ) {
        VectorCopy( best->s.origin, org ); VectorCopy( best->s.angles, ang );
        trap_SendServerCommand( ent - g_entities, va("print \"^5Replace %s -> %s at %2.f %2.f %2.f\n\"", best->classname?best->classname:"<unknown>", toit->classname, org[0], org[1], org[2]) );
        if ( s_irReplCount < 128 && !ir_suppressLog ) {
            Q_strncpyz( s_irRepls[s_irReplCount].from, best->classname?best->classname:"", sizeof(s_irRepls[0].from) );
            Q_strncpyz( s_irRepls[s_irReplCount].to, toit->classname, sizeof(s_irRepls[0].to) );
            s_irRepls[s_irReplCount].origin[0]=org[0]; s_irRepls[s_irReplCount].origin[1]=org[1]; s_irRepls[s_irReplCount].origin[2]=org[2];
            VectorCopy( ang, s_irRepls[s_irReplCount].angles ); s_irRepls[s_irReplCount].hasAngles=1;
            s_irRepls[s_irReplCount].hasApplyOrigin = 0;
            s_irReplCount++;
        }
        /* push undo: replacement can be undone by reverse replacement */
        {
            ir_editop_t op; memset(&op,0,sizeof(op)); op.type = IRO_REPLACE;
            Q_strncpyz(op.classA, best->classname?best->classname:"", sizeof(op.classA));
            Q_strncpyz(op.classB, toit->classname, sizeof(op.classB));
            VectorCopy( org, op.orgA ); VectorCopy( ang, op.angA ); op.hasAngA = 1;
            IR_PushUndo( &op );
        }
        G_FreeEntity( best );
        {
            gentity_t *it_ent = IR_SpawnItemAt( toit->classname, org, ang, qtrue );
        }
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3No nearby item to replace\n\"" );
    }
}

/* Admin: move nearest item: irmove [x y z] */
void irmove_f(gentity_t *ent) {
    float radius = 128.0f;
    gentity_t *best = NULL; float bestD2 = 9e9f; vec3_t p; int i;
    vec3_t newOrg; qboolean haveCoords = qfalse;
    char xb[32], yb[32], zb[32];
    if (!ent||!ent->client||!ent->authed) return;
    if ( trap_Argc() >= 4 ) {
        trap_Argv(1, xb, sizeof(xb)); trap_Argv(2, yb, sizeof(yb)); trap_Argv(3, zb, sizeof(zb));
        newOrg[0] = (float)atof(xb); newOrg[1] = (float)atof(yb); newOrg[2] = (float)atof(zb);
        haveCoords = qtrue;
    }
    VectorCopy( ent->client->ps.origin, p );
    for ( i = 0; i < level.num_entities; ++i ) {
        gentity_t *e = &g_entities[i]; float dx, dy, dz, d2;
        if ( !e->inuse ) continue; if ( !e->item ) continue;
        dx = e->r.currentOrigin[0]-p[0]; dy = e->r.currentOrigin[1]-p[1]; dz = e->r.currentOrigin[2]-p[2]; d2 = dx*dx+dy*dy+dz*dz;
        if ( d2 < bestD2 ) { bestD2 = d2; best = e; }
    }
    if ( !best || bestD2 > radius*radius ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3No nearby item to move\n\"" );
        return;
    }
    if ( !haveCoords ) {
        VectorCopy( ent->client->ps.origin, newOrg ); newOrg[2] += 16;
    }
    /* push undo: move can be undone by move back */
    {
        ir_editop_t op; memset(&op,0,sizeof(op)); op.type = IRO_MOVE;
        Q_strncpyz(op.classA, best->classname?best->classname:"", sizeof(op.classA));
        VectorCopy( best->r.currentOrigin, op.orgA );
        VectorCopy( newOrg, op.orgB );
        IR_PushUndo( &op );
    }
    IR_MoveEntityTo( best, newOrg );
    trap_SendServerCommand( ent - g_entities, va("print \"^2Moved %s to %2.f %2.f %2.f\n\"", best->classname?best->classname:"<unknown>", newOrg[0], newOrg[1], newOrg[2]) );
    /* Log as replacement with same class to persist via irsave */
    if ( s_irReplCount < 128 && !ir_suppressLog ) {
        Q_strncpyz( s_irRepls[s_irReplCount].from, best->classname?best->classname:"", sizeof(s_irRepls[0].from) );
        Q_strncpyz( s_irRepls[s_irReplCount].to, best->classname?best->classname:"", sizeof(s_irRepls[0].to) );
        s_irRepls[s_irReplCount].origin[0]=best->r.currentOrigin[0]; s_irRepls[s_irReplCount].origin[1]=best->r.currentOrigin[1]; s_irRepls[s_irReplCount].origin[2]=best->r.currentOrigin[2];
        VectorCopy( best->s.angles, s_irRepls[s_irReplCount].angles ); s_irRepls[s_irReplCount].hasAngles=1;
        VectorCopy( newOrg, s_irRepls[s_irReplCount].applyOrigin ); s_irRepls[s_irReplCount].hasApplyOrigin = 1;
        s_irReplCount++;
    }
}

/* Admin: undo last item edit */
void irundo_f(gentity_t *ent) {
    ir_editop_t op, redo;
    gentity_t *e;
    if (!ent||!ent->client||!ent->authed) return;
    if ( ir_undoCount <= 0 ) { trap_SendServerCommand( ent - g_entities, "print \"^3Nothing to undo\n\"" ); return; }
    op = ir_undo[ --ir_undoCount ];
    memset(&redo,0,sizeof(redo));
    ir_suppressLog = qtrue;
    switch ( op.type ) {
    case IRO_ADD:
        e = IR_FindNearestItemByClassAt( op.classA, op.orgA, 64.0f );
        if ( e ) { redo = op; G_FreeEntity( e ); }
        break;
    case IRO_REMOVE:
        e = IR_SpawnItemAt( op.classA, op.orgA, op.hasAngA?op.angA:NULL, op.hasAngA );
        redo = op; /* redo removal */
        break;
    case IRO_REPLACE:
        /* find new class at same origin, replace back to old */
        e = IR_FindNearestItemByClassAt( op.classB, op.orgA, 64.0f );
        if ( e ) {
            vec3_t org, ang;
            VectorCopy( op.orgA, org ); VectorCopy( op.angA, ang );
            {
                gitem_t *toit = IR_FindItemByClassnameOrPickup( op.classA );
                if ( toit ) {
                    gentity_t *ne;
                    G_FreeEntity( e );
                    ne = IR_SpawnItemAt( toit->classname, org, ang, op.hasAngA ); (void)ne;
                    /* redo becomes forward replace */
                    Q_strncpyz( redo.classA, op.classA, sizeof(redo.classA) );
                    Q_strncpyz( redo.classB, op.classB, sizeof(redo.classB) );
                    VectorCopy( op.orgA, redo.orgA ); VectorCopy( op.angA, redo.angA ); redo.hasAngA = op.hasAngA;
                }
            }
        }
        break;
    case IRO_MOVE:
        e = IR_FindNearestItemByClassAt( op.classA, op.orgB, 64.0f );
        if ( e ) { vec3_t back; VectorCopy( op.orgA, back ); IR_MoveEntityTo( e, back ); redo = op; }
        break;
    default: break;
    }
    ir_suppressLog = qfalse;
    if ( op.type != IRO_NONE ) { IR_PushRedo( &redo ); }
    trap_SendServerCommand( ent - g_entities, "print \"^2Undo done\n\"" );
}

/* Admin: redo last undone edit */
void irredo_f(gentity_t *ent) {
    ir_editop_t op, undo;
    gentity_t *e;
    if (!ent||!ent->client||!ent->authed) return;
    if ( ir_redoCount <= 0 ) { trap_SendServerCommand( ent - g_entities, "print \"^3Nothing to redo\n\"" ); return; }
    op = ir_redo[ --ir_redoCount ];
    memset(&undo,0,sizeof(undo));
    ir_suppressLog = qtrue;
    switch ( op.type ) {
    case IRO_ADD:
        e = IR_SpawnItemAt( op.classA, op.orgA, op.hasAngA?op.angA:NULL, op.hasAngA ); undo = op; break;
    case IRO_REMOVE:
        e = IR_FindNearestItemByClassAt( op.classA, op.orgA, 64.0f ); if ( e ) { undo = op; G_FreeEntity( e ); } break;
    case IRO_REPLACE:
        e = IR_FindNearestItemByClassAt( op.classA, op.orgA, 64.0f );
        if ( e ) { vec3_t org, ang; VectorCopy( op.orgA, org ); VectorCopy( op.angA, ang );
            {
                gitem_t *toit = IR_FindItemByClassnameOrPickup( op.classB );
                if ( toit ) { gentity_t *ne; G_FreeEntity( e ); ne = IR_SpawnItemAt( toit->classname, org, ang, op.hasAngA ); (void)ne; undo = op; }
            }
        }
        break;
    case IRO_MOVE:
        e = IR_FindNearestItemByClassAt( op.classA, op.orgA, 64.0f ); if ( e ) { IR_MoveEntityTo( e, op.orgB ); undo = op; } break;
    default: break;
    }
    ir_suppressLog = qfalse;
    if ( op.type != IRO_NONE ) { IR_PushUndo( &undo ); }
    trap_SendServerCommand( ent - g_entities, "print \"^2Redo done\n\"" );
}

/* Admin: respawn all items: kill and respawn per original map items and current adds */
void irrespawn_f(gentity_t *ent) {
    int i;
    if (!ent||!ent->client||!ent->authed) return;
    /* remove all item entities */
    for ( i = MAX_CLIENTS; i < level.num_entities; ++i ) {
        gentity_t *e = &g_entities[i];
        if ( !e->inuse ) continue; if ( !e->item ) continue;
        if ( e->item->giTag == PW_NEUTRALFLAG || e->item->giTag == PW_REDFLAG || e->item->giTag == PW_BLUEFLAG ) continue;
        G_FreeEntity( e );
    }
    /* respawn original map items from captured list, applying current rules */
    for ( i = 0; i < ir_orig_itemCount; ++i ) {
        gentity_t stub;
        int rc;
        memset( &stub, 0, sizeof(stub) );
        stub.classname = ir_orig_items[i].classname;
        VectorCopy( ir_orig_items[i].origin, stub.s.origin );
        VectorCopy( ir_orig_items[i].angles, stub.s.angles );
        stub.spawnflags = ir_orig_items[i].spawnflags;
        rc = IR_ApplyToEntity( &stub );
        if ( rc < 0 ) {
            continue; /* removed by rule */
        }
        IR_SpawnItemAt( stub.classname, stub.s.origin, stub.s.angles, qtrue );
    }
    /* respawn persistent adds for current map */
    IR_SpawnAdds();
    trap_SendServerCommand( ent - g_entities, "print \"^2Items respawned\n\"" );
}

/* Save current edit log into itemreplace.txt and itemadds.txt */
void irsave_f(gentity_t *ent) {
    fileHandle_t f; int openRes; int i; char line[256];
    if (!ent||!ent->client||!ent->authed) return;
    /* append removal/replacement rules */
    openRes = trap_FS_FOpenFile( "itemreplace.txt", &f, FS_APPEND );
    if ( openRes >= 0 ) {
        for ( i=0; i<s_irRmCount; ++i ) {
            Com_sprintf( line, sizeof(line), "map.%s.rule.rm%03d.match.classname = %s\n", g_mapname.string, i, s_irRms[i].classname ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.rule.rm%03d.match.origin = %.0f %.0f %.0f\n", g_mapname.string, i, s_irRms[i].origin[0], s_irRms[i].origin[1], s_irRms[i].origin[2] ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.rule.rm%03d.match.tolerance = %d\n", g_mapname.string, i, s_irRms[i].tolerance ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.rule.rm%03d.apply.remove = 1\n", g_mapname.string, i ); trap_FS_Write( line, (int)strlen(line), f );
        }
        for ( i=0; i<s_irReplCount; ++i ) {
            Com_sprintf( line, sizeof(line), "map.%s.rule.rp%03d.match.classname = %s\n", g_mapname.string, i, s_irRepls[i].from ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.rule.rp%03d.match.origin = %.0f %.0f %.0f\n", g_mapname.string, i, s_irRepls[i].origin[0], s_irRepls[i].origin[1], s_irRepls[i].origin[2] ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.rule.rp%03d.match.tolerance = %d\n", g_mapname.string, i, 32 ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.rule.rp%03d.apply.classname = %s\n", g_mapname.string, i, s_irRepls[i].to ); trap_FS_Write( line, (int)strlen(line), f );
            if ( s_irRepls[i].hasAngles ) {
                Com_sprintf( line, sizeof(line), "map.%s.rule.rp%03d.apply.angles = %.0f %.0f %.0f\n", g_mapname.string, i, s_irRepls[i].angles[0], s_irRepls[i].angles[1], s_irRepls[i].angles[2] ); trap_FS_Write( line, (int)strlen(line), f );
            }
            if ( s_irRepls[i].hasApplyOrigin ) {
                Com_sprintf( line, sizeof(line), "map.%s.rule.rp%03d.apply.origin = %.0f %.0f %.0f\n", g_mapname.string, i, s_irRepls[i].applyOrigin[0], s_irRepls[i].applyOrigin[1], s_irRepls[i].applyOrigin[2] ); trap_FS_Write( line, (int)strlen(line), f );
            }
        }
        trap_FS_FCloseFile( f );
    }
    /* append additions into itemreplace.txt as map.<map>.add.<id>.* */
    openRes = trap_FS_FOpenFile( "itemreplace.txt", &f, FS_APPEND );
    if ( openRes >= 0 ) {
        for ( i=0; i<s_irAddCount; ++i ) {
            Com_sprintf( line, sizeof(line), "map.%s.add.ad%03d.classname = %s\n", g_mapname.string, i, s_irAdds[i].classname ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.add.ad%03d.origin = %.0f %.0f %.0f\n", g_mapname.string, i, s_irAdds[i].origin[0], s_irAdds[i].origin[1], s_irAdds[i].origin[2] ); trap_FS_Write( line, (int)strlen(line), f );
            Com_sprintf( line, sizeof(line), "map.%s.add.ad%03d.angles = %.0f %.0f %.0f\n", g_mapname.string, i, s_irAdds[i].angles[0], s_irAdds[i].angles[1], s_irAdds[i].angles[2] ); trap_FS_Write( line, (int)strlen(line), f );
        }
        trap_FS_FCloseFile( f );
    }
    trap_SendServerCommand( ent - g_entities, va("print \"^2Saved: %d removed, %d replaced, %d added\n\"", s_irRmCount, s_irReplCount, s_irAddCount) );
}

/* Load additions for current map and spawn items (call after world is spawned) */
void IR_SpawnAdds(void) {
    int i;
    if ( !ir_loaded ) {
        IR_Load();
    }
    for ( i = 0; i < ir_addCount; ++i ) {
        ir_addrule_t *a = &ir_adds[i];
        gitem_t *it;
        gentity_t *it_ent;
        if ( Q_stricmp( a->map, g_mapname.string ) != 0 ) {
            continue;
        }
        if ( !a->classname[0] || !a->hasOrigin ) {
            continue;
        }
        it = IR_FindItemByClassnameOrPickup( a->classname );
        if ( !it ) {
            continue;
        }
        it_ent = G_Spawn();
        it_ent->classname = it->classname;
        VectorCopy( a->origin, it_ent->s.origin );
        VectorCopy( a->origin, it_ent->r.currentOrigin );
        VectorCopy( a->origin, it_ent->s.pos.trBase );
        if ( a->hasAngles ) {
            VectorCopy( a->angles, it_ent->s.angles );
        }
        G_SpawnItem( it_ent, it );
        FinishSpawningItem( it_ent );
    }
}


