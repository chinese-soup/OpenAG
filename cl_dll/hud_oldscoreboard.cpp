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
// Those who play on killed-off 32bit Apple don't deserve old_scoreboard looking good
// as I am too lazy to build & test in a MacOS, sorry.
#ifdef LINUX
	//#define ROW_GAP  15
	//#define ROW_RANGE_MIN 17

	// TODO: add docstring?
	#define ROW_GAP ( gHUD.m_scrinfo.iCharHeight - 5)
	#define ROW_RANGE_MIN ( gHUD.m_scrinfo.iCharHeight + 3)

	// Since Linux's Trebuchet MS is bigger than on Linux, the default width is set to 360 on Linux
	// Since it doesn't look good on the original 320 default, like it does on Windows.
	#define DEFAULT_WIDTH "380"
	#define DEFAULT_WIDTH_NUM 380

	#define LINUX_WIDTH 4

	// gHUD.DrawStringRightAligned on Linux 30 pixels to the left than it should be (38 vs 30 on Windows)
	// e.g. on 1280x720 DrawStringRightAligned(x=1280):
	// Linux: Last letter's pixel is positioned 38 pixels from the right corner
	// Windows: Last letter's pixel is positioned 8 pixels from the right corner
	//#define RIGHT_ALIGNED_LINUX_FIX 30
#else
	#define ROW_GAP  ( gHUD.m_scrinfo.iCharHeight - 5)
	#define ROW_RANGE_MIN ( gHUD.m_scrinfo.iCharHeight + 2)
	#define DEFAULT_WIDTH "320"
	#define DEFAULT_WIDTH_NUM 320
	#define LINUX_WIDTH 0

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
	m_pCvarOldScoreboardWidth = CVAR_CREATE("cl_old_scoreboard_width", DEFAULT_WIDTH, FCVAR_ARCHIVE);
	m_pCvarOldScoreboardShowColorsTags = CVAR_CREATE("cl_old_scoreboard_colortags", "0", FCVAR_ARCHIVE);
	m_bShowScoreboard = false;
	m_iFlags = 0;
	gHUD.AddHudElem(this);
	return 1;
};

int CHudOldScoreboard::VidInit(void)
{
	m_iFlags |= HUD_ACTIVE;
	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission

	int iSprite = 0;
	iSprite = gHUD.GetSpriteIndex("icon_ctf_score");
	m_IconFlagScore.spr = gHUD.GetSprite(iSprite);
	m_IconFlagScore.rc = gHUD.GetSpriteRect(iSprite);
	m_WidthScale = m_pCvarOldScoreboardWidth->value / 320.0f;
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

const char CHudOldScoreboard::CutNicknameOff(char name, int nameoffset)
{
	return 'a';
}

int CHudOldScoreboard::Draw(float fTime)
{
	// Let them use 320 even on Linux, if they really want to.
	if (m_pCvarOldScoreboardWidth->value < 320 || m_pCvarOldScoreboardWidth->value > ScreenWidth)
	{
		gEngfuncs.Con_Printf("Invalid cl_oldscoreboard_width value (%d).", (int)m_pCvarOldScoreboardWidth->value);
		gEngfuncs.Con_Printf("(minimum %d, maximum %d)\n", 320, ScreenWidth);
		gEngfuncs.Con_Printf("Resetting to default: %s", DEFAULT_WIDTH);
		m_pCvarOldScoreboardWidth->value = (float)DEFAULT_WIDTH_NUM;
	}

	if (!IsVisible())
		return 1;

	/*for(int i=97; i < 124; i++)
	{
		gEngfuncs.Con_Printf("gHUD.m_scrinfo.%c= %d\n", (char)i, gHUD.m_scrinfo.charWidths[static_cast<unsigned char>((char)i)]);
	}
	gEngfuncs.Con_Printf("gHUD.m_scrinfo.height = %d\n", gHUD.m_scrinfo.iHeight);
	gEngfuncs.Con_Printf("gHUD.m_scrinfo.width = %d\n", gHUD.m_scrinfo.iWidth);
	*/

	/* As close to the original as we can get */
	m_WidthScale = m_pCvarOldScoreboardWidth->value / 320.0f;
	NAME_RANGE_MIN = 20 * m_WidthScale + LINUX_WIDTH;
	NAME_RANGE_MAX = 145 * m_WidthScale + LINUX_WIDTH;
	KILLS_RANGE_MIN = 130 * m_WidthScale + LINUX_WIDTH;
	KILLS_RANGE_MAX = 160 * m_WidthScale + LINUX_WIDTH;
	DIVIDER_POS     = 180 * m_WidthScale + LINUX_WIDTH;
	DEATHS_RANGE_MIN = 190 * m_WidthScale + LINUX_WIDTH;
	//DEATHS_RANGE_MIN = 185 * m_WidthScale;
	DEATHS_RANGE_MAX = 220 * m_WidthScale + LINUX_WIDTH;
	PING_RANGE_MIN  = 245 * m_WidthScale + LINUX_WIDTH; // TODO: we stopped using this?
	PING_RANGE_MAX  = 295 * m_WidthScale + LINUX_WIDTH;

	//gEngfuncs.Con_Printf("NAME_RANGE_MIN = %i \n", NAME_RANGE_MIN);
	//gEngfuncs.Con_Printf("PING_RANGE_MAX = %i \n", PING_RANGE_MAX);

	int r, g, b;
	UnpackRGB(r,g,b, gHUD.m_iDefaultHUDColor);

	int xpos = 0;
	int ypos = 0;
	// just sort the list on the fly
	// list is sorted first by frags, then by deaths
	float list_slot = 0;
	int xpos_rel = ((ScreenWidth - m_pCvarOldScoreboardWidth->value) / 2);

	/* blah:
	 * WINDOWS = Trebuchet MS = 9x8
	 * LINUX = Trebuchet MS = 11x10 */

	// print the heading line
	ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);
	xpos = NAME_RANGE_MIN + xpos_rel;

	if (!gHUD.m_Teamplay)
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, CHudTextMessage::BufferedLocaliseTextString("#PLAYERS"), r, g, b );
	else
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, CHudTextMessage::BufferedLocaliseTextString("#TEAMS"), r, g, b );

	//gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, CHudTextMessage::BufferedLocaliseTextString("#SCORE"), r, g, b );
	//gHUD.DrawHudStringRightAligned( DIVIDER_POS + xpos , ypos, CHudTextMessage::BufferedLocaliseTextString("#SCORE"), r, g, b );
	gHUD.DrawHudString( KILLS_RANGE_MIN + xpos_rel  , ypos, 0, CHudTextMessage::BufferedLocaliseTextString("#SCORE"), r, g, b );
	gHUD.DrawHudString( DIVIDER_POS + xpos_rel, ypos, 0, "/", r, g, b );
	gHUD.DrawHudString( DEATHS_RANGE_MIN + xpos_rel, ypos, ScreenWidth, CHudTextMessage::BufferedLocaliseTextString("#DEATHS"), r, g, b );
	// can't use #LATENCY as RightAligned is not a friend with BufferedLocaliseTextString for some reason, sorry
	xpos = (int)m_pCvarOldScoreboardWidth->value + xpos_rel; //- (m_pCvarOldScoreboardWidth->value - PING_RANGE_MAX);
	gHUD.DrawHudStringRightAligned(xpos, ypos, "Ping/loss", r, g, b );

	list_slot += 1.5;
	ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);
	xpos = NAME_RANGE_MIN + xpos_rel;
	FillRGBA( xpos, ypos, PING_RANGE_MAX, 1, r, g, b, 255);  // draw the seperator line

	list_slot += 0.8;

	// draw the players, in order,  and restricted to team if set

	const auto scoreboard = gViewPort->GetScoreBoard();
	gViewPort->GetAllPlayersInfo();

	for (int iRow = 0; iRow < scoreboard->m_iRows; ++iRow)
	{
		const auto sorted = scoreboard->m_iSortedRows[iRow];
		int nameoffset = 0;

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

			char szTeamName[128];
			if (scoreboard->m_iIsATeam[iRow] == TEAM_SPECTATORS)
			{
				r = g = b = 100;
				snprintf(szTeamName, ARRAYSIZE(szTeamName), "%s", CHudTextMessage::BufferedLocaliseTextString( "#Spectators"));
			}
			else
			{
				r = iTeamColors[team_info->teamnumber % iNumberOfTeamColors][0];
				g = iTeamColors[team_info->teamnumber % iNumberOfTeamColors][1];
				b = iTeamColors[team_info->teamnumber % iNumberOfTeamColors][2];
				snprintf(szTeamName, ARRAYSIZE(szTeamName), "%s", team_info->name);
			}

			// cut off the name if it'd overlap score
			while( gHUD.GetHudStringWidth(szTeamName) + nameoffset + NAME_RANGE_MIN > KILLS_RANGE_MIN )
			{
				szTeamName[strlen(szTeamName) - 1] = '\0';
			}
			// draw their name (left to right)
			gHUD.DrawHudString( xpos, ypos, 0, szTeamName, r, g, b );

			// draw kills (right to left)
			xpos = KILLS_RANGE_MAX + xpos_rel;
			//gHUD.DrawHudNumberString( xpos, ypos, KILLS_RANGE_MIN + xpos_rel, team_info->frags, r, g, b );
			gHUD.DrawHudNumberStringFixed( xpos, ypos, team_info->frags, r, g, b );

			// draw divider
			xpos = DIVIDER_POS + xpos_rel;
			gHUD.DrawHudStringCentered( xpos, ypos, "/", r, g, b );

			// draw deaths
			xpos = DEATHS_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberStringFixed( xpos, ypos, team_info->deaths, r, g, b );
			list_slot++;
		}
		else
		{
			//Draw player info
			if (gHUD.m_Teamplay)
				nameoffset = 10;

			hud_player_info_t* pl_info = &g_PlayerInfoList[sorted];
			extra_player_info_t* pl_info_extra = &g_PlayerExtraInfo[sorted];

			ypos = ROW_TOP + ROW_RANGE_MIN + (list_slot * ROW_GAP);

			// check we haven't drawn too far down
			if ( ypos > ROW_RANGE_MAX )  // don't draw to close to the lower border
				break;

			int xpos = NAME_RANGE_MIN + xpos_rel;
			int r = 255, g = 255, b = 255; // TODO: THIS INITIALIZATION ISNT USED AT ALL
			r = iTeamColors[pl_info_extra->teamnumber % iNumberOfTeamColors][0];
			g = iTeamColors[pl_info_extra->teamnumber % iNumberOfTeamColors][1];
			b = iTeamColors[pl_info_extra->teamnumber % iNumberOfTeamColors][2];

			// TODO: steal CTF stuff from vgui_scoreboard
			if (gHUD.m_CTF.GetBlueFlagPlayerIndex() == sorted || gHUD.m_CTF.GetRedFlagPlayerIndex() == sorted)
			{
			}

			SPR_Set(m_IconFlagScore.spr, r, g, b );
			SPR_DrawAdditive( 0, xpos - 10, ypos, &m_IconFlagScore.rc );

			if (pl_info->thisplayer) // if it's their name, draw it a different color
			{
				r = g = b = 255;
				// overlay the background in blue, then draw the score text over it
				FillRGBA( NAME_RANGE_MIN + xpos_rel - 5, ypos + 5, PING_RANGE_MAX - 5, ROW_GAP, 0, 0, 255, 70 );
			}

			char szName[128];
			char colorlessName[128];
			int specoffset = 0;
			snprintf(szName, ARRAYSIZE(szName), "%s", pl_info->name);
			color_tags::strip_color_tags(colorlessName, szName, ARRAYSIZE(colorlessName));

			// If this player is a spectator we need to also fit " (S)" in, let's prepare for that
			if (g_IsSpectator[scoreboard->m_iSortedRows[iRow]])
			{
				std::string spec_str(" (S)");
				for( char& c : spec_str )
				{
					auto width = gHUD.m_scrinfo.charWidths[ static_cast<unsigned char>(c) ];
					specoffset += width;
				}
			}

			// cut off the name if it'd overlap score
			while( gHUD.GetHudStringWidth(colorlessName) + nameoffset + NAME_RANGE_MIN > KILLS_RANGE_MIN )
			{
				colorlessName[strlen(colorlessName) - 1] = '\0';
			}

			// Now that we have space for it, add the (S)
			if (g_IsSpectator[scoreboard->m_iSortedRows[iRow]])
			{
				colorlessName[strlen(colorlessName) - 1] = ')';
				colorlessName[strlen(colorlessName) - 2] = 'S';
				colorlessName[strlen(colorlessName) - 3] = '(';
				colorlessName[strlen(colorlessName) - 4] = ' ';
			}

			gHUD.DrawHudStringWithColorTags( xpos + nameoffset, ypos, colorlessName, r, g, b );

			// draw kills (right to left)
			xpos = KILLS_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberStringFixed( xpos, ypos, pl_info_extra->frags, r, g, b );

			// draw divider
			xpos = DIVIDER_POS + xpos_rel;
			//xpos = ((KILLS_RANGE_MAX - DEATHS_RANGE_MAX) / 2) + xpos_rel;
			gHUD.DrawHudStringCentered( xpos, ypos, "/", r, g, b );

			// draw deaths
			xpos = DEATHS_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberStringFixed( xpos, ypos, pl_info_extra->deaths, r, g, b );

			// xpos = ((PING_RANGE_MAX - PING_RANGE_MIN) / 2) + PING_RANGE_MIN + xpos_rel + 25;
			//xpos = PING_RANGE_MAX + xpos_rel;

			// draw ping & packetloss
			// Why 25: 320 (default width in the original AG) - 295 (PING_RANGE_MAX) = 25
			xpos = m_pCvarOldScoreboardWidth->value + xpos_rel - (m_pCvarOldScoreboardWidth->value - PING_RANGE_MAX);
			static char buf[64];
			sprintf( buf, "%d/%d", (int)pl_info->ping, (int)pl_info->packetloss );
			gHUD.DrawHudStringRightAligned( xpos , ypos, buf, r, g, b );

			list_slot++;
		}
	}

	return 1;
}
