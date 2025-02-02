/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// server.h

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/vm_local.h"
#include "../game/g_public.h"
#include "../game/bg_public.h"

#define USE_BANS
#define STANDALONE

//=============================================================================

#define	PERS_SCORE				0		// !!! MUST NOT CHANGE, SERVER AND
										// GAME BOTH REFERENCE !!!

#define	MAX_ENT_CLUSTERS	16

typedef struct svEntity_s {
	struct worldSector_s *worldSector;
	struct svEntity_s *nextEntityInWorldSector;

	entityState_t	baseline;		// for delta compression of initial sighting
	int			numClusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			lastCluster;		// if all the clusters don't fit in clusternums
	int			areanum, areanum2;
	int			snapshotCounter;	// used to prevent double adding from portal views
} svEntity_t;

typedef enum {
	SS_DEAD,			// no map loaded
	SS_LOADING,			// spawning level entities
	SS_GAME				// actively running
} serverState_t;

// we might not use all MAX_GENTITIES every frame
// so leave more room for slow-snaps clients etc.
#define NUM_SNAPSHOT_FRAMES (PACKET_BACKUP*4)

typedef struct snapshotFrame_s {
	entityState_t *ents[ MAX_GENTITIES ];
	int	frameNum;
	int start;
	int count;
} snapshotFrame_t;

typedef struct {
	serverState_t	state;
	qboolean		restarting;			// if true, send configstring changes during SS_LOADING
	int				serverId;			// changes each server start
	int				restartedServerId;	// serverId before a map_restart
	int				checksumFeed;		// the feed key that we use to compute the pure checksum strings
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
	// the serverId associated with the current checksumFeed (always <= serverId)
	int				checksumFeedServerId;
	int				snapshotCounter;	// incremented for each snapshot built
	int				timeResidual;		// <= 1000 / sv_frame->value
	int				nextFrameTime;		// when time > nextFrameTime, process world
	char			*configstrings[MAX_CONFIGSTRINGS];
	svEntity_t		svEntities[MAX_GENTITIES];

	const char		*entityParsePoint;	// used during game VM init

	// the game virtual machine will update these on init and changes
	sharedEntity_t	*gentities;
	int				gentitySize;
	int				num_entities;		// current number, <= MAX_GENTITIES

	playerState_t	*gameClients;
	int				gameClientSize;		// will be > sizeof(playerState_t) due to game private data

	int				restartTime;
	int				time;

	byte			baselineUsed[ MAX_GENTITIES ];

	char				lastSpecChat[MAX_EDIT_LINE];
} server_t;

typedef struct {
	int				areabytes;
	byte			areabits[MAX_MAP_AREA_BYTES];		// portalarea visibility bits
	playerState_t	ps;
	int				num_entities;
#ifdef USE_MV
	qboolean		multiview;
	int				version;
	int				mergeMask;
	int				first_psf;				// first playerState index
	int				num_psf;				// number of playerStates to send
	byte			psMask[MAX_CLIENTS/8];	// playerState mask
#endif
	int				messageSent;		// time the message was transmitted
	int				messageAcked;		// time the message was acked
	int				messageSize;		// used to rate drop packets

	int				frameNum;			// from snapshot storage to compare with last valid
#ifdef USE_MV
	entityState_t	*ents[ MAX_GENTITIES ];
#else
	entityState_t	*ents[ MAX_SNAPSHOT_ENTITIES ];
#endif
} clientSnapshot_t;

#ifdef USE_MV

#define MAX_MV_FILES 4096 // for directory caching

typedef byte entMask_t[ MAX_GENTITIES / 8 ];

typedef struct psFrame_s {
    int				clientSlot;
    int				areabytes;
    byte			areabits[ MAX_MAP_AREA_BYTES ]; // portalarea visibility bits
    playerState_t	ps;
    entMask_t		entMask;
} psFrame_t;

#endif // USE_MV

typedef enum {
	CS_FREE = 0,	// can be reused for a new connection
	CS_ZOMBIE,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	CS_CONNECTED,	// has been assigned to a client_t, but no gamestate yet
	CS_PRIMED,		// gamestate has been sent, but client hasn't sent a usercmd
	CS_ACTIVE		// client is fully in game
} clientState_t;

typedef struct netchan_buffer_s {
	msg_t           msg;
	byte            msgBuffer[MAX_MSGLEN];
	char		clientCommandString[MAX_STRING_CHARS];	// valid command string for SV_Netchan_Encode
	struct netchan_buffer_s *next;
} netchan_buffer_t;

typedef struct rateLimit_s {
	int			lastTime;
	int			burst;
} rateLimit_t;

typedef struct leakyBucket_s leakyBucket_t;
struct leakyBucket_s {
	netadrtype_t	type;

	union {
		byte	_4[4];
		byte	_6[16];
	} ipv;

	rateLimit_t rate;

	int			hash;
	int			toxic;

	leakyBucket_t *prev, *next;
};

typedef struct client_s {
	clientState_t	state;
	char			userinfo[MAX_INFO_STRING];		// name, etc

	char			reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	int				reliableSequence;		// last added reliable message, not necessarily sent or acknowledged yet
	int				reliableAcknowledge;	// last acknowledged reliable message
    int             reliableSent;           // last sent reliable message, not necesarily acknowledged yet
	int				messageAcknowledge;

	int				gamestateMessageNum;	// netchan->outgoingSequence of gamestate
	int				challenge;

	usercmd_t		lastUsercmd;
    int             lastMessageNum;         // for delta compression
	int				lastClientCommand;	// reliable client message sequence
	char			lastClientCommandString[MAX_STRING_CHARS];
	sharedEntity_t	*gentity;			// SV_GentityNum(clientnum)
	char			name[MAX_NAME_LENGTH];			// extracted from userinfo, high bits masked

	// downloading
	char			downloadName[MAX_QPATH]; // if not empty string, we are downloading
	fileHandle_t	download;			// file being downloaded
 	int				downloadSize;		// total bytes (can't use EOF because of paks)
 	int				downloadCount;		// bytes sent
	int				downloadClientBlock;	// last block we sent to the client, awaiting ack
	int				downloadCurrentBlock;	// current block number
	int				downloadXmitBlock;	// last block we xmited
	unsigned char	*downloadBlocks[MAX_DOWNLOAD_WINDOW];	// the buffers for the download blocks
	int				downloadBlockSize[MAX_DOWNLOAD_WINDOW];
	qboolean		downloadEOF;		// We have sent the EOF block
	int				downloadSendTime;	// time we last got an ack from the client

	int				deltaMessage;		// frame last client usercmd message
	int				lastPacketTime;		// svs.time when packet was last received
	int				lastConnectTime;	// svs.time when connection started
	int				lastDisconnectTime;
	int				lastSnapshotTime;	// svs.time of last sent snapshot
	qboolean		rateDelayed;		// true if nextSnapshotTime was set based on rate instead of snapshotMsec
	int				timeoutCount;		// must timeout a few frames in a row so debugging doesn't break
	clientSnapshot_t	frames[PACKET_BACKUP];	// updates can be delta'd from here
	int				ping;
	int				rate;				// bytes / second, 0 - unlimited
	int				snapshotMsec;		// requests a snapshot every snapshotMsec unless rate choked
	qboolean		pureAuthentic;
	qboolean		gotCP;				// TTimo - additional flag to distinguish between a bad pure checksum, and no cp command at all
	netchan_t		netchan;
	// TTimo
	// queuing outgoing fragmented messages to send them properly, without udp packet bursts
	// in case large fragmented messages are stacking up
	// buffer them into this queue, and hand them out to netchan as needed
	netchan_buffer_t *netchan_start_queue;
	netchan_buffer_t **netchan_end_queue;

	int				oldServerTime;
	qboolean		csUpdated[MAX_CONFIGSTRINGS];
	qboolean		compat;

	// flood protection
	rateLimit_t		cmd_rate;
	rateLimit_t		info_rate;
	rateLimit_t		gamestate_rate;

	// client can decode long strings
	qboolean		longstr;

	qboolean		justConnected;

	char			tld[3]; // "XX\0"
	const char		*country;

#ifdef USE_AUTH
    char auth[MAX_NAME_LENGTH];
#endif

#ifdef USE_MV
    struct {
        int				protocol;

        int				scoreQueryTime;
        int				lastRecvTime; // any received command
        int				lastSentTime; // any sent command
#ifdef USE_MV_ZCMD
        //  command compression
		struct			{
			int			deltaSeq;
			lzctx_t		ctx;
			lzstream_t	stream[ MAX_RELIABLE_COMMANDS ];
		} z;
#endif
        qboolean		recorder;

    } multiview;
#endif // USE_MV

#ifdef USE_SERVER_DEMO
	qboolean	demo_recording;	// are we currently recording this client?
	fileHandle_t	demo_file;	// the file we are writing the demo to
	qboolean	demo_waiting;	// are we still waiting for the first non-delta frame?
	int		demo_backoff;	// how many packets (-1 actually) between non-delta frames?
	int		demo_deltas;	// how many delta frames did we let through so far?
#endif

	char plainName[MAX_NAME_LENGTH];
	char colourName[MAX_NAME_LENGTH * 3];
	qboolean isColourName;
} client_t;

//=============================================================================


// this structure will be cleared only when the game dll changes
typedef struct {
	qboolean	initialized;				// sv_init has completed

	int			time;						// will be strictly increasing across level changes
	int			msgTime;					// will be used as precise sent time

	int			snapFlagServerBit;			// ^= SNAPFLAG_SERVERCOUNT every SV_SpawnServer()

	client_t	*clients;					// [sv_maxclients->integer];
	int			numSnapshotEntities;		// PACKET_BACKUP*MAX_SNAPSHOT_ENTITIES
	entityState_t	*snapshotEntities;		// [numSnapshotEntities]
	int			nextHeartbeatTime;

	netadr_t	authorizeAddress;			// for rcon return messages
	int			masterResolveTime[MAX_MASTER_SERVERS]; // next svs.time that server should do dns lookup for master server

	// common snapshot storage
	int			freeStorageEntities;
	int			currentStoragePosition;	// next snapshotEntities to use
	int			snapshotFrame;			// incremented with each common snapshot built
	int			currentSnapshotFrame;	// for initializing empty frames
	int			lastValidFrame;			// updated with each snapshot built
	snapshotFrame_t	snapFrames[ NUM_SNAPSHOT_FRAMES ];
	snapshotFrame_t	*currFrame; // current frame that clients can refer
    netadr_t 		redirectAddress;

#ifdef USE_MV
	int			numSnapshotPSF;				// sv_democlients->integer*PACKET_BACKUP*MAX_CLIENTS
	int			nextSnapshotPSF;			// next snapshotPS to use
	int			modSnapshotPSF;				// clamp value
	psFrame_t	*snapshotPSF;				// [numSnapshotPS]
	qboolean	emptyFrame;					// true if no game logic run during SV_Frame()
#endif // USE_MV

} serverStatic_t;

#ifdef USE_BANS
#define SERVER_MAXBANS	1024
// Structure for managing bans
typedef struct
{
	netadr_t ip;
	// For a CIDR-Notation type suffix
	int subnet;

	qboolean isexception;
} serverBan_t;
#endif

//=============================================================================

extern	serverStatic_t	svs;				// persistant server info across maps
extern	server_t		sv;					// cleared each map
extern	vm_t			*gvm;				// game virtual machine

extern	cvar_t	*sv_fps;
extern	cvar_t	*sv_timeout;
extern	cvar_t	*sv_zombietime;
extern	cvar_t	*sv_rconPassword;
extern	cvar_t	*sv_privatePassword;
extern	cvar_t	*sv_allowDownload;
extern	cvar_t	*sv_maxclients;
extern	cvar_t	*sv_maxclientsPerIP;
extern	cvar_t	*sv_clientTLD;

#ifdef USE_MV
extern	fileHandle_t	sv_demoFile;
extern	char	sv_demoFileName[ MAX_OSPATH ];
extern	char	sv_demoFileNameLast[ MAX_OSPATH ];

extern	int		sv_demoClientID;
extern	int		sv_lastAck;
extern	int		sv_lastClientSeq;

extern	cvar_t	*sv_mvClients;
extern	cvar_t	*sv_mvPassword;
extern	cvar_t	*sv_demoFlags;
extern	cvar_t	*sv_autoRecord;

extern	cvar_t	*sv_mvFileCount;
extern	cvar_t	*sv_mvFolderSize;

#endif // USE_MV

extern	cvar_t	*sv_privateClients;
extern	cvar_t	*sv_hostname;
extern	cvar_t	*sv_master[MAX_MASTER_SERVERS];
extern	cvar_t	*sv_reconnectlimit;
extern	cvar_t	*sv_padPackets;
extern	cvar_t	*sv_killserver;
extern	cvar_t	*sv_mapname;
extern	cvar_t	*sv_mapChecksum;
extern	cvar_t	*sv_referencedPakNames;
extern	cvar_t	*sv_serverid;
extern	cvar_t	*sv_minRate;
extern	cvar_t	*sv_maxRate;
extern	cvar_t	*sv_minPing;
extern	cvar_t	*sv_maxPing;
extern	cvar_t	*sv_dlRate;
extern	cvar_t	*sv_gametype;
extern	cvar_t	*sv_pure;
extern	cvar_t	*sv_floodProtect;
extern	cvar_t	*sv_lanForceRate;
extern	cvar_t	*sv_strictAuth;

extern	cvar_t *sv_levelTimeReset;
extern	cvar_t *sv_filter;

#ifdef USE_AUTH
extern	cvar_t	*sv_authServerIP;
extern  cvar_t  *sv_auth_engine;
#endif

#ifdef USE_BANS
extern	cvar_t	*sv_banFile;
extern	serverBan_t serverBans[SERVER_MAXBANS];
extern	int serverBansCount;
#endif

#ifdef USE_SERVER_DEMO
extern	cvar_t	*sv_demonotice;
extern  cvar_t  *sv_demofolder;
#endif

extern	cvar_t	*sv_bad_password_message;
extern	cvar_t	*sv_sayprefix;
extern	cvar_t	*sv_tellprefix;
extern	cvar_t	*g_teamnamered;
extern	cvar_t	*g_teamnameblue;

extern  cvar_t  *sv_hideChatCmd;
extern	cvar_t	*sv_specChatGlobal;
extern	cvar_t	*sv_colourNames;


//===========================================================

//
// sv_main.c
//
qboolean SVC_RateLimit( rateLimit_t *bucket, int burst, int period );
qboolean SVC_RateLimitAddress( const netadr_t *from, int burst, int period );
void SVC_RateRestoreBurstAddress( const netadr_t *from, int burst, int period );
void SVC_RateRestoreToxicAddress( const netadr_t *from, int burst, int period );
void SVC_RateDropAddress( const netadr_t *from, int burst, int period );

void SV_FinalMessage( const char *message );
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

void SV_AddOperatorCommands( void );
void SV_RemoveOperatorCommands( void );

void SV_MasterShutdown( void );
int SV_RateMsec( const client_t *client );


//
// sv_init.c
//
void SV_SetConfigstring( int index, const char *val );
void SV_GetConfigstring( int index, char *buffer, int bufferSize );
void SV_UpdateConfigstrings( client_t *client );

void SV_SetUserinfo( int index, const char *val );
void SV_GetUserinfo( int index, char *buffer, int bufferSize );

void SV_ChangeMaxClients( void );
void SV_SpawnServer( const char *mapname, qboolean killBots );



//
// sv_client.c
//
void SV_GetChallenge( const netadr_t *from );
void SV_InitChallenger( void );

void SV_DirectConnect( const netadr_t *from );

void SV_ExecuteClientMessage( client_t *cl, msg_t *msg );
void SV_UserinfoChanged( client_t *cl, qboolean updateUserinfo, qboolean runFilter );

void SV_ClientEnterWorld( client_t *client, usercmd_t *cmd );
void SV_FreeClient( client_t *client );
void SV_DropClient( client_t *drop, const char *reason );

#ifdef USE_AUTH
void SV_Auth_DropClient( client_t *drop, const char *reason, const char *message );
#endif

qboolean SV_ExecuteClientCommand( client_t *cl, const char *s );
void SV_ClientThink( client_t *cl, usercmd_t *cmd );

int SV_SendDownloadMessages( void );
int SV_SendQueuedMessages( void );

void SV_FreeIP4DB( void );
void SV_PrintLocations_f( client_t *client );

#ifdef USE_MV
void SV_TrackDisconnect( int clientNum );
void SV_ForwardServerCommands( client_t *recorder /*, const client_t *client */ );
void SV_MultiViewStopRecord_f( void );
int SV_FindActiveClient( qboolean checkCommands, int skipClientNum, int minActive );
void SV_SetTargetClient( int clientNum );
#endif // USE_MV

//
// sv_mod.c
//
#define CS_URT_PLAYERS  544

void SVM_Init( void );
int* SVM_ItemFind(playerState_t *ps, long itemsMask);
int* SVM_WeaponFind(playerState_t *ps, long weaponsMask);
int SVM_ClientThink(client_t *cl);
int SVMP_ClientThink(client_t *cl);
char* SVM_ClientConnect(client_t *cl);
void QDECL SV_LogPrintf(const char *fmt, ...);
//qboolean SVM_OnLogPrint(char *string, int len);
char* SVM_OnGamePrint(char *string);
int SVM_OnClientCommand( client_t *cl, char *s );
int SVM_OnServerCommand(client_t **pcl, char *message);

//
// sv_subnets.c
//
#define SUBNETS_CHUNK_SIZE  1024

typedef struct svm_subnets_s {
	netadr_t  *start, *cur;
	size_t    count;
	size_t    cap;
} svm_subnets_t;

#define ERR_SVM_Subnets_SetFromString  1
#define ERR_SVM_Subnets_Add            2

void SVM_Subnets_Init(svm_subnets_t *subnets);
netadr_t *SVM_Subnets_Add(svm_subnets_t *subnets);
size_t SVM_Subnets_Remove(svm_subnets_t *subnets, netadr_t *adr);
int SVM_Subnet_SetFromString(netadr_t *adr, char* string);
int SVM_Subnets_AddFromString(svm_subnets_t *subnets, char* string);
void SVM_Subnets_AddFromFile(svm_subnets_t *subnets, char *filename);
void SVM_Subnets_Commit(svm_subnets_t *subnets);
netadr_t *SVM_Subnets_FindByAdr(svm_subnets_t *subnets, netadr_t *adr);
netadr_t *SVM_Subnets_FindByAdrString(svm_subnets_t *subnets, char* string);
netadr_t *SVM_Subnets_FindByAdrUC(svm_subnets_t *subnets, netadr_t *adr);
netadr_t *SVM_Subnets_FindByAdrStringUC(svm_subnets_t *subnets, char* string);
size_t SVM_Subnets_RemoveNC(svm_subnets_t *subnets, netadr_t *adr);
void SVM_Subnets_Free(svm_subnets_t *subnets);

//
// sv_ccmds.c
//
void SV_Heartbeat_f( void );
client_t *SV_GetPlayerByHandle( void );
qboolean SV_ParseCIDRNotation(netadr_t *dest, int *mask, char *adrstr);

#ifdef USE_SERVER_DEMO
void SVD_WriteDemoFile(const client_t*, const msg_t*);
#endif

#ifdef USE_MV
void SV_LoadRecordCache( void );
void SV_SaveRecordCache( void );
#endif

//
// sv_snapshot.c
//
void SV_AddServerCommand( client_t *client, const char *cmd );
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg );
void SV_WriteFrameToClient( client_t *client, msg_t *msg );
void SV_SendMessageToClient( msg_t *msg, client_t *client );
void SV_SendClientMessages( void );
void SV_SendClientSnapshot( client_t *client );

void SV_InitSnapshotStorage( void );
void SV_IssueNewSnapshot( void );

int SV_RemainingGameState( void );

//
// sv_game.c
//
int	SV_NumForGentity( sharedEntity_t *ent );
sharedEntity_t *SV_GentityNum( int num );
playerState_t *SV_GameClientNum( int num );
svEntity_t	*SV_SvEntityForGentity( sharedEntity_t *gEnt );
sharedEntity_t *SV_GEntityForSvEntity( svEntity_t *svEnt );
void		SV_InitGameProgs ( void );
void		SV_ShutdownGameProgs ( void );
void		SV_RestartGameProgs( void );
qboolean	SV_inPVS (const vec3_t p1, const vec3_t p2);

//
// sv_bot.c
//
void		SV_BotFrame( int time );
int			SV_BotAllocateClient(void);
void		SV_BotFreeClient( int clientNum );

void		SV_BotInitCvars(void);
int			SV_BotLibSetup( void );
int			SV_BotLibShutdown( void );
int			SV_BotGetSnapshotEntity( int client, int ent );
int			SV_BotGetConsoleMessage( int client, char *buf, int size );

int BotImport_DebugPolygonCreate(int color, int numPoints, vec3_t *points);
void BotImport_DebugPolygonDelete(int id);

void SV_BotInitBotLib(void);

//============================================================
//
// high level object sorting to reduce interaction tests
//

void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_UnlinkEntity( sharedEntity_t *ent );
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void SV_LinkEntity( sharedEntity_t *ent );
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->r.absmin and ent->r.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid


clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent );


void SV_SectorList_f( void );


int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount );
// fills in a table of entity numbers with entities that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// The world entity is never returned in this list.


int SV_PointContents( const vec3_t p, int passEntityNum );
// returns the CONTENTS_* value from the world and all entities at the given point.


void SV_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask, qboolean capsule );
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passEntityNum is explicitly excluded from clipping checks (normally ENTITYNUM_NONE)

void SV_TraceAtCrosshair( trace_t *results, playerState_t *ps, const vec3_t mins, const vec3_t maxs, int contentmask, qboolean capsule );

void SV_ClipToEntity( trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int entityNum, int contentmask, qboolean capsule );
// clip to a specific entity

//
// sv_net_chan.c
//
void SV_Netchan_Transmit( client_t *client, msg_t *msg);
int SV_Netchan_TransmitNextFragment( client_t *client );
qboolean SV_Netchan_Process( client_t *client, msg_t *msg );
void SV_Netchan_FreeQueue( client_t *client );

//
// sv_filter.c
//
void SV_LoadFilters( const char *filename );
const char *SV_RunFilters( const char *userinfo, const netadr_t *addr );
void SV_AddFilter_f( void );
void SV_AddFilterCmd_f( void );
