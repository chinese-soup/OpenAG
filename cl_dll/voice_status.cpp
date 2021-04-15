//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// There are hud.h's coming out of the woodwork so this ensures that we get the right one.
#if defined(THREEWAVE) || defined(DMC_BUILD)
	#include "../dmc/cl_dll/hud.h"
#elif defined(CSTRIKE)
	#include "../cstrike/cl_dll/hud.h"
#elif defined(DOD)
	#include "../dod/cl_dll/hud.h"
#else
	#include "hud.h"
#endif

#include "cl_util.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "hud_servers.h"
#include "demo.h"
#include "demo_api.h"
#include "voice_status.h"
#include "r_efx.h"
#include "entity_types.h"
#include "VGUI_ActionSignal.h"
#include "VGUI_Scheme.h"
#include "VGUI_TextImage.h"
#include "vgui_loadtga.h"
#include "vgui_helpers.h"
#include "VGUI_MouseCode.h"
#include "triangleapi.h"
//#include "hud.h"


using namespace vgui;


extern int cam_thirdperson;


#define VOICE_MODEL_INTERVAL		0.3
#define SCOREBOARD_BLINK_FREQUENCY	0.3	// How often to blink the scoreboard icons.
#define SQUELCHOSCILLATE_PER_SECOND	2.0f


extern BitmapTGA *LoadTGA( const char* pImageName );



// ---------------------------------------------------------------------- //
// The voice manager for the client.
// ---------------------------------------------------------------------- //
CVoiceStatus g_VoiceStatus;

CVoiceStatus* GetClientVoiceMgr()
{
	return &g_VoiceStatus;
}



// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

static CVoiceStatus *g_pInternalVoiceStatus = NULL;

int __MsgFunc_VoiceMask(const char *pszName, int iSize, void *pbuf)
{
	if(g_pInternalVoiceStatus)
		g_pInternalVoiceStatus->HandleVoiceMaskMsg(iSize, pbuf);

	return 1;
}

int __MsgFunc_ReqState(const char *pszName, int iSize, void *pbuf)
{
	if(g_pInternalVoiceStatus)
		g_pInternalVoiceStatus->HandleReqStateMsg(iSize, pbuf);

	return 1;
}


int g_BannedPlayerPrintCount;
void ForEachBannedPlayer(char id[16])
{
	char str[256];
	sprintf(str, "Ban %d: %2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x\n",
		g_BannedPlayerPrintCount++,
		id[0], id[1], id[2], id[3], 
		id[4], id[5], id[6], id[7], 
		id[8], id[9], id[10], id[11], 
		id[12], id[13], id[14], id[15]
		);
#ifdef _WIN32
	strupr(str);
#endif
	gEngfuncs.pfnConsolePrint(str);
}


void ShowBannedCallback()
{
	if(g_pInternalVoiceStatus)
	{
		g_BannedPlayerPrintCount = 0;
		gEngfuncs.pfnConsolePrint("------- BANNED PLAYERS -------\n");
		g_pInternalVoiceStatus->m_BanMgr.ForEachBannedPlayer(ForEachBannedPlayer);
		gEngfuncs.pfnConsolePrint("------------------------------\n");
	}
}


// ---------------------------------------------------------------------- //
// CVoiceStatus.
// ---------------------------------------------------------------------- //

CVoiceStatus::CVoiceStatus()
{
	m_bBanMgrInitialized = false;
	m_LastUpdateServerState = 0;

	m_pSpeakerLabelIcon = NULL;
	m_pScoreboardNeverSpoken = NULL;
	m_pScoreboardNotSpeaking = NULL;
	m_pScoreboardSpeaking = NULL;
	m_pScoreboardSpeaking2 = NULL;
	m_pScoreboardSquelch = NULL;
	m_pScoreboardBanned = NULL;
	
	m_pLocalBitmap = NULL;
	m_pAckBitmap = NULL;

	m_bTalking = m_bServerAcked = false;

	memset(m_pBanButtons, 0, sizeof(m_pBanButtons));

	m_pParentPanel = NULL;

	m_bServerModEnable = -1;

	m_pchGameDir = NULL;
}


CVoiceStatus::~CVoiceStatus()
{
	g_pInternalVoiceStatus = NULL;
	
#ifdef POSIX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
	for(int i=0; i < MAX_VOICE_SPEAKERS; i++)
	{
		delete m_Labels[i].m_pLabel;
		m_Labels[i].m_pLabel = NULL;

		delete m_Labels[i].m_pIcon;
		m_Labels[i].m_pIcon = NULL;
		
		delete m_Labels[i].m_pBackground;
		m_Labels[i].m_pBackground = NULL;
	}				

	delete m_pLocalLabel;
	m_pLocalLabel = NULL;
#ifdef POSIX
#pragma GCC diagnostic pop
#endif

	FreeBitmaps();

	if(m_pchGameDir)
	{
		if(m_bBanMgrInitialized)
		{
			m_BanMgr.SaveState(m_pchGameDir);
		}

		free(m_pchGameDir);
	}
}


int CVoiceStatus::Init(
	IVoiceStatusHelper *pHelper,
	Panel **pParentPanel)
{
	CVAR_CREATE("cl_player_names_head", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_player_names_head_scale", "0.25", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_player_names_head_render_amt", "255", FCVAR_ARCHIVE);


	
	// Setup the voice_modenable cvar.
	gEngfuncs.pfnRegisterVariable("voice_modenable", "1", FCVAR_ARCHIVE);
	
	gEngfuncs.pfnRegisterVariable("voice_clientdebug", "0", 0);

	gEngfuncs.pfnAddCommand("voice_showbanned", ShowBannedCallback);

	if(gEngfuncs.pfnGetGameDirectory())
	{
		m_BanMgr.Init(gEngfuncs.pfnGetGameDirectory());
		m_bBanMgrInitialized = true;
	}

	assert(!g_pInternalVoiceStatus);
	g_pInternalVoiceStatus = this;

	m_BlinkTimer = 0;
	m_VoiceHeadModel = 0;
	m_VoiceHeadModel2 = 0;
	memset(m_Labels, 0, sizeof(m_Labels));
	
	for(int i=0; i < MAX_VOICE_SPEAKERS; i++)
	{
		CVoiceLabel *pLabel = &m_Labels[i];

		pLabel->m_pBackground = new Label("");

		if(pLabel->m_pLabel = new Label(""))
		{
			pLabel->m_pLabel->setVisible( true );
			pLabel->m_pLabel->setFont( Scheme::sf_primary2 );
			pLabel->m_pLabel->setTextAlignment( Label::a_east );
			pLabel->m_pLabel->setContentAlignment( Label::a_east );
			pLabel->m_pLabel->setParent( pLabel->m_pBackground );
		}

		if( pLabel->m_pIcon = new ImagePanel( NULL ) )
		{
			pLabel->m_pIcon->setVisible( true );
			pLabel->m_pIcon->setParent( pLabel->m_pBackground );
		}

		pLabel->m_clientindex = -1;
	}

	m_pLocalLabel = new ImagePanel(NULL);

	m_bInSquelchMode = false;

	m_pHelper = pHelper;
	m_pParentPanel = pParentPanel;
	gHUD.AddHudElem(this);
	m_iFlags = HUD_ACTIVE;
	HOOK_MESSAGE(VoiceMask);
	HOOK_MESSAGE(ReqState);

	// Cache the game directory for use when we shut down
	const char *pchGameDirT = gEngfuncs.pfnGetGameDirectory();
	m_pchGameDir = (char *)malloc(strlen(pchGameDirT) + 1);
	strcpy(m_pchGameDir, pchGameDirT);

	return 1;
}


int CVoiceStatus::VidInit()
{
	FreeBitmaps();


	if( m_pLocalBitmap = vgui_LoadTGA("gfx/vgui/icntlk_pl.tga") )
	{
		m_pLocalBitmap->setColor(Color(255,255,255,135));
	}

	if( m_pAckBitmap = vgui_LoadTGA("gfx/vgui/icntlk_sv.tga") )
	{
		m_pAckBitmap->setColor(Color(255,255,255,135));	// Give just a tiny bit of translucency so software draws correctly.
	}

	m_pLocalLabel->setImage( m_pLocalBitmap );
	m_pLocalLabel->setVisible( false );


	if( m_pSpeakerLabelIcon = vgui_LoadTGANoInvertAlpha("gfx/vgui/speaker4.tga" ) )
		m_pSpeakerLabelIcon->setColor( Color(255,255,255,1) );		// Give just a tiny bit of translucency so software draws correctly.

	if (m_pScoreboardNeverSpoken = vgui_LoadTGANoInvertAlpha("gfx/vgui/640_speaker1.tga"))
		m_pScoreboardNeverSpoken->setColor(Color(255,255,255,1));	// Give just a tiny bit of translucency so software draws correctly.

	if(m_pScoreboardNotSpeaking = vgui_LoadTGANoInvertAlpha("gfx/vgui/640_speaker2.tga"))
		m_pScoreboardNotSpeaking->setColor(Color(255,255,255,1));	// Give just a tiny bit of translucency so software draws correctly.
	
	if(m_pScoreboardSpeaking = vgui_LoadTGANoInvertAlpha("gfx/vgui/640_speaker3.tga"))
		m_pScoreboardSpeaking->setColor(Color(255,255,255,1));	// Give just a tiny bit of translucency so software draws correctly.
	
	if(m_pScoreboardSpeaking2 = vgui_LoadTGANoInvertAlpha("gfx/vgui/640_speaker4.tga"))
		m_pScoreboardSpeaking2->setColor(Color(255,255,255,1));	// Give just a tiny bit of translucency so software draws correctly.
	
	if(m_pScoreboardSquelch  = vgui_LoadTGA("gfx/vgui/icntlk_squelch.tga"))
		m_pScoreboardSquelch->setColor(Color(255,255,255,1));	// Give just a tiny bit of translucency so software draws correctly.

	if(m_pScoreboardBanned = vgui_LoadTGA("gfx/vgui/640_voiceblocked.tga"))
		m_pScoreboardBanned->setColor(Color(255,255,255,1));	// Give just a tiny bit of translucency so software draws correctly.

	// Figure out the voice head model height.
	m_VoiceHeadModelHeight = 45;
	char *pFile = (char *)gEngfuncs.COM_LoadFile("scripts/voicemodel.txt", 5, NULL);
	if(pFile)
	{
		char token[4096];
		gEngfuncs.COM_ParseFile(pFile, token);
		if(token[0] >= '0' && token[0] <= '9')
		{
			m_VoiceHeadModelHeight = (float)atof(token);
		}

		gEngfuncs.COM_FreeFile(pFile);
	}

	//m_VoiceHeadModel = gEngfuncs.pfnSPR_Load("sprites/voiceicon.spr");
	m_VoiceHeadModel = gEngfuncs.pfnSPR_Load("sprites/alphabet_test.spr");

	//m_VoiceHeadModel2 = gEngfuncs.pfnSPR_Load("sprites/alphabet_test4.spr");
	m_VoiceHeadModel2 = gEngfuncs.pfnSPR_Load("sprites/ascii_table_test.spr");
	
	return TRUE;
}


void CVoiceStatus::Frame(double frametime)
{
	// check server banned players once per second
	if(gEngfuncs.GetClientTime() - m_LastUpdateServerState > 1)
	{
		UpdateServerState(false);
	}

	m_BlinkTimer += frametime;

	// Update speaker labels.
	if( m_pHelper->CanShowSpeakerLabels() )
	{
		for( int i=0; i < MAX_VOICE_SPEAKERS; i++ )
			m_Labels[i].m_pBackground->setVisible( m_Labels[i].m_clientindex != -1 );
	}
	else
	{
		for( int i=0; i < MAX_VOICE_SPEAKERS; i++ )
			m_Labels[i].m_pBackground->setVisible( false );
	}

	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
		UpdateBanButton(i);
}


void CVoiceStatus::CreateEntities()
{
	if(!m_VoiceHeadModel)
		return;

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	/*for (int i = 0; i < MAX_PLAYERS; ++i) {
				// Make sure the information is up to date.
				gEngfuncs.pfnGetPlayerInfo(i + 1, &g_PlayerInfoList[i + 1]);

				// This player slot is empty.
				if (g_PlayerInfoList[i + 1].name == nullptr)
					continue;

				if (!name_contains_color_tags) {
					// Try to match by the user ID.
					if (uid != -1 && IEngineStudio.PlayerInfo(i)->userid == uid) {
						// Got a match by the user ID.
						matched_player_index = i;
						break;
					}

					// Try to match by the SteamID.
					if (!strcmp(steam_id::get_steam_id(i).c_str(), name)) {
						// Got a match by the SteamID.
						matched_player_index = i;
						break;
					}
				}*/
	int lx;
		
	int iOutModel = 0;
	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		/*if(!m_VoicePlayers[i])
			continue;*/
		
		cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(i+1);
		
		if(CVAR_GET_FLOAT("cl_player_names_head") != 1)
			continue;

		// Don't show an icon if the player is not in our PVS.
		if(!pClient || pClient->curstate.messagenum < localPlayer->curstate.messagenum)
			continue; // TODO: UNCOMMENT

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if(pClient->curstate.effects & EF_NODRAW)
			continue;

		// Don't show an icon for the local player unless we're in thirdperson mode.
		if(pClient == localPlayer && !cam_thirdperson)
			continue;

		//char playerID[16];
		//gEngfuncs.GetPlayer(i+1, playerID);
		/*hud_player_info_t info;
		memset( &info, 0, sizeof( info ) );
		gEngfuncs.pfnGetPlayerInfo( i, &info );*/

		char mrdka[256];
		vec3_t			origin, angles, point, forward, right, left, up, world, screen, offset;
		
		VectorCopy(pClient->origin, origin);
		gEngfuncs.pTriAPI->WorldToScreen(origin, screen);
		screen[0] = XPROJECT(screen[0]);
		screen[1] = YPROJECT(screen[1]);
		screen[2] = 0.0f;


		char test[256];
		
		/*gHUD.DrawConsoleStringWithColorTags(
			100, //- lx,
			200,
			test,
			true);*/
		gEngfuncs.pfnDrawConsoleString(100, 200, test);
			
		//VectorSubtract(offset, screen, offset );

		int playerNum = pClient->index - 1;

		m_vPlayerPos[playerNum][0] = screen[0];	
		m_vPlayerPos[playerNum][1] = screen[1]; //+ Length(offset);	
		m_vPlayerPos[playerNum][2] = 1;	// mark player as visible 
		
		_snprintf( mrdka, sizeof( mrdka ), "fm_vPlayerPos[playerNum][0] = %f, m_vPlayerPos[playerNum][1] = %f \n", m_vPlayerPos[playerNum][0], m_vPlayerPos[playerNum][1]);
		
		//gEngfuncs.pfnConsolePrint( mrdka );

		
		char string2[256];
		// draw the players name and health underneath
		gEngfuncs.pfnGetPlayerInfo(i + 1, &g_PlayerInfoList[i + 1]);
		if (g_PlayerInfoList[i + 1].name == nullptr)
			continue;

		_snprintf( mrdka, sizeof( mrdka ), "name = %s \n", g_PlayerInfoList[i+1].name);
	
		//gEngfuncs.pfnConsolePrint( mrdka );

		sprintf(string2, "AHOJ: %s", g_PlayerInfoList[i+1].name );
		
		lx = strlen(string2)*3; // 3 is avg. character length :)

		gHUD.DrawConsoleStringWithColorTags(
			m_vPlayerPos[i][0], //- lx,
			m_vPlayerPos[i][1],
			string2,
			true);
			

		/*
		gEngfuncs.pTriAPI->SpriteTexture(  (struct model_s*)gEngfuncs.GetSpritePointer(m_VoiceHeadModel2), 0 );
		
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );

			gEngfuncs.pTriAPI->Begin(TRI_QUADS);

			// set the color to be that of the team
			gEngfuncs.pTriAPI->Color4f(200, 255, 2, 1.0f);


			// finish up
			gEngfuncs.pTriAPI->End();
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

		gEngfuncs.pfnGetPlayerInfo(i + 1, &g_PlayerInfoList[i + 1]);*/
		
		// This player slot is empty.
		if (g_PlayerInfoList[i + 1].name == nullptr)
			continue;
		else
		{
			char msg[256];
			char name[512];
			char name_alphabet_only[512];
			color_tags::strip_color_tags(name, g_PlayerInfoList[i + 1].name, ARRAYSIZE(name));
			color_tags::only_alphabet(name_alphabet_only, name, ARRAYSIZE(name_alphabet_only));
			std::string name_alphabet_only_as_str(name_alphabet_only);
			
			//_snprintf( msg, sizeof( msg ), "CVoiceStatus::name = %s, name_alphabet_only = %s \n", name, name_alphabet_only);
			//gEngfuncs.pfnConsolePrint( msg );
			//_snprintf( msg, sizeof( msg ), "alphabet = %s \n", name_alphabet_only_as_str);
			//gEngfuncs.pfnConsolePrint( msg );

			float cl_player_names_head_scale = CVAR_GET_FLOAT("cl_player_names_head_scale");
			float cl_player_names_head_render_amt = CVAR_GET_FLOAT("cl_player_names_head_render_amt");
			float origin_const_scaled = cl_player_names_head_scale * 16;

			for(int i=0; i < sizeof(name_alphabet_only) && name_alphabet_only[i] != '\0'; i++)
			{
				cl_entity_s *pEnt = &m_VoiceHeadModels[iOutModel];
				++iOutModel;
				int frame = name_alphabet_only[i] - 32;
							
				//_snprintf( msg, sizeof( msg ), "frame = %i | name_alphabet_only[i] = %d | length = %d | array size = (%d) | length str = %d \n", frame, 
				//name_alphabet_only[i], sizeof(name_alphabet_only), ARRAYSIZE(name_alphabet_only), name_alphabet_only_as_str.length());

				//gEngfuncs.pfnConsolePrint( msg );

				memset(pEnt, 0, sizeof(*pEnt));			
			
				pEnt->curstate.rendermode = kRenderTransAdd;
				pEnt->curstate.renderamt = cl_player_names_head_render_amt;
				pEnt->baseline.renderamt = cl_player_names_head_render_amt;
				pEnt->curstate.renderfx = kRenderFxNoDissipation;
				pEnt->curstate.framerate = 1;
				pEnt->curstate.frame = 0;
				pEnt->model = (struct model_s*)gEngfuncs.GetSpritePointer(m_VoiceHeadModel2);
				//pEnt->curstate.scale = 0.25f;
				
				pEnt->curstate.scale = cl_player_names_head_scale;
				
				pEnt->angles[0] = pClient->angles[0];
				pEnt->angles[2] = pClient->angles[2];

				pEnt->origin[0] = 0;
				pEnt->origin[1] = 0;
				pEnt->origin[2] = 40;

				float localPlayerAnglePositive;
				float localPlayerAngle = localPlayer->angles[1];
				float nameLength = name_alphabet_only_as_str.length();

				//mrdat = pClient->angles[1];
				localPlayerAnglePositive = localPlayer->angles[1];

				if(localPlayerAnglePositive < 0.0) 
					localPlayerAnglePositive *= -1.0;

				if(localPlayerAnglePositive > 135) // we are facing the same way as origin
				{
					pEnt->angles[1] = 180;
					//pEnt->angles[1] = localPlayerAngle;
					pEnt->origin[1] = (-(nameLength)/2)*origin_const_scaled + (origin_const_scaled*i);

					_snprintf( msg, sizeof( msg ), "pEnt x=%f,y=%f,z=%f angle x=%f,y=%f,z=%f | \npl x=%f y=%f z=%f | ORIGIN | PL X=%f Y=%f Z=%f\n", pEnt->origin[0], 
					pEnt->origin[1], 
					pEnt->origin[2],
					pEnt->angles[0], pEnt->angles[1], pEnt->angles[2], localPlayer->angles[0], localPlayer->angles[1], localPlayer->angles[2],
					localPlayer->origin[0], localPlayer->origin[1], localPlayer->origin[2]
					);

					//gEngfuncs.pfnConsolePrint( msg );
				}
				else if(localPlayerAngle < 135 && localPlayerAngle > 45) // we are looking from side
				{
					pEnt->angles[1] = 90; 
					// pEnt->angles[1] = localPlayerAngle;
					pEnt->origin[0] = (-(nameLength)/2)*origin_const_scaled + (origin_const_scaled*i);


					_snprintf( msg, sizeof( msg ), "pEnt x=%f,y=%f,z=%f angle x=%f,y=%f,z=%f | \npl x=%f y=%f z=%f | LEFT | PL ORIGIN: X=%f Y=%f Z=%f\n ", pEnt->origin[0], 
					pEnt->origin[1], 
					pEnt->origin[2],
					pEnt->angles[0], pEnt->angles[1], pEnt->angles[2], localPlayer->angles[0], localPlayer->angles[1], localPlayer->angles[2],
					localPlayer->origin[0], localPlayer->origin[1], localPlayer->origin[2]
					);
					//gEngfuncs.pfnConsolePrint( msg );
				}
				else if(localPlayerAngle > -135 && localPlayerAngle < -45) // we are looking from THE OTHER side
				{
					pEnt->angles[1] = 270; 
					//pEnt->angles[1] = localPlayerAngle;					
					pEnt->origin[0] = (nameLength/2)*origin_const_scaled - origin_const_scaled*i;

					_snprintf( msg, sizeof( msg ), "pEnt x=%f,y=%f,z=%f angle x=%f,y=%f,z=%f | \npl x=%f y=%f z=%f | RIGHT | PL ORIGIN: X=%f Y=%f Z=%f \n", pEnt->origin[0], 
					pEnt->origin[1], 
					pEnt->origin[2],
					pEnt->angles[0], pEnt->angles[1], pEnt->angles[2], localPlayer->angles[0], localPlayer->angles[1], localPlayer->angles[2],
					localPlayer->origin[0], localPlayer->origin[1], localPlayer->origin[2]
					);
					//gEngfuncs.pfnConsolePrint( msg );
				}
				else //WE LOOKING at origin from in front of it
				{
					pEnt->angles[1] = 0; 
					//pEnt->angles[1] = localPlayerAngle;					
					//pEnt->origin[1] = ((nameLength/2)*16 - 16*i);
					//pEnt->origin[1] = (nameLength/2)*5 - 5*i;
					pEnt->origin[1] = (nameLength/2)*origin_const_scaled - origin_const_scaled*i;
				
					//pEnt->origin[0] = 16 - pEnt->origin[1]; // TOHLE JE NA PICU A MAS ZKURVENEJ ZUB KRETENE
					//
					//pEnt->origin[1] = (localPlayerAnglePositive / 3.0f) + (16*i);
					//píčovina below		
					


					_snprintf( msg, sizeof( msg ), "ENT ORIGIN x=%f,y=%f,z=%f | ANGLE x=%f,y=%f,z=%f |\n pl x=%f y=%f z=%f | ELSE | PL ORIGIN: X=%f Y=%f Z=%f \n", pEnt->origin[0], 
					pEnt->origin[1], 
					pEnt->origin[2],
					pEnt->angles[0], pEnt->angles[1], pEnt->angles[2], localPlayer->angles[0], localPlayer->angles[1], localPlayer->angles[2],
					localPlayer->origin[0], localPlayer->origin[1], localPlayer->origin[2]
					);
					//gEngfuncs.pfnConsolePrint( msg );
				}
				
				VectorAdd(pEnt->origin, pClient->origin, pEnt->origin);

				//pEnt2->origin[1] += 5.0; 
				gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, pEnt);
			}

		}
	
	}
}


void CVoiceStatus::UpdateSpeakerStatus( int entindex, qboolean bTalking )
{
	cvar_t *pVoiceLoopback = NULL;

	if ( !m_pParentPanel || !*m_pParentPanel )
	{
		return;
	}

	if ( gEngfuncs.pfnGetCvarFloat( "voice_clientdebug" ) )
	{
		char msg[256];
		_snprintf( msg, sizeof( msg ), "CVoiceStatus::UpdateSpeakerStatus: ent %d talking = %d\n", entindex, bTalking );
		gEngfuncs.pfnConsolePrint( msg );
	}

	int iLocalPlayerIndex = gEngfuncs.GetLocalPlayer()->index;

	// Is it the local player talking?
	if ( entindex == -1 )
	{
		m_bTalking = !!bTalking;
		if( bTalking )
		{
			// Enable voice for them automatically if they try to talk.
			gEngfuncs.pfnClientCmd( "voice_modenable 1" );
		}
		
		// now set the player index to the correct index for the local player
		// this will allow us to have the local player's icon flash in the scoreboard
		entindex = iLocalPlayerIndex;

		pVoiceLoopback = gEngfuncs.pfnGetCvarPointer( "voice_loopback" );
	}
	else if ( entindex == -2 )
	{
		m_bServerAcked = !!bTalking;
	}

	if ( entindex >= 0 && entindex <= VOICE_MAX_PLAYERS )
	{
		int iClient = entindex - 1;
		if ( iClient < 0 )
		{
			return;
		}

		CVoiceLabel *pLabel = FindVoiceLabel( iClient );
		if ( bTalking )
		{
			m_VoicePlayers[iClient] = true;
			m_VoiceEnabledPlayers[iClient] = true;

			// If we don't have a label for this guy yet, then create one.
			if ( !pLabel )
			{
				// if this isn't the local player (unless they have voice_loopback on)
				if ( ( entindex != iLocalPlayerIndex ) || ( pVoiceLoopback && pVoiceLoopback->value ) )
				{
					if ( pLabel = GetFreeVoiceLabel() )
					{
						// Get the name from the engine.
						hud_player_info_t info;
						memset( &info, 0, sizeof( info ) );
						gEngfuncs.pfnGetPlayerInfo( entindex, &info );

						char name[512];
						color_tags::strip_color_tags(name, info.name, ARRAYSIZE(name));

						int color[3];
						m_pHelper->GetPlayerTextColor( entindex, color );

						if ( pLabel->m_pBackground )
						{
							pLabel->m_pBackground->setBgColor( color[0], color[1], color[2], 135 );
							pLabel->m_pBackground->setParent( *m_pParentPanel );
							pLabel->m_pBackground->setVisible( m_pHelper->CanShowSpeakerLabels() );
						}

						if ( pLabel->m_pLabel )
						{
							pLabel->m_pLabel->setFgColor( 255, 255, 255, 0 );
							pLabel->m_pLabel->setBgColor( 0, 0, 0, 255 );
							pLabel->m_pLabel->setText( "%s   ", name);
						}
						
						pLabel->m_clientindex = iClient;
					}
				}
			}
		}
		else
		{
			m_VoicePlayers[iClient] = false;

			// If we have a label for this guy, kill it.
			if ( pLabel )
			{
				pLabel->m_pBackground->setVisible( false );
				pLabel->m_clientindex = -1;
			}
		}
	}

	RepositionLabels();
}


void CVoiceStatus::UpdateServerState(bool bForce)
{
	// Can't do anything when we're not in a level.
	char const *pLevelName = gEngfuncs.pfnGetLevelName();
	if( pLevelName[0] == 0 )
	{
		if( gEngfuncs.pfnGetCvarFloat("voice_clientdebug") )
		{
			gEngfuncs.pfnConsolePrint( "CVoiceStatus::UpdateServerState: pLevelName[0]==0\n" );
		}

		return;
	}
	
	int bCVarModEnable = !!gEngfuncs.pfnGetCvarFloat("voice_modenable");
	if(bForce || m_bServerModEnable != bCVarModEnable)
	{
		m_bServerModEnable = bCVarModEnable;

		char str[256];
		_snprintf(str, sizeof(str), "VModEnable %d", m_bServerModEnable);
		ServerCmd(str);

		if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char msg[256];
			snprintf(msg, sizeof(msg), "CVoiceStatus::UpdateServerState: Sending '%s'\n", str);
			gEngfuncs.pfnConsolePrint(msg);
		}
	}

	char str[2048];
	sprintf(str, "vban");
	bool bChange = false;

	for(unsigned long dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{	
		unsigned long serverBanMask = 0;
		unsigned long banMask = 0;
		for(unsigned long i=0; i < 32; i++)
		{
			char playerID[16];
			if(!gEngfuncs.GetPlayerUniqueID(i+1, playerID))
				continue;

			if(m_BanMgr.GetPlayerBan(playerID))
				banMask |= 1 << i;

			if(m_ServerBannedPlayers[dw*32 + i])
				serverBanMask |= 1 << i;
		}

		if(serverBanMask != banMask)
			bChange = true;

		// Ok, the server needs to be updated.
		char numStr[512];
		sprintf(numStr, " %lx", banMask);
		strcat(str, numStr);
	}

	if(bChange || bForce)
	{
		if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char msg[256];
			snprintf(msg, sizeof(msg), "CVoiceStatus::UpdateServerState: Sending '%s'\n", str);
			gEngfuncs.pfnConsolePrint(msg);
		}

		gEngfuncs.pfnServerCmdUnreliable(str);	// Tell the server..
	}
	else
	{
		if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			gEngfuncs.pfnConsolePrint( "CVoiceStatus::UpdateServerState: no change\n" );
		}
	}
	
	m_LastUpdateServerState = gEngfuncs.GetClientTime();
}

void CVoiceStatus::UpdateSpeakerImage(Label *pLabel, int iPlayer)
{
	m_pBanButtons[iPlayer-1] = pLabel;
	UpdateBanButton(iPlayer-1);
}

void CVoiceStatus::UpdateBanButton(int iClient)
{
	Label *pPanel = m_pBanButtons[iClient];

	if (!pPanel)
		return;

	char playerID[16];
	extern bool HACK_GetPlayerUniqueID( int iPlayer, char playerID[16] );
	if(!HACK_GetPlayerUniqueID(iClient+1, playerID))
		return;

	// Figure out if it's blinking or not.
	bool bBlink   = fmod(m_BlinkTimer, SCOREBOARD_BLINK_FREQUENCY*2) < SCOREBOARD_BLINK_FREQUENCY;
	bool bTalking = !!m_VoicePlayers[iClient];
	bool bBanned  = m_BanMgr.GetPlayerBan(playerID);
	bool bNeverSpoken = !m_VoiceEnabledPlayers[iClient];

	// Get the appropriate image to display on the panel.
	if (bBanned)
	{
		pPanel->setImage(m_pScoreboardBanned);
	}
	else if (bTalking)
	{
		if (bBlink)
		{
			pPanel->setImage(m_pScoreboardSpeaking2);
		}
		else
		{
			pPanel->setImage(m_pScoreboardSpeaking);
		}
		pPanel->setFgColor(255, 170, 0, 1);
	}
	else if (bNeverSpoken)
	{
		pPanel->setImage(m_pScoreboardNeverSpoken);
		pPanel->setFgColor(100, 100, 100, 1);
	}
	else
	{
		pPanel->setImage(m_pScoreboardNotSpeaking);
	}
}


void CVoiceStatus::HandleVoiceMaskMsg(int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	unsigned long dw;
	for(dw=0; dw < VOICE_MAX_PLAYERS_DW; dw++)
	{
		m_AudiblePlayers.SetDWord(dw, (unsigned long)READ_LONG());
		m_ServerBannedPlayers.SetDWord(dw, (unsigned long)READ_LONG());

		if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
		{
			char str[256];
			gEngfuncs.pfnConsolePrint("CVoiceStatus::HandleVoiceMaskMsg\n");
			
			sprintf(str, "    - m_AudiblePlayers[%lu] = %u\n", dw, m_AudiblePlayers.GetDWord(dw));
			gEngfuncs.pfnConsolePrint(str);
			
			sprintf(str, "    - m_ServerBannedPlayers[%lu] = %u\n", dw, m_ServerBannedPlayers.GetDWord(dw));
			gEngfuncs.pfnConsolePrint(str);
		}
	}

	m_bServerModEnable = READ_BYTE();
}

void CVoiceStatus::HandleReqStateMsg(int iSize, void *pbuf)
{
	if(gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint("CVoiceStatus::HandleReqStateMsg\n");
	}

	UpdateServerState(true);	
}

void CVoiceStatus::StartSquelchMode()
{
	if(m_bInSquelchMode)
		return;

	m_bInSquelchMode = true;
	m_pHelper->UpdateCursorState();
}

void CVoiceStatus::StopSquelchMode()
{
	m_bInSquelchMode = false;
	m_pHelper->UpdateCursorState();
}

bool CVoiceStatus::IsInSquelchMode()
{
	return m_bInSquelchMode;
}

CVoiceLabel* CVoiceStatus::FindVoiceLabel(int clientindex)
{
	for(int i=0; i < MAX_VOICE_SPEAKERS; i++)
	{
		if(m_Labels[i].m_clientindex == clientindex)
			return &m_Labels[i];
	}

	return NULL;
}


CVoiceLabel* CVoiceStatus::GetFreeVoiceLabel()
{
	return FindVoiceLabel(-1);
}


void CVoiceStatus::RepositionLabels()
{
	// find starting position to draw from, along right-hand side of screen
	int y = ScreenHeight / 2;
	
	int iconWide = 8, iconTall = 8;
	if( m_pSpeakerLabelIcon )
	{
		m_pSpeakerLabelIcon->getSize( iconWide, iconTall );
	}
	
	// Reposition active labels.
	for(int i = 0; i < MAX_VOICE_SPEAKERS; i++)
	{
		CVoiceLabel *pLabel = &m_Labels[i];

		if( pLabel->m_clientindex == -1 || !pLabel->m_pLabel )
		{
			if( pLabel->m_pBackground )
				pLabel->m_pBackground->setVisible( false );

			continue;
		}

		int textWide, textTall;
		pLabel->m_pLabel->getContentSize( textWide, textTall );

		// Don't let it stretch too far across their screen.
		if( textWide > (ScreenWidth*2)/3 )
			textWide = (ScreenWidth*2)/3;

		// Setup the background label to fit everything in.
		int border = 2;
		int bgWide = textWide + iconWide + border*3;
		int bgTall = max( textTall, iconTall ) + border*2;
		pLabel->m_pBackground->setBounds( ScreenWidth - bgWide - 8, y, bgWide, bgTall );

		// Put the text at the left.
		pLabel->m_pLabel->setBounds( border, (bgTall - textTall) / 2, textWide, textTall );

		// Put the icon at the right.
		int iconLeft = border + textWide + border;
		int iconTop = (bgTall - iconTall) / 2;
		if( pLabel->m_pIcon )
		{
			pLabel->m_pIcon->setImage( m_pSpeakerLabelIcon );
			pLabel->m_pIcon->setBounds( iconLeft, iconTop, iconWide, iconTall );
		}

		y += bgTall + 2;
	}

	if( m_pLocalBitmap && m_pAckBitmap && m_pLocalLabel && (m_bTalking || m_bServerAcked) )
	{
		m_pLocalLabel->setParent(*m_pParentPanel);
		m_pLocalLabel->setVisible( true );

		if( m_bServerAcked && !!gEngfuncs.pfnGetCvarFloat("voice_clientdebug") )
			m_pLocalLabel->setImage( m_pAckBitmap );
		else
			m_pLocalLabel->setImage( m_pLocalBitmap );

		int sizeX, sizeY;
		m_pLocalBitmap->getSize(sizeX, sizeY);

		int local_xPos = ScreenWidth - sizeX - 10;
		int local_yPos = m_pHelper->GetAckIconHeight() - sizeY;
		
		m_pLocalLabel->setPos( local_xPos, local_yPos );
	}
	else
	{
		m_pLocalLabel->setVisible( false );
	}
}


void CVoiceStatus::FreeBitmaps()
{
#ifdef POSIX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
	// Delete all the images we have loaded.
	delete m_pLocalBitmap;
	m_pLocalBitmap = NULL;

	delete m_pAckBitmap;
	m_pAckBitmap = NULL;

	delete m_pSpeakerLabelIcon;
	m_pSpeakerLabelIcon = NULL;

	delete m_pScoreboardNeverSpoken;
	m_pScoreboardNeverSpoken = NULL;

	delete m_pScoreboardNotSpeaking;
	m_pScoreboardNotSpeaking = NULL;

	delete m_pScoreboardSpeaking;
	m_pScoreboardSpeaking = NULL;

	delete m_pScoreboardSpeaking2;
	m_pScoreboardSpeaking2 = NULL;

	delete m_pScoreboardSquelch;
	m_pScoreboardSquelch = NULL;

	delete m_pScoreboardBanned;
	m_pScoreboardBanned = NULL;
#ifdef POSIX
#pragma GCC diagnostic pop
#endif

	// Clear references to the images in panels.
	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		if (m_pBanButtons[i])
		{
			m_pBanButtons[i]->setImage(NULL);
		}
	}

	if(m_pLocalLabel)
		m_pLocalLabel->setImage(NULL);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the target client has been banned
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerBlocked(int iPlayer)
{
	char playerID[16];
	if (!gEngfuncs.GetPlayerUniqueID(iPlayer, playerID))
		return false;

	return m_BanMgr.GetPlayerBan(playerID);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the player can't hear the other client due to game rules (eg. the other team)
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVoiceStatus::IsPlayerAudible(int iPlayer)
{
	return !!m_AudiblePlayers[iPlayer-1];
}

//-----------------------------------------------------------------------------
// Purpose: blocks/unblocks the target client from being heard
// Input  : playerID - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVoiceStatus::SetPlayerBlockedState(int iPlayer, bool blocked)
{
	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::SetPlayerBlockedState part 1\n" );
	}

	char playerID[16];
	if (!gEngfuncs.GetPlayerUniqueID(iPlayer, playerID))
		return;

	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		gEngfuncs.pfnConsolePrint( "CVoiceStatus::SetPlayerBlockedState part 2\n" );
	}

	// Squelch or (try to) unsquelch this player.
	if (gEngfuncs.pfnGetCvarFloat("voice_clientdebug"))
	{
		char str[256];
		sprintf(str, "CVoiceStatus::SetPlayerBlockedState: setting player %d ban to %d\n", iPlayer, !m_BanMgr.GetPlayerBan(playerID));
		gEngfuncs.pfnConsolePrint(str);
	}

	m_BanMgr.SetPlayerBan( playerID, blocked );
	UpdateServerState(false);
}
