#ifndef OLDSCOREBOARD_H
#define OLDSCOREBOARD_H
#pragma once

#include "cl_entity.h"
#include "interpolation.h"

class CHudOldScoreboard: public CHudBase
{
public:
	int Init( void );
	int VidInit( void );
	int Draw( float flTime );
	void Reset( void );

	bool IsVisible( );
	const char CutNicknameOff( char name, int nameoffset = 0 );
	void ShowScoreboard( bool bShow = true );
private:
	typedef struct
	{
		HSPRITE spr;
		wrect_t rc;
	} icon_flagstatus_t;

	icon_flagstatus_t m_IconFlagScore;

	bool m_bShowScoreboard;

	cvar_t *m_pCvarOldScoreboard;
	cvar_t *m_pCvarOldScoreboardWidth;
	cvar_t *m_pCvarOldScoreboardShowColorsTags;

	float m_WidthScale;
	int NAME_RANGE_MIN;
	int NAME_RANGE_MAX;
	int KILLS_RANGE_MIN;
	int KILLS_RANGE_MAX;
	int DIVIDER_POS;
	int DEATHS_RANGE_MIN;
	int DEATHS_RANGE_MAX;
	int PING_RANGE_MIN;
	int PING_RANGE_MAX;

};

#endif //OLD_SCOREBOARD_H