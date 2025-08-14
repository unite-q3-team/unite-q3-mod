// code/game/cmds/cmds.c
#include "cmds.h"

/* -------- Weapons file-based config -------- */
extern void G_InitFireRatios(void);

typedef struct {
    const char *key;
    const char *cvarName;
    vmCvar_t *vc;
} wc_map_t;

static wc_map_t s_wcMap[] = {
    { "damage.gauntlet",        "g_gauntlet_damage",        &g_gauntlet_damage },
    { "damage.mg",              "g_mg_damage",              &g_mg_damage },
    { "damage.mgTeam",          "g_mg_damageTeam",          &g_mg_damageTeam },
    { "damage.sg",              "g_sg_damage",              &g_sg_damage },
    { "damage.gl",              "g_gl_damage",              &g_gl_damage },
    { "damage.rl",              "g_rl_damage",              &g_rl_damage },
    { "damage.lg",              "g_lg_damage",              &g_lg_damage },
    { "damage.rg",              "g_rg_damage",              &g_rg_damage },
    { "damage.pg",              "g_pg_damage",              &g_pg_damage },
    { "damage.bfg",             "g_bfg_damage",             &g_bfg_damage },

    { "projectile.rl.speed",    "g_rl_projectileSpeed",     &g_rl_projectileSpeed },
    { "projectile.pg.speed",    "g_pg_projectileSpeed",     &g_pg_projectileSpeed },
    { "projectile.bfg.speed",   "g_bfg_projectileSpeed",    &g_bfg_projectileSpeed },

    { "fireRatio.gauntlet",     "g_gauntlet_fireRatio",     &g_gauntlet_fireRatio },
    { "fireRatio.lg",           "g_lg_fireRatio",           &g_lg_fireRatio },
    { "fireRatio.sg",           "g_sg_fireRatio",           &g_sg_fireRatio },
    { "fireRatio.mg",           "g_mg_fireRatio",           &g_mg_fireRatio },
    { "fireRatio.gl",           "g_gl_fireRatio",           &g_gl_fireRatio },
    { "fireRatio.rl",           "g_rl_fireRatio",           &g_rl_fireRatio },
    { "fireRatio.pg",           "g_pg_fireRatio",           &g_pg_fireRatio },
    { "fireRatio.rg",           "g_rg_fireRatio",           &g_rg_fireRatio },
    { "fireRatio.bfg",          "g_bfg_fireRatio",          &g_bfg_fireRatio },
};
static const int s_wcMapCount = (int)(sizeof(s_wcMap)/sizeof(s_wcMap[0]));

static void WC_TrimRight(char *s) {
    int n = (int)strlen(s);
    while ( n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n') ) { s[n-1]='\0'; n--; }
}
static void WC_TrimLeft(char *s) {
    int i=0,n=(int)strlen(s); char *p;
    while ( i<n && (s[i]==' '||s[i]=='\t'||s[i]=='\r'||s[i]=='\n') ) i++;
    if ( i>0 ) { p=s+i; memmove(s,p,strlen(p)+1); }
}
static void WC_Trim(char *s) { WC_TrimRight(s); WC_TrimLeft(s); }

static wc_map_t *WC_FindEntry(const char *key) {
    int i; for ( i=0; i<s_wcMapCount; ++i ) { if ( Q_stricmp(s_wcMap[i].key, key)==0 ) return &s_wcMap[i]; } return NULL;
}

static void WC_WriteDefaultFile(void) {
    fileHandle_t f; int r; int i; char line[256];
    r = trap_FS_FOpenFile("weapons.txt", &f, FS_WRITE);
    if ( r < 0 ) return;
    trap_FS_Write("# weapons.txt — weapon configuration (key = value)\n", 57, f);
    trap_FS_Write("# groups: damage.<wp>, projectile.<wp>.speed, fireRatio.<wp>\n\n", 66, f);
    for ( i = 0; i < s_wcMapCount; ++i ) {
        vmCvar_t *vc = s_wcMap[i].vc;
        Com_sprintf( line, sizeof(line), "%s = %s\n", s_wcMap[i].key, (vc ? vc->string : "0") );
        trap_FS_Write( line, (int)strlen(line), f );
    }
    trap_FS_FCloseFile( f );
}

static void WC_LoadFile(void) {
    fileHandle_t f; int flen; char *buf; char *p;
    flen = trap_FS_FOpenFile("weapons.txt", &f, FS_READ);
    if ( flen <= 0 ) {
        WC_WriteDefaultFile();
        flen = trap_FS_FOpenFile("weapons.txt", &f, FS_READ);
        if ( flen <= 0 ) return;
    }
    if ( flen > 32*1024 ) flen = 32*1024;
    buf = (char*)G_Alloc(flen+1); if (!buf) { trap_FS_FCloseFile(f); return; }
    trap_FS_Read(buf, flen, f); trap_FS_FCloseFile(f); buf[flen]='\0';
    p = buf;
    while ( *p ) {
        char *nl; int linelen; char line[256]; char *eq; char key[128]; char val[64]; vmCvar_t *vc;
        nl = strchr(p,'\n'); linelen = nl ? (int)(nl-p) : (int)strlen(p);
        if ( linelen > (int)sizeof(line)-1 ) linelen = (int)sizeof(line)-1;
        Q_strncpyz(line,p,linelen+1); p = nl ? nl+1 : p+linelen;
        WC_Trim(line); if (!line[0]) continue; if (line[0]=='#' || (line[0]=='/'&&line[1]=='/')) continue;
        eq = strchr(line,'='); if (!eq) continue;
        Q_strncpyz(key,line,(int)(eq-line)+1); Q_strncpyz(val,eq+1,sizeof(val)); WC_Trim(key); WC_Trim(val);
        {
            wc_map_t *m = WC_FindEntry(key);
            if ( m && m->vc ) { trap_Cvar_Set(m->cvarName, val); trap_Cvar_Update(m->vc); }
        }
    }
}

void Weapons_Init(void) { WC_LoadFile(); G_InitFireRatios(); }

void wtreload_f(gentity_t *ent) { if (!ent||!ent->client||!ent->authed) return; WC_LoadFile(); G_InitFireRatios(); trap_SendServerCommand(ent-g_entities, "print \"^2weapons: reloaded\n\""); }

void wtlist_f(gentity_t *ent) {
    int i; char line[256]; if (!ent||!ent->client||!ent->authed) return; 
    trap_SendServerCommand(ent-g_entities, "print \"\n^2Weapons Config (effective)^7\n\"");
    trap_SendServerCommand(ent-g_entities, "print \"^7-----------------------------\n\"");
    for ( i=0; i<s_wcMapCount; ++i ) { Com_sprintf(line,sizeof(line), "^5%-22s ^7= ^2%s", s_wcMap[i].key, s_wcMap[i].vc ? s_wcMap[i].vc->string : ""); trap_SendServerCommand(ent-g_entities, va("print \"%s\n\"", line)); }
}

/* ---------------- Disabled commands config (disabledcmds.txt) ---------------- */

static char s_dc_names[256][32];
static int s_dc_count = 0;
static qboolean s_dc_loaded = qfalse;

static void DC_Trim(char *s) {
    int n = (int)strlen(s);
    while ( n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n') ) { s[n-1] = '\0'; n--; }
    while ( *s == ' ' || *s == '\t' ) { memmove(s, s+1, strlen(s)); }
}

void DC_Init(void) {
    fileHandle_t f; int flen; char *buf; char *p;
    char line[256]; char *nl; int linelen;
    if ( s_dc_loaded ) return; s_dc_loaded = qtrue; s_dc_count = 0;
    flen = trap_FS_FOpenFile("disabledcmds.txt", &f, FS_READ);
    if ( flen <= 0 ) {
        /* create a default example file */
        fileHandle_t wf; int wr;
        static const char *dc_defaultText =
            "# disabledcmds.txt — one command per line to disable\n"
            "# comments with # or // are supported; blank lines ignored\n"
            "# Example: disable give and god, and a custom command\n"
            "# give\n"
            "# god\n"
            "# poscp\n";
        wr = trap_FS_FOpenFile("disabledcmds.txt", &wf, FS_WRITE);
        if ( wr >= 0 ) {
            G_Printf("disabledcmds: creating default disabledcmds.txt\n");
            trap_FS_Write(dc_defaultText, (int)strlen(dc_defaultText), wf);
            trap_FS_FCloseFile(wf);
        } else {
            G_Printf("disabledcmds: failed to create disabledcmds.txt (FS_WRITE)\n");
        }
        /* try to read again; if still missing, silently proceed with none disabled */
        flen = trap_FS_FOpenFile("disabledcmds.txt", &f, FS_READ);
        if ( flen <= 0 ) {
            return;
        }
    }
    if ( flen > 64 * 1024 ) flen = 64 * 1024;
    buf = (char*)G_Alloc(flen + 1); if ( !buf ) { trap_FS_FCloseFile(f); return; }
    trap_FS_Read(buf, flen, f); trap_FS_FCloseFile(f); buf[flen] = '\0';
    p = buf;
    while ( *p ) {
        nl = strchr(p, '\n'); if ( nl ) linelen = (int)(nl - p); else linelen = (int)strlen(p);
        if ( linelen > (int)sizeof(line) - 1 ) linelen = (int)sizeof(line) - 1;
        Q_strncpyz(line, p, linelen + 1);
        if ( nl ) p = nl + 1; else p = p + linelen;
        /* strip comments starting with # or // */
        { char *c = strstr(line, "//"); if ( c ) *c = '\0'; }
        { char *c = strchr(line, '#'); if ( c ) *c = '\0'; }
        DC_Trim(line); if ( !line[0] ) continue;
        if ( s_dc_count < (int)(sizeof(s_dc_names)/sizeof(s_dc_names[0])) ) {
            Q_strncpyz( s_dc_names[s_dc_count], line, sizeof(s_dc_names[0]) );
            s_dc_count++;
        }
    }
}

qboolean DC_IsDisabled(const char *cmdName) {
    int i; if ( !s_dc_loaded ) DC_Init();
    if ( !cmdName || !*cmdName ) return qfalse;
    for ( i = 0; i < s_dc_count; ++i ) {
        if ( Q_stricmp( cmdName, s_dc_names[i] ) == 0 ) return qtrue;
    }
    return qfalse;
}

/* GeoIP Legacy: country ID -> ISO code/name tables (0..254) */
#define GEOIP_NUM_COUNTRIES 255
static const char *GeoIPCountryCodes[GEOIP_NUM_COUNTRIES] = {
    "--","AP","EU","AD","AE","AF","AG","AI","AL","AM","AN","AO","AQ","AR","AS","AT","AU","AW","AZ","BA",
    "BB","BD","BE","BF","BG","BH","BI","BJ","BM","BN","BO","BR","BS","BT","BV","BW","BY","BZ","CA","CC",
    "CD","CF","CG","CH","CI","CK","CL","CM","CN","CO","CR","CU","CV","CX","CY","CZ","DE","DJ","DK","DM",
    "DO","DZ","EC","EE","EG","EH","ER","ES","ET","FI","FJ","FK","FM","FO","FR","FX","GA","GB","GD","GE",
    "GF","GH","GI","GL","GM","GN","GP","GQ","GR","GS","GT","GU","GW","GY","HK","HM","HN","HR","HT","HU",
    "ID","IE","IL","IN","IO","IQ","IR","IS","IT","JM","JO","JP","KE","KG","KH","KI","KM","KN","KP","KR",
    "KW","KY","KZ","LA","LB","LC","LI","LK","LR","LS","LT","LU","LV","LY","MA","MC","MD","MG","MH","MK",
    "ML","MM","MN","MO","MP","MQ","MR","MS","MT","MU","MV","MW","MX","MY","MZ","NA","NC","NE","NF","NG",
    "NI","NL","NO","NP","NR","NU","NZ","OM","PA","PE","PF","PG","PH","PK","PL","PM","PN","PR","PS","PT",
    "PW","PY","QA","RE","RO","RU","RW","SA","SB","SC","SD","SE","SG","SH","SI","SJ","SK","SL","SM","SN",
    "SO","SR","ST","SV","SY","SZ","TC","TD","TF","TG","TH","TJ","TK","TM","TN","TO","TL","TR","TT","TV",
    "TW","TZ","UA","UG","UM","US","UY","UZ","VA","VC","VE","VG","VI","VN","VU","WF","WS","YE","YT","RS",
    "ZA","ZM","ME","ZW","A1","A2","O1","AX","GG","IM","JE","BL","MF","BQ","SS"
};

static const char *GeoIPCountryNames[GEOIP_NUM_COUNTRIES] = {
    "Unknown","Asia/Pacific Region","Europe","Andorra","United Arab Emirates","Afghanistan","Antigua and Barbuda","Anguilla","Albania","Armenia","Netherlands Antilles","Angola","Antarctica","Argentina","American Samoa","Austria","Australia","Aruba","Azerbaijan","Bosnia and Herzegovina",
    "Barbados","Bangladesh","Belgium","Burkina Faso","Bulgaria","Bahrain","Burundi","Benin","Bermuda","Brunei Darussalam","Bolivia","Brazil","Bahamas","Bhutan","Bouvet Island","Botswana","Belarus","Belize","Canada","Cocos (Keeling) Islands",
    "Congo, The Democratic Republic of the","Central African Republic","Congo","Switzerland","Cote D'Ivoire","Cook Islands","Chile","Cameroon","China","Colombia","Costa Rica","Cuba","Cape Verde","Christmas Island","Cyprus","Czech Republic","Germany","Djibouti","Denmark","Dominica",
    "Dominican Republic","Algeria","Ecuador","Estonia","Egypt","Western Sahara","Eritrea","Spain","Ethiopia","Finland","Fiji","Falkland Islands (Malvinas)","Micronesia, Federated States of","Faroe Islands","France","France, Metropolitan","Gabon","United Kingdom","Grenada","Georgia",
    "French Guiana","Ghana","Gibraltar","Greenland","Gambia","Guinea","Guadeloupe","Equatorial Guinea","Greece","South Georgia and the South Sandwich Islands","Guatemala","Guam","Guinea-Bissau","Guyana","Hong Kong","Heard Island and McDonald Islands","Honduras","Croatia","Haiti","Hungary",
    "Indonesia","Ireland","Israel","India","British Indian Ocean Territory","Iraq","Iran, Islamic Republic of","Iceland","Italy","Jamaica","Jordan","Japan","Kenya","Kyrgyzstan","Cambodia","Kiribati","Comoros","Saint Kitts and Nevis","Korea, Democratic People's Republic of","Korea, Republic of",
    "Kuwait","Cayman Islands","Kazakhstan","Lao People's Democratic Republic","Lebanon","Saint Lucia","Liechtenstein","Sri Lanka","Liberia","Lesotho","Lithuania","Luxembourg","Latvia","Libya","Morocco","Monaco","Moldova, Republic of","Madagascar","Marshall Islands","Macedonia",
    "Mali","Myanmar","Mongolia","Macau","Northern Mariana Islands","Martinique","Mauritania","Montserrat","Malta","Mauritius","Maldives","Malawi","Mexico","Malaysia","Mozambique","Namibia","New Caledonia","Niger","Norfolk Island","Nigeria",
    "Nicaragua","Netherlands","Norway","Nepal","Nauru","Niue","New Zealand","Oman","Panama","Peru","French Polynesia","Papua New Guinea","Philippines","Pakistan","Poland","Saint Pierre and Miquelon","Pitcairn Islands","Puerto Rico","Palestinian Territory","Portugal",
    "Palau","Paraguay","Qatar","Reunion","Romania","Russian Federation","Rwanda","Saudi Arabia","Solomon Islands","Seychelles","Sudan","Sweden","Singapore","Saint Helena","Slovenia","Svalbard and Jan Mayen","Slovakia","Sierra Leone","San Marino","Senegal",
    "Somalia","Suriname","Sao Tome and Principe","El Salvador","Syrian Arab Republic","Swaziland","Turks and Caicos Islands","Chad","French Southern Territories","Togo","Thailand","Tajikistan","Tokelau","Turkmenistan","Tunisia","Tonga","Timor-Leste","Turkey","Trinidad and Tobago","Tuvalu",
    "Taiwan","Tanzania, United Republic of","Ukraine","Uganda","United States Minor Outlying Islands","United States","Uruguay","Uzbekistan","Holy See (Vatican City State)","Saint Vincent and the Grenadines","Venezuela","Virgin Islands, British","Virgin Islands, U.S.","Vietnam","Vanuatu","Wallis and Futuna","Samoa","Yemen","Mayotte","Serbia",
    "South Africa","Zambia","Montenegro","Zimbabwe","Anonymous Proxy","Satellite Provider","Other","Aland Islands","Guernsey","Isle of Man","Jersey","Saint Barthelemy","Saint Martin","Bonaire, Sint Eustatius and Saba","South Sudan"
};

static const char *GeoIPCountryCodeById(int id)
{
    if (id < 0 || id >= GEOIP_NUM_COUNTRIES) return "--";
    return GeoIPCountryCodes[id];
}

static const char *GeoIPCountryNameById(int id)
{
    if (id < 0 || id >= GEOIP_NUM_COUNTRIES) return "Unknown";
    return GeoIPCountryNames[id];
}

/*
============================================================
 Helper: parse IPv4 address (optionally with port) to 32-bit
============================================================
*/
static qboolean G_ParseIPv4ToUint(const char *ipString, unsigned int *outIpv4)
{
    /* Accept forms like "A.B.C.D" or "A.B.C.D:port" */
    char buf[64];
    int i, colonCount;
    int idx, part;
    unsigned int a, b, c, d;

    if (!ipString || !ipString[0] || !outIpv4) {
        return qfalse;
    }

    Q_strncpyz(buf, ipString, sizeof(buf));

    /* Trim at first whitespace */
    for (i = 0; buf[i]; ++i) {
        if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r') {
            buf[i] = '\0';
            break;
        }
    }

    /* Handle port or IPv6 */
    colonCount = 0;
    for (i = 0; buf[i]; ++i) {
        if (buf[i] == ':') colonCount++;
    }
    if (colonCount > 1) {
        return qfalse; /* looks like IPv6 */
    }
    if (colonCount == 1) {
        /* strip port */
        for (i = 0; buf[i]; ++i) {
            if (buf[i] == ':') { buf[i] = '\0'; break; }
        }
    }

    a = b = c = d = 0;
    idx = 0;
    for (part = 0; part < 4; ++part) {
        unsigned int val = 0u;
        int haveDigit = 0;
        while (buf[idx] >= '0' && buf[idx] <= '9') {
            haveDigit = 1;
            val = val * 10u + (unsigned int)(buf[idx] - '0');
            if (val > 255u) return qfalse;
            idx++;
        }
        if (!haveDigit) return qfalse;
        if (part < 3) {
            if (buf[idx] != '.') return qfalse;
            idx++;
        }
        if (part == 0) a = val; else if (part == 1) b = val; else if (part == 2) c = val; else d = val;
    }
    /* ensure no trailing garbage */
    if (buf[idx] != '\0') return qfalse;

    *outIpv4 = (a << 24) | (b << 16) | (c << 8) | d;
    return qtrue;
}

void Cmd_NewTest_f(gentity_t *ent) {
    int filelen;
    char path[MAX_QPATH];
    fileHandle_t fh;
    /* We will not read the whole file into memory to avoid large allocations */
    const char *filename;
    char userinfo[MAX_INFO_STRING];
    const char *ipStr;
    unsigned int ipv4;
    qboolean haveIPv4;
    int argc;
    char arg1[64];

    if (!ent || !ent->client) {
        return;
    }

    /* allow testing with explicit IPv4: atest 8.8.8.8 */
    argc = trap_Argc();
    if (argc >= 2) {
        trap_Argv(1, arg1, (int)sizeof(arg1));
        ipStr = arg1;
    } else {
        /* fetch client IP string from userinfo */
        trap_GetUserinfo(ent->client->ps.clientNum, userinfo, sizeof(userinfo));
        ipStr = Info_ValueForKey(userinfo, "ip");
        if (!ipStr || !ipStr[0]) {
            trap_SendServerCommand(ent - g_entities, "print \"^1No IP found in userinfo.\\n\"");
            return;
        }
    }

    haveIPv4 = G_ParseIPv4ToUint(ipStr, &ipv4);
    if (argc >= 2 && !haveIPv4) {
        trap_SendServerCommand(ent - g_entities, "print \"^1Invalid IPv4 address. Usage: atest <A.B.C.D>\\n\"");
        return;
    }

    filename = "geoip";
    Com_sprintf(path, MAX_QPATH, "%s.dat", filename);

    filelen = trap_FS_FOpenFile(path, &fh, FS_READ);
    if (filelen <= 0 || fh == FS_INVALID_HANDLE) {
        Com_Printf("^1geoip file %s not found\n", path);
        return;
    }

    /* Detect GeoIP Legacy database type and segments (search for 0xFFFFFF marker near EOF) */
    {
        unsigned char tail[64];
        int readLen;
        int i;
        int markerIndex;
        int databaseType;
        unsigned int databaseSegments;
        const int recordLength = 3;

        readLen = sizeof(tail);
        if (filelen < readLen) {
            readLen = filelen;
        }
        markerIndex = -1;
        databaseType = -1;
        databaseSegments = 16776960u; /* default COUNTRY_BEGIN */

        /* seek to tail and read */
        if (trap_FS_Seek(fh, -((long)readLen), FS_SEEK_END) == 0) {
            trap_FS_Read(tail, readLen, fh);
            for (i = readLen - 7; i >= 0; --i) {
                if (tail[i] == 0xFF && tail[i+1] == 0xFF && tail[i+2] == 0xFF) {
                    markerIndex = i;
                    break;
                }
            }
            if (markerIndex >= 0) {
                databaseType = (int)tail[markerIndex + 3];
                if (markerIndex + 7 < readLen) {
                    databaseSegments = (unsigned int)tail[markerIndex + 4]
                                     | ((unsigned int)tail[markerIndex + 5] << 8)
                                     | ((unsigned int)tail[markerIndex + 6] << 16);
                }
            }
        }

        /* Traverse binary tree to find leaf for IPv4 */
        if (haveIPv4) {
                unsigned int offset;
                unsigned int nextPtr;
                int depth;
                unsigned char nodeBuf[6];
                unsigned long nodeByteOffset;
                unsigned int leftPtr, rightPtr;
                int bit;

                offset = 0u;
                nextPtr = 0u;
                for (depth = 31; depth >= 0; --depth) {
                    bit = (int)((ipv4 >> depth) & 1u);
                    nodeByteOffset = (unsigned long)offset * 2ul * (unsigned long)recordLength;
                    if (nodeByteOffset + (2 * recordLength) > (unsigned long)filelen) {
                        break;
                    }
                    if (trap_FS_Seek(fh, (long)nodeByteOffset, FS_SEEK_SET) != 0) {
                        break;
                    }
                    trap_FS_Read(nodeBuf, 6, fh);
                    leftPtr  = (unsigned int)nodeBuf[0]
                             | ((unsigned int)nodeBuf[1] << 8)
                             | ((unsigned int)nodeBuf[2] << 16);
                    rightPtr = (unsigned int)nodeBuf[3]
                             | ((unsigned int)nodeBuf[4] << 8)
                             | ((unsigned int)nodeBuf[5] << 16);
                    nextPtr = bit ? rightPtr : leftPtr;
                    if (nextPtr >= databaseSegments) {
                        break; /* reached leaf */
                    }
                    offset = nextPtr;
                }

            if (databaseType == 1) {
                /* Country edition */
                int countryId = (int)nextPtr - (int)databaseSegments;
                if (countryId < 0) countryId = 0;
                trap_SendServerCommand(
                    ent - g_entities,
                    va("print \"^3[GeoIP]^7 %s loaded. IP: %s -> country=%s (%s) id=%d\n\"",
                       path, ipStr, GeoIPCountryNameById(countryId), GeoIPCountryCodeById(countryId), countryId)
                );
            } else if (databaseType == 2 || databaseType == 6) {
                /* City edition Rev1 or Rev0: resolve record at records area */
                unsigned long recordsBase;
                unsigned long recPos;
                char cityBuf[128];
                int p;
                int countryIndex;
                unsigned char ch;

                recordsBase = (unsigned long)databaseSegments * 2ul * (unsigned long)recordLength;
                recPos = recordsBase;
                countryIndex = -1;
                if (nextPtr >= databaseSegments) {
                    recPos += ((unsigned long)nextPtr - (unsigned long)databaseSegments);
                }
                if (recPos < (unsigned long)filelen) {
                    if (trap_FS_Seek(fh, (long)recPos, FS_SEEK_SET) == 0) {
                        /* country index (1 byte) */
                        trap_FS_Read(&ch, 1, fh); countryIndex = (int)ch; recPos++;
                        /* region (skip zero-terminated string) */
                        while (recPos < (unsigned long)filelen) {
                            trap_FS_Read(&ch, 1, fh); recPos++;
                            if (ch == '\0') break;
                        }
                        /* city string */
                        p = 0;
                        while (recPos < (unsigned long)filelen && p < (int)sizeof(cityBuf) - 1) {
                            trap_FS_Read(&ch, 1, fh); recPos++;
                            if (ch == '\0') break;
                            cityBuf[p++] = (char)ch;
                        }
                        cityBuf[p] = '\0';
                    }
                }

                if (countryIndex < 0) countryIndex = 0;
                trap_SendServerCommand(
                    ent - g_entities,
                    va("print \"^3[GeoIP]^7 %s loaded. IP: %s -> country=%s (%s) id=%d, city=%s\n\"",
                       path, ipStr, GeoIPCountryNameById(countryIndex), GeoIPCountryCodeById(countryIndex), countryIndex, cityBuf)
                );
            } else {
                trap_SendServerCommand(
                    ent - g_entities,
                    va("print \"^3[GeoIP]^7 %s loaded. IP: %s -> dbType=%d (lookup not implemented)\n\"",
                       path, ipStr, databaseType)
                );
            }
        } else {
            trap_SendServerCommand(ent - g_entities, va("print \"^3[GeoIP]^7 %s loaded. IP: %s (IPv6 not supported yet)\n\"", path, ipStr));
        }
    }
    trap_FS_FCloseFile(fh);
}
