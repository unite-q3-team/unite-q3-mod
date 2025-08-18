// code/game/cmds/player.c
#include "cmds.h"
/* Toggle: visualize available spawn points with harmless plasma blobs */
void spawns_f(gentity_t *ent) {
    qboolean enable;
    int i;
    if ( !ent || !ent->client || !ent->authed ) return;
    enable = !ent->client->pers.showSpawnMarkers;
    ent->client->pers.showSpawnMarkers = enable;
    if ( enable ) {
        int sent = 0;
        for ( i = 0; i < level.numSpawnSpots; ++i ) {
            gentity_t *spot = level.spawnSpots[i];
            vec3_t org;
            vec3_t dir;
            gentity_t *m;
            gentity_t *flag;
            if ( !spot ) continue;
            VectorCopy( spot->r.currentOrigin, org );
            org[2] += 24;
            VectorSet( dir, 0, 0, 1 );
            m = fire_plasma( ent, org, dir );
            if ( !m ) continue;
            /* make it a harmless, stationary marker visible only to this client */
            m->damage = 0;
            m->splashDamage = 0;
            m->splashRadius = 0;
            m->clipmask = 0;
            m->r.contents = 0;
            m->touch = NULL;
            m->s.pos.trType = TR_STATIONARY;
            m->s.pos.trTime = level.time;
            VectorClear( m->s.pos.trDelta );
            m->s.time = level.time;
            m->r.svFlags |= SVF_SINGLECLIENT;
            m->r.singleClient = ent->s.number;
            m->think = G_FreeEntity;
            m->nextthink = level.time + 3200;
            /* add team flag indicator for RED/BLUE */
            if ( spot->fteam == TEAM_RED || spot->fteam == TEAM_BLUE ) {
                flag = G_TempEntity( org, EV_GENERAL_SOUND ); /* temp entity type as carrier */
                flag->s.eType = ET_ITEM;
                flag->r.svFlags |= SVF_SINGLECLIENT;
                flag->r.singleClient = ent->s.number;
                /* reuse eventParm to signal red/blue; client can render default pickup effect; minimal */
                flag->s.eventParm = (spot->fteam == TEAM_RED) ? PW_REDFLAG : PW_BLUEFLAG;
                flag->think = G_FreeEntity;
                flag->nextthink = level.time + 3200;
            }
            sent++;
            if ( sent >= 128 ) break; /* throttle per call */
        }
        trap_SendServerCommand( ent - g_entities, "print \"^3spawns: ^2ON\n\"" );
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3spawns: ^1OFF\n\"" );
    }
}

#include "../g_team.h"

/* GeoIP Legacy tables (codes and names) subset imported from cmds.c to avoid cross-file deps */
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

static const char *GeoIPCountryCodeById(int id) { if (id < 0 || id >= GEOIP_NUM_COUNTRIES) return "--"; return GeoIPCountryCodes[id]; }
static const char *GeoIPCountryNameById(int id) { if (id < 0 || id >= GEOIP_NUM_COUNTRIES) return "Unknown"; return GeoIPCountryNames[id]; }

/* Minimal IPv4 parser (A.B.C.D or A.B.C.D:port) */
static qboolean G_ParseIPv4ToUint_local(const char *ipString, unsigned int *outIpv4) {
    char buf[64];
    int i, colonCount, idx, part;
    unsigned int a, b, c, d;
    if (!ipString || !ipString[0] || !outIpv4) return qfalse;
    Q_strncpyz(buf, ipString, sizeof(buf));
    for (i = 0; buf[i]; ++i) {
        if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r') { buf[i] = '\0'; break; }
    }
    colonCount = 0; for (i = 0; buf[i]; ++i) if (buf[i] == ':') colonCount++;
    if (colonCount > 1) return qfalse; if (colonCount == 1) { for (i = 0; buf[i]; ++i) { if (buf[i] == ':') { buf[i] = '\0'; break; } } }
    a = b = c = d = 0; idx = 0;
    for (part = 0; part < 4; ++part) {
        unsigned int val = 0u; int haveDigit = 0;
        while (buf[idx] >= '0' && buf[idx] <= '9') { haveDigit = 1; val = val * 10u + (unsigned int)(buf[idx] - '0'); if (val > 255u) return qfalse; idx++; }
        if (!haveDigit) return qfalse; if (part < 3) { if (buf[idx] != '.') return qfalse; idx++; }
        if (part == 0) a = val; else if (part == 1) b = val; else if (part == 2) c = val; else d = val;
    }
    if (buf[idx] != '\0') return qfalse; *outIpv4 = (a << 24) | (b << 16) | (c << 8) | d; return qtrue;
}

/* Lookup country index from geoip.dat for IPv4 string; optionally returns city if City DB */
qboolean GeoIP_LookupCountryForIP(const char *ipStr, int *outCountryIndex, char *outCode, int outCodeSize, char *outName, int outNameSize, char *outCity, int outCitySize) {
    char path[MAX_QPATH];
    fileHandle_t fh;
    int filelen;
    unsigned int ipv4;
    unsigned char tail[64];
    int readLen, i, markerIndex, databaseType;
    unsigned int databaseSegments;
    const int recordLength = 3;
    unsigned int offset, nextPtr;
    int depth, bit;
    unsigned long nodeByteOffset;
    unsigned char nodeBuf[6];
    unsigned int leftPtr, rightPtr;
    unsigned long recordsBase, recPos;
    unsigned char ch;
    int p;

    if (outCountryIndex) *outCountryIndex = 0;
    if (outCode && outCodeSize) outCode[0] = '\0';
    if (outName && outNameSize) outName[0] = '\0';
    if (outCity && outCitySize) outCity[0] = '\0';
    if (!G_ParseIPv4ToUint_local(ipStr, &ipv4)) return qfalse;

    Com_sprintf(path, sizeof(path), "%s.dat", "geoip");
    filelen = trap_FS_FOpenFile(path, &fh, FS_READ);
    if (filelen <= 0 || fh == FS_INVALID_HANDLE) return qfalse;

    readLen = sizeof(tail); if (filelen < readLen) readLen = filelen;
    markerIndex = -1; databaseType = -1; databaseSegments = 16776960u;
    if (trap_FS_Seek(fh, -((long)readLen), FS_SEEK_END) == 0) {
        trap_FS_Read(tail, readLen, fh);
        for (i = readLen - 7; i >= 0; --i) { if (tail[i] == 0xFF && tail[i+1] == 0xFF && tail[i+2] == 0xFF) { markerIndex = i; break; } }
        if (markerIndex >= 0) {
            databaseType = (int)tail[markerIndex + 3];
            if (markerIndex + 7 < readLen) {
                databaseSegments = (unsigned int)tail[markerIndex + 4] | ((unsigned int)tail[markerIndex + 5] << 8) | ((unsigned int)tail[markerIndex + 6] << 16);
            }
        }
    }

    offset = 0u; nextPtr = 0u;
    for (depth = 31; depth >= 0; --depth) {
        bit = (int)((ipv4 >> depth) & 1u);
        nodeByteOffset = (unsigned long)offset * 2ul * (unsigned long)recordLength;
        if (nodeByteOffset + (2 * recordLength) > (unsigned long)filelen) break;
        if (trap_FS_Seek(fh, (long)nodeByteOffset, FS_SEEK_SET) != 0) break;
        trap_FS_Read(nodeBuf, 6, fh);
        leftPtr  = (unsigned int)nodeBuf[0] | ((unsigned int)nodeBuf[1] << 8) | ((unsigned int)nodeBuf[2] << 16);
        rightPtr = (unsigned int)nodeBuf[3] | ((unsigned int)nodeBuf[4] << 8) | ((unsigned int)nodeBuf[5] << 16);
        nextPtr = bit ? rightPtr : leftPtr;
        if (nextPtr >= databaseSegments) break; offset = nextPtr;
    }

    if (databaseType == 1) {
        i = (int)nextPtr - (int)databaseSegments; if (i < 0) i = 0;
        if (outCountryIndex) *outCountryIndex = i;
        if (outCode && outCodeSize) Q_strncpyz(outCode, GeoIPCountryCodeById(i), outCodeSize);
        if (outName && outNameSize) Q_strncpyz(outName, GeoIPCountryNameById(i), outNameSize);
        trap_FS_FCloseFile(fh);
        return qtrue;
    } else if (databaseType == 2 || databaseType == 6) {
        recordsBase = (unsigned long)databaseSegments * 2ul * (unsigned long)recordLength;
        recPos = recordsBase; if (nextPtr >= databaseSegments) recPos += ((unsigned long)nextPtr - (unsigned long)databaseSegments);
        if (recPos < (unsigned long)filelen && trap_FS_Seek(fh, (long)recPos, FS_SEEK_SET) == 0) {
            /* country index */
            trap_FS_Read(&ch, 1, fh); i = (int)ch; recPos++;
            /* skip region (zero-terminated) */
            for (;;) {
                if (recPos >= (unsigned long)filelen) break;
                trap_FS_Read(&ch, 1, fh); recPos++;
                if (ch == '\0') break;
            }
            /* read city (zero-terminated) */
            if (outCity && outCitySize) {
                p = 0;
                for (;;) {
                    if (recPos >= (unsigned long)filelen) break;
                    trap_FS_Read(&ch, 1, fh); recPos++;
                    if (ch == '\0') break;
                    if (p < outCitySize - 1) outCity[p++] = (char)ch;
                }
                outCity[p] = '\0';
            }
        } else {
            i = 0; if (outCity && outCitySize) outCity[0] = '\0';
        }
        if (outCountryIndex) *outCountryIndex = i;
        if (outCode && outCodeSize) Q_strncpyz(outCode, GeoIPCountryCodeById(i), outCodeSize);
        if (outName && outNameSize) Q_strncpyz(outName, GeoIPCountryNameById(i), outNameSize);
        trap_FS_FCloseFile(fh);
        return qtrue;
    }
    trap_FS_FCloseFile(fh);
    return qfalse;
}

void AlignString(char *dest, int destSize, const char *src, int width, qboolean leftAlign) {
    int len;
    int pad;

    len = strlen(src);
    if (len > width) len = width;
    pad = width - len;

    if (leftAlign) {
        Q_strncpyz(dest, src, len + 1);
        if (pad > 0) memset(dest + len, ' ', pad);
    } else {
        if (pad > 0) memset(dest, ' ', pad);
        Q_strncpyz(dest + pad, src, len + 1);
    }
    dest[width] = '\0';
}


void Cmd_Plrlist_f(gentity_t *ent) {
    char buffer[8192];
    int i;
    gclient_t *cl;
    char userinfo[MAX_INFO_STRING];
    char name_padded[MAX_QPATH];
    char rate_buf[16];
    char snaps_buf[16];
    char cc_buf[16];
    const char *osp_str;
    const char *mod_str;
    const char *cc_str;
    char rate_aligned[7];
    char snaps_aligned[5];
    char mod_aligned[10];
    char cc_aligned[5];
    char teamChar[4];
    int activePlayers = 0;
    int nudge;

    // Инициализация буфера
    buffer[0] = '\0';
    Q_strcat(buffer, sizeof(buffer),
        va(" ^3Map^7 ^1: ^2%s\n\n  ^3ID ^1: ^3Players                          Nudge   Rate  Snaps  MOD  CC\n", g_mapname.string));
    Q_strcat(buffer, sizeof(buffer),
        "^1----------------------------------------------------------------------\n");

    // Основной цикл
    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];

        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        activePlayers++;

        // Определение команды
        switch (cl->sess.sessionTeam) {
        case TEAM_RED:
            strcpy(teamChar, "^1R");
            break;
        case TEAM_BLUE:
            strcpy(teamChar, "^4B");
            break;
        case TEAM_SPECTATOR:
            strcpy(teamChar, "^3S");
            break;
        case TEAM_FREE:
            strcpy(teamChar, "^7F");
            break;
        default:
            strcpy(teamChar, " ");
            break;
        }

        trap_GetUserinfo(i, userinfo, sizeof(userinfo));

        // Получение данных
        nudge = GetUserinfoInt(userinfo, "cl_timeNudge", 0);

        strncpy(rate_buf, GetUserinfoString(userinfo, "rate", "0"), sizeof(rate_buf) - 1);
        rate_buf[sizeof(rate_buf) - 1] = '\0';

        strncpy(snaps_buf, GetUserinfoString(userinfo, "snaps", "0"), sizeof(snaps_buf) - 1);
        snaps_buf[sizeof(snaps_buf) - 1] = '\0';

        osp_str = GetUserinfoString(userinfo, "osp_client", "");

        if (osp_str && osp_str[0] != '\0') {
            if (Q_stristr(osp_str, "1008_OSP2")) {
                mod_str = "^1OSP2";
            } else if (Q_stristr(osp_str, "1008_OSP2_be")) {
                mod_str = "^1OSP2^7BE";
            } else {
                mod_str = "^3OSP";
            }
        } else {
            mod_str = "^1---";
        }


        {
            const char *ip;
            int countryIndex;
            char cc[8];
            char name[64];
            char dummyCity[2];
            ip = GetUserinfoString(userinfo, "ip", "");
            if (ip && ip[0] && GeoIP_LookupCountryForIP(ip, &countryIndex, cc, sizeof(cc), name, sizeof(name), dummyCity, sizeof(dummyCity))) {
                Com_sprintf(cc_buf, sizeof(cc_buf), "^2%s", cc);
                cc_str = cc_buf;
            } else {
                cc_str = "^1??";
            }
        }

        // Выравнивание данных
        AlignString(rate_aligned, sizeof(rate_aligned), rate_buf, 6, qfalse);
        AlignString(snaps_aligned, sizeof(snaps_aligned), snaps_buf, 4, qfalse);
        AlignString(mod_aligned, sizeof(mod_aligned), mod_str, 8, qfalse);
        // AlignString(cc_aligned, sizeof(cc_aligned), cc_str, 6, qfalse);

        // Форматирование имени
        Q_FixNameWidth(cl->pers.netname, name_padded, 32);

        // Добавление строки в буфер
        Q_strcat(buffer, sizeof(buffer),
            va("%s ^7%2d ^1:^7 %s ^7%5d %6s   %4s %s %s\n",
                teamChar, i, name_padded, nudge, rate_aligned, snaps_aligned, mod_aligned, cc_str));
    }

        Q_strcat(buffer, sizeof(buffer),
            va("\n^3Total players: ^7%d\n", activePlayers));

    SendServerCommandInChunks(ent, va("\n%s\n", buffer));
}

void Cmd_From_f(gentity_t *ent){
    char buffer[8192];
    char header[256];
    char serverIP[64];
    char serverCC[8];
    char serverCountry[64];
    char serverCity[64];
    int i, active;
    gclient_t *cl;
    char userinfo[MAX_INFO_STRING];
    char name_aligned[40];
    char country_aligned[32];
    int countryIndex;
    const char *ip;

    /* column widths to match reference */
    const int NAME_W = 31;
    const int COUNTRY_W = 23;

    buffer[0] = '\0';
    header[0] = '\0';
    serverIP[0] = '\0';
    trap_Cvar_VariableStringBuffer("sv_ip", serverIP, sizeof(serverIP));
    if (!serverIP[0]) trap_Cvar_VariableStringBuffer("net_ip", serverIP, sizeof(serverIP));
    if (serverIP[0]) {
        if (!GeoIP_LookupCountryForIP(serverIP, &countryIndex, serverCC, sizeof(serverCC), serverCountry, sizeof(serverCountry), serverCity, sizeof(serverCity))) {
            serverCountry[0] = '\0'; serverCC[0] = '\0'; serverCity[0] = '\0';
        }
    }
    if (serverCity[0] || serverCC[0]) {
        Q_strncpyz(header, va("^3Server^1: ^2%s%s%s\n\n", serverCity[0]?serverCity:"", serverCity[0]?", ":"", serverCC[0]?serverCC:"--"), sizeof(header));
    } else {
        Q_strncpyz(header, "^3Server^1: ^2Unknown, --\n\n", sizeof(header));
    }
    Q_strcat(buffer, sizeof(buffer), header);

    Q_strcat(buffer, sizeof(buffer), "^1ID  Name                             Country                  CC\n");
    Q_strcat(buffer, sizeof(buffer), "^3-----------------------------------------------------------------\n");

    active = 0;
    for (i = 0; i < level.maxclients; i++) {
        cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) continue; active++;
        trap_GetUserinfo(i, userinfo, sizeof(userinfo));
        ip = GetUserinfoString(userinfo, "ip", "");
        serverCountry[0] = '\0'; serverCC[0] = '\0';
        if (g_entities[i].r.svFlags & SVF_BOT) {
            Q_strncpyz(serverCountry, "Botland", sizeof(serverCountry));
            Q_strncpyz(serverCC, "BT", sizeof(serverCC));
        } else if (ip && ip[0]) {
            GeoIP_LookupCountryForIP(ip, &countryIndex, serverCC, sizeof(serverCC), serverCountry, sizeof(serverCountry), serverCity, sizeof(serverCity));
        }
        if (!serverCountry[0]) Q_strncpyz(serverCountry, "Unknown", sizeof(serverCountry));
        if (!serverCC[0]) Q_strncpyz(serverCC, "--", sizeof(serverCC));

        Q_FixNameWidth(cl->pers.netname, name_aligned, NAME_W);
        AlignString(country_aligned, sizeof(country_aligned), serverCountry, COUNTRY_W, qtrue);

        Q_strcat(buffer, sizeof(buffer),
            va("^7%2d  %s  ^2%s  ^2%s\n", i, name_aligned, country_aligned, serverCC));
    }

    SendServerCommandInChunks(ent, va("\n%s\n", buffer));
}

void killplayer_f(gentity_t *ent){
    char buf[MAX_TOKEN_CHARS];
    gentity_t *victim;

    if (!ent->client || !ent->authed) return;

    if (trap_Argc() != 2) {
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage: killplayer <id>\n\"");
        return;
    }

    trap_Argv(1, buf, sizeof(buf));
    victim = &g_entities[atoi(buf)];

    if (!victim->client){
        trap_SendServerCommand(ent - g_entities, "print \"^1Invalid client!\n\"");
        return;
    }

	if ((g_freeze.integer && ftmod_isSpectator(victim->client)) || (!g_freeze.integer && victim->client->sess.sessionTeam == TEAM_SPECTATOR)) {
        trap_SendServerCommand(ent - g_entities, "print \"^1Can't kill a spectator!\n\"");
		return;
	}

    if (victim->health <= 0){
        trap_SendServerCommand(ent - g_entities, "print \"^1Can't kill already dead person\n\"");
        return;
    }

    victim->flags &= ~FL_GODMODE;
    victim->client->ps.stats[STAT_HEALTH] = victim->health = -999;

    player_die(victim, victim, victim, 100000, (MOD_SUICIDE));
}

/* Admin: force change a player's name: setname <clientNum> <new name...> */
void setname_f(gentity_t *ent) {
    char idbuf[MAX_TOKEN_CHARS];
    char newname[MAX_INFO_STRING];
    char userinfo[MAX_INFO_STRING];
    gentity_t *victim;
    int clientNum;

    if (!ent || !ent->client || !ent->authed) return;

    if ( trap_Argc() < 3 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3Usage: setname <id> <new name>\n\"" );
        return;
    }

    trap_Argv( 1, idbuf, sizeof(idbuf) );
    clientNum = atoi( idbuf );
    if ( clientNum < 0 || clientNum >= level.maxclients ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid client id.\n\"" );
        return;
    }

    victim = &g_entities[ clientNum ];
    if ( !victim || !victim->client || level.clients[clientNum].pers.connected != CON_CONNECTED ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Invalid client!\n\"" );
        return;
    }

    /* Gather the rest of args as the new name, from argv[2..] */
    {
        char part[MAX_TOKEN_CHARS];
        int i, n = trap_Argc();
        newname[0] = '\0';
        for ( i = 2; i < n; ++i ) {
            trap_Argv( i, part, sizeof(part) );
            Q_strcat( newname, sizeof(newname), part );
            if ( i + 1 < n ) Q_strcat( newname, sizeof(newname), " " );
        }
    }

    /* Sanitize name similar to bot add */
    {
        char cleaned[MAX_INFO_STRING];
        BG_CleanName( newname, cleaned, sizeof(cleaned), "unnamed" );
        Q_strncpyz( newname, cleaned, sizeof(newname) );
    }

    trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );
    Info_SetValueForKey( userinfo, "name", newname );
    trap_SetUserinfo( clientNum, userinfo );
    ClientUserinfoChanged( clientNum );
    trap_SendServerCommand( ent - g_entities, va("print \"^2setname: renamed client ^7%d -> ^N%s\n\"", clientNum, newname) );
}

void fteam_f(gentity_t *ent){
	gentity_t	*victim;
	char		str[MAX_TOKEN_CHARS];

    if (!ent->authed) return;

	if ( trap_Argc() < 3 ) {
		trap_SendServerCommand(ent - g_entities, "print \"^3Usage:\n   ^3fteam <player> <team>\n\"");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );
    victim = &g_entities[atoi(str)];

    if (!victim->client){
        trap_SendServerCommand(ent - g_entities, "print \"^1Invalid client!\n\"");
        return;
    }

	// set the team
	trap_Argv( 2, str, sizeof( str ) );
	SetTeam( &g_entities[victim->client - level.clients], str );
}

/* Soft team move without killing: returns qtrue if changed */
static qboolean SoftMoveToTeam( gentity_t *ent, team_t newTeam ) {
    gclient_t *client;
    int clientNum;
    team_t oldTeam;
    int teamLeader;
    qboolean hadFlag = qfalse;

    if ( !ent || !ent->client ) return qfalse;
    client = ent->client;
    oldTeam = client->sess.sessionTeam;
    if ( oldTeam == newTeam ) return qfalse;
    clientNum = ent - g_entities;

    /* Return CTF flags if carried */
#ifdef MISSIONPACK
    if ( client->ps.powerups[PW_NEUTRALFLAG] ) { Team_ReturnFlag( TEAM_FREE ); client->ps.powerups[PW_NEUTRALFLAG] = 0; hadFlag = qtrue; }
#endif
    if ( client->ps.powerups[PW_REDFLAG] ) { Team_ReturnFlag( TEAM_RED ); client->ps.powerups[PW_REDFLAG] = 0; hadFlag = qtrue; }
    if ( client->ps.powerups[PW_BLUEFLAG] ) { Team_ReturnFlag( TEAM_BLUE ); client->ps.powerups[PW_BLUEFLAG] = 0; hadFlag = qtrue; }
    /* Clear all powerups */
    memset( client->ps.powerups, 0, sizeof(client->ps.powerups) );

    /* Prepare to spawn on new team */
    client->pers.teamState.state = TEAM_BEGIN;

    /* Revert any cast votes when switching between playing teams */
    if ( oldTeam != newTeam ) {
        G_RevertVote( client );
    }

    /* Reset extended stats similar to SetTeam */
    {
        int w;
        client->accuracy_hits = 0;
        client->accuracy_shots = 0;
        client->totalDamageGiven = 0;
        client->totalDamageTaken = 0;
        client->kills = 0;
        client->deaths = 0;
        client->currentKillStreak = 0;
        client->armorPickedTotal = 0;
        client->healthPickedTotal = 0;
        for ( w = 0; w < WP_NUM_WEAPONS; ++w ) {
            client->perWeaponDamageGiven[w] = 0;
            client->perWeaponDamageTaken[w] = 0;
            client->perWeaponShots[w] = 0;
            client->perWeaponHits[w] = 0;
            client->perWeaponKills[w] = 0;
            client->perWeaponDeaths[w] = 0;
            client->perWeaponPickups[w] = 0;
            client->perWeaponDrops[w] = 0;
        }
    }

    /* Apply team switch */
    client->sess.sessionTeam = newTeam;
    client->sess.spectatorState = SPECTATOR_NOT;
    /* Maintain spectatorClient as-is */

    /* Ensure team has a leader */
    if ( newTeam == TEAM_RED || newTeam == TEAM_BLUE ) {
        teamLeader = TeamLeader( newTeam );
        if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
            SetLeader( newTeam, clientNum );
        }
    }
    /* Maintain leader on the team the player left */
    if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
        if ( client->sess.teamLeader ) {
            CheckTeamLeader( oldTeam );
        }
        client->sess.teamLeader = qfalse;
    }

    G_WriteClientSessionData( client );
    BroadcastTeamChange( client, oldTeam );
    ClientUserinfoChanged( clientNum );
    ent->freezeState = qfalse;
    ClientBegin( clientNum );
    /* Optional: announce soft swap silently; shuffle handles announcements */
    (void)hadFlag;
    return qtrue;
}

void shuffle_f(gentity_t *ent){
    int i;
    char mode[16];
    int playerIdx[MAX_CLIENTS];
    int playerScore[MAX_CLIENTS];
    int numPlayers;
    int targetRed;
    int targetBlue;
    int oldTFB;
    const char *modeText;

    if (!ent || !ent->client || !ent->authed) {
        return;
    }

    if (g_gametype.integer < GT_TEAM) {
        trap_SendServerCommand(ent - g_entities, "print \"^1Command is only available in team gametypes.\n\"");
        return;
    }

    if (trap_Argc() < 2) {
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage: shuffle <random|score>\n\"");
        return;
    }

    trap_Argv(1, mode, sizeof(mode));

    if (Q_stricmp(mode, "random") != 0 && Q_stricmp(mode, "score") != 0) {
        trap_SendServerCommand(ent - g_entities, "print \"^3Usage: shuffle <random|score>\n\"");
        return;
    }

    numPlayers = 0;
    for (i = 0; i < level.maxclients; ++i) {
        gclient_t *cl = &level.clients[i];
        if (cl->pers.connected != CON_CONNECTED) {
            continue;
        }
        if (g_freeze.integer ? ftmod_isSpectator(cl) : (cl->sess.sessionTeam == TEAM_SPECTATOR)) {
            continue;
        }
        playerIdx[numPlayers] = i;
        playerScore[numPlayers] = cl->ps.persistant[PERS_SCORE];
        numPlayers++;
    }

    if (numPlayers < 2) {
        trap_SendServerCommand(ent - g_entities, "print \"^1Not enough players to shuffle.\n\"");
        return;
    }

    targetRed = numPlayers / 2;
    targetBlue = numPlayers / 2;
    if (numPlayers % 2) {
        if (rand() & 1) targetRed++; else targetBlue++;
    }

    if (Q_stricmp(mode, "random") == 0) {
        int j;
        for (i = numPlayers - 1; i > 0; --i) {
            int r = rand() % (i + 1);
            int tmpIdx = playerIdx[i];
            int tmpSc = playerScore[i];
            playerIdx[i] = playerIdx[r];
            playerScore[i] = playerScore[r];
            playerIdx[r] = tmpIdx;
            playerScore[r] = tmpSc;
        }
        modeText = "RANDOM";
    } else {
        int a, b, maxPos;
        /* selection sort by score desc */
        for (a = 0; a < numPlayers - 1; ++a) {
            maxPos = a;
            for (b = a + 1; b < numPlayers; ++b) {
                if (playerScore[b] > playerScore[maxPos]) {
                    maxPos = b;
                }
            }
            if (maxPos != a) {
                int ti = playerIdx[a];
                int ts = playerScore[a];
                playerIdx[a] = playerIdx[maxPos];
                playerScore[a] = playerScore[maxPos];
                playerIdx[maxPos] = ti;
                playerScore[maxPos] = ts;
            }
        }
        modeText = "SCORE";
    }

    /* For score mode, distribute to balance totals while respecting target counts */
    {
        int redCount = 0, blueCount = 0;
        int redSum = 0, blueSum = 0;
        team_t assignedTeam[MAX_CLIENTS];
        int k;

        if (Q_stricmp(mode, "score") == 0) {
            for (k = 0; k < numPlayers; ++k) {
                int sc = playerScore[k];
                team_t t;
                if (redCount >= targetRed) {
                    t = TEAM_BLUE;
                } else if (blueCount >= targetBlue) {
                    t = TEAM_RED;
                } else if (redSum <= blueSum) {
                    t = TEAM_RED;
                } else {
                    t = TEAM_BLUE;
                }
                assignedTeam[k] = t;
                if (t == TEAM_RED) { redSum += sc; redCount++; }
                else { blueSum += sc; blueCount++; }
            }
        } else {
            for (k = 0; k < numPlayers; ++k) {
                team_t t = (redCount < targetRed) ? TEAM_RED : TEAM_BLUE;
                assignedTeam[k] = t;
                if (t == TEAM_RED) redCount++; else blueCount++;
            }
        }

        /* Announce */
        G_BroadcastServerCommand(-1, va("print \"^3Shuffling teams:^1 %s\n\"", modeText));

        /* Temporarily disable team force-balance to avoid blocking reshuffle */
        oldTFB = trap_Cvar_VariableIntegerValue("g_teamForceBalance");
        if (oldTFB != 0) {
            trap_Cvar_Set("g_teamForceBalance", "0");
        }

        for (k = 0; k < numPlayers; ++k) {
            int ci = playerIdx[k];
            team_t t = assignedTeam[k];
            const char *tDisp = (t == TEAM_RED) ? "^1RED" : "^4BLUE";
            gentity_t *ply = &g_entities[ci];
            if ( level.clients[ci].sess.sessionTeam != t ) {
                if ( SoftMoveToTeam( ply, t ) ) {
                    G_BroadcastServerCommand(-1, va("print \"^3Moved ^7%s ^3to %s ^3team\n\"", level.clients[ci].pers.netname, tDisp));
                }
            }
        }

        if (oldTFB != 0) {
            trap_Cvar_Set("g_teamForceBalance", va("%d", oldTFB));
        }
    }

    CalculateRanks();
}

/* Perform team shuffle with mode: "random" or "score"; returns qtrue if applied */
qboolean Shuffle_Perform( const char *mode ) {
    int i;
    int playerIdx[MAX_CLIENTS];
    int playerScore[MAX_CLIENTS];
    int numPlayers;
    int targetRed;
    int targetBlue;
    int oldTFB;
    const char *modeText;
    team_t assignedTeam[MAX_CLIENTS];
    int redCount, blueCount, redSum, blueSum;
    int k;

    if ( !mode || !mode[0] ) return qfalse;
    if ( g_gametype.integer < GT_TEAM ) return qfalse;

    /* collect active players */
    numPlayers = 0;
    for ( i = 0; i < level.maxclients; ++i ) {
        gclient_t *cl = &level.clients[i];
        if ( cl->pers.connected != CON_CONNECTED ) continue;
        if ( g_freeze.integer ? ftmod_isSpectator(cl) : (cl->sess.sessionTeam == TEAM_SPECTATOR) ) continue;
        playerIdx[numPlayers] = i;
        playerScore[numPlayers] = cl->ps.persistant[PERS_SCORE];
        numPlayers++;
    }
    if ( numPlayers < 2 ) return qfalse;

    targetRed = numPlayers / 2;
    targetBlue = numPlayers / 2;
    if ( numPlayers % 2 ) {
        if ( rand() & 1 ) targetRed++; else targetBlue++;
    }

    if ( Q_stricmp(mode, "random") == 0 ) {
        int j;
        for ( i = numPlayers - 1; i > 0; --i ) {
            int r = rand() % (i + 1);
            int tmpIdx = playerIdx[i];
            int tmpSc = playerScore[i];
            playerIdx[i] = playerIdx[r];
            playerScore[i] = playerScore[r];
            playerIdx[r] = tmpIdx;
            playerScore[r] = tmpSc;
        }
        modeText = "RANDOM";
    } else {
        int a, b, maxPos;
        for ( a = 0; a < numPlayers - 1; ++a ) {
            maxPos = a;
            for ( b = a + 1; b < numPlayers; ++b ) {
                if ( playerScore[b] > playerScore[maxPos] ) maxPos = b;
            }
            if ( maxPos != a ) {
                int ti = playerIdx[a];
                int ts = playerScore[a];
                playerIdx[a] = playerIdx[maxPos];
                playerScore[a] = playerScore[maxPos];
                playerIdx[maxPos] = ti;
                playerScore[maxPos] = ts;
            }
        }
        modeText = "SCORE";
    }

    redCount = 0; blueCount = 0; redSum = 0; blueSum = 0;
    if ( Q_stricmp(mode, "score") == 0 ) {
        for ( k = 0; k < numPlayers; ++k ) {
            int sc = playerScore[k];
            team_t t;
            if ( redCount >= targetRed ) t = TEAM_BLUE;
            else if ( blueCount >= targetBlue ) t = TEAM_RED;
            else if ( redSum <= blueSum ) t = TEAM_RED;
            else t = TEAM_BLUE;
            assignedTeam[k] = t;
            if ( t == TEAM_RED ) { redSum += sc; redCount++; } else { blueSum += sc; blueCount++; }
        }
    } else {
        for ( k = 0; k < numPlayers; ++k ) {
            team_t t = (redCount < targetRed) ? TEAM_RED : TEAM_BLUE;
            assignedTeam[k] = t;
            if ( t == TEAM_RED ) redCount++; else blueCount++;
        }
    }

    G_BroadcastServerCommand( -1, va("print \"^3Shuffling teams:^1 %s\n\"", modeText) );
    oldTFB = trap_Cvar_VariableIntegerValue("g_teamForceBalance");
    if ( oldTFB != 0 ) trap_Cvar_Set("g_teamForceBalance", "0");
    for ( k = 0; k < numPlayers; ++k ) {
        int ci = playerIdx[k];
        team_t t = assignedTeam[k];
        const char *tDisp = (t == TEAM_RED) ? "^1RED" : "^4BLUE";
        gentity_t *ply = &g_entities[ci];
        if ( level.clients[ci].sess.sessionTeam != t ) {
            if ( SoftMoveToTeam( ply, t ) ) {
                G_BroadcastServerCommand( -1, va("print \"^3Moved ^7%s ^3to %s ^3team\n\"", level.clients[ci].pers.netname, tDisp) );
            }
        }
    }
    if ( oldTFB != 0 ) trap_Cvar_Set("g_teamForceBalance", va("%d", oldTFB) );
    CalculateRanks();
    return qtrue;
}

void printRoundTime (void) {
    Com_Printf("Round time:%i", level.freezeRoundStartTime);
}


void Cmd_UserinfoDump_f(gentity_t *ent) {
    char userinfo[MAX_INFO_STRING];
    char buffer[1024];
    int i;

    Q_strncpyz(buffer, "^2--- Userinfo Dump ---\n", sizeof(buffer));

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected != CON_CONNECTED) {
            continue;
        }

        trap_GetUserinfo(i, userinfo, sizeof(userinfo));

        if (userinfo[0] == '\0') {
            Q_strcat(buffer, sizeof(buffer),
                va("^1Client %d: <empty userinfo>\n", i));
        } else {
            Q_strcat(buffer, sizeof(buffer),
                va("^3Client %d^7: %s\n", i, userinfo));
        }

        // Если буфер почти переполнен, отправим кусок
        if (strlen(buffer) > 900) {
            trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", buffer));
            buffer[0] = '\0';
        }
    }

    if (buffer[0]) {
        trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", buffer));
    }
}

/* Adds a new FFA spawn point at player's position */
void spawnadd_f(gentity_t *ent) {
    gentity_t *spot;
    vec3_t org;
    SpawnHistory_Push();
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( level.numSpawnSpots >= NUM_SPAWN_SPOTS - 1 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Spawn list full\n\"" );
        return;
    }
    VectorCopy( ent->client->ps.origin, org );
    spot = G_Spawn();
    spot->classname = "info_player_deathmatch";
    spot->fteam = TEAM_FREE;
    spot->count = 1;
    G_SetOrigin( spot, org );
    VectorCopy( org, spot->s.origin );
    trap_LinkEntity( spot );
    level.spawnSpots[ level.numSpawnSpots++ ] = spot;
    level.numSpawnSpotsFFA++;
    trap_SendServerCommand( ent - g_entities, va("print \"^2Added spawn at ^7%2.f %2.f %2.f\n\"", org[0], org[1], org[2]) );
}

/* Removes the nearest spawn point within small radius */
void spawnrm_f(gentity_t *ent) {
    int i, idx;
    float bestDist2;
    vec3_t p;
    SpawnHistory_Push();
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( level.numSpawnSpots <= 0 ) return;
    VectorCopy( ent->client->ps.origin, p );
    bestDist2 = 999999999.0f;
    idx = -1;
    for ( i = 0; i < level.numSpawnSpots; ++i ) {
        gentity_t *spot = level.spawnSpots[i];
        float dx, dy, dz, d2;
        if ( !spot ) continue;
        dx = spot->r.currentOrigin[0] - p[0];
        dy = spot->r.currentOrigin[1] - p[1];
        dz = spot->r.currentOrigin[2] - p[2];
        d2 = dx*dx + dy*dy + dz*dz;
        if ( d2 < bestDist2 ) { bestDist2 = d2; idx = i; }
    }
    if ( idx >= 0 && bestDist2 <= 128.0f * 128.0f ) {
        gentity_t *spot = level.spawnSpots[idx];
        trap_SendServerCommand( ent - g_entities, va("print \"^1Removed spawn at ^7%2.f %2.f %2.f\n\"", spot->r.currentOrigin[0], spot->r.currentOrigin[1], spot->r.currentOrigin[2]) );
        G_FreeEntity( spot );
        for ( i = idx + 1; i < level.numSpawnSpots; ++i ) {
            level.spawnSpots[i - 1] = level.spawnSpots[i];
        }
        level.numSpawnSpots--;
        if ( spot->fteam == TEAM_FREE && level.numSpawnSpotsFFA > 0 ) level.numSpawnSpotsFFA--;
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3No spawn point nearby (<=128u)\n\"" );
    }
}

/* Saves current spawn points to spawns.txt using simple key= values */
void spawnsave_f(gentity_t *ent) {
    fileHandle_t f;
    int openRes;
    int i;
    char line[192];
    if ( !ent || !ent->client || !ent->authed ) return;
    openRes = trap_FS_FOpenFile( "spawns.txt", &f, FS_WRITE );
    if ( openRes < 0 ) {
        trap_SendServerCommand( ent - g_entities, "print \"^1Failed to open spawns.txt for write\n\"" );
        return;
    }
    for ( i = 0; i < level.numSpawnSpots; ++i ) {
        gentity_t *spot = level.spawnSpots[i];
        const char *teamStr;
        int isBase;
        if ( !spot ) continue;
        teamStr = (spot->fteam == TEAM_RED) ? "RED" : (spot->fteam == TEAM_BLUE) ? "BLUE" : "FREE";
        isBase = (spot->count == 0) ? 1 : 0;
        Com_sprintf( line, sizeof(line), "spawn.%s.%04d = %s %d %d %d %d %d %d %d\n",
            g_mapname.string,
            i,
            teamStr,
            isBase,
            (int)spot->r.currentOrigin[0], (int)spot->r.currentOrigin[1], (int)spot->r.currentOrigin[2],
            (int)spot->s.angles[PITCH], (int)spot->s.angles[YAW], (int)spot->s.angles[ROLL]
        );
        trap_FS_Write( line, (int)strlen(line), f );
    }
    trap_FS_FCloseFile( f );
    trap_SendServerCommand( ent - g_entities, va("print \"^2Saved %d spawns to spawns.txt\n\"", level.numSpawnSpots) );
}

/* Reload spawns.txt for current map without restart */
void spawnreload_f(gentity_t *ent) {
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( !g_customSpawns.integer ) {
        trap_SendServerCommand( ent - g_entities, "print \"^3g_customSpawns is disabled\n\"" );
        return;
    }
    G_LoadCustomSpawns();
    trap_SendServerCommand( ent - g_entities, "print \"^2Custom spawns reloaded\n\"" );
}

/* Simple undo/redo stacks for spawns: store snapshots in memory */
#define SPAWN_HISTORY_MAX 16
static int s_spawnHistTop = 0; /* points to next free slot */
static int s_spawnHistCount = 0;
static struct { int count; vec3_t pos[128]; team_t team[128]; int base[128]; vec3_t ang[128]; } s_spawnHist[SPAWN_HISTORY_MAX];

static void SpawnHistory_Push(void) {
    int i;
    int idx;
    idx = s_spawnHistTop % SPAWN_HISTORY_MAX;
    s_spawnHist[idx].count = level.numSpawnSpots;
    for ( i = 0; i < level.numSpawnSpots && i < 128; ++i ) {
        gentity_t *spot = level.spawnSpots[i];
        VectorCopy( spot->r.currentOrigin, s_spawnHist[idx].pos[i] );
        s_spawnHist[idx].team[i] = spot->fteam;
        s_spawnHist[idx].base[i] = (spot->count == 0);
        VectorCopy( spot->s.angles, s_spawnHist[idx].ang[i] );
    }
    s_spawnHistTop = (s_spawnHistTop + 1) % SPAWN_HISTORY_MAX;
    if ( s_spawnHistCount < SPAWN_HISTORY_MAX ) s_spawnHistCount++;
}

static qboolean SpawnHistory_Restore(int stepBack) {
    int idx;
    int i;
    if ( s_spawnHistCount <= 0 ) return qfalse;
    /* stepBack 1 => previous snapshot */
    idx = (s_spawnHistTop - stepBack - 1 + SPAWN_HISTORY_MAX) % SPAWN_HISTORY_MAX;
    if ( s_spawnHist[idx].count <= 0 ) return qfalse;
    level.numSpawnSpots = 0;
    level.numSpawnSpotsFFA = 0;
    level.numSpawnSpotsTeam = 0;
    for ( i = 0; i < s_spawnHist[idx].count && i < 128; ++i ) {
        gentity_t *spot = G_Spawn();
        vec3_t org; VectorCopy( s_spawnHist[idx].pos[i], org );
        if ( s_spawnHist[idx].team[i] == TEAM_FREE ) { spot->classname = "info_player_deathmatch"; level.numSpawnSpotsFFA++; }
        else if ( s_spawnHist[idx].team[i] == TEAM_RED ) { spot->classname = s_spawnHist[idx].base[i] ? "team_CTF_redplayer" : "team_CTF_redspawn"; level.numSpawnSpotsTeam++; }
        else { spot->classname = s_spawnHist[idx].base[i] ? "team_CTF_blueplayer" : "team_CTF_bluespawn"; level.numSpawnSpotsTeam++; }
        spot->fteam = s_spawnHist[idx].team[i];
        spot->count = s_spawnHist[idx].base[i] ? 0 : 1;
        VectorCopy( s_spawnHist[idx].ang[i], spot->s.angles );
        G_SetOrigin( spot, org ); VectorCopy(org, spot->s.origin); trap_LinkEntity( spot );
        level.spawnSpots[level.numSpawnSpots++] = spot;
    }
    return qtrue;
}

void spawnundo_f(gentity_t *ent) {
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( SpawnHistory_Restore(1) ) {
        trap_SendServerCommand( ent - g_entities, "print \"^2Undo OK\n\"" );
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3Nothing to undo\n\"" );
    }
}

void spawnredo_f(gentity_t *ent) {
    if ( !ent || !ent->client || !ent->authed ) return;
    if ( SpawnHistory_Restore(0) ) {
        trap_SendServerCommand( ent - g_entities, "print \"^2Redo OK\n\"" );
    } else {
        trap_SendServerCommand( ent - g_entities, "print \"^3Nothing to redo\n\"" );
    }
}

/* Set yaw/pitch for nearest spawn */
static gentity_t *FindNearestSpawn(gentity_t *ent, float *outDist2) {
    int i; gentity_t *best=NULL; float bestD2=9e9f; vec3_t p; VectorCopy(ent->client->ps.origin, p);
    for ( i=0;i<level.numSpawnSpots;i++ ) { gentity_t *s=level.spawnSpots[i]; float dx=s->r.currentOrigin[0]-p[0]; float dy=s->r.currentOrigin[1]-p[1]; float dz=s->r.currentOrigin[2]-p[2]; float d2=dx*dx+dy*dy+dz*dz; if ( d2<bestD2 ){bestD2=d2;best=s;} }
    if ( outDist2 ) *outDist2 = bestD2;
    return best;
}

/* Set both yaw and pitch with one command. Usage:
   - spawnang <yaw> <pitch>
   - spawnang          (copy angles from player's view)
   - spawnang <yaw>    (set only yaw; keep pitch)
*/
void spawnang_f(gentity_t *ent) {
    char byaw[32], bpitch[32];
    int argc; gentity_t *s; float d2; float yaw=0, pitch=0; qboolean setYaw=qfalse, setPitch=qfalse;
    if (!ent||!ent->client||!ent->authed) return;
    argc = trap_Argc();
    s = FindNearestSpawn(ent, &d2);
    if (!s || d2> (128.0f*128.0f)) { trap_SendServerCommand(ent-g_entities, "print \"^3No nearby spawn (<=128u)\n\"" ); return; }
    if (argc >= 2) { trap_Argv(1, byaw, sizeof(byaw)); yaw = (float)atof(byaw); setYaw = qtrue; }
    if (argc >= 3) { trap_Argv(2, bpitch, sizeof(bpitch)); pitch = (float)atof(bpitch); setPitch = qtrue; }
    if (!setYaw && !setPitch) {
        yaw = ent->client->ps.viewangles[YAW];
        pitch = ent->client->ps.viewangles[PITCH];
        setYaw = setPitch = qtrue;
    }
    if (setYaw) s->s.angles[YAW] = yaw;
    if (setPitch) s->s.angles[PITCH] = pitch;
    trap_SendServerCommand(ent-g_entities, va("print \"^2Angles: yaw %.1f pitch %.1f\n\"", s->s.angles[YAW], s->s.angles[PITCH]));
}

/* CenterPrint info for nearest spawn */
void spawnscp_f(gentity_t *ent) {
    if (!ent||!ent->client||!ent->authed) return;
    ent->client->pers.spawnCpEnabled = !ent->client->pers.spawnCpEnabled;
    trap_SendServerCommand( ent - g_entities,
        ent->client->pers.spawnCpEnabled ? "print \"^3spawnscp: ^2ON\n\"" : "print \"^3spawnscp: ^1OFF\n\""
    );
}
