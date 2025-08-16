// code/game/cmds/announcer.c
#include "cmds.h"

/* Simple auto-announcer with file-based config similar to votesystem */

#define AN_MAX_LINES     128
#define AN_MAX_NAME_LEN   32
#define AN_MAX_TEXT_LEN  512

typedef struct {
    char name[ AN_MAX_NAME_LEN ];
    int enabled;           /* 1/0 */
    char text[ AN_MAX_TEXT_LEN ];
} an_line_t;

static an_line_t s_anLines[ AN_MAX_LINES ];
static int s_anCount = 0;
static int s_anLoaded = 0; /* 0 not loaded, 1 loaded */
static int s_nextAnnounceTime = 0; /* level.time ms */
static int s_nextIndex = 0; /* for sequential order */

/* --- helpers --- */
static void AN_Reset(void) {
    int i;
    s_anCount = 0;
    for ( i = 0; i < AN_MAX_LINES; ++i ) {
        s_anLines[i].name[0] = '\0';
        s_anLines[i].enabled = 1;
        s_anLines[i].text[0] = '\0';
    }
}

static void AN_TrimRight(char *s) {
    int n = (int)strlen(s);
    while ( n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n') ) {
        s[--n] = '\0';
    }
}
static void AN_TrimLeft(char *s) {
    int i = 0, n = (int)strlen(s);
    if ( n <= 0 ) return;
    while ( i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n') ) i++;
    if ( i > 0 ) memmove( s, s + i, strlen(s + i) + 1 );
}
static void AN_Trim(char *s) { AN_TrimRight(s); AN_TrimLeft(s); }
static void AN_Unquote(char *s) {
    int n = (int)strlen(s);
    if ( n >= 2 && ((s[0] == '"' && s[n-1] == '"') || (s[0] == '\'' && s[n-1] == '\'')) ) {
        s[n-1] = '\0';
        memmove( s, s + 1, n - 1 );
    }
}

static an_line_t *AN_GetOrCreate(const char *name) {
    int i;
    for ( i = 0; i < s_anCount; ++i ) {
        if ( !Q_stricmp( s_anLines[i].name, name ) ) {
            return &s_anLines[i];
        }
    }
    if ( s_anCount >= AN_MAX_LINES ) return NULL;
    Q_strncpyz( s_anLines[s_anCount].name, name, sizeof(s_anLines[0].name) );
    s_anLines[s_anCount].enabled = 1;
    s_anLines[s_anCount].text[0] = '\0';
    s_anCount++;
    return &s_anLines[s_anCount - 1];
}

static const char *an_defaultText =
    "# announcements.txt — auto-announcer lines\n"
    "# Format: <name>.<attr> = <value>\n"
    "# attrs: enabled (0/1), text (string, supports ^color and \n)\n"
    "welcome.enabled = 1\n"
    "welcome.text = ^3Welcome to ^7Unite Q3 Server! ^2Have fun!\n"
    "rules.enabled = 1\n"
    "rules.text = ^51) ^7Be nice. ^52) ^7No cheats. ^53) ^7Have fun!\n"
    "help.enabled = 1\n"
    "help.text = ^3Type ^7\\help ^3for commands. ^3Type ^7\\cv ^3for callvote list.\n";

static void AN_CreateDefaultFile(void) {
    fileHandle_t f;
    int res;
    char fname[128];
    fname[0] = '\0';
    Q_strncpyz( fname, (g_announcer_file.string[0] ? g_announcer_file.string : "announcements.txt"), sizeof(fname) );
    res = trap_FS_FOpenFile( fname, &f, FS_WRITE );
    if ( res >= 0 ) {
        G_Printf("announcer: creating default %s\n", fname);
        trap_FS_Write( an_defaultText, (int)strlen(an_defaultText), f );
        trap_FS_FCloseFile( f );
    } else {
        G_Printf("announcer: failed to create %s (FS_WRITE)\n", fname);
    }
}

static void AN_Load(void) {
    fileHandle_t f;
    int flen;
    char *buf;
    char fname[128];
    if ( s_anLoaded ) return;
    s_anLoaded = 1;
    AN_Reset();
    fname[0] = '\0';
    Q_strncpyz( fname, (g_announcer_file.string[0] ? g_announcer_file.string : "announcements.txt"), sizeof(fname) );
    flen = trap_FS_FOpenFile( fname, &f, FS_READ );
    if ( flen <= 0 ) {
        G_Printf("announcer: %s not found, creating default...\n", fname);
        AN_CreateDefaultFile();
        flen = trap_FS_FOpenFile( fname, &f, FS_READ );
        if ( flen <= 0 ) {
            G_Printf("announcer: still cannot open %s after create attempt\n", fname);
            return;
        }
    }
    if ( flen > 32 * 1024 ) flen = 32 * 1024;
    buf = (char*)G_Alloc( flen + 1 );
    if ( !buf ) { trap_FS_FCloseFile( f ); return; }
    trap_FS_Read( buf, flen, f );
    trap_FS_FCloseFile( f );
    buf[flen] = '\0';

    {
        char *p = buf;
        while ( *p ) {
            char line[1024];
            char *nl;
            int linelen;
            char *comment;
            char *eq;
            char key[128];
            char val[ AN_MAX_TEXT_LEN ];
            char name[64];
            char attr[64];
            int dotPos;
            an_line_t *item;

            nl = strchr( p, '\n' );
            linelen = nl ? (int)(nl - p) : (int)strlen( p );
            if ( linelen > (int)sizeof(line) - 1 ) linelen = (int)sizeof(line) - 1;
            Q_strncpyz( line, p, linelen + 1 );
            p = nl ? nl + 1 : p + linelen;

            comment = strstr( line, "//" );
            if ( comment ) *comment = '\0';
            comment = strchr( line, '#' );
            if ( comment ) *comment = '\0';
            AN_Trim( line );
            if ( line[0] == '\0' ) continue;

            eq = strchr( line, '=' );
            if ( !eq ) continue;
            Q_strncpyz( key, line, (int)(eq - line) + 1 );
            Q_strncpyz( val, eq + 1, sizeof(val) );
            AN_Trim( key );
            AN_Trim( val );
            AN_Unquote( val );

            /* key format: <name>.<attr> */
            {
                int i2, klen;
                dotPos = -1;
                klen = (int)strlen( key );
                for ( i2 = 0; i2 < klen; ++i2 ) { if ( key[i2] == '.' ) { dotPos = i2; break; } }
            }
            if ( dotPos <= 0 ) continue;
            Q_strncpyz( name, key, dotPos + 1 );
            Q_strncpyz( attr, key + dotPos + 1, sizeof(attr) );
            AN_Trim( name ); AN_Trim( attr );
            if ( name[0] == '\0' || attr[0] == '\0' ) continue;

            item = AN_GetOrCreate( name );
            if ( !item ) continue;

            if ( !Q_stricmp( attr, "enabled" ) ) {
                item->enabled = (!Q_stricmp(val, "1") || !Q_stricmp(val, "true") || !Q_stricmp(val, "yes") || !Q_stricmp(val, "on")) ? 1 : 0;
            } else if ( !Q_stricmp( attr, "text" ) ) {
                Q_strncpyz( item->text, val, sizeof(item->text) );
            }
        }
    }
}

static int AN_GetEnabledCount(void) {
    int i, c = 0;
    for ( i = 0; i < s_anCount; ++i ) if ( s_anLines[i].enabled && s_anLines[i].text[0] ) c++;
    return c;
}

static int AN_PickNextIndex(void) {
    int enabledCount;
    int tries;
    if ( s_anCount <= 0 ) return -1;
    enabledCount = AN_GetEnabledCount();
    if ( enabledCount <= 0 ) return -1;
    if ( g_announcer_order.integer == 0 ) {
        int i, loops = 0;
        for ( loops = 0; loops < s_anCount; ++loops ) {
            i = s_nextIndex % s_anCount;
            s_nextIndex = (s_nextIndex + 1) % s_anCount;
            if ( s_anLines[i].enabled && s_anLines[i].text[0] ) return i;
        }
        return -1;
    } else {
        /* random */
        tries = 0;
        while ( tries < 32 ) {
            int idx = rand() % s_anCount;
            if ( s_anLines[idx].enabled && s_anLines[idx].text[0] ) return idx;
            tries++;
        }
        /* fallback to sequential */
        return AN_PickNextIndex();
    }
}

static void AN_BroadcastPrint(const char *msg) {
    /* Broadcast a print command; expand \n escapes; supports color codes */
    char tmp[ AN_MAX_TEXT_LEN * 2 ];
    int i, j;
    if ( !msg || !msg[0] ) return;
    j = 0;
    for ( i = 0; msg[i] && j < (int)sizeof(tmp) - 1; ++i ) {
        if ( msg[i] == '\\' && msg[i+1] == 'n' ) {
            tmp[j++] = '\n';
            i++; /* skip 'n' */
        } else {
            tmp[j++] = msg[i];
        }
    }
    tmp[j] = '\0';
    /* ensure trailing newline for chat */
    {
        int ln = (int)strlen(tmp);
        if ( ln == 0 || tmp[ln-1] != '\n' ) {
            if ( ln < (int)sizeof(tmp) - 1 ) { tmp[ln] = '\n'; tmp[ln+1] = '\0'; }
        }
    }
    G_BroadcastServerCommand( -1, va( "print \"%s\"", tmp ) );
}

/* --- public API --- */
void AN_Init(void) {
    s_anLoaded = 0;
    s_nextAnnounceTime = level.time + (g_announcer_interval.integer > 0 ? g_announcer_interval.integer * 1000 : 120000);
    s_nextIndex = 0;
    AN_Load();
}

void AN_ForceReload(void) {
    s_anLoaded = 0;
    AN_Load();
}

void AN_RunFrame(void) {
    int intervalMs;
    int idx;
    if ( !g_announcer.integer ) return;
    /* refresh interval from cvar each frame */
    intervalMs = g_announcer_interval.integer * 1000;
    if ( intervalMs <= 0 ) intervalMs = 120000;
    if ( level.time < s_nextAnnounceTime ) return;
    if ( AN_GetEnabledCount() <= 0 ) {
        s_nextAnnounceTime = level.time + intervalMs;
        return;
    }
    idx = AN_PickNextIndex();
    if ( idx >= 0 ) {
        AN_BroadcastPrint( s_anLines[idx].text );
    }
    s_nextAnnounceTime = level.time + intervalMs;
}

/* --- client commands --- */
static void Cmd_AnnReload_f( gentity_t *ent ) {
    if ( !ent || !ent->authed ) { trap_SendServerCommand( ent - g_entities, "print \"^1! ^3Auth required.\n\"" ); return; }
    AN_ForceReload();
    trap_SendServerCommand( ent - g_entities, "print \"^2Announcer list reloaded.\n\"" );
}

static void Cmd_AnnList_f( gentity_t *ent ) {
    int i;
    char buf[MAX_STRING_CHARS];
    int len = 0;
    buf[0] = '\0';
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^2Announcements:^7\n" );
    len += Com_sprintf( buf + len, sizeof(buf) - len, "^7--------------\n" );
    for ( i = 0; i < s_anCount; ++i ) {
        const char *state = s_anLines[i].enabled ? "^2on^7" : "^1off^7";
        len += Com_sprintf( buf + len, sizeof(buf) - len, "%2d. ^5%-12s ^7[%s]^7: %s\n", i + 1, s_anLines[i].name, state, s_anLines[i].text );
        if ( len > (int)sizeof(buf) - 256 ) {
            trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
            buf[0] = '\0';
            len = 0;
        }
    }
    if ( len > 0 ) trap_SendServerCommand( ent - g_entities, va("print \"%s\"", buf) );
}

static void Cmd_AnnEnable_f( gentity_t *ent ) {
    char name[MAX_TOKEN_CHARS];
    char val[MAX_TOKEN_CHARS];
    int i;
    int set;
    if ( !ent || !ent->authed ) { trap_SendServerCommand( ent - g_entities, "print \"^1! ^3Auth required.\n\"" ); return; }
    if ( trap_Argc() < 3 ) { trap_SendServerCommand( ent - g_entities, "print \"Usage: annenable <name|index> <0|1>\n\"" ); return; }
    trap_Argv( 1, name, sizeof(name) );
    trap_Argv( 2, val, sizeof(val) );
    set = atoi( val ) ? 1 : 0;
    /* allow numeric index (1-based) */
    {
        int isNum = 1;
        int j;
        for ( j = 0; name[j]; ++j ) { if ( name[j] < '0' || name[j] > '9' ) { isNum = 0; break; } }
        if ( isNum && name[0] ) {
            i = atoi( name ) - 1;
            if ( i >= 0 && i < s_anCount ) {
                s_anLines[i].enabled = set;
                trap_SendServerCommand( ent - g_entities, va("print \"Set '%s' to %s.\n\"", s_anLines[i].name, set?"on":"off") );
                return;
            }
        }
    }
    for ( i = 0; i < s_anCount; ++i ) {
        if ( !Q_stricmp( s_anLines[i].name, name ) ) {
            s_anLines[i].enabled = set;
            trap_SendServerCommand( ent - g_entities, va("print \"Set '%s' to %s.\n\"", s_anLines[i].name, set?"on":"off") );
            return;
        }
    }
    trap_SendServerCommand( ent - g_entities, "print \"^1! ^3Announcement not found.\n\"" );
}

/* exported for registration */
void AN_Cmd_AnnReload( gentity_t *ent ) { Cmd_AnnReload_f( ent ); }
void AN_Cmd_AnnList( gentity_t *ent ) { Cmd_AnnList_f( ent ); }
void AN_Cmd_AnnEnable( gentity_t *ent ) { Cmd_AnnEnable_f( ent ); }

/* --- server console commands --- */
void Svcmd_AnnouncerReload_f( void ) {
    AN_ForceReload();
    G_Printf("announcer: reloaded announcements list.\n");
}
void Svcmd_AnnouncerList_f( void ) {
    int i;
    G_Printf("Announcements (%d):\n", s_anCount);
    for ( i = 0; i < s_anCount; ++i ) {
        G_Printf("%2d. %s [%s]: %s\n", i + 1, s_anLines[i].name, (s_anLines[i].enabled?"on":"off"), s_anLines[i].text );
    }
}
void Svcmd_AnnouncerEnable_f( void ) {
    char arg1[MAX_TOKEN_CHARS];
    char arg2[MAX_TOKEN_CHARS];
    int set;
    int i;
    if ( trap_Argc() < 3 ) { G_Printf("Usage: ann_enable <name|index> <0|1>\n"); return; }
    trap_Argv( 1, arg1, sizeof(arg1) );
    trap_Argv( 2, arg2, sizeof(arg2) );
    set = atoi( arg2 ) ? 1 : 0;
    {
        int isNum = 1; int j;
        for ( j = 0; arg1[j]; ++j ) { if ( arg1[j] < '0' || arg1[j] > '9' ) { isNum = 0; break; } }
        if ( isNum && arg1[0] ) {
            i = atoi( arg1 ) - 1;
            if ( i >= 0 && i < s_anCount ) {
                s_anLines[i].enabled = set;
                G_Printf("Set '%s' to %s.\n", s_anLines[i].name, set?"on":"off");
                return;
            }
        }
    }
    for ( i = 0; i < s_anCount; ++i ) {
        if ( !Q_stricmp( s_anLines[i].name, arg1 ) ) {
            s_anLines[i].enabled = set;
            G_Printf("Set '%s' to %s.\n", s_anLines[i].name, set?"on":"off");
            return;
        }
    }
    G_Printf("Announcement not found.\n");
}


