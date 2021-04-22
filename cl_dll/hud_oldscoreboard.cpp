#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include <demo_api.h>
#include "vgui_TeamFortressViewport.h"
#include "hud_oldscoreboard.h"
#include "vgui_ScorePanel.h"

/* The scoreboard
We have a minimum width of 1-320 - we could have the field widths scale with it?
*/

// X positions
// relative to the side of the scoreboard
/*#define NAME_RANGE_MIN  20
#define NAME_RANGE_MAX  145
#define KILLS_RANGE_MIN 130
#define KILLS_RANGE_MAX 170
#define DIVIDER_POS		180
#define DEATHS_RANGE_MIN  185
#define DEATHS_RANGE_MAX  210
#define PING_RANGE_MIN	245
#define PING_RANGE_MAX	295*/

//#define SCOREBOARD_WIDTH 320

// Y positions
// Those who play on Apple 32 bit don't deserve old_scoreboard looking good, sorry
#ifdef LINUX
//#define ROW_GAP  15
#define ROW_GAP ( gHUD.m_scrinfo.iCharHeight - 5)
//#define ROW_RANGE_MIN 17
#define ROW_RANGE_MIN ( gHUD.m_scrinfo.iCharHeight + 3)
#define MAGIC 0
#else
#define ROW_GAP  ( gHUD.m_scrinfo.iCharHeight )
#define ROW_RANGE_MIN ( gHUD.m_scrinfo.iCharHeight + 2)
#define MAGIC 2
#endif // LINUX

#define ROW_RANGE_MAX ( ScreenHeight - 50 )
#define ROW_TOP 40

#define TEAM_NO				0
#define TEAM_YES			1
#define TEAM_SPECTATORS		2
#define TEAM_BLANK			3

int CHudOldScoreboard::Init(void)
{
	m_pCvarOldScoreboard = CVAR_CREATE("cl_old_scoreboard", "0", FCVAR_ARCHIVE);
	m_pCvarOldScoreboardWidth = CVAR_CREATE("cl_old_scoreboard_width", "320", FCVAR_ARCHIVE);
	m_pCvarOldScoreboardShowColorsTags = CVAR_CREATE("cl_old_scoreboard_colortags", "0", FCVAR_ARCHIVE);
	m_bShowScoreboard = false;
	m_iFlags = 0;
	m_WidthScale = m_pCvarOldScoreboardWidth->value / 320.0f;
	gHUD.AddHudElem(this);
	return 1;
};

int CHudOldScoreboard::VidInit(void)
{
	m_iFlags |= HUD_ACTIVE;
	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission

	//int iSprite = 0;
	//iSprite = gHUD.GetSpriteIndex("icon_ctf_score");
	//m_IconFlagScore.spr = gHUD.GetSprite(iSprite);
	//m_IconFlagScore.rc = gHUD.GetSpriteRect(iSprite);
	m_bShowScoreboard = false;

	return 1;
}

void CHudOldScoreboard::Reset(void)
{
}

bool CHudOldScoreboard::IsVisible()
{
	return m_bShowScoreboard;
}

void CHudOldScoreboard::ShowScoreboard(bool bShow)
{
	if (bShow && gViewPort && gViewPort->m_pScoreBoard)
		gViewPort->m_pScoreBoard->RebuildTeams();
	m_bShowScoreboard = bShow;
}
//extern int arrHudColor[3];

int CHudOldScoreboard::Draw(float fTime)
{
	if (!m_bShowScoreboard)
		return 1;

	/* As close to the original as we can get */
	m_WidthScale = m_pCvarOldScoreboardWidth->value / 320.0f;
	NAME_RANGE_MIN = 20 * m_WidthScale;
	NAME_RANGE_MAX = 145 * m_WidthScale;
	KILLS_RANGE_MIN = 130 * m_WidthScale;
	KILLS_RANGE_MAX = 170 * m_WidthScale;
	DIVIDER_POS     = 180 * m_WidthScale;
	DEATHS_RANGE_MIN = 185 * m_WidthScale;
	DEATHS_RANGE_MAX = 210 * m_WidthScale;
	PING_RANGE_MIN  = 245 * m_WidthScale;
	PING_RANGE_MAX  = 295 * m_WidthScale;

	gEngfuncs.Con_Printf("NAME_RANGE_MIN = %i \n", NAME_RANGE_MIN);
	gEngfuncs.Con_Printf("PING_RANGE_MAX = %i \n", PING_RANGE_MAX);

	int r, g, b;
	UnpackRGB(r,g,b, gHUD.m_iDefaultHUDColor);

	int xpos = 0;
	int ypos = 0;
	// just sort the list on the fly
	// list is sorted first by frags, then by deaths
	float list_slot = 0;
	int xpos_rel = ((ScreenWidth - m_pCvarOldScoreboardWidth->value) / 2) - MAGIC;

	/* INFO:
	 * WINDOWS e Trebuchet MS = 9x8
	 * LINUX e Trebuchet MS = 11x10 */

	// print the heading line
	ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);
	xpos = NAME_RANGE_MIN + xpos_rel;

	if ( !gHUD.m_Teamplay )
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, "Player", r, g, b );
	else
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, "Teams", r, g, b );

	//gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, CHudTextMessage::BufferedLocaliseTextString("#SCORE"), r, g, b );
	gHUD.DrawHudStringRightAligned( DIVIDER_POS + xpos , ypos, CHudTextMessage::BufferedLocaliseTextString("#SCORE"), r, g, b );
	gHUD.DrawHudString( DIVIDER_POS + xpos_rel, ypos, ScreenWidth, "/", r, g, b );
	gHUD.DrawHudString( DEATHS_RANGE_MIN + xpos_rel + 5, ypos, ScreenWidth, CHudTextMessage::BufferedLocaliseTextString("#DEATHS"), r, g, b );
	//gHUD.DrawHudString( PING_RANGE_MAX + xpos_rel - 35, ypos, 0, CHudTextMessage::BufferedLocaliseTextString("#LATENCY"), r, g, b );

	xpos = ((PING_RANGE_MAX - PING_RANGE_MIN) / 2) + PING_RANGE_MIN + xpos_rel + 95;
	gHUD.DrawHudStringRightAligned(xpos, ypos, CHudTextMessage::BufferedLocaliseTextString("#LATENCY"), r, g, b );

	list_slot += 1.5;
	ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);
	xpos = NAME_RANGE_MIN + xpos_rel;
	FillRGBA( xpos, ypos, PING_RANGE_MAX, 1, r, g, b, 255);  // draw the seperator line

	list_slot += 0.8;

	// draw the players, in order,  and restricted to team if set

	const auto scoreboard = gViewPort->GetScoreBoard();
	gViewPort->GetAllPlayersInfo();

	for (int iRow = 0; iRow < scoreboard->m_iRows; ++iRow) {

		const auto sorted = scoreboard->m_iSortedRows[iRow];

		if (scoreboard->m_iIsATeam[iRow] == TEAM_BLANK)
			continue;

		else if (scoreboard->m_iIsATeam[iRow] && gHUD.m_Teamplay)
		{
			team_info_t* team_info = &g_TeamInfo[sorted];

			if (0 != iRow)
				list_slot += 0.3;
			ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);

			// check we haven't drawn too far down
			if ( ypos > ROW_RANGE_MAX )  // don't draw to close to the lower border
				break;

			xpos = NAME_RANGE_MIN + xpos_rel;
			int r = 255, g = 225, b = 55; // draw the stuff kinda yellowish
			r = iTeamColors[team_info->teamnumber % iNumberOfTeamColors][0];
			g = iTeamColors[team_info->teamnumber % iNumberOfTeamColors][1];
			b = iTeamColors[team_info->teamnumber % iNumberOfTeamColors][2];
			if (scoreboard->m_iIsATeam[iRow] == TEAM_SPECTATORS)
			{
				r = g = b = 100;
				strcpy(team_info->name,CHudTextMessage::BufferedLocaliseTextString( "#Spectators"));
			}

			// draw their name (left to right)
			gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, team_info->name, r, g, b );

			// draw kills (right to left)
			xpos = KILLS_RANGE_MAX + xpos_rel;
			//gHUD.DrawHudNumberString( xpos, ypos, KILLS_RANGE_MIN + xpos_rel, team_info->frags, r, g, b );
			gHUD.DrawHudNumberStringFixed( xpos, ypos, team_info->frags, r, g, b );

			// draw divider
			xpos = DIVIDER_POS + xpos_rel;
			gHUD.DrawHudString( xpos, ypos, xpos + 20, "/", r, g, b );

			// draw deaths
			xpos = DEATHS_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberStringFixed( xpos, ypos, team_info->deaths, r, g, b );

			// draw ping
			// draw ping & packetloss
			/*static char buf[64];
			sprintf( buf, "%d/%d", (int)team_info->ping,(int)team_info->packetloss);
			xpos = ((PING_RANGE_MAX - PING_RANGE_MIN) / 2) + PING_RANGE_MIN + xpos_rel + 25;
			gHUD.DrawHudStringReverse( xpos, ypos, xpos - 50, buf, r, g, b );*/

			list_slot++;
		}
		else
		{
			//Draw team info
			int nameoffset = 0;

			if (gHUD.m_Teamplay)
				nameoffset = 10;
			/*for(int i=97; i < 124; i++)
			{
				gEngfuncs.Con_Printf("gHUD.m_scrinfo.%c= %d\n", (char)i, gHUD.m_scrinfo.charWidths[static_cast<unsigned char>((char)i)]);
			}
			gEngfuncs.Con_Printf("gHUD.m_scrinfo.height = %d\n", gHUD.m_scrinfo.iHeight);
			gEngfuncs.Con_Printf("gHUD.m_scrinfo.width = %d\n", gHUD.m_scrinfo.iWidth);
			gEngfuncs.Con_Printf("gHUD.m_scrinfo.charheight = %d\n", gHUD.m_scrinfo.iCharHeight);*/

			hud_player_info_t* pl_info = &g_PlayerInfoList[sorted];
			extra_player_info_t* pl_info_extra = &g_PlayerExtraInfo[sorted];

			int ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);

			// check we haven't drawn too far down
			if ( ypos > ROW_RANGE_MAX )  // don't draw to close to the lower border
				break;

			int xpos = NAME_RANGE_MIN + xpos_rel;
			int r = 255, g = 255, b = 255;
			r = iTeamColors[pl_info_extra->teamnumber % iNumberOfTeamColors][0];
			g = iTeamColors[pl_info_extra->teamnumber % iNumberOfTeamColors][1];
			b = iTeamColors[pl_info_extra->teamnumber % iNumberOfTeamColors][2];
			//ScaleColors(r,g,b,135);

			if (pl_info->thisplayer) // if it is their name, draw it a different color
			{
				r = g = b = 255;
				// overlay the background in blue,  then draw the score text over it
				FillRGBA( NAME_RANGE_MIN + xpos_rel - 5, ypos + 5, PING_RANGE_MAX - 5, ROW_GAP, 0, 0, 255, 70 );
			}

			// TODO: steal CTF stuff from vgui_scoreboard
			/*if (gHUD.m_CTF.GetBlueFlagPlayerIndex() == sorted[iRow] || gHUD.m_CTF.GetRedFlagPlayerIndex() == sorted[iRow])
			{
				SPR_Set(m_IconFlagScore.spr, 200, 200, 200 );
				SPR_DrawAdditive( 0, xpos - 26, ypos, &m_IconFlagScore.rc );
			}*/

			// Awful hack begins here:
			// "m" is /probably/ the widest letter in any font the player might be using
			// let's use it to estimate how many letters of nickname we can fit
			// since our draw hud functions no longer work letter-by-letter and don't check the charWidths like
			// the original AG 6.6 client DLL does (probably because of charWidths obviously not supporting unicode?)
			int letter_m_width = gHUD.m_scrinfo.charWidths[static_cast<unsigned char>('m')];

			std::string szName_string(pl_info->name);
			int max_length = 0;
			for( int i = szName_string.length(); i > 0; i-- )
			{
				if ( letter_m_width * i > KILLS_RANGE_MIN )
					continue;
				else
				{
					max_length = i;
					break;
				}
			}
			szName_string = szName_string.substr(0, max_length);

			if (g_IsSpectator[scoreboard->m_iSortedRows[iRow]])
				szName_string += "  (S)";

			char szName[128];
			snprintf(szName, ARRAYSIZE(szName), "%s", pl_info->name);
			strcpy(szName, szName_string.c_str());

			if (m_pCvarOldScoreboardShowColorsTags->value != 1)
				color_tags::strip_color_tags(szName, szName, ARRAYSIZE(szName));

			// draw their name (left to right)
			//gHUD.DrawHudString( xpos + nameoffset, ypos, NAME_RANGE_MAX + xpos_rel, szName, r, g, b );
			gHUD.DrawHudStringWithColorTags( xpos + nameoffset, ypos, szName, r, g, b );

			// draw kills (right to left)
			xpos = KILLS_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberStringFixed( xpos, ypos, pl_info_extra->frags, r, g, b );

			// draw divider
			xpos = DIVIDER_POS + xpos_rel;
			gHUD.DrawHudString( xpos, ypos, xpos + 20, "/", r, g, b );

			// draw deaths
			xpos = DEATHS_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberStringFixed( xpos, ypos, pl_info_extra->deaths, r, g, b );

			// draw ping & packetloss
			static char buf[64];
			sprintf( buf, "%d/%d", (int)pl_info->ping,(int)pl_info->packetloss );
			xpos = ((PING_RANGE_MAX - PING_RANGE_MIN) / 2) + PING_RANGE_MIN + xpos_rel + 25;
			gHUD.DrawHudStringRightAligned( xpos , ypos, buf, r, g, b );

			list_slot++;
		}
	}

	return 1;
}
