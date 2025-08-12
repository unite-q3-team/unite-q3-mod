// code/game/cmds/votesystem.c
#include "cmds.h"

/* Local map cache for vote helpers */
static int vs_cachedMapCount = 0;
static char vs_cachedMaps[512][64];

/* Runtime-configurable vote rules (loaded from votesystem.txt) */
#define VS_MAX_RULES 64
typedef struct {
    char name[32];
    int enabled;            /* 1 enabled (default), 0 disabled */
    char action[256];       /* template: may contain {arg} */
    char usage[128];        /* optional usage string */
    int requireArg;         /* 1 if argument is required */
    int numericOnly;        /* 1 if argument must be numeric */
    char displayName[64];   /* friendly name for lists/announcements */
    char valueCvar[64];     /* cvar to read current value from (optional) */
    int visible;            /* 1 visible in /cv by default (default 1) */
} vs_rule_t;

static vs_rule_t vs_rules[VS_MAX_RULES];
static int vs_ruleCount = 0;
static qboolean vs_rulesLoaded = qfalse;

static void VS_ResetRules(void) {
    int i;
    vs_ruleCount = 0;
    for ( i = 0; i < VS_MAX_RULES; i++ ) {
        vs_rules[i].name[0] = '\0';
        vs_rules[i].enabled = 1;
        vs_rules[i].action[0] = '\0';
        vs_rules[i].usage[0] = '\0';
        vs_rules[i].requireArg = 0;
        vs_rules[i].numericOnly = 0;
        vs_rules[i].displayName[0] = '\0';
        vs_rules[i].valueCvar[0] = '\0';
        vs_rules[i].visible = 1;
    }
}

static void VS_TrimRight(char *s) {
    int n;
    n = (int)strlen(s);
    while ( n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n') ) {
        s[n-1] = '\0';
        n--;
    }
}

static void VS_TrimLeft(char *s) {
    int i;
    int n;
    char *p;
    n = (int)strlen(s);
    i = 0;
    while ( i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') ) {
        i++;
    }
    if ( i > 0 ) {
        p = s + i;
        memmove(s, p, strlen(p) + 1);
    }
}

static void VS_Trim(char *s) {
    VS_TrimRight(s);
    VS_TrimLeft(s);
}

static void VS_Unquote(char *s) {
    int n;
    n = (int)strlen(s);
    if ( n >= 2 && ((s[0] == '"' && s[n-1] == '"') || (s[0] == '\'' && s[n-1] == '\'')) ) {
        s[n-1] = '\0';
        memmove(s, s + 1, n - 1);
    }
}

static int VS_StringToBool(const char *v, int defaultValue) {
    if ( v == NULL || v[0] == '\0' ) return defaultValue;
    if ( !Q_stricmp(v, "1") || !Q_stricmp(v, "true") || !Q_stricmp(v, "yes") || !Q_stricmp(v, "on") ) return 1;
    if ( !Q_stricmp(v, "0") || !Q_stricmp(v, "false") || !Q_stricmp(v, "no") || !Q_stricmp(v, "off") ) return 0;
    return atoi(v) != 0;
}

static int VS_FindRuleIndex(const char *name) {
    int i;
    for ( i = 0; i < vs_ruleCount; i++ ) {
        if ( !Q_stricmp(vs_rules[i].name, name) ) {
            return i;
        }
    }
    return -1;
}

static vs_rule_t *VS_GetRule(const char *name) {
    int idx;
    idx = VS_FindRuleIndex(name);
    if ( idx >= 0 ) return &vs_rules[idx];
    return NULL;
}

static vs_rule_t *VS_GetOrCreateRule(const char *name) {
    int idx;
    idx = VS_FindRuleIndex(name);
    if ( idx >= 0 ) return &vs_rules[idx];
    if ( vs_ruleCount >= VS_MAX_RULES ) return NULL;
    Q_strncpyz(vs_rules[vs_ruleCount].name, name, sizeof(vs_rules[0].name));
    vs_rules[vs_ruleCount].enabled = 1;
    vs_rules[vs_ruleCount].action[0] = '\0';
    vs_rules[vs_ruleCount].usage[0] = '\0';
    vs_rules[vs_ruleCount].requireArg = 0;
    vs_rules[vs_ruleCount].numericOnly = 0;
    vs_rules[vs_ruleCount].displayName[0] = '\0';
    vs_rules[vs_ruleCount].valueCvar[0] = '\0';
    vs_rules[vs_ruleCount].visible = 1;
    vs_ruleCount++;
    return &vs_rules[vs_ruleCount - 1];
}

static vs_rule_t *VS_FindRuleByActionPrefix(const char *prefix) {
    int i;
    int plen;
    plen = (int)strlen(prefix);
    for ( i = 0; i < vs_ruleCount; i++ ) {
        if ( vs_rules[i].enabled && vs_rules[i].action[0] && Q_strncmp(vs_rules[i].action, prefix, plen) == 0 ) {
            return &vs_rules[i];
        }
    }
    return NULL;
}

static void VS_ReplaceTemplate(const char *tmpl, const char *arg, char *out, int outSize) {
    int i;
    int j;
    int tlen;
    const char *pat;
    int alen;
    if ( outSize <= 0 ) return;
    out[0] = '\0';
    j = 0;
    tlen = (int)strlen(tmpl);
    pat = "{arg}";
    alen = (int)strlen(pat);
    for ( i = 0; i < tlen && j < outSize - 1; ) {
        if ( i + alen <= tlen && Q_strncmp(tmpl + i, pat, alen) == 0 ) {
            const char *a;
            a = arg ? arg : "";
            while ( *a && j < outSize - 1 ) {
                out[j++] = *a++;
            }
            i += alen;
        } else {
            out[j++] = tmpl[i++];
        }
    }
    out[j] = '\0';
}

/* default rules content written when no votesystem.txt exists */
static const char *vs_defaultRulesText =
    "# votesystem.txt — runtime vote rules\n"
    "# Attributes:\n"
    "#   enabled:      1 or 0 (default 1)\n"
    "#   action:       server command template; {arg} is replaced by the vote argument\n"
    "#   usage:        message shown when argument is missing/invalid\n"
    "#   requireArg:   1 if argument is required (default 0)\n"
    "#   numericOnly:  1 if argument must be a number (default 0)\n"
    "\n"
    "map.name = Map change\n"
    "map.enabled = 1\n"
    "map_restart.name = Map restart\n"
    "map_restart.enabled = 1\n"
    "map_restart.action = map_restart\n"
    "g_gametype.name = Gametype\n"
    "g_gametype.enabled = 1\n"
    "g_gametype.visible = 1\n"
    "g_gametype.cvar = g_gametype\n"
    "nextmap.name = Next map\n"
    "nextmap.enabled = 1\n"
    "nextmap.action = rotate\n"
    "rotate.enabled = 1\n"
    "\n"
    "# Use cvar names for current values in list\n"
    "instagib.cvar = g_instagib\n"
    "freeze.cvar = g_freeze\n"
    "noquad.cvar = disable_item_quad\n"
    "timelimit.cvar = timelimit\n"
    "fraglimit.cvar = fraglimit\n"
    "capturelimit.cvar = capturelimit\n"
    "\n"
    "instagib.name = Instagib\n"
    "instagib.enabled = 1\n"
    "instagib.action = g_instagib {arg}; map_restart\n"
    "instagib.requireArg = 1\n"
    "instagib.numericOnly = 1\n"
    "instagib.usage = Usage: instagib <0|1>\n"
    "\n"
    "freeze.name = Freeze tag\n"
    "freeze.enabled = 1\n"
    "freeze.action = g_freeze {arg}; map_restart\n"
    "freeze.requireArg = 1\n"
    "freeze.numericOnly = 1\n"
    "freeze.usage = Usage: freeze <0|1>\n"
    "\n"
    "noquad.name = Disable Quad\n"
    "noquad.enabled = 1\n"
    "noquad.action = set disable_item_quad {arg}; map_restart\n"
    "noquad.requireArg = 1\n"
    "noquad.numericOnly = 1\n"
    "noquad.usage = Usage: noquad <0|1>\n"
    "\n"
    "timelimit.name = Time limit\n"
    "timelimit.enabled = 0\n"
    "timelimit.visible = 0\n"
    "timelimit.action = timelimit {arg}\n"
    "timelimit.requireArg = 1\n"
    "timelimit.numericOnly = 1\n"
    "\n"
    "fraglimit.name = Frag limit\n"
    "fraglimit.enabled = 0\n"
    "fraglimit.visible = 0\n"
    "fraglimit.action = fraglimit {arg}\n"
    "fraglimit.requireArg = 1\n"
    "fraglimit.numericOnly = 1\n"
    "\n"
    "capturelimit.name = Capture limit\n"
    "capturelimit.enabled = 0\n"
    "capturelimit.visible = 0\n"
    "capturelimit.action = capturelimit {arg}\n"
    "capturelimit.requireArg = 1\n"
    "capturelimit.numericOnly = 1\n"
    "\n"
    "clientkick.name = Kick client\n"
    "clientkick.enabled = 0\n"
    "clientkick.visible = 0\n"
    "clientkick.action = clientkick {arg}\n"
    "clientkick.requireArg = 1\n"
    "clientkick.numericOnly = 1\n"
    "clientkick.usage = Usage: clientkick <clientNum>\n"
    "\n"
    "kick.name = Kick player\n"
    "kick.enabled = 0\n"
    "kick.visible = 0\n"
    "kick.action = kick {arg}\n"
    "kick.requireArg = 1\n"
    "kick.numericOnly = 0\n"
    "kick.usage = Usage: kick <name>\n";

static void VS_CreateDefaultRulesFile(void) {
    fileHandle_t f;
    int openRes;
    openRes = trap_FS_FOpenFile("votesystem.txt", &f, FS_WRITE);
    if ( openRes >= 0 ) {
        G_Printf("votesystem: creating default votesystem.txt\n");
        trap_FS_Write( vs_defaultRulesText, (int)strlen(vs_defaultRulesText), f );
        trap_FS_FCloseFile( f );
        G_Printf("votesystem: default votesystem.txt written\n");
    } else {
        G_Printf("votesystem: failed to create votesystem.txt (FS_WRITE)\n");
    }
}

static void VS_LoadRules(void) {
    fileHandle_t f;
    int flen;
    char *buf;
    char *p;
    if ( vs_rulesLoaded ) return;
    vs_rulesLoaded = qtrue;
    VS_ResetRules();

    flen = trap_FS_FOpenFile("votesystem.txt", &f, FS_READ);
    if ( flen <= 0 ) {
        /* auto-create default file in mod folder and try again */
        G_Printf("votesystem: votesystem.txt not found, creating default...\n");
        VS_CreateDefaultRulesFile();
        flen = trap_FS_FOpenFile("votesystem.txt", &f, FS_READ);
        if ( flen <= 0 ) {
            G_Printf("votesystem: still cannot open votesystem.txt after create attempt\n");
            return;
        }
    }
    if ( flen > 32 * 1024 ) flen = 32 * 1024;
    buf = (char*)G_Alloc(flen + 1);
    if ( !buf ) {
        trap_FS_FCloseFile(f);
        return;
    }
    trap_FS_Read(buf, flen, f);
    trap_FS_FCloseFile(f);
    buf[flen] = '\0';

    p = buf;
    while ( *p ) {
        char line[512];
        char *nl;
        int linelen;
        char *comment;
        char *eq;
        char key[128];
        char val[256];
        char name[64];
        char attr[64];
        vs_rule_t *rule;
        int dotPos;

        nl = strchr(p, '\n');
        if ( nl ) linelen = (int)(nl - p); else linelen = (int)strlen(p);
        if ( linelen > (int)sizeof(line) - 1 ) linelen = (int)sizeof(line) - 1;
        Q_strncpyz(line, p, linelen + 1);
        if ( nl ) p = nl + 1; else p = p + linelen;

        /* strip comments starting with # or // */
        comment = strstr(line, "//");
        if ( comment ) *comment = '\0';
        comment = strchr(line, '#');
        if ( comment ) *comment = '\0';

        VS_Trim(line);
        if ( line[0] == '\0' ) continue;

        eq = strchr(line, '=');
        if ( !eq ) continue;
        Q_strncpyz(key, line, (int)(eq - line) + 1);
        Q_strncpyz(val, eq + 1, sizeof(val));
        VS_Trim(key);
        VS_Trim(val);
        VS_Unquote(val);

        /* key format: <name>.<attr> */
        dotPos = -1;
        {
            int i;
            int klen;
            klen = (int)strlen(key);
            for ( i = 0; i < klen; i++ ) {
                if ( key[i] == '.' ) { dotPos = i; break; }
            }
        }
        if ( dotPos <= 0 ) continue;
        Q_strncpyz(name, key, dotPos + 1);
        Q_strncpyz(attr, key + dotPos + 1, sizeof(attr));
        VS_Trim(name);
        VS_Trim(attr);
        if ( name[0] == '\0' || attr[0] == '\0' ) continue;

        rule = VS_GetOrCreateRule(name);
        if ( !rule ) continue;

        if ( !Q_stricmp(attr, "enabled") ) {
            rule->enabled = VS_StringToBool(val, 1);
        } else if ( !Q_stricmp(attr, "action") ) {
            Q_strncpyz(rule->action, val, sizeof(rule->action));
        } else if ( !Q_stricmp(attr, "usage") ) {
            Q_strncpyz(rule->usage, val, sizeof(rule->usage));
        } else if ( !Q_stricmp(attr, "requireArg") ) {
            rule->requireArg = VS_StringToBool(val, 0);
        } else if ( !Q_stricmp(attr, "numericOnly") ) {
            rule->numericOnly = VS_StringToBool(val, 0);
        } else if ( !Q_stricmp(attr, "name") ) {
            Q_strncpyz(rule->displayName, val, sizeof(rule->displayName));
        } else if ( !Q_stricmp(attr, "cvar") || !Q_stricmp(attr, "value") ) {
            Q_strncpyz(rule->valueCvar, val, sizeof(rule->valueCvar));
        } else if ( !Q_stricmp(attr, "visible") ) {
            rule->visible = VS_StringToBool(val, 1);
        }
    }
    /* buffer is allocated with G_Alloc; free is not required for VM */
}

static void VS_GetDisplayNameFor(const char *cmdKey, char *out, int outSize) {
    vs_rule_t *r;
    int i;
    r = VS_GetRule(cmdKey);
    if ( r ) {
        if ( r->displayName[0] ) Q_strncpyz(out, r->displayName, outSize);
        else Q_strncpyz(out, cmdKey, outSize);
        return;
    }
    /* Fallback: find a rule that maps to this command via action template prefix */
    for ( i = 0; i < vs_ruleCount; i++ ) {
        if ( vs_rules[i].action[0] && Q_strncmp(vs_rules[i].action, cmdKey, (int)strlen(cmdKey)) == 0 ) {
            if ( vs_rules[i].displayName[0] ) Q_strncpyz(out, vs_rules[i].displayName, outSize);
            else Q_strncpyz(out, vs_rules[i].name, outSize);
            return;
        }
    }
    Q_strncpyz(out, cmdKey, outSize);
}

static void VS_GetValueForKey(const char *cmdKey, char *out, int outSize) {
    vs_rule_t *r;
    out[0] = '\0';
    r = VS_GetRule(cmdKey);
    if ( r && r->valueCvar[0] ) {
        trap_Cvar_VariableStringBuffer( r->valueCvar, out, outSize );
        return;
    }
    /* built-in fallbacks */
    if ( !Q_stricmp(cmdKey, "map") ) {
        Q_strncpyz( out, g_mapname.string, outSize );
        return;
    }
}

static void VS_ForceReloadRules(void) {
    vs_rulesLoaded = qfalse;
    VS_LoadRules();
}

static void VS_EnsureMapListCache(void) {
    char listbuf[8192];
    int count;
    int i;
    int pos;
    if ( vs_cachedMapCount > 0 ) {
        return;
    }
    vs_cachedMapCount = 0;
    count = trap_FS_GetFileList( "maps", ".bsp", listbuf, sizeof(listbuf) );
    pos = 0;
    for ( i = 0; i < count && vs_cachedMapCount < 512; ++i ) {
        char *nm;
        int nlen;
        if ( pos >= (int)sizeof(listbuf) ) {
            break;
        }
        nm = &listbuf[pos];
        nlen = (int)strlen( nm );
        if ( nlen > 0 ) {
            char tmp[64];
            Q_strncpyz( tmp, nm, sizeof(tmp) );
            nlen = (int)strlen( tmp );
            if ( nlen > 4 && !Q_stricmp( tmp + nlen - 4, ".bsp" ) ) {
                tmp[nlen - 4] = '\0';
            }
            Q_strncpyz( vs_cachedMaps[vs_cachedMapCount], tmp, sizeof(vs_cachedMaps[0]) );
            vs_cachedMapCount++;
        }
        pos += (int)strlen( nm ) + 1;
    }
}

static qboolean VS_GetMapNameByIndex( int indexOneBased, char *out, int outSize ) {
    VS_EnsureMapListCache();
    if ( indexOneBased <= 0 || indexOneBased > vs_cachedMapCount ) {
        return qfalse;
    }
    Q_strncpyz( out, vs_cachedMaps[indexOneBased - 1], outSize );
    return qtrue;
}

static void VS_PrintMapList( gentity_t *ent ) {
    char row[256];
    char buf[MAX_STRING_CHARS];
    int len;
    int perRow;
    int i;
    VS_EnsureMapListCache();
    buf[0] = '\0';
    len = 0;
    perRow = 3;
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^2Available Maps:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7---------------\n" );
    row[0] = '\0';
    for ( i = 0; i < vs_cachedMapCount; ++i ) {
        char cell[96];
        Com_sprintf( cell, sizeof(cell), "^7%3d.^7 ^2%-16s^7", i + 1, vs_cachedMaps[i] );
        Q_strcat( row, sizeof(row), cell );
        if ( ((i + 1) % perRow) == 0 || i + 1 == vs_cachedMapCount ) {
            len += Com_sprintf( buf + len, sizeof(buf) - len, "%s\n", row );
            row[0] = '\0';
            if ( len > (int)sizeof(buf) - 256 ) {
                trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
                buf[0] = '\0';
                len = 0;
            }
        } else {
            Q_strcat( row, sizeof(row), "  " );
        }
    }
    if ( len > 0 ) {
        trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
    }
}

static const char *voteCommands[] = {
    "map",
    "g_gametype"
};

static void Cmd_CV_HelpList( gentity_t *ent ) {
    char buf[MAX_STRING_CHARS];
    int len;
    int i2;
    buf[0] = '\0';
    len = 0;
    VS_ForceReloadRules();
    len += Com_sprintf( buf + len, sizeof(buf) - len, "\n^2Callvote Commands:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7------------------\n" );
    for ( i2 = 0; i2 < vs_ruleCount; i2++ ) {
        vs_rule_t *r;
        char dn[64];
        char vb[64];
        r = &vs_rules[i2];
        if ( !r->visible ) continue;
        if ( r->name[0] == '\0' ) continue;
        VS_GetDisplayNameFor( r->name, dn, sizeof(dn) );
        vb[0] = '\0';
        VS_GetValueForKey( r->name, vb, sizeof(vb) );
        if ( r->enabled ) {
            if ( vb[0] ) len += Com_sprintf( buf + len, sizeof(buf) - len, "^5%-12s  ^5%-16s ^7[%s]\n", r->name, (dn[0]?dn:r->name), vb );
            else len += Com_sprintf( buf + len, sizeof(buf) - len, "^5%-12s  ^5%-16s\n", r->name, (dn[0]?dn:r->name) );
        } else {
            if ( vb[0] ) len += Com_sprintf( buf + len, sizeof(buf) - len, "^1%-12s  ^1%-16s ^1[%s]\n", r->name, (dn[0]?dn:r->name), vb );
            else len += Com_sprintf( buf + len, sizeof(buf) - len, "^1%-12s  ^1%-16s\n", r->name, (dn[0]?dn:r->name) );
        }
    }
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7Usage: ^3\\callvote <command> [arg]^7\n" );
    trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
}

static qboolean ValidVoteCommand( int clientNum, char *command ) {
    char buf[MAX_CVAR_VALUE_STRING];
    char *base;
    char *s;
    int i;
    vs_rule_t *rule;
    char resolved[64];
    char rest[MAX_CVAR_VALUE_STRING];
    if ( strchr( command, ';' ) || strchr( command, '\n' ) || strchr( command, '\r' ) ) {
        trap_SendServerCommand( clientNum, "print \"Invalid vote command.\n\"" );
        return qfalse;
    }
    VS_ForceReloadRules();
    base = command;
    s = buf;
    while ( *command != '\0' && *command != ' ' ) {
        *s = *command; s++; command++;
    }
    *s = '\0';
    while ( *command == ' ' || *command == '\t' ) {
        command++;
    }
    /* support calling by display name alias; resolve to canonical key */
    VS_GetDisplayNameFor( buf, resolved, sizeof(resolved) );
    /* If resolved equals buf, it might still be a display alias; check rules list */
    {
        vs_rule_t *r;
        r = VS_GetRule( buf );
        Q_strncpyz( rest, command, sizeof(rest) );
        VS_TrimLeft( rest );
        if ( !r ) {
            /* try resolve by display name: find a rule whose displayName equals buf */
            int ri;
            int found;
            found = 0;
            for ( ri = 0; ri < vs_ruleCount; ri++ ) {
                if ( vs_rules[ri].displayName[0] && Q_stricmp( vs_rules[ri].displayName, buf ) == 0 ) {
                    /* rewrite base string to canonical name + rest */
                    BG_sprintf( base, "%s%s%s", vs_rules[ri].name, (rest[0] ? " " : ""), rest );
                    /* reparse name token into buf */
                    s = buf; command = base;
                    while ( *command != '\0' && *command != ' ' ) { *s = *command; s++; command++; }
                    *s = '\0';
                    while ( *command == ' ' || *command == '\t' ) { command++; }
                    found = 1;
                    break;
                }
            }
            if ( !found ) {
                /* leave as-is */
            }
        }
    }
    rule = VS_GetRule( buf );
    /* If a raw engine command (e.g. 'quad') is in the fixed list but we have a config rule mapping that engine command via action prefix,
       prefer the config rule (so aliases like 'noquad' replace 'quad'). */
    if ( (!rule || rule->action[0] == '\0') && buf[0] ) {
        vs_rule_t *byAction;
        byAction = VS_FindRuleByActionPrefix( buf );
        if ( byAction ) {
            rule = byAction;
        }
    }
    /* Default-allow only the fixed safe keywords; others must exist in rules */
    for ( i = 0; i < (int)ARRAY_LEN( voteCommands ); i++ ) {
        if ( !Q_stricmp( buf, voteCommands[i] ) ) break;
    }
    if ( i == (int)ARRAY_LEN( voteCommands ) ) {
        if ( rule == NULL ) {
            Cmd_CV_HelpList( g_entities + clientNum );
            return qfalse;
        }
    }
    /* If built-in keyword but config maps via action, prefer config rule */
    if ( rule == NULL || rule->action[0] == '\0' ) {
        vs_rule_t *mapped;
        mapped = VS_FindRuleByActionPrefix( buf );
        if ( mapped ) rule = mapped;
    }
    /* If configured and action provided (except for special-case 'map'), handle via template */
    if ( rule && rule->enabled == 0 ) {
        trap_SendServerCommand( clientNum, "print \"^3This vote is ^1disabled\n\"" );
        return qfalse;
    }
    if ( Q_stricmp( buf, "map" ) == 0 ) {
        vs_rule_t *mapRule;
        mapRule = VS_GetRule( "map" );
        if ( mapRule && mapRule->enabled == 0 ) {
            trap_SendServerCommand( clientNum, "print \"^3This vote is ^1disabled\n\"" );
            return qfalse;
        }
        /* fall through to default complex handling below */
    } else if ( rule && rule->action[0] ) {
        char argbuf[MAX_CVAR_VALUE_STRING];
        int j;
        int isNum;
        char expanded[512];
        /* command currently points to the first non-space char after name */
        Q_strncpyz( argbuf, command, sizeof(argbuf) );
        /* trim leading spaces/tabs */
        VS_TrimLeft( argbuf );
        /* allow action-driven votes even if the input keyword differs from rule->name (alias such as 'noquad') */
        buf[0] = '\0';
        {
            /* rebuild base to match rule->name so downstream messages show friendly mapping */
            char rebuilt[MAX_CVAR_VALUE_STRING];
            if ( argbuf[0] ) BG_sprintf( rebuilt, "%s %s", rule->name, argbuf );
            else BG_sprintf( rebuilt, "%s", rule->name );
            Q_strncpyz( base, rebuilt, MAX_CVAR_VALUE_STRING );
        }
        if ( rule->requireArg && argbuf[0] == '\0' ) {
            if ( rule->usage[0] ) trap_SendServerCommand( clientNum, va("print \"%s\n\"", rule->usage) );
            else trap_SendServerCommand( clientNum, va("print \"Usage: %s <arg>\n\"", buf) );
            return qfalse;
        }
        if ( rule->numericOnly ) {
            isNum = 1;
            for ( j = 0; argbuf[j]; ++j ) {
                if ( argbuf[j] < '0' || argbuf[j] > '9' ) { isNum = 0; break; }
            }
            if ( !isNum && argbuf[0] != '\0' ) {
                if ( rule->usage[0] ) trap_SendServerCommand( clientNum, va("print \"%s\n\"", rule->usage) );
                else trap_SendServerCommand( clientNum, va("print \"Usage: %s <number>\n\"", buf) );
                return qfalse;
            }
        }
        /* Disallow unsafe chars in arg */
        if ( strchr( argbuf, ';' ) || strchr( argbuf, '\n' ) || strchr( argbuf, '\r' ) ) {
            trap_SendServerCommand( clientNum, "print \"Invalid vote argument.\n\"" );
            return qfalse;
        }
        VS_ReplaceTemplate( rule->action, argbuf, expanded, sizeof(expanded) );
        BG_sprintf( base, "%s", expanded );
        return qtrue;
    }

    if ( Q_stricmp( buf, "g_gametype" ) == 0 ) {
        int gt;
        gt = -1;
        if ( command[0] == '\0' ) {
            trap_SendServerCommand( clientNum, va( "print \"Usage: g_gametype <ffa|duel|tdm|ctf|#> (current %d)\n\"", g_gametype.integer ) );
            return qfalse;
        }
        if ( !Q_stricmp( command, "ffa" ) ) gt = GT_FFA;
        else if ( !Q_stricmp( command, "duel" ) ) gt = GT_TOURNAMENT;
        else if ( !Q_stricmp( command, "tdm" ) ) gt = GT_TEAM;
        else if ( !Q_stricmp( command, "ctf" ) ) gt = GT_CTF;
        else gt = atoi( command );
        if ( gt == GT_SINGLE_PLAYER || gt < GT_FFA || gt >= GT_MAX_GAME_TYPE ) {
            trap_SendServerCommand( clientNum, va( "print \"Invalid gametype %i.\n\"", gt ) );
            return qfalse;
        }
        BG_sprintf( base, "g_gametype %i; map_restart", gt );
        return qtrue;
    }
    if ( Q_stricmp( buf, "map" ) == 0 ) {
        int isNum;
        int j;
        isNum = 1;
        for ( j = 0; command[j]; ++j ) {
            if ( command[j] < '0' || command[j] > '9' ) {
                isNum = 0; break;
            }
        }
        if ( isNum && command[0] != '\0' ) {
            int want;
            char mapname[64];
            want = atoi( command );
            if ( VS_GetMapNameByIndex( want, mapname, sizeof(mapname) ) ) {
                BG_sprintf( base, "map %s", mapname );
                return qtrue;
            } else {
                trap_SendServerCommand( clientNum, va( "print \"No such map index: %s.\n\"", command ) );
                return qfalse;
            }
        }
        if ( !G_MapExist( command ) ) {
            trap_SendServerCommand( clientNum, va( "print \"No such map on server: %s.\n\"", command ) );
            return qfalse;
        }
        return qtrue;
    }
    /* all other votes must be defined in votesystem.txt */
    return qtrue;
}

void Cmd_CallVote_f( gentity_t *ent ) {
    int i;
    int n;
    char arg[MAX_STRING_TOKENS];
    char *argn[4];
    char cmd[MAX_STRING_TOKENS];
    char *s;
    char userFirstRaw[256];
    userFirstRaw[0] = '\0';
    if ( trap_Argc() == 1 ) {
        Cmd_CV_HelpList( ent );
        return;
    } else if ( trap_Argc() == 2 ) {
        trap_Argv( 1, cmd, sizeof( cmd ) );
        if ( Q_stricmp( cmd, "map" ) == 0 ) {
            VS_PrintMapList( ent );
            return;
        }
    }
    if ( !g_allowVote.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"^3Voting not allowed here.\n\"" );
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent-g_entities, "print \"^1! ^3Spectators cannot call votes.\n\"" );
        return;
    }
    if ( level.voteTime ) {
        trap_SendServerCommand( ent-g_entities, "print \"^3A vote is already in progress.\n\"" );
        return;
    }
    if ( level.voteExecuteTime || level.restarted ) {
        trap_SendServerCommand( ent-g_entities, "print \"^3Previous vote command is waiting execution.^7\n\"" );
        return;
    }
    if ( ent->client->pers.voteCount >= g_voteLimit.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"^3You have called the maximum number of votes.\n\"" );
        return;
    }
    arg[0] = '\0';
    s = arg;
    for ( i = 1; i < trap_Argc(); i++ ) {
        if ( arg[0] ) {
            s = Q_stradd( s, " " );
        }
        trap_Argv( i, cmd, sizeof( cmd ) );
        s = Q_stradd( s, cmd );
    }
    n = Com_Split( arg, argn, ARRAY_LEN( argn ), ';' );
    if ( n == 0 || *argn[0] == '\0' ) {
        return;
    }
    /* preserve original first segment before ValidVoteCommand may rewrite it */
    Q_strncpyz( userFirstRaw, argn[0], sizeof(userFirstRaw) );
    for ( i = 0; i < n; i++ ) {
        if ( !ValidVoteCommand( ent - g_entities, argn[i] ) ) {
            return;
        }
    }
    cmd[0] = '\0';
    for ( s = cmd, i = 0; i < n; i++ ) {
        if ( cmd[0] ) {
            s = Q_stradd( s, ";" );
        }
        s = Q_stradd( s, argn[i] );
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ), cmd );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", level.voteString );
    {
        char dispName[64];
        char voteKey[MAX_TOKEN_CHARS];
        char firstCmd[256];
        const char *argPart;
        const char *displayCmdKey;
        int pos;
        int k;
        int fcLen;
        int spIdx;
        /* Extract vote key (first token) from the original user input (first segment) */
        pos = 0;
        k = 0;
        while ( userFirstRaw[pos] && userFirstRaw[pos] != ' ' && k < (int)sizeof(voteKey) - 1 ) {
            voteKey[k++] = userFirstRaw[pos++];
        }
        voteKey[k] = '\0';
        /* Find start of arguments within userFirstRaw */
        spIdx = pos;
        while ( userFirstRaw[spIdx] == ' ' || userFirstRaw[spIdx] == '\t' ) { spIdx++; }
        if ( userFirstRaw[spIdx] ) argPart = &userFirstRaw[spIdx]; else argPart = "";

        /* Display name should reflect canonical rule name if aliased */
        /* Prefer friendly name for canonical rule if this is an alias */
        displayCmdKey = voteKey;
        {
            vs_rule_t *rk;
            rk = VS_GetRule( voteKey );
            if ( !rk ) {
                rk = VS_FindRuleByActionPrefix( voteKey );
                if ( rk ) displayCmdKey = rk->name;
            }
        }
        VS_GetDisplayNameFor( displayCmdKey, dispName, sizeof(dispName) );
        if ( dispName[0] ) {
            /* set HUD display string */
            Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s%s%s", dispName, (argPart[0] ? " " : ""), argPart );
            trap_SendServerCommand( -1, va( "print \"%s called a vote (%s).\n\"", ent->client->pers.netname, level.voteDisplayString ) );
        } else {
            /* fallback to raw original */
            Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s", userFirstRaw );
            trap_SendServerCommand( -1, va( "print \"%s called a vote (%s).\n\"", ent->client->pers.netname, userFirstRaw ) );
        }
    }
    level.voteTime = level.time;
    level.voteYes = 1;
    level.voteNo = 0;
    for ( i = 0 ; i < level.maxclients ; i++ ) {
        level.clients[i].ps.eFlags &= ~EF_VOTED;
        level.clients[i].pers.voted = 0;
    }
    ent->client->ps.eFlags |= EF_VOTED;
    ent->client->pers.voted = 1;
    ent->client->pers.voteCount++;
    trap_SetConfigstring( CS_VOTE_TIME, va("%i", level.voteTime ) );
    trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
    trap_SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
    trap_SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );
}

void Cmd_Vote_f( gentity_t *ent ) {
    char msg[64];
    if ( !level.voteTime ) {
        trap_SendServerCommand( ent-g_entities, "print \"No vote in progress.\n\"" );
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" );
        return;
    }

    trap_Argv( 1, msg, sizeof( msg ) );
    if ( msg[0] != 'y' && msg[0] != 'Y' && msg[0] != '1' && msg[0] != 'n' && msg[0] != 'N' && msg[0] != '0' ) {
        trap_SendServerCommand( ent-g_entities, "print \"Usage: vote <yes|no>\n\"" );
        return;
    }

    /* handle vote (and possibly change) */
    if ( ent->client->pers.voted != 0 ) {
        if ( !g_allowVoteChange.integer ) {
            trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
            return;
        }
        /* revert previous vote */
        if ( ent->client->pers.voted == 1 ) {
            if ( msg[0] == 'y' || msg[0] == 'Y' || msg[0] == '1' ) {
                trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
                return;
            }
            if ( level.voteYes > 0 ) level.voteYes--;
        } else if ( ent->client->pers.voted == -1 ) {
            if ( msg[0] == 'n' || msg[0] == 'N' || msg[0] == '0' ) {
                trap_SendServerCommand( ent-g_entities, "print \"Vote already cast.\n\"" );
                return;
            }
            if ( level.voteNo > 0 ) level.voteNo--;
        }
    } else {
        /* first time voting */
        // trap_SendServerCommand( ent-g_entities, "print \"Vote cast.\n\"" );
        ent->client->ps.eFlags |= EF_VOTED;
    }

    if ( msg[0] == 'y' || msg[0] == 'Y' || msg[0] == '1' ) {
        level.voteYes++;
        ent->client->pers.voted = 1;
        trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
        trap_SendServerCommand( ent-g_entities, "print \"Your vote: ^2YES\n\"" );
    } else {
        level.voteNo++;
        ent->client->pers.voted = -1;
        trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
        trap_SendServerCommand( ent-g_entities, "print \"Your vote: ^1NO\n\"" );
    }
}

void Cmd_CV_f( gentity_t *ent ) {
    if ( trap_Argc() == 1 ) {
        Cmd_CV_HelpList( ent );
        return;
    } else if ( trap_Argc() == 2 ) {
        char sub[MAX_TOKEN_CHARS];
        trap_Argv( 1, sub, sizeof(sub) );
        if ( Q_stricmp( sub, "map" ) == 0 ) {
            VS_PrintMapList( ent );
            return;
        }
    }
    Cmd_CallVote_f( ent );
}

/* Move from g_cmds.c: revert player and team votes on leave/team change */
void G_RevertVote( gclient_t *client ) {
    if ( level.voteTime ) {
        if ( client->pers.voted == 1 ) {
            level.voteYes--;
            client->pers.voted = 0;
            client->ps.eFlags &= ~EF_VOTED;
            trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
        } else if ( client->pers.voted == -1 ) {
            level.voteNo--;
            client->pers.voted = 0;
            client->ps.eFlags &= ~EF_VOTED;
            trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
        }
    }
    if ( client->sess.sessionTeam == TEAM_RED || client->sess.sessionTeam == TEAM_BLUE ) {
        int cs_offset;
        if ( client->sess.sessionTeam == TEAM_RED ) cs_offset = 0; else cs_offset = 1;
        if ( client->pers.teamVoted == 1 ) {
            level.teamVoteYes[cs_offset]--;
            client->pers.teamVoted = 0;
            client->ps.eFlags &= ~EF_TEAMVOTED;
            trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
        } else if ( client->pers.teamVoted == -1 ) {
            level.teamVoteNo[cs_offset]--;
            client->pers.teamVoted = 0;
            client->ps.eFlags &= ~EF_TEAMVOTED;
            trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
        }
    }
}

/* Move from g_cmds.c: team vote call */
void Cmd_CallTeamVote_f( gentity_t *ent ) {
    int i, team, cs_offset;
    char arg1[MAX_STRING_TOKENS];
    char arg2[MAX_STRING_TOKENS];

    team = ent->client->sess.sessionTeam;
    if ( team == TEAM_RED ) cs_offset = 0; else if ( team == TEAM_BLUE ) cs_offset = 1; else return;

    if ( !g_allowVote.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here.\n\"" );
        return;
    }
    if ( level.teamVoteTime[cs_offset] ) {
        trap_SendServerCommand( ent-g_entities, "print \"A team vote is already in progress.\n\"" );
        return;
    }
    if ( ent->client->pers.teamVoteCount >= g_teamVoteLimit.integer ) {
        trap_SendServerCommand( ent-g_entities, "print \"You have called the maximum number of team votes.\n\"" );
        return;
    }
    if ( level.voteExecuteTime || level.restarted ) {
        return;
    }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
        trap_SendServerCommand( ent-g_entities, "print \"Not allowed to call a vote as spectator.\n\"" );
        return;
    }

    trap_Argv( 1, arg1, sizeof( arg1 ) );
    arg2[0] = '\0';
    for ( i = 2; i < trap_Argc(); i++ ) {
        if ( i > 2 ) strcat( arg2, " " );
        trap_Argv( i, &arg2[strlen(arg2)], sizeof( arg2 ) - (int)strlen(arg2) );
    }
    if ( strchr( arg1, ';' ) || strchr( arg2, ';' ) || strchr( arg2, '\n' ) || strchr( arg2, '\r' ) ) {
        trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
        return;
    }

    if ( !Q_stricmp( arg1, "leader" ) ) {
        char netname[MAX_NETNAME], leader[MAX_NETNAME];
        if ( !arg2[0] ) {
            i = ent->client->ps.clientNum;
        } else {
            int k;
            for ( k = 0; k < 3; k++ ) { if ( !arg2[k] || arg2[k] < '0' || arg2[k] > '9' ) break; }
            if ( k >= 3 || !arg2[k] ) {
                i = atoi( arg2 );
                if ( i < 0 || i >= level.maxclients ) { trap_SendServerCommand( ent-g_entities, va("print \"Bad client slot: %i\n\"", i) ); return; }
                if ( !g_entities[i].inuse ) { trap_SendServerCommand( ent-g_entities, va("print \"Client %i is not active\n\"", i) ); return; }
            } else {
                Q_strncpyz( leader, arg2, sizeof(leader) ); Q_CleanStr( leader );
                for ( i = 0; i < level.maxclients; i++ ) {
                    if ( level.clients[i].pers.connected == CON_DISCONNECTED ) continue;
                    if ( level.clients[i].sess.sessionTeam != team ) continue;
                    Q_strncpyz( netname, level.clients[i].pers.netname, sizeof(netname) ); Q_CleanStr( netname );
                    if ( !Q_stricmp( netname, leader ) ) break;
                }
                if ( i >= level.maxclients ) { trap_SendServerCommand( ent-g_entities, va("print \"%s is not a valid player on your team.\n\"", arg2) ); return; }
            }
        }
        Com_sprintf( arg2, sizeof(arg2), "%d", i );
    } else {
        trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
        trap_SendServerCommand( ent-g_entities, "print \"Team vote commands are: leader <player>.\n\"" );
        return;
    }

    Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "%s %s", arg1, arg2 );
    for ( i = 0; i < level.maxclients; i++ ) {
        if ( level.clients[i].pers.connected == CON_DISCONNECTED ) continue;
        if ( level.clients[i].sess.sessionTeam == team ) trap_SendServerCommand( i, va("print \"%s called a team vote.\n\"", ent->client->pers.netname ) );
    }
    level.teamVoteTime[cs_offset] = level.time;
    level.teamVoteYes[cs_offset] = 1;
    level.teamVoteNo[cs_offset]  = 0;
    for ( i = 0; i < level.maxclients; i++ ) {
        if ( level.clients[i].sess.sessionTeam == team ) {
            level.clients[i].ps.eFlags &= ~EF_TEAMVOTED;
            level.clients[i].pers.teamVoted = 0;
        }
    }
    ent->client->ps.eFlags |= EF_TEAMVOTED;
    ent->client->pers.teamVoted = 1;
    ent->client->pers.teamVoteCount++;
    trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
    trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteString[cs_offset] );
    trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
    trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/* Move from g_cmds.c: team vote cast */
void Cmd_TeamVote_f( gentity_t *ent ) {
    int team, cs_offset;
    char msg[64];
    team = ent->client->sess.sessionTeam;
    if ( team == TEAM_RED ) cs_offset = 0; else if ( team == TEAM_BLUE ) cs_offset = 1; else return;
    if ( !level.teamVoteTime[cs_offset] ) { trap_SendServerCommand( ent-g_entities, "print \"No team vote in progress.\n\"" ); return; }
    if ( ent->client->pers.teamVoted != 0 ) { trap_SendServerCommand( ent-g_entities, "print \"Team vote already cast.\n\"" ); return; }
    if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) { trap_SendServerCommand( ent-g_entities, "print \"Not allowed to vote as spectator.\n\"" ); return; }
    trap_SendServerCommand( ent-g_entities, "print \"Team vote cast.\n\"" );
    ent->client->ps.eFlags |= EF_TEAMVOTED;
    ent->client->pers.teamVoteCount++;
    trap_Argv( 1, msg, sizeof( msg ) );
    if ( msg[0] == 'y' || msg[0] == 'Y' || msg[0] == '1' ) {
        level.teamVoteYes[cs_offset]++;
        trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
    } else {
        level.teamVoteNo[cs_offset]++;
        trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
    }
}

void VS_Init(void) {
    VS_LoadRules();
}

/* end */

