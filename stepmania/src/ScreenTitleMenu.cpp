#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 Class: ScreenTitleMenu

 Desc: See header.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/

#include "ScreenTitleMenu.h"
#include "ScreenManager.h"
#include "GameConstantsAndTypes.h"
#include "RageUtil.h"
#include "StepMania.h"
#include "PrefsManager.h"
#include "RageLog.h"
#include "SongManager.h"
#include "AnnouncerManager.h"
#include "GameState.h"
#include "GameManager.h"
#include "InputMapper.h"
#include "ThemeManager.h"
#include "SDL_Utils.h"
#include "RageSoundManager.h"


#define HELP_X									THEME->GetMetricF("ScreenTitleMenu","HelpX")
#define HELP_Y									THEME->GetMetricF("ScreenTitleMenu","HelpY")
#define CHOICES_X								THEME->GetMetricF("ScreenTitleMenu","ChoicesX")
#define CHOICES_START_Y							THEME->GetMetricF("ScreenTitleMenu","ChoicesStartY")
#define CHOICES_SPACING_Y						THEME->GetMetricF("ScreenTitleMenu","ChoicesSpacingY")
#define CHOICES_SHADOW_LENGTH					THEME->GetMetricF("ScreenTitleMenu","ChoicesShadowLength")
#define COLOR_NOT_SELECTED						THEME->GetMetricC("ScreenTitleMenu","ColorNotSelected")
#define COLOR_SELECTED							THEME->GetMetricC("ScreenTitleMenu","ColorSelected")
#define ZOOM_NOT_SELECTED						THEME->GetMetricF("ScreenTitleMenu","ZoomNotSelected")
#define ZOOM_SELECTED							THEME->GetMetricF("ScreenTitleMenu","ZoomSelected")
#define SECONDS_BETWEEN_COMMENTS				THEME->GetMetricF("ScreenTitleMenu","SecondsBetweenComments")
#define SECONDS_BETWEEN_JOINT_PREMIUM_SCROLL	THEME->GetMetricF("ScreenTitleMenu","SecondsBetweenJointPremiumScroll")
#define SECONDS_BEFORE_ATTRACT					THEME->GetMetricF("ScreenTitleMenu","SecondsBeforeAttract")
#define HELP_TEXT_HOME							THEME->GetMetric("ScreenTitleMenu","HelpTextHome")
#define HELP_TEXT_PAY							THEME->GetMetric("ScreenTitleMenu","HelpTextPay")
#define HELP_TEXT_FREE							THEME->GetMetric("ScreenTitleMenu","HelpTextFree")
#define NEXT_SCREEN								THEME->GetMetric("ScreenTitleMenu","NextScreen")

const ScreenMessage SM_PlayComment			=	ScreenMessage(SM_User+1);
const ScreenMessage SM_GoToAttractLoop		=	ScreenMessage(SM_User+13);
const ScreenMessage SM_ScrollJointPremium	=	ScreenMessage(SM_User+14);

ScreenTitleMenu::ScreenTitleMenu()
{
	LOG->Trace( "ScreenTitleMenu::ScreenTitleMenu()" );

	if( PREFSMAN->m_bJointPremium )
	{
		m_sprJointPremiumMsg.Load( THEME->GetPathTo("Graphics","joint premium scroll message") );
		this->AddChild( &m_sprJointPremiumMsg );
		m_sprJointPremiumMsg.SetX( SCREEN_RIGHT+m_sprJointPremiumMsg.GetUnzoomedWidth()/2 );
		m_sprJointPremiumMsg.SetY( SCREEN_BOTTOM/2 );
	}

	m_textHelp.LoadFromFont( THEME->GetPathTo("Fonts","help") );
	CString sHelpText;
	switch( PREFSMAN->m_CoinMode )
	{
	case PrefsManager::COIN_HOME:	sHelpText = HELP_TEXT_HOME;	break;
	case PrefsManager::COIN_PAY:	sHelpText = HELP_TEXT_PAY;	break;
	case PrefsManager::COIN_FREE:	sHelpText = HELP_TEXT_FREE;	break;
	default:			ASSERT(0);
	}
	m_textHelp.SetText( sHelpText );
	m_textHelp.SetXY( HELP_X, HELP_Y );
	m_textHelp.SetZoom( 0.5f );
	m_textHelp.SetEffectBlinking();
	m_textHelp.TurnShadowOn();
	m_textHelp.SetShadowLength( 2 );
	this->AddChild( &m_textHelp );

	switch( PREFSMAN->m_CoinMode )
	{
	case PrefsManager::COIN_HOME:
		int i;
		for( i=0; i<NUM_CHOICES; i++ )
		{
			m_textChoice[i].LoadFromFont( THEME->GetPathTo("Fonts","titlemenu") );
			m_textChoice[i].SetText( ChoiceToString((Choice)i) );
			m_textChoice[i].SetXY( CHOICES_X, CHOICES_START_Y + i*CHOICES_SPACING_Y );
			m_textChoice[i].SetShadowLength( CHOICES_SHADOW_LENGTH );
			m_textChoice[i].TurnShadowOn();
			this->AddChild( &m_textChoice[i] );
		}	
		break;
	case PrefsManager::COIN_PAY:
		break;
	case PrefsManager::COIN_FREE:
		break;
	default:
		ASSERT(0);
	}


	SOUNDMAN->PlayOnceFromDir( ANNOUNCER->GetPathTo("title menu game name") );


	m_soundAttract.Load( ANNOUNCER->GetPathTo("title menu attract") );
	m_soundChange.Load( THEME->GetPathTo("Sounds","title menu change") );	
	m_soundSelect.Load( THEME->GetPathTo("Sounds","menu start") );
	m_soundInvalid.Load( THEME->GetPathTo("Sounds","menu invalid") );

	m_Choice = CHOICE_GAME_START;

	for( int i=0; i<NUM_CHOICES; i++ )
		LoseFocus( i );
	GainFocus( m_Choice );

	SOUNDMAN->PlayMusic( THEME->GetPathTo("Sounds","title menu music") );

	this->SendScreenMessage( SM_PlayComment, SECONDS_BETWEEN_COMMENTS);

	this->MoveToTail( &(ScreenAttract::m_Fade) );	// put it in the back so it covers up the stuff we just added
	this->SendScreenMessage( SM_ScrollJointPremium, 1);
}

ScreenTitleMenu::~ScreenTitleMenu()
{
	LOG->Trace( "ScreenTitleMenu::~ScreenTitleMenu()" );
}


void ScreenTitleMenu::Input( const DeviceInput& DeviceI, const InputEventType type, const GameInput &GameI, const MenuInput &MenuI, const StyleInput &StyleI )
{
	LOG->Trace( "ScreenTitleMenu::Input( %d-%d )", DeviceI.device, DeviceI.button );	// debugging gameport joystick problem

	if( type != IET_FIRST_PRESS )
		return;

	if( DeviceI.device == DEVICE_KEYBOARD && DeviceI.button == SDLK_F3 )
	{
		/* Coin mode changed.. since this effects how this screen appears, JUST the 
		screen is just reloaded, not a reset as was before -- Miryokuteki */
		(int&)PREFSMAN->m_CoinMode = (PREFSMAN->m_CoinMode+1) % PrefsManager::NUM_COIN_MODES;
		CString sMessage = "Coin Mode: ";
		switch( PREFSMAN->m_CoinMode )
		{
			case PrefsManager::COIN_HOME:	sMessage += "HOME";	break;
			case PrefsManager::COIN_PAY:	sMessage += "PAY";	break;
			case PrefsManager::COIN_FREE:	sMessage += "FREE";	break;
		}
		SCREENMAN->SystemMessage( sMessage );
		SCREENMAN->RefreshCreditsMessages();
		SCREENMAN->SetNewScreen("ScreenTitleMenu");
		return;
	}

	if( !MenuI.IsValid() )
		return;

	if( PREFSMAN->m_CoinMode==PrefsManager::COIN_HOME && m_Fade.IsClosing() )
		return;

	switch( MenuI.button )
	{
	case MENU_BUTTON_UP:
		switch( PREFSMAN->m_CoinMode )
		{
		case PrefsManager::COIN_HOME:
			TimeToDemonstration.GetDeltaTime();	/* Reset the demonstration timer when a key is pressed. */
			LoseFocus( m_Choice );
			if( m_Choice == 0 ) // wrap around
				m_Choice = (Choice)((int)NUM_CHOICES); 
			m_Choice = Choice( m_Choice-1 );
			m_soundChange.PlayRandom();
			GainFocus( m_Choice );
			break;
		case PrefsManager::COIN_PAY:
		case PrefsManager::COIN_FREE:
			return;	// ignore
		default:
			ASSERT(0);
		}
		break;
	case MENU_BUTTON_DOWN:
		switch( PREFSMAN->m_CoinMode )
		{
		case PrefsManager::COIN_HOME:
			TimeToDemonstration.GetDeltaTime();	/* Reset the demonstration timer when a key is pressed. */
			LoseFocus( m_Choice );
			if( m_Choice == (int)ScreenTitleMenu::NUM_CHOICES-1 ) 
				m_Choice = (Choice)-1; // wrap around
			m_Choice = Choice( m_Choice+1 );
			m_soundChange.PlayRandom();
			GainFocus( m_Choice );
			break;
		case PrefsManager::COIN_PAY:
		case PrefsManager::COIN_FREE:
			return;	// ignore
		}
		break;
	case MENU_BUTTON_BACK:
		m_Fade.CloseWipingRight( SM_GoToAttractLoop );
		break;
	case MENU_BUTTON_START:
		/* If this side is already in, don't re-join (and re-pay!). */
		if(GAMESTATE->m_bSideIsJoined[MenuI.player])
			break;

		switch( PREFSMAN->m_CoinMode )
		{
		case PrefsManager::COIN_PAY:
			if( GAMESTATE->m_iCoins < PREFSMAN->m_iCoinsPerCredit )
				break;	// not enough coins
			else
				GAMESTATE->m_iCoins -= PREFSMAN->m_iCoinsPerCredit;
				SCREENMAN->RefreshCreditsMessages();
			// fall through
		case PrefsManager::COIN_HOME:
		case PrefsManager::COIN_FREE:
			GAMESTATE->m_bSideIsJoined[MenuI.player] = true;
			GAMESTATE->m_MasterPlayerNumber = MenuI.player;
			GAMESTATE->m_bPlayersCanJoin = false;

			switch( m_Choice )
			{
			case CHOICE_GAME_START:
			case CHOICE_SELECT_GAME:
			case CHOICE_MAP_KEY_JOY:
			case CHOICE_OPTIONS:
			#ifdef _DEBUG
			case CHOICE_SANDBOX:
			#endif
				m_soundSelect.PlayRandom();
				m_Fade.CloseWipingRight( SM_GoToNextScreen );
				break;
			case CHOICE_JUKEBOX:
			case CHOICE_EDIT:
				if( SONGMAN->GetNumSongs() == 0 )
				{
					m_soundInvalid.PlayRandom();
					SCREENMAN->SystemMessage( "No songs are installed" );
				}
				else
				{
					m_soundSelect.PlayRandom();
					m_Fade.CloseWipingRight( SM_GoToNextScreen );
				}
				break;
			case CHOICE_EXIT:
				m_soundSelect.PlayRandom();
				ExitGame();
				LOG->Trace("CHOICE_EXIT: shutting down");
				return;
			default:
				ASSERT(0);
			}
			break;	
		default:
			ASSERT(0);
		}
		break;
	}

	Screen::Input( DeviceI, type, GameI, MenuI, StyleI );
}

void ScreenTitleMenu::Update( float fDelta )
{
	if(TimeToDemonstration.PeekDeltaTime() >= SECONDS_BEFORE_ATTRACT)
	{
		this->SendScreenMessage( SM_GoToAttractLoop, 0 );
		TimeToDemonstration.GetDeltaTime();
	}
	if( PREFSMAN->m_bJointPremium && TimeToJPScroll.PeekDeltaTime() >= SECONDS_BETWEEN_JOINT_PREMIUM_SCROLL )
	{
		this->SendScreenMessage( SM_ScrollJointPremium, 0 );
		TimeToJPScroll.GetDeltaTime();
	}
	Screen::Update(fDelta);
}


void ScreenTitleMenu::HandleScreenMessage( const ScreenMessage SM )
{
	switch( SM )
	{
	case SM_ScrollJointPremium:
		if( PREFSMAN->m_bJointPremium )
		{
			m_sprJointPremiumMsg.StopTweening();
			m_sprJointPremiumMsg.SetDiffuse( RageColor(1,1,1,1) );
			
			m_sprJointPremiumMsg.SetX( SCREEN_RIGHT+m_sprJointPremiumMsg.GetUnzoomedWidth()/2 );
			m_sprJointPremiumMsg.SetY( SCREEN_BOTTOM/2 );
			
			m_sprJointPremiumMsg.BeginTweening( 13.0f, Actor::TWEEN_BIAS_BEGIN ); // slide on
			m_sprJointPremiumMsg.SetTweenX( SCREEN_LEFT-m_sprJointPremiumMsg.GetUnzoomedWidth()*2 );
		}
		break;
	case SM_PlayComment:
		m_soundAttract.PlayRandom();
		this->SendScreenMessage( SM_PlayComment, SECONDS_BETWEEN_COMMENTS );
		break;
	case SM_GoToNextScreen:
		switch( m_Choice )
		{
		case CHOICE_GAME_START:
			SCREENMAN->SetNewScreen( NEXT_SCREEN );
			break;
		case CHOICE_SELECT_GAME:
			SCREENMAN->SetNewScreen( "ScreenSelectGame" );
			break;
		case CHOICE_MAP_KEY_JOY:
			SCREENMAN->SetNewScreen( "ScreenMapControllers" );
			break;
		case CHOICE_OPTIONS:
			SCREENMAN->SetNewScreen( "ScreenOptionsMenu" );
			break;
		case CHOICE_JUKEBOX:
			SCREENMAN->SetNewScreen( "ScreenJukeboxMenu" );
			break;
		#ifdef _DEBUG
		case CHOICE_SANDBOX:
			SCREENMAN->SetNewScreen( "ScreenTest" );
			break;
		#endif
		case CHOICE_EDIT:
			SCREENMAN->SetNewScreen( "ScreenEditMenu" );
			break;
		case CHOICE_EXIT:
		default:
			RAGE_ASSERT_M(0, "CHOICE_EXIT reached?");	// should never get here
			break;
		}
		break;
	case SM_GoToAttractLoop:
		SCREENMAN->SetNewScreen( "ScreenCompany" );
		break;
	}
}


void ScreenTitleMenu::LoseFocus( int iChoiceIndex )
{
	m_textChoice[iChoiceIndex].SetEffectNone();
	m_textChoice[iChoiceIndex].StopTweening();
	m_textChoice[iChoiceIndex].BeginTweening( 0.3f );
	m_textChoice[iChoiceIndex].SetTweenZoom( ZOOM_NOT_SELECTED );
}

void ScreenTitleMenu::GainFocus( int iChoiceIndex )
{
	m_textChoice[iChoiceIndex].StopTweening();
	m_textChoice[iChoiceIndex].BeginTweening( 0.3f );
	m_textChoice[iChoiceIndex].SetTweenZoom( ZOOM_SELECTED );
	RageColor color1, color2;
	color1 = COLOR_SELECTED;
	color2 = color1 * 0.5f;
	color2.a = 1;
	m_textChoice[iChoiceIndex].SetEffectCamelion( 2.5f, color1, color2 );
}

