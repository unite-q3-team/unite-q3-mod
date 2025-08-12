// code/game/cmds/itemreplace.c
// Runtime item replacement system (per-map), similar in spirit to votesystem
// C89-compatible

#include "cmds.h"

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
    ir_ruleCount = 0; ir_cmapCount = 0;

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

/* Public: initialize (load config) */
void IR_Init(void) {
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

