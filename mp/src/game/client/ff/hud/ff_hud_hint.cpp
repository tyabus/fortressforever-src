//	=============== Fortress Forever ==============
//	======== A modification for Half-Life 2 =======
//
//	@file ff_hud_hint.cpp
//	@author Patrick O'Leary (Mulchman)
//	@date 05/13/2005
//	@brief Hud Hint class - container for all active
//			hud hints - manages them all
//
//	REVISIONS
//	---------
//	05/13/2005, Mulchman: 
//		First created
//
//	07/11/2005, Mulchman:
//		Added client side ability to add hints
//
//	Mirv: Rewritten and fully implemented this

//
//	!!!IMPORTANT!!!
//
//	The underlying way that hints are identified needs sorting out.
//	Currently map hints are stored separately but this will be annoying when the
//	same hint is shown for each map. Or will it?
//
//	There also needs to be implemented a system to register hints and get an id 
//	for them.

#include "cbase.h"
IFileSystem **pFilesystem = &filesystem;

#include "ff_hud_hint.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "filesystem.h"

//#include "iclientmode.h"
//#include "engine/IVDebugOverlay.h"

//using namespace vgui;

//#include <vgui_controls/Panel.h>
//#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/RichText.h>
using namespace vgui;
// [integer] Duration [in seconds] that each hut hint is
// drawn on the screen
// Jiggles: Sorry, but I want to use this :)
//static ConVar hint_duration( "ffdev_hint_duration", "10" );
//static ConVar hint_on("cl_hints", "1");

// Helper var
CHudHint *pHudHintHelper = NULL;

typedef CUtlVector<unsigned short> HintVector;

// There is a rather lot of globals here right now!
static HintVector sMapHints;
static HintVector sGeneralHints;

static char szMapPath[MAX_PATH];

DECLARE_HUDELEMENT( CHudHint );
DECLARE_HUD_MESSAGE( CHudHint, FF_HudHint );

CHudHint::CHudHint( const char *pElementName ) : CHudElement( pElementName ), vgui::Panel( NULL, "HudHint" ) 
{
	// Set our parent window
	SetParent( g_pClientMode->GetViewport() );

	SetHiddenBits( 0 );

	szMapPath[0] = 0;

	m_bActive = true;

	m_flNextHint = m_flDuration = m_flStarted = 0;

	m_pRichText = NULL;
}

CHudHint::~CHudHint()
{
	pHudHintHelper = NULL;
}

void CHudHint::VidInit( void )
{
	// Point our helper to us
	pHudHintHelper = this;

	// Set up the rich text box that will contain the hud hint stuff
	if (!m_pRichText)
	{
		m_pRichText = new RichText(this, "HintText");
		m_pRichText->SetPos(0, 20);
		m_pRichText->SetWide(GetWide() - 2);
		m_pRichText->SetTall(GetTall() - 22);
		m_pRichText->SetVerticalScrollbar(false);
		m_pRichText->SetBorder(NULL);
	}

	SetPaintBackgroundEnabled(false);
}

void CHudHint::Init( void )
{
	HOOK_HUD_MESSAGE( CHudHint, FF_HudHint );
}

void CHudHint::AddHudHint(byte bType, unsigned short wID, const char *pszMessage, const char *pszSound)
{
	// First off, we're now ignoring hints which are triggered while a hint is
	// playing. We don't queue them up because they'll probably have lost relevancy
	// by the time they are played.
	if (gpGlobals->curtime < m_flNextHint && m_bActive)
	{
		//DevMsg("[Hud Hint] Hint ignored (%s)\n", pszMessage);
		return;
	}

	//DevMsg( "[Hud Hint] AddHudHint: %s\n", pszMessage );

	// When adding a hud hint we need to do a couple of things
	// Firstly, check to see if it's a new hint (ie. a hint
	// that the user hasn't seen before)
	HintVector *sHint = NULL;

	if (bType == HINT_GENERAL)
		sHint = &sGeneralHints;
	else
		sHint = &sMapHints;

	// Already in list of shown hints, so not active by default
	m_bActive = sHint->HasElement(wID);

	// Secondly, if it's a new hint, add it to our log file
	// thing
	if (m_bActive)
		sHint->AddToTail(wID);

	// Thirdly, display the new hint
	// Fourthly, if it's not a new hint we simply show
	// an icon on the screen that tells the user they can
	// hit their "show hint" key to view the old hint


	// TODO: TODO: TODO:
	// Do this here for now in case the dev team
	// is testing different durations and/or trying
	// to find the duration they want. Hard code it
	// later in VidInit or Init. This just lets it
	// get updated everytime we get a new hud hint.
	m_flDuration = 10;//hint_duration.GetInt();
	m_flStarted = gpGlobals->curtime;
	m_flNextHint = m_flStarted + m_flDuration + 2.0f;

	// Set the text now
	m_pRichText->SetText(pszMessage);

	// And play a sound, if there's one
	if (pszSound)
	{
		CBasePlayer *pLocal = CBasePlayer::GetLocalPlayer();

		if (!pLocal)
			return;

		CPASAttenuationFilter filter(pLocal, pszSound);

		EmitSound_t params;
		params.m_pSoundName = pszSound;
		params.m_flSoundTime = 0.0f;
		params.m_pflSoundDuration = NULL;
		params.m_bWarnOnDirectWaveReference = false;

		pLocal->EmitSound(filter, pLocal->entindex(), params);
	}
}

void CHudHint::MsgFunc_FF_HudHint( bf_read &msg )
{
	// Buffer
	char szString[ 4096 ];
	char szSound[ 4096 ];

	// Hint type [general, map]
	byte bType = msg.ReadByte();

	// Hint id
	unsigned short wID = msg.ReadWord();

	// Grab the string up to newline
	if( !msg.ReadString( szString, sizeof( szString ), true ) )
	{
		Warning( "[Hud Hint] String larger than buffer - not parsing!\n" );

		return;
	}

	if( !msg.ReadString( szSound, sizeof( szSound ), true ) )
		Warning( "[Hud Hint] Sound path larger than buffer - ignoring sound!\n" );

	// Pass the string along
	AddHudHint(bType, wID, szString, szSound[0] == 0 ? NULL : szSound);	
}

// This is currently A MESS!
void CHudHint::Paint()
{
	// TODO: Let's not actually do this every loop
	if (1/*!hint_on.GetBool()*/)
	{
		if (m_pRichText->IsVisible())
			m_pRichText->SetVisible(false);

		return;
	}

	bool bVisible = false;

	if (/*hint_on.GetBool() &&*/ gpGlobals->curtime > m_flStarted && gpGlobals->curtime < m_flNextHint)
	{
		// Make sure we are visible
		if (!IsVisible())
			SetVisible(true);

		// Get our scheme and font information
		vgui::HScheme scheme = vgui::scheme()->GetScheme("ClientScheme");
		vgui::HFont hFont = vgui::scheme()->GetIScheme(scheme)->GetFont("Default");

		g_pVGuiLocalize->ConvertANSIToUnicode("<IMG>oh god a hint</IMG>", m_pText, sizeof(m_pText));

		// Draw our text	
		surface()->DrawSetTextFont(hFont); // set the font	
		surface()->DrawSetTextColor(GetFgColor()); // white
		surface()->DrawSetTextPos(5, 2); // x,y position
		surface()->DrawPrintText(m_pText, wcslen(m_pText)); // print text

		if (m_bActive)
			bVisible = true;
	}

	// Update the status of the text box displaying the hint
	if (m_pRichText->IsVisible() != bVisible)
		m_pRichText->SetVisible(bVisible);
}

/************************************************************************/
/* Load hints into given container					*/
/************************************************************************/
void LoadHints(const char *pFilename, HintVector &hints)
{
	// There's no need for any of this if they don't want hints
	// (TODO: remember to freshly load hints if they turn them on)
	//if (!hint_on.GetBool())
	//	return;

	FileHandle_t f = (*pFilesystem)->Open(pFilename, "rb", "MOD");

	if (!f)
		return;

	// Load file into buffer
	int fileSize = (*pFilesystem)->Size(f);
	char *pBuffer = (char *) MemAllocScratch(fileSize + 1);

	Assert(pBuffer);

	(*pFilesystem)->Read(pBuffer, fileSize, f);
	pBuffer[fileSize] = 0;
	(*pFilesystem)->Close(f);

	// Parse hints
	// I'm assuming that it's safe to screw around with the buffer content
	char *pHintID = strtok(pBuffer, " ");

	while (pHintID)
	{
		int nHintID = atoi(pHintID);
		hints.AddToTail(nHintID);

		//DevMsg("Reading hint: %hu\n", nHintID);

		pHintID = strtok(NULL, " ");
	}

	// Cleaning up
	MemFreeScratch();
}

/************************************************************************/
/* Save the hints from a container into a file                          */
/************************************************************************/
void SaveHints(const char *pFilename, HintVector &hints)
{
	FileHandle_t f = (*pFilesystem)->Open(pFilename, "wb", "MOD");

	if (!f)
		return;

	// Shouldn't need any more space than this
	char szOutputBuffer[2048] = "";
	char szBuf[16];

	// Add to a buffer space delimited
	for (int i = 0; i < hints.Count(); i++)
	{
		Q_snprintf(szBuf, 15, "%hu", hints[i]);
		Q_strncat(szOutputBuffer, szBuf, 2047);

		//DevMsg("Adding hint: %u %hu\n", i, hints[i]);
	}

	(*pFilesystem)->Write(szOutputBuffer, Q_strlen(szOutputBuffer), f);
	(*pFilesystem)->Close(f);
}

// This is called when the map has finished or the client shuts down
// Record of viewed hints are saved here
void HudHintSave()
{
	if (!szMapPath[0])
		return;

	// Save only if there is something to save.
	// We don't clear this because it can just carry on as our running
	// record of map hints
	if (sGeneralHints.Count() > 0)
		SaveHints("cache/hints.txt", sGeneralHints);

	// Once again only save if needed
	// We always save and clear this because we don't know what map is next
	if (sMapHints.Count() > 0)
		SaveHints(szMapPath, sMapHints);
}

// Load hints, this is done when the client has finished loading a map
void HudHintLoad(const char *pMapName)
{
	// Make sure our map path is updated at all times
	Q_snprintf(szMapPath, MAX_PATH - 1, "cache/%s.txt", pMapName);

	// Only load general hints if empty, since we keep them as a running record
    if (sGeneralHints.Count() == 0)
		LoadHints("cache/hints.txt", sGeneralHints);

	// Always clear and load the map, since it has probably changed
	sMapHints.RemoveAll();
	LoadHints(szMapPath, sMapHints);
}

CON_COMMAND(showhint, "Show a trigger'd hint")
{
	if (!pHudHintHelper)
		return;

	pHudHintHelper->m_bActive = true;

	// Oh yeah and do all this mess again
	pHudHintHelper->m_flDuration = 10;//hint_duration.GetInt();
	pHudHintHelper->m_flStarted = gpGlobals->curtime;
	pHudHintHelper->m_flNextHint = pHudHintHelper->m_flStarted + pHudHintHelper->m_flDuration + 2.0f;
}
