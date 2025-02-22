/*
 *      Copyright (C) 2005-2009 Team XBMC
 *      http://www.xbmc.org
 *
 *		Copyright (C) 2010-2013 Eduard Kytmanov
 *		http://www.avmedia.su
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifdef HAS_DS_PLAYER

#include "DSPlayer.h"
#include "DSUtil/DSUtil.h" // unload loaded filters
#include "DSUtil/SmartPtr.h"
#include "Filters/RendererSettings.h"

#include "windowing/windows/winsystemwin32.h" //Important needed to get the right hwnd
#include "xbmc/GUIInfoManager.h"
#include "utils/SystemInfo.h"
#include "input/mouse/MouseStat.h"
#include "settings/Settings.h"
#include "FileItem.h"
#include "utils/log.h"
#include "URL.h"

#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogBusy.h"
#include "windowing/WinSystem.h"
#include "dialogs/GUIDialogOK.h"
#include "PixelShaderList.h"
#include "guilib/GUIComponent.h"
#include "guilib/LocalizeStrings.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "cores/AudioEngine/Engines/ActiveAE/ActiveAE.h"
#include "messaging/ApplicationMessenger.h"
#include "DSInputStreamPVRManager.h"
#include "pvr/PVRManager.h"
#include "pvr/windows/GUIWindowPVRBase.h"
#include "pvr/channels/PVRChannel.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "Application.h"
#include "AppInboundProtocol.h"
#include "GUIUserMessages.h"
#include "input/Key.h"
#include "cores/VideoPlayer/Interface/Addon/TimingConstants.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "cores/DSPlayer/dsgraph.h"
#include "settings/MediaSettings.h"
#include "settings/DisplaySettings.h"
#include "ServiceBroker.h"
#include "cores/DataCacheCore.h"
#include "DSFilterVersion.h"
#include "DVDFileInfo.h"

using namespace PVR;
using namespace std;
using namespace KODI::MESSAGING;

DSPLAYER_STATE CDSPlayer::PlayerState = DSPLAYER_CLOSED;
CGUIDialogBoxBase *CDSPlayer::errorWindow = NULL;
ThreadIdentifier CDSPlayer::m_threadID = 0;
HWND CDSPlayer::m_hWnd = 0;

CDSPlayer::CDSPlayer(IPlayerCallback& callback)
  : IPlayer(callback),
  CThread("CDSPlayer thread"),
  m_hReadyEvent(true),
  m_pGraphThread(this),
  m_bEof(false),
  m_renderManager(this)
{
  m_CurrentVideoRenderer = DIRECTSHOW_RENDERER_UNDEF;
  m_pAllocatorCallback = nullptr;
  m_pPaintCallback = nullptr;
  m_renderOnDs = false;
  m_bPerformStop = false;
  m_lastActiveVideoRect = { 0, 0, 0, 0 };

  m_canTempo = false;
  m_HasVideo = false;
  m_HasAudio = false;
  m_isMadvr = (CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_VIDEORENDERER) == "madVR");

  if (InitWindow(m_hWnd))
    CLog::Log(LOGDEBUG, "%s : Create DSPlayer window - hWnd: %i", __FUNCTION__, static_cast<void*>(m_hWnd));

  /* Suspend AE temporarily so exclusive or hog-mode sinks */
  /* don't block DSPlayer access to audio device  */
  if (!CServiceBroker::GetActiveAE()->Suspend())
  {
    CLog::Log(LOGNOTICE, __FUNCTION__, "Failed to suspend AudioEngine before launching DSPlayer");
  }

  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  m_pClock.GetClock(); // Reset the clock
  g_dsGraph = new CDSGraph(callback);

  // Change DVD Clock, time base
  m_pClock.SetTimeBase((int64_t)DS_TIME_BASE);

  m_processInfo = CProcessInfo::CreateInstance();

  CServiceBroker::GetWinSystem()->Register(this);
  CServiceBroker::GetWinSystem()->RegisterRenderLoop(this);
}

CDSPlayer::~CDSPlayer()
{
  CServiceBroker::GetWinSystem()->UnregisterRenderLoop(this);
  CServiceBroker::GetWinSystem()->Unregister(this);

  /* Resume AE processing of XBMC native audio */
  if (!CServiceBroker::GetActiveAE()->Resume())
  {
    CLog::Log(LOGFATAL, __FUNCTION__, "Failed to restart AudioEngine after return from DSPlayer");
  }

  if (PlayerState != DSPLAYER_CLOSED)
    CloseFile();

  UnloadExternalObjects();
  CLog::Log(LOGDEBUG, "%s External objects unloaded", __FUNCTION__);

  // Restore DVD Player time base clock
  m_pClock.SetTimeBase(DVD_TIME_BASE);

  // Save Shader settings
  g_dsSettings.pixelShaderList->SaveXML();

  CoUninitialize();

  DeInitWindow();

  SAFE_DELETE(g_dsGraph);
  SAFE_DELETE(g_pPVRStream);

  CLog::Log(LOGNOTICE, "%s DSPlayer is now closed", __FUNCTION__);
}

int CDSPlayer::GetSubtitleCount()
{
  return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetSubtitleCount() : 0;
}

int CDSPlayer::GetSubtitle()
{
  return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetSubtitle() : 0;
}

void CDSPlayer::SetSubtitle(int iStream)
{
   if (CStreamsManager::Get()) 
     CStreamsManager::Get()->SetSubtitle(iStream);
}

bool CDSPlayer::GetSubtitleVisible()
{
  return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetSubtitleVisible() : true;
}

void CDSPlayer::SetSubtitleVisible(bool bVisible)
{
  if (CStreamsManager::Get())
    CStreamsManager::Get()->SetSubtitleVisible(bVisible);
}

void CDSPlayer::AddSubtitle(const std::string& strSubPath) 
{
  if (CStreamsManager::Get())
    CStreamsManager::Get()->SetSubtitle(CStreamsManager::Get()->AddSubtitle(strSubPath));
}

bool CDSPlayer::WaitForFileClose()
{
  if (!WaitForThreadExit(100) || !m_pGraphThread.WaitForThreadExit(100))
    return false;

  return true;
}

bool CDSPlayer::OpenFileInternal(const CFileItem& file)
{
  m_processInfo->ResetVideoCodecInfo();
  m_processInfo->ResetAudioCodecInfo();
  try
  {
    CLog::Log(LOGNOTICE, "%s - DSPlayer: Opening: %s", __FUNCTION__, CURL::GetRedacted(file.GetPath()).c_str());
    if (PlayerState != DSPLAYER_CLOSED)
      CloseFile();

    if(!WaitForFileClose())
      return false;

    PlayerState = DSPLAYER_LOADING;
    m_currentFileItem = file;

    m_hReadyEvent.Reset();

    Create();

    m_callback.OnPlayBackStarted(m_currentFileItem);

    m_hReadyEvent.WaitMSec(500);

    return (PlayerState != DSPLAYER_ERROR);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s - Exception thrown on open", __FUNCTION__);
    return false;
  }
}

void CDSPlayer::LoadVideoSettings(const CFileItem& file)
{

}

void CDSPlayer::SetCurrentVideoRenderer(const std::string &videoRenderer)
{
  m_CurrentVideoRenderer = DIRECTSHOW_RENDERER_MADVR;
}

bool CDSPlayer::OpenFile(const CFileItem& file, const CPlayerOptions &options)
{
  if (m_isMadvr && !CDSFilterVersion::Get()->IsRegisteredFilter(MADVR_FILTERSTR))
  {
    CLog::Log(LOGDEBUG, "%s - madVR it's not installed on the system pls download it before to use it with DSPlayer", __FUNCTION__);

    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, g_localizeStrings.Get(90023), g_localizeStrings.Get(90024), 6000L, false);
    return false;
  }

  if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_VIDEOSCREEN_FAKEFULLSCREEN))
  {
    CGraphFilters::Get()->SetKodiRealFS(true);
    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::SETTING_VIDEOSCREEN_FAKEFULLSCREEN, true);
  }

  CGraphFilters::Get()->SetAuxAudioDelay();

  CLog::Log(LOGNOTICE, "%s - DSPlayer: Opening: %s", __FUNCTION__, CURL::GetRedacted(file.GetPath()).c_str());

  CFileItem fileItem = file;
  m_PlayerOptions = options;
  m_canTempo = true;

  if (file.IsVideoDb())
  {
    fileItem.SetPath(file.GetVideoInfoTag()->m_strFileNameAndPath);
    fileItem.SetProperty("original_listitem_url", file.GetPath());
  }

  if (fileItem.IsInternetStream())
  {
    CURL url(fileItem.GetPath());
    url.SetProtocolOptions("");
    fileItem.SetPath(url.Get());
  }
  else if (fileItem.IsPVR())
  {
    g_pPVRStream = new CDSInputStreamPVRManager(this);
    return g_pPVRStream->Open(fileItem);
    m_canTempo = false;
  }

  return OpenFileInternal(fileItem);
}

bool CDSPlayer::CloseFile(bool reopen)
{
  CSingleLock lock(m_CleanSection);

  if (PlayerState == DSPLAYER_LOADING)
  {
    g_dsGraph->QueueStop();
    return false;
  }

  // zoom
  if (m_pAllocatorCallback)
    m_lastActiveVideoRect = m_pAllocatorCallback->GetActiveVideoRect();

  // if needed restore the currentrate
  if (m_pGraphThread.GetCurrentRate() != 1)
    SetSpeed(1);

  // reset intial delay in decoder interface
  if (CStreamsManager::Get()) CStreamsManager::Get()->resetDelayInterface();

  if (PlayerState == DSPLAYER_CLOSED || PlayerState == DSPLAYER_CLOSING)
    return true;

  PlayerState = DSPLAYER_CLOSING;
  m_HasVideo = false;
  m_HasAudio = false;
  m_canTempo = false;

  // set the abort request so that other threads can finish up
  m_bEof = g_dsGraph->IsEof();

  // stop the rendering on dsplayer device
  m_renderOnDs = false;

  g_dsGraph->CloseFile();

  PlayerState = DSPLAYER_CLOSED;

  // Stop threads
  m_pGraphThread.StopThread();
  StopThread();

  m_renderManager.UnInit();

  CLog::Log(LOGDEBUG, "%s File closed", __FUNCTION__);
  return true;
}

void CDSPlayer::GetVideoStreamInfo(int streamId, VideoStreamInfo &info)
{
  CSingleLock lock(m_StateSection);
  std::string strStreamName;

  if (CStreamsManager::Get()) CStreamsManager::Get()->GetVideoStreamName(strStreamName);
  info.name = strStreamName;
  info.width = (GetPictureWidth());
  info.height = (GetPictureHeight());
  info.codecName = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetVideoCodecName() : "";
  info.videoAspectRatio = (float)info.width / (float)info.height;
  CRect viewRect;
  m_renderManager.GetVideoRect(info.SrcRect, info.DestRect, viewRect);
  info.stereoMode = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetStereoMode() : "";
  if (info.stereoMode == "mono")
    info.stereoMode = "";
}

void CDSPlayer::GetAudioStreamInfo(int index, AudioStreamInfo &info)
{
  if (index == CURRENT_STREAM)
    index = GetAudioStream();

  CSingleLock lock(m_StateSection);

  std::string strStreamName;
  std::string label;
  std::string codecname;

  info.bitrate = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetBitsPerSample(index) : 0;
  info.codecName = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetAudioCodecName(index) : "";
  if (CStreamsManager::Get()) CStreamsManager::Get()->GetAudioStreamName(index, strStreamName);
  info.language = strStreamName;
  info.channels = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetChannels(index) : 0;
  info.samplerate = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetSampleRate(index) : 0;
  codecname = (CStreamsManager::Get()) ? CStreamsManager::Get()->GetAudioCodecDisplayName(index) : "";

  if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_SHOWSPLITTERDETAIL) ||
      CGraphFilters::Get()->UsingMediaPortalTsReader())
  { 
    label = StringUtils::Format("%s - (%s, %d Hz, %i Channels)", strStreamName.c_str(), codecname.c_str(), info.samplerate, info.channels);
    info.name = label;
  }
  else
    info.name = strStreamName;
}

void CDSPlayer::GetSubtitleStreamInfo(int index, SubtitleStreamInfo &info)
{
  std::string strStreamName;
 if (CStreamsManager::Get()) CStreamsManager::Get()->GetSubtitleName(index, strStreamName);
  
  info.language = strStreamName;
  info.name = strStreamName;
}

bool CDSPlayer::IsPlaying() const
{
  return !m_bStop;
}

bool CDSPlayer::HasVideo() const
{
  return m_HasVideo;
}
bool CDSPlayer::HasAudio() const
{
  return m_HasAudio;
}

void CDSPlayer::GetDebugInfo(std::string &audio, std::string &video, std::string &general)
{
  audio = g_dsGraph->GetAudioInfo();
  video = g_dsGraph->GetVideoInfo();
  GetGeneralInfo(general);
}

void CDSPlayer::GetGeneralInfo(std::string& strGeneralInfo)
{
  CSingleLock lock(m_StateSection);
  strGeneralInfo = g_dsGraph->GetGeneralInfo();
}

float CDSPlayer::GetAVDelay()
{
  float fValue = 0.0f;

  if (CStreamsManager::Get())
    fValue = CStreamsManager::Get()->GetAVDelay();

  return fValue;
}

void CDSPlayer::SetAVDelay(float fValue)
{
  //get displaylatency
  int iDisplayLatency = m_renderManager.GetDisplayLatency() * 1000;

  if (CStreamsManager::Get()) CStreamsManager::Get()->SetAVDelay(fValue,iDisplayLatency);

  SetAudioCodeDelayInfo();
}

void CDSPlayer::SetAudioStream(int iStream) 
{ 
  if (CStreamsManager::Get()) CStreamsManager::Get()->SetAudioStream(iStream); 
    UpdateProcessInfo(iStream);
}

float CDSPlayer::GetSubTitleDelay()
{
  float fValue = 0.0f;

  if (CStreamsManager::Get())
    fValue = CStreamsManager::Get()->GetSubTitleDelay();

  return fValue;
}

void CDSPlayer::SetSubTitleDelay(float fValue)
{
  if (CStreamsManager::Get())
    CStreamsManager::Get()->SetSubTitleDelay(fValue);
}

bool CDSPlayer::InitWindow(HWND &hWnd)
{
  m_hInstance = (HINSTANCE)GetModuleHandle(NULL);
  if (m_hInstance == NULL)
    CLog::Log(LOGDEBUG, "%s : GetModuleHandle failed with %d", __FUNCTION__, GetLastError());

  RESOLUTION_INFO res = CDisplaySettings::GetInstance().GetCurrentResolutionInfo();
  int nWidth = res.iWidth;
  int nHeight = res.iHeight;
  m_className = L"Kodi:DSPlayer";

  // Register the windows class
  WNDCLASS wndClass;

  wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;
  wndClass.lpfnWndProc = WndProc;
  wndClass.cbClsExtra = 0;
  wndClass.cbWndExtra = 0;
  wndClass.hInstance = m_hInstance;
  wndClass.hIcon = NULL;
  wndClass.hCursor = NULL;
  wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wndClass.lpszMenuName = NULL;
  wndClass.lpszClassName = m_className.c_str();

  if (!RegisterClass(&wndClass))
  {
    CLog::Log(LOGERROR, "%s : RegisterClass failed with %d", __FUNCTION__, GetLastError());
    return false;
  }

  hWnd = CreateWindow(m_className.c_str(), m_className.c_str(),
    WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
    0, 0, nWidth, nHeight, 
    g_hWnd, NULL, m_hInstance, NULL);
  if (hWnd == NULL)
  {
    CLog::Log(LOGERROR, "%s : CreateWindow failed with %d", __FUNCTION__, GetLastError());
    return false;
  }

  if (hWnd)
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  return true;
}

void CDSPlayer::DeInitWindow()
{
  // remove ourself as user data to ensure we're not called anymore
  SetWindowLongPtr(m_hWnd, GWLP_USERDATA, 0);

  // destroy the hidden window
  DestroyWindow(m_hWnd);

  // unregister the window class
  UnregisterClass(m_className.c_str(), m_hInstance);

  // reset the hWnd
  m_hWnd = NULL;
}

void CDSPlayer::SetDSWndVisible(bool bVisible)
{
  int cmd;
  bVisible ? cmd = SW_SHOW : cmd = SW_HIDE;
  ShowWindow(m_hWnd, cmd);
  UpdateWindow(m_hWnd);
}

void CDSPlayer::SetRenderOnDS(bool bRender)
{
  m_renderOnDs = bRender;

  if (bRender)
    SetDSWndVisible(bRender);
}

LRESULT CALLBACK CDSPlayer::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_MOUSEMOVE:
  case WM_LBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_MBUTTONUP:
  case WM_RBUTTONUP:
  case WM_MOUSEWHEEL:
  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_SYSKEYUP:
  case WM_SYSKEYDOWN:
  case WM_APPCOMMAND:
  case WM_SETFOCUS:
    ::PostMessage(g_hWnd, uMsg, wParam, lParam);
    return(0);
  case WM_SIZE:
    SetWindowPos(hWnd, 0, 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
    PostMessage(new CDSMsgBool(CDSMsg::RESET_DEVICE, true), false);
    return(0);
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//CThread
void CDSPlayer::OnStartup()
{
  m_threadID = CThread::GetCurrentThreadId();
}

void CDSPlayer::OnExit()
{
  if (PlayerState == DSPLAYER_LOADING)
    PlayerState = DSPLAYER_ERROR;

  // In case of, set the ready event
  // Prevent a dead loop
  m_hReadyEvent.Set();
  m_bStop = true;
  m_threadID = 0;
  if (!m_bEof || PlayerState == DSPLAYER_ERROR)
    m_callback.OnPlayBackStopped();
  else
    m_callback.OnPlayBackEnded();
}

void CDSPlayer::Process()
{
  HRESULT hr = E_FAIL;
  CLog::Log(LOGNOTICE, "%s - Creating DS Graph", __FUNCTION__);

  // Set the selected video renderer
  SetCurrentVideoRenderer(CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_VIDEORENDERER));

  // Create DirectShow Graph
  hr = g_dsGraph->SetFile(m_currentFileItem, m_PlayerOptions);

  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "%s - Failed creating DS Graph", __FUNCTION__);
    PlayerState = DSPLAYER_ERROR;
    return;
  }

  m_pGraphThread.SetCurrentRate(1);
  m_pGraphThread.Create();
  UpdateApplication();

  // Start playback
  // If there's an error, the lock must be released in order to show the error dialog
  m_hReadyEvent.Set();

  if (PlayerState != DSPLAYER_ERROR)
  {
    m_HasVideo = true;
    m_HasAudio = true;

    // Select Audio Stream, Delay
    if (CStreamsManager::Get()) CStreamsManager::Get()->SelectBestAudio();
    SetAVDelay(g_application.GetAppPlayer().GetVideoSettings().m_AudioDelay);

    // Select Subtitle Stream, Delay, On/Off
    if (CStreamsManager::Get()) CStreamsManager::Get()->SelectBestSubtitle(m_currentFileItem.GetPath());
    SetSubTitleDelay(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleDelay);
    SetSubtitleVisible(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn);

    // Seek
    if (m_PlayerOptions.starttime > 0)
      g_dsGraph->Seek(SEC_TO_DS_TIME(m_PlayerOptions.starttime), 1U, false);
    else
      g_dsGraph->Seek(SEC_TO_DS_TIME(0), 1U, false);

    // Starts playback
    g_dsGraph->Play(true);
    m_callback.OnAVStarted(m_currentFileItem);

    if (CGraphFilters::Get()->IsDVD())
      CStreamsManager::Get()->LoadDVDStreams();
  }

  while (!m_bStop && PlayerState != DSPLAYER_CLOSED && PlayerState != DSPLAYER_LOADING)
    HandleMessages();
}

void CDSPlayer::HandleMessages()
{
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) != 0)
  {
    if (msg.message == WM_GRAPHMESSAGE)
    {
      CDSMsg* pMsg = reinterpret_cast<CDSMsg *>(msg.lParam);
      CLog::Log(LOGDEBUG, "%s Message received : %d on thread 0x%X", __FUNCTION__, pMsg->GetMessageType(), m_threadID);

      if (CDSPlayer::PlayerState == DSPLAYER_CLOSED || CDSPlayer::PlayerState == DSPLAYER_LOADING)
      {
        pMsg->Set();
        pMsg->Release();
        break;
      }
      if (pMsg->IsType(CDSMsg::GENERAL_SET_WINDOW_POS))
      {
        g_dsGraph->UpdateWindowPosition();
      }
      if (pMsg->IsType(CDSMsg::RESET_DEVICE))
      {
        CDSMsgBool* speMsg = reinterpret_cast<CDSMsgBool *>(pMsg);
        g_application.GetAppPlayer().Reset(speMsg->m_value);
      }
      if (pMsg->IsType(CDSMsg::SET_WINDOW_POS))
      {
        SetPosition();
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_SEEK_TIME))
      {
        CDSMsgPlayerSeekTime* speMsg = reinterpret_cast<CDSMsgPlayerSeekTime *>(pMsg);
        g_dsGraph->Seek(speMsg->GetTime(), speMsg->GetFlags(), speMsg->ShowPopup());
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_SEEK))
      {
        CDSMsgPlayerSeek* speMsg = reinterpret_cast<CDSMsgPlayerSeek*>(pMsg);
        g_dsGraph->Seek(speMsg->Forward(), speMsg->LargeStep());
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_SEEK_PERCENT))
      {
        CDSMsgDouble * speMsg = reinterpret_cast<CDSMsgDouble *>(pMsg);
        g_dsGraph->SeekPercentage((float)speMsg->m_value);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_PAUSE))
      {
        g_dsGraph->Pause();
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_STOP))
      {
        CDSMsgBool* speMsg = reinterpret_cast<CDSMsgBool *>(pMsg);
        g_dsGraph->Stop(speMsg->m_value);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_PLAY))
      {
        CDSMsgBool* speMsg = reinterpret_cast<CDSMsgBool *>(pMsg);
        g_dsGraph->Play(speMsg->m_value);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_UPDATE_TIME))
      {
        g_dsGraph->UpdateTime();
      }

      /*DVD COMMANDS*/
      if (pMsg->IsType(CDSMsg::PLAYER_DVD_MOUSE_MOVE))
      {
        CDSMsgInt* speMsg = reinterpret_cast<CDSMsgInt *>(pMsg);
        //TODO make the xbmc gui stay hidden when moving mouse over menu
        POINT pt;
        pt.x = GET_X_LPARAM(speMsg->m_value);
        pt.y = GET_Y_LPARAM(speMsg->m_value);
        ULONG pButtonIndex;
        /**** Didnt found really where VideoPlayer are doing it exactly so here it is *****/
        XBMC_Event newEvent;
        newEvent.type = XBMC_MOUSEMOTION;
        newEvent.motion.x = (uint16_t)pt.x;
        newEvent.motion.y = (uint16_t)pt.y;
        CServiceBroker::GetAppPort()->OnEvent(newEvent);
        /*CGUIMessage pMsg(GUI_MSG_VIDEO_MENU_STARTED, 0, 0);
        CServiceBroker::GetGUI()->GetWindowManager().SendMessage(pMsg);*/
        /**** End of ugly hack ***/
        if (SUCCEEDED(CGraphFilters::Get()->DVD.dvdInfo->GetButtonAtPosition(pt, &pButtonIndex)))
          CGraphFilters::Get()->DVD.dvdControl->SelectButton(pButtonIndex);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MOUSE_CLICK))
      {
        CDSMsgInt* speMsg = reinterpret_cast<CDSMsgInt *>(pMsg);
        POINT pt;
        pt.x = GET_X_LPARAM(speMsg->m_value);
        pt.y = GET_Y_LPARAM(speMsg->m_value);
        ULONG pButtonIndex;
        if (SUCCEEDED(CGraphFilters::Get()->DVD.dvdInfo->GetButtonAtPosition(pt, &pButtonIndex)))
          CGraphFilters::Get()->DVD.dvdControl->SelectAndActivateButton(pButtonIndex);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_NAV_UP))
      {
        CGraphFilters::Get()->DVD.dvdControl->SelectRelativeButton(DVD_Relative_Upper);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_NAV_DOWN))
      {
        CGraphFilters::Get()->DVD.dvdControl->SelectRelativeButton(DVD_Relative_Lower);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_NAV_LEFT))
      {
        CGraphFilters::Get()->DVD.dvdControl->SelectRelativeButton(DVD_Relative_Left);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_NAV_RIGHT))
      {
        CGraphFilters::Get()->DVD.dvdControl->SelectRelativeButton(DVD_Relative_Right);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_ROOT))
      {
        CGUIMessage _msg(GUI_MSG_VIDEO_MENU_STARTED, 0, 0);
        CServiceBroker::GetGUI()->GetWindowManager().SendMessage(_msg);
        CGraphFilters::Get()->DVD.dvdControl->ShowMenu(DVD_MENU_Root, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_EXIT))
      {
        CGraphFilters::Get()->DVD.dvdControl->Resume(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_BACK))
      {
        CGraphFilters::Get()->DVD.dvdControl->ReturnFromSubmenu(DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_SELECT))
      {
        CGraphFilters::Get()->DVD.dvdControl->ActivateButton();
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_TITLE))
      {
        CGraphFilters::Get()->DVD.dvdControl->ShowMenu(DVD_MENU_Title, DVD_CMD_FLAG_Block | DVD_CMD_FLAG_Flush, NULL);
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_SUBTITLE))
      {
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_AUDIO))
      {
      }
      else if (pMsg->IsType(CDSMsg::PLAYER_DVD_MENU_ANGLE))
      {
      }
      pMsg->Set();
      pMsg->Release();
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void CDSPlayer::Pause()
{  
  if (PlayerState == DSPLAYER_CLOSING)
    return;

  if (PlayerState == DSPLAYER_LOADING || PlayerState == DSPLAYER_LOADED )
  {
    g_dsGraph->QueuePause();
    return;
  }

  g_dsGraph->UpdateState();

  m_pGraphThread.SetSpeedChanged(true);
  if (PlayerState == DSPLAYER_PAUSED)
  {
    SetSpeed(1);
    m_callback.OnPlayBackResumed();
  }
  else
  {
    SetSpeed(0);
    m_callback.OnPlayBackPaused();
  }
  PostMessage(new CDSMsg(CDSMsg::PLAYER_PAUSE));
}
void CDSPlayer::SetSpeed(float iSpeed)
{
  if (iSpeed != 1 && iSpeed != 0)
    CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetPlayerInfoProvider().SetDisplayAfterSeek();

  m_pGraphThread.SetCurrentRate(iSpeed);
  m_pGraphThread.SetSpeedChanged(true);
  CDataCacheCore::GetInstance().SetSpeed(1.0, iSpeed);
}

void CDSPlayer::SetTempo(float tempo)
{
  // TODO
}

bool CDSPlayer::SupportsTempo()
{
  return m_canTempo;
}

void CDSPlayer::Seek(bool bPlus, bool bLargeStep, bool bChapterOverride)
{
  PostMessage(new CDSMsgPlayerSeek(bPlus, bLargeStep));
}

void CDSPlayer::SeekPercentage(float iPercent)
{
  PostMessage(new CDSMsgDouble(CDSMsg::PLAYER_SEEK_PERCENT, iPercent));
}

bool CDSPlayer::OnAction(const CAction &action)
{
  if (g_dsGraph->IsDvd())
  {
    if (action.GetID() == ACTION_SHOW_VIDEOMENU)
    {
      PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_MENU_ROOT), false);
      return true;
    }
    if (g_dsGraph->IsInMenu())
    {
      switch (action.GetID())
      {
      case ACTION_PREVIOUS_MENU:
        PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_MENU_BACK), false);
        break;
      case ACTION_MOVE_LEFT:
        PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_NAV_LEFT), false);
        break;
      case ACTION_MOVE_RIGHT:
        PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_NAV_RIGHT), false);
        break;
      case ACTION_MOVE_UP:
        PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_NAV_UP), false);
        break;
      case ACTION_MOVE_DOWN:
        PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_NAV_DOWN), false);
        break;
        /*case ACTION_MOUSE_MOVE:
        case ACTION_MOUSE_LEFT_CLICK:
        {
        CRect rs, rd;
        GetVideoRect(rs, rd);
        CPoint pt(action.GetAmount(), action.GetAmount(1));
        if (!rd.PtInRect(pt))
        return false;
        pt -= CPoint(rd.x1, rd.y1);
        pt.x *= rs.Width() / rd.Width();
        pt.y *= rs.Height() / rd.Height();
        pt += CPoint(rs.x1, rs.y1);
        if (action.GetID() == ACTION_MOUSE_LEFT_CLICK)
        SendMessage(g_hWnd, WM_COMMAND, ID_DVD_MOUSE_CLICK,MAKELPARAM(pt.x,pt.y));
        else
        SendMessage(g_hWnd, WM_COMMAND, ID_DVD_MOUSE_MOVE,MAKELPARAM(pt.x,pt.y));
        return true;
        }
        break;*/
      case ACTION_SELECT_ITEM:
      {
        // show button pushed overlay
        PostMessage(new CDSMsg(CDSMsg::PLAYER_DVD_MENU_SELECT), false);
      }
        break;
      case REMOTE_0:
      case REMOTE_1:
      case REMOTE_2:
      case REMOTE_3:
      case REMOTE_4:
      case REMOTE_5:
      case REMOTE_6:
      case REMOTE_7:
      case REMOTE_8:
      case REMOTE_9:
      {
        // Offset from key codes back to button number
        // int button = action.actionId - REMOTE_0;
        //CLog::Log(LOGDEBUG, " - button pressed %d", button);
        //pStream->SelectButton(button);
      }
        break;
      default:
        return false;
        break;
      }
      return true; // message is handled
    }
  }

  if (g_pPVRStream)
  {
    // TODO
    return true;
  }

  switch (action.GetID())
  {
  case ACTION_NEXT_ITEM:
  case ACTION_PAGE_UP:
    if (GetChapterCount() > 0)
    {
      SeekChapter(GetChapter() + 1);
      CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetPlayerInfoProvider().SetDisplayAfterSeek();
      return true;
    }
    else
      break;
  case ACTION_PREV_ITEM:
  case ACTION_PAGE_DOWN:
    if (GetChapterCount() > 0)
    {
      SeekChapter(GetChapter() - 1);
      CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetPlayerInfoProvider().SetDisplayAfterSeek();
      return true;
    }
    else
      break;
  case ACTION_PLAYER_DEBUG:
    m_renderManager.ToggleDebug();
    break;

  case ACTION_PLAYER_PROCESS_INFO:
    if (CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindow() != WINDOW_DIALOG_DSPLAYER_PROCESS_INFO)
    {
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_DSPLAYER_PROCESS_INFO);
      return true;
    }
    break;  
  }

  // return false to inform the caller we didn't handle the message
  return false;
}

// Time is in millisecond
void CDSPlayer::SeekTime(__int64 iTime)
{
  int seekOffset = (int)(iTime - DS_TIME_TO_MSEC(g_dsGraph->GetTime(true)));
  PostMessage(new CDSMsgPlayerSeekTime(MSEC_TO_DS_TIME((iTime < 0) ? 0 : iTime)));
  m_callback.OnPlayBackSeek((int)iTime, seekOffset);
}

// Time is in millisecond
bool CDSPlayer::SeekTimeRelative(__int64 iTime)
{
  __int64 abstime = (__int64)(DS_TIME_TO_MSEC(g_dsGraph->GetTime(true)) + iTime);
  PostMessage(new CDSMsgPlayerSeekTime(MSEC_TO_DS_TIME((abstime < 0) ? 0 : abstime)));
  m_callback.OnPlayBackSeek((int)abstime, (int)iTime);
  return true;
}

void CDSPlayer::UpdateApplication()
{
  if (g_pPVRStream)
  {
    // TODO
  }
}

// Called after fast channel switch.
void CDSPlayer::UpdateChannelSwitchSettings()
{
  if (g_dsGraph->GetTime(true) + MSEC_TO_DS_TIME(3000) < g_dsGraph->GetTotalTime(true))
  {
    // Pause playback
    PostMessage(new CDSMsgBool(CDSMsg::PLAYER_PAUSE, true), false);
    // Seek to the end of timeshift buffer
    PostMessage(new CDSMsgPlayerSeekTime(g_dsGraph->GetTotalTime(true) - MSEC_TO_DS_TIME(3000)));
    // Start playback
    PostMessage(new CDSMsgBool(CDSMsg::PLAYER_PLAY, true), false);
  }

#ifdef HAS_VIDEO_PLAYBACK
  // when using fast channel switching some shortcuts are taken which 
  // means we'll have to update the view mode manually
  m_renderManager.SetViewMode(g_application.GetAppPlayer().GetVideoSettings().m_ViewMode);
#endif
}

bool CDSPlayer::SelectChannel(bool bNext)
{
  bool bShowPreview = false;/*(CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt("pvrplayback.channelentrytimeout") > 0);*/ // TODO

  if (!bShowPreview)
  {
    CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetPlayerInfoProvider().SetDisplayAfterSeek(100000);
  }

  bool bResult = (bNext ? g_pPVRStream->NextChannel(bShowPreview) : g_pPVRStream->PrevChannel(bShowPreview));

  return bResult;
}

bool CDSPlayer::ShowPVRChannelInfo()
{
  bool bReturn(false);
  // TODO
  return bReturn;
}

void CDSPlayer::FrameMove()
{
  m_renderManager.FrameMove();
  CDataCacheCore::GetInstance().SetPlayTimes(0, GetTime(), 0, GetTotalTime());
}

void CDSPlayer::Render(bool clear, uint32_t alpha, bool gui)
{
  m_renderManager.Render(clear, 0, alpha, gui);
}

void CDSPlayer::FlushRenderer()
{
  m_renderManager.Flush();
}

void CDSPlayer::SetRenderViewMode(int mode, float zoom, float par, float shift, bool stretch)
{
  m_renderManager.SetViewMode(mode);
}

float CDSPlayer::GetRenderAspectRatio()
{
  return m_renderManager.GetAspectRatio();
}

void CDSPlayer::TriggerUpdateResolution()
{
  m_renderManager.TriggerUpdateResolution(0, 0, 0);
}

bool CDSPlayer::IsRenderingVideo()
{
  return m_renderManager.IsConfigured();
}

bool CDSPlayer::Supports(EINTERLACEMETHOD method)
{
  if (method == VS_INTERLACEMETHOD_NONE
    || method == VS_INTERLACEMETHOD_AUTO
    || method == VS_INTERLACEMETHOD_DEINTERLACE)
    return true;

  return false;
}

bool CDSPlayer::Supports(ESCALINGMETHOD method)
{
  return m_renderManager.Supports(method);
}

bool CDSPlayer::Supports(ERENDERFEATURE feature)
{
  return m_renderManager.Supports(feature);
}

void CDSPlayer::OnLostDisplay()
{
}

void CDSPlayer::OnResetDisplay()
{
}

void CDSPlayer::UpdateProcessInfo(int index)
{
  if (index == CURRENT_STREAM)
    index = g_application.GetAppPlayer().GetAudioStream();

  std::string info;

  //Renderers in dsplayer
  info = StringUtils::Format("%s, %s", CGraphFilters::Get()->VideoRenderer.osdname.c_str(), CGraphFilters::Get()->AudioRenderer.osdname.c_str());
  m_processInfo->SetVideoPixelFormat(info);
  //filters in dsplayer
  m_processInfo->SetVideoDeintMethod(g_dsGraph->GetGeneralInfo());

  //AUDIO

  //add visible track number
  info = StringUtils::Format("%i %s", CStreamsManager::Get() ? CStreamsManager::Get()->GetChannels(index) : 0, g_localizeStrings.Get(14301).c_str());
  if (GetAudioStreamCount() > 1)
    info = StringUtils::Format("(%i/%i) %s ", index + 1, GetAudioStreamCount(), info.c_str());
  m_processInfo->SetAudioChannels(info);

  m_processInfo->SetAudioBitsPerSample(CStreamsManager::Get() ? CStreamsManager::Get()->GetBitsPerSample(index) : 0);
  m_processInfo->SetAudioSampleRate(CStreamsManager::Get() ? CStreamsManager::Get()->GetSampleRate(index) : 0);

  //add delay info to audio decoder name;
  SetAudioCodeDelayInfo(index);

  //VIDEO
  unsigned int width = GetPictureWidth();
  unsigned int heigth = GetPictureHeight();
  m_processInfo->SetVideoDimensions(width, heigth);

  info = CStreamsManager::Get() ? CStreamsManager::Get()->GetVideoCodecName() : "";

  // add active decoder info
  std::pair<std::string, bool> activeDecoder;
  CGraphFilters::Get()->GetActiveDecoder(activeDecoder);

  if (!activeDecoder.first.empty())
    info += " (" + activeDecoder.first + ")";

  m_processInfo->SetVideoDecoderName(info, activeDecoder.second);
  m_processInfo->SetVideoDAR((float)width / (float)heigth);
  m_processInfo->SetVideoFps(m_fps);

  CServiceBroker::GetDataCacheCore().SignalAudioInfoChange();
  CServiceBroker::GetDataCacheCore().SignalVideoInfoChange();
}

void CDSPlayer::SetAudioCodeDelayInfo(int index)
{
  if (index == CURRENT_STREAM)
    index = g_application.GetAppPlayer().GetAudioStream();

  std::string info;
  int iAudioDelay = CStreamsManager::Get() ? CStreamsManager::Get()->GetLastAVDelay() : 0;
  info = StringUtils::Format("%s, %ims delay", CStreamsManager::Get() ? CStreamsManager::Get()->GetAudioCodecDisplayName(index).c_str() : "", iAudioDelay);

  m_processInfo->SetAudioDecoderName(info);

  CServiceBroker::GetDataCacheCore().SignalAudioInfoChange();
}

void CDSPlayer::DisplayChange(bool bExternalChange)
{
  m_renderManager.DisplayChange(bExternalChange);

  if (UsingDS())
    m_pAllocatorCallback->DisplayChange(bExternalChange);
}

// IDSRendererAllocatorCallback
CRect CDSPlayer::GetActiveVideoRect()
{
  CRect activeVideoRect(0, 0, 0, 0);

  if (ReadyDS())
    activeVideoRect = m_pAllocatorCallback->GetActiveVideoRect();

  return activeVideoRect;
}

bool CDSPlayer::IsEnteringExclusive()
{
  if (UsingDS(DIRECTSHOW_RENDERER_MADVR))
    return m_pAllocatorCallback->IsEnteringExclusive();

  return false;
}

void CDSPlayer::EnableExclusive(bool bEnable)
{
  if (UsingDS(DIRECTSHOW_RENDERER_MADVR))
    m_pAllocatorCallback->EnableExclusive(bEnable);
}

void CDSPlayer::SetPixelShader()
{
  if (UsingDS(DIRECTSHOW_RENDERER_MADVR))
    m_pAllocatorCallback->SetPixelShader();
}

void CDSPlayer::SetResolution()
{
  if (UsingDS(DIRECTSHOW_RENDERER_MADVR))
    m_pAllocatorCallback->SetResolution();
}

void CDSPlayer::SetPosition(CRect sourceRect, CRect videoRect, CRect viewRect)
{
  m_sourceRect = sourceRect;
  m_videoRect = videoRect;
  m_viewRect = viewRect;

  if (!CDSPlayer::IsCurrentThread())
  {
    CDSPlayer::PostMessage(new CDSMsg(CDSMsg::SET_WINDOW_POS), false);
    return;
  }
  else
    SetPosition();
}

void CDSPlayer::SetPosition()
{
  if (UsingDS())
    m_pAllocatorCallback->SetPosition(m_sourceRect, m_videoRect, m_viewRect);
}

bool CDSPlayer::ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM *wParam, LPARAM *lParam, LRESULT *ret)
{
  if (UsingDS(DIRECTSHOW_RENDERER_MADVR))
    return m_pAllocatorCallback->ParentWindowProc(hWnd, uMsg, wParam, lParam, ret);

  return false;
}

void CDSPlayer::Reset(bool bForceWindowed)
{

}

// IDSRendererPaintCallback
void CDSPlayer::BeginRender()
{
  if (m_pPaintCallback && ReadyDS())
    m_pPaintCallback->BeginRender();
}

void CDSPlayer::RenderToTexture(DS_RENDER_LAYER layer)
{
  if (m_pPaintCallback && ReadyDS())
    m_pPaintCallback->RenderToTexture(layer);
}

void CDSPlayer::EndRender()
{
  m_renderManager.EndRender();

  if (m_pPaintCallback && ReadyDS())
    m_pPaintCallback->EndRender();
}

void CDSPlayer::IncRenderCount()
{
  if (m_pPaintCallback && ReadyDS())
    m_pPaintCallback->IncRenderCount();
}

// IDSPlayer
bool CDSPlayer::Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags)
{
  m_fps = fps;
  UpdateProcessInfo();
  return m_renderManager.Configure(width, height, d_width, d_height, fps, flags);
}

bool CDSPlayer::UsingDS(DIRECTSHOW_RENDERER videoRenderer)
{
  if (videoRenderer == DIRECTSHOW_RENDERER_UNDEF)
    videoRenderer = m_CurrentVideoRenderer;

  return (m_pAllocatorCallback != NULL && m_CurrentVideoRenderer == videoRenderer);
}

bool CDSPlayer::ReadyDS(DIRECTSHOW_RENDERER videoRenderer)
{
  if (videoRenderer == DIRECTSHOW_RENDERER_UNDEF)
    videoRenderer = m_CurrentVideoRenderer;

  return (m_pAllocatorCallback != NULL && m_renderOnDs && m_CurrentVideoRenderer == videoRenderer);
}

CGraphManagementThread::CGraphManagementThread(CDSPlayer * pPlayer)
  : m_pPlayer(pPlayer), m_bSpeedChanged(false), CThread("CGraphManagementThread thread")
{
}

void CGraphManagementThread::OnStartup()
{
}

void CGraphManagementThread::Process()
{
  while (!this->m_bStop)
  {

    if (CDSPlayer::PlayerState == DSPLAYER_CLOSED)
      break;
    if (m_bSpeedChanged)
    {
      m_pPlayer->GetClock().SetSpeed(m_currentRate * 1000);
      m_clockStart = m_pPlayer->GetClock().GetClock();
      m_bSpeedChanged = false;

      if (m_currentRate == 1)
        g_dsGraph->Play();

    }
    if (CDSPlayer::PlayerState == DSPLAYER_CLOSED)
      break;
    // Handle Rewind or Fast Forward
    if (m_currentRate != 1)
    {
      if (g_dsGraph->SetSpeed(m_currentRate))
      {
        if (g_dsGraph->GetTime() >= g_dsGraph->GetTotalTime()
          || g_dsGraph->GetTotalTime() <= 0)
        {
          m_currentRate = 1;
          m_pPlayer->GetPlayerCallback().OnPlayBackSpeedChanged(1);
          m_bSpeedChanged = true;
        }
      }
      else
      {
        double clock = m_pPlayer->GetClock().GetClock() - m_clockStart; // Time elapsed since the rate change
                                                                        // Only seek if elapsed time is greater than 250 ms
        if (abs(DS_TIME_TO_MSEC(clock)) >= 250)
        {
          //CLog::Log(LOGDEBUG, "Seeking time : %f", DS_TIME_TO_MSEC(clock));

          // New position
          uint64_t newPos = g_dsGraph->GetTime() + (uint64_t)clock;
          //CLog::Log(LOGDEBUG, "New position : %f", DS_TIME_TO_SEC(newPos));

          // Check boundaries
          if (newPos <= 0)
          {
            newPos = 0;
            m_currentRate = 1;
            m_pPlayer->GetPlayerCallback().OnPlayBackSpeedChanged(1);
            m_bSpeedChanged = true;
          }
          else if (newPos >= g_dsGraph->GetTotalTime())
          {
            CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_STOP);
            break;
          }

          g_dsGraph->Seek(newPos);

          m_clockStart = m_pPlayer->GetClock().GetClock();
        }
      }
    }
    if (CDSPlayer::PlayerState == DSPLAYER_CLOSED)
      break;

    // Handle rare graph event
    g_dsGraph->HandleGraphEvent();

    // Update displayed time
    g_dsGraph->UpdateTime();

    Sleep(250);
    if (CDSPlayer::PlayerState == DSPLAYER_CLOSED)
      break;
  }
}

#endif
