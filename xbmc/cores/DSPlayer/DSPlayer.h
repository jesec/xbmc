#pragma once

/*
*      Copyright (C) 2005-2009 Team XBMC
*      http://www.xbmc.org
*
*	   Copyright (C) 2010-2013 Eduard Kytmanov
*	   http://www.avmedia.su
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

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "cores/IPlayer.h"
#include "threads/Thread.h"
#include "threads/SingleLock.h"
#include "DSGraph.h"
#include "DVDClock.h"
#include "url.h"
#include "StreamsManager.h"
#include "ChaptersManager.h"

#include "utils/TimeUtils.h"
#include "Threads/Event.h"
#include "dialogs/GUIDialogBoxBase.h"
#include "settings/Settings.h"

#include "pvr/PVRManager.h"
#include "Application.h"
#include "Videorenderers/RenderDSManager.h"
#include "cores/VideoPlayer/Process/ProcessInfo.h"

#if !defined(NPT_POINTER_TO_LONG)
#define NPT_POINTER_TO_LONG(_p) ((long)(_p))
#endif

#define START_PERFORMANCE_COUNTER { int64_t start = CurrentHostCounter();
#define END_PERFORMANCE_COUNTER(fn) int64_t end = CurrentHostCounter(); \
  CLog::Log(LOGINFO, "%s %s. Elapsed time: %.2fms", __FUNCTION__, fn, 1000.f * (end - start) / CurrentHostFrequency()); }

enum DSPLAYER_STATE
{
  DSPLAYER_LOADING,
  DSPLAYER_LOADED,
  DSPLAYER_PLAYING,
  DSPLAYER_PAUSED,
  DSPLAYER_STOPPED,
  DSPLAYER_CLOSING,
  DSPLAYER_CLOSED,
  DSPLAYER_ERROR
};

class CDSInputStreamPVRManager;
class CDSPlayer;
class CGraphManagementThread : public CThread
{
private:
  bool          m_bSpeedChanged;
  double        m_clockStart;
  double        m_currentRate;
  CDSPlayer*    m_pPlayer;
public:
  CGraphManagementThread(CDSPlayer * pPlayer);

  void SetSpeedChanged(bool value) { m_bSpeedChanged = value; }
  void SetCurrentRate(double rate) { m_currentRate = rate; }
  double GetCurrentRate() const { return m_currentRate; }
protected:
  void OnStartup();
  void Process();
};

class CDSPlayer : public IPlayer, public CThread, public IDispResource, public IRenderDSMsg
{
public:
  // IPlayer
  CDSPlayer(IPlayerCallback& callback);
  virtual ~CDSPlayer();
  virtual bool OpenFile(const CFileItem& file, const CPlayerOptions &options) override;
  virtual bool CloseFile(bool reopen = false) override;
  virtual bool IsPlaying() const override;
  virtual bool HasVideo() const override;
  virtual bool HasAudio() const override;
  virtual void Pause() override;
  virtual bool CanSeek()  override { return g_dsGraph->CanSeek(); }
  virtual void Seek(bool bPlus, bool bLargeStep, bool bChapterOverride) override;
  virtual void SeekPercentage(float iPercent) override;
  virtual float GetPercentage() override { return g_dsGraph->GetPercentage(); }
  virtual float GetCachePercentage() override { return g_dsGraph->GetCachePercentage(); }
  virtual void SetVolume(float nVolume)  override { g_dsGraph->SetVolume(nVolume); }
  virtual void SetMute(bool bOnOff)  override { if (bOnOff) g_dsGraph->SetVolume(VOLUME_MINIMUM); }
  virtual void SetAVDelay(float fValue = 0.0f) override;
  virtual float GetAVDelay() override;

  virtual void SetSubTitleDelay(float fValue = 0.0f) override;
  virtual float GetSubTitleDelay() override;
  virtual int  GetSubtitleCount() override;
  virtual int  GetSubtitle() override;
  virtual void GetSubtitleStreamInfo(int index, SPlayerSubtitleStreamInfo &info) override;
  virtual void SetSubtitle(int iStream) override;
  virtual bool GetSubtitleVisible() override;
  virtual void SetSubtitleVisible(bool bVisible) override;
  virtual void AddSubtitle(const std::string& strSubPath) override;

  virtual int  GetAudioStreamCount() override { return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetAudioStreamCount() : 0; }
  virtual int  GetAudioStream() override { return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetAudioStream() : 0; }
  virtual void SetAudioStream(int iStream) override;

  //virtual int GetVideoStream() {} const override;
  virtual int GetVideoStreamCount() const override { return 1; }
  virtual void GetVideoStreamInfo(int streamId, SPlayerVideoStreamInfo &info) override;
  //virtual void SetVideoStream(int iStream);

  virtual int  GetChapterCount() override { CSingleLock lock(m_StateSection); return CChaptersManager::Get()->GetChapterCount(); }
  virtual int  GetChapter() override { CSingleLock lock(m_StateSection); return CChaptersManager::Get()->GetChapter(); }
  virtual void GetChapterName(std::string& strChapterName, int chapterIdx = -1) override { CSingleLock lock(m_StateSection); CChaptersManager::Get()->GetChapterName(strChapterName, chapterIdx); }
  virtual int64_t GetChapterPos(int chapterIdx = -1) override { return CChaptersManager::Get()->GetChapterPos(chapterIdx); }
  virtual int  SeekChapter(int iChapter) override { return CChaptersManager::Get()->SeekChapter(iChapter); }

  virtual void SeekTime(__int64 iTime = 0) override;
  virtual bool SeekTimeRelative(__int64 iTime) override;
  virtual __int64 GetTime() override { CSingleLock lock(m_StateSection); return llrint(DS_TIME_TO_MSEC(g_dsGraph->GetTime())); }
  virtual __int64 GetTotalTime() override { CSingleLock lock(m_StateSection); return llrint(DS_TIME_TO_MSEC(g_dsGraph->GetTotalTime())); }
  virtual void SetSpeed(float iSpeed) override;
  virtual float GetSpeed() override;
  virtual bool SupportsTempo() override;
  virtual bool OnAction(const CAction &action) override;
  virtual bool HasMenu() const override { return g_dsGraph->IsDvd(); };
  bool IsInMenu() const override { return g_dsGraph->IsInMenu(); };
  virtual void GetAudioStreamInfo(int index, SPlayerAudioStreamInfo &info) override;

  virtual bool SwitchChannel(const PVR::CPVRChannelPtr &channel) override;

  // RenderManager
  virtual void FrameMove() override;
  virtual void Render(bool clear, uint32_t alpha = 255, bool gui = true) override;
  virtual void FlushRenderer() override;
  virtual void SetRenderViewMode(int mode) override;
  float GetRenderAspectRatio() override;
  virtual void TriggerUpdateResolution() override;
  virtual bool IsRenderingVideo() override;
  virtual bool IsRenderingGuiLayer() override;
  virtual bool IsRenderingVideoLayer() override;
  virtual bool Supports(EINTERLACEMETHOD method) override;
  virtual bool Supports(ESCALINGMETHOD method) override;
  virtual bool Supports(ERENDERFEATURE feature) override;
  
  virtual bool Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags) override;
  virtual void GetVideoRect(CRect &source, CRect &dest, CRect &view) override;

  // IDispResource interface
  virtual void OnLostDisplay();
  virtual void OnResetDisplay();

  virtual bool IsCaching() const override { return false; }

  //Editions selection
  virtual int  GetEditionsCount() override { return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetEditionsCount() : 0; }
  virtual int  GetEdition() override { return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetEdition() : 0; }
  virtual void GetEditionInfo(int iEdition, std::string &strEditionName, REFERENCE_TIME *prt) override { if (CStreamsManager::Get()) CStreamsManager::Get()->GetEditionInfo(iEdition, strEditionName, prt); };
  virtual void SetEdition(int iEdition) override { if (CStreamsManager::Get()) CStreamsManager::Get()->SetEdition(iEdition); };
  virtual bool IsMatroskaEditions() override { return (CStreamsManager::Get()) ? CStreamsManager::Get()->IsMatroskaEditions() : false; }

  //CDSPlayer
  CDVDClock&  GetClock() { return m_pClock; }
  IPlayerCallback& GetPlayerCallback() { return m_callback; }

  static DSPLAYER_STATE PlayerState;
  static CGUIDialogBoxBase* errorWindow;
  CFileItem m_currentFileItem;
  CPlayerOptions m_PlayerOptions;

  CCriticalSection m_StateSection;
  CCriticalSection m_CleanSection;

  int GetPictureWidth() { return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetPictureWidth() : 0; }
  int GetPictureHeight()  { return (CStreamsManager::Get()) ? CStreamsManager::Get()->GetPictureHeight() : 0; }

  void GetGeneralInfo(std::string& strGeneralInfo);

  void ShowEditionDlg(bool playStart);
  bool WaitForFileClose();
  bool OpenFileInternal(const CFileItem& file);
  void UpdateApplication();
  void UpdateChannelSwitchSettings();
  void LoadVideoSettings(const CFileItem& file);
  void SetPosition();

  void UpdateProcessInfo(int index = CURRENT_STREAM);
  void SetAudioCodeDelayInfo(int index = CURRENT_STREAM);
  
  //madVR Window
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static HWND m_hWnd;
  bool InitWindow(HWND &hWnd);
  void DeInitWindow();
  std::string m_className;
  HINSTANCE m_hInstance; 
  bool m_isMadvr;

  CProcessInfo *m_processInfo;

  static void PostMessage(CDSMsg *msg, bool wait = true)
  {
    if (!m_threadID || PlayerState == DSPLAYER_CLOSING || PlayerState == DSPLAYER_CLOSED)
    {
      msg->Release();
      return;
    }

    if (wait)
      msg->Acquire();

    CLog::Log(LOGDEBUG, "%s Message posted : %d on thread 0x%X", __FUNCTION__, msg->GetMessageType(), m_threadID);
    PostThreadMessage(m_threadID, WM_GRAPHMESSAGE, msg->GetMessageType(), (LPARAM)msg);

    if (wait)
    {
      msg->Wait();
      msg->Release();
    }
  }

  static bool IsCurrentThread() { return CThread::IsCurrentThread(m_threadID); }
  static HWND GetDShWnd(){ return m_hWnd; }

protected:
  void SetDSWndVisible(bool bVisible) override;
  void SetRenderOnDS(bool bRender) override;
  void VideoParamsChange() override { };
  void GetDebugInfo(std::string &audio, std::string &video, std::string &general) override;

  void StopThread(bool bWait = true)
  {
    if (m_threadID)
    {
      PostThreadMessage(m_threadID, WM_QUIT, 0, 0);
      m_threadID = 0;
    }
    CThread::StopThread(bWait);
  }

  void HandleMessages();

  bool ShowPVRChannelInfo();

  CGraphManagementThread m_pGraphThread;
  CDVDClock m_pClock;
  CEvent m_hReadyEvent;
  static ThreadIdentifier m_threadID;
  bool m_bEof;

  bool SelectChannel(bool bNext);
  bool SwitchChannel(unsigned int iChannelNumber);
  void LoadMadvrSettings(int id);

  // CThread
  virtual void OnStartup() override;
  virtual void OnExit() override;
  virtual void Process() override;

  bool m_HasVideo;
  bool m_HasAudio;

  std::atomic_bool m_canTempo;

  CRenderDSManager m_renderManager;

  float m_fps;
  CRect m_sourceRect;
  CRect m_videoRect;
  CRect m_viewRect;

  void SetVisibleScreenArea(CRect activeVideoRect);
  int VideoDimsToResolution(int iWidth, int iHeight);
  CRect m_lastActiveVideoRect;

  // IDSPlayer
  bool UsingDS(DIRECTSHOW_RENDERER renderer = DIRECTSHOW_RENDERER_UNDEF);
  bool ReadyDS(DIRECTSHOW_RENDERER renderer = DIRECTSHOW_RENDERER_UNDEF);
  void SetCurrentVideoLayer(DS_RENDER_LAYER layer) { m_currentVideoLayer = layer; }
  void IncRenderCount();
  void ResetRenderCount();
  bool GuiVisible(DS_RENDER_LAYER layer = RENDER_LAYER_ALL);
  DIRECTSHOW_RENDERER GetCurrentRenderer() { return m_CurrentRenderer; }
  void SetCurrentRenderer(DIRECTSHOW_RENDERER renderer) { m_CurrentRenderer = renderer; }

  // IDSRendererAllocatorCallback
  CRect GetActiveVideoRect();
  bool IsEnteringExclusive();
  void EnableExclusive(bool bEnable);
  void SetPixelShader();
  void SetResolution();
  void SetPosition(CRect sourceRect, CRect videoRect, CRect viewRect);
  bool ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM *wParam, LPARAM *lParam, LRESULT *ret);
  void Reset(bool bForceWindowed);
  void DisplayChange(bool bExternalChange);
  void Register(IDSRendererAllocatorCallback* pAllocatorCallback) { m_pAllocatorCallback = pAllocatorCallback; }
  void Unregister(IDSRendererAllocatorCallback* pAllocatorCallback) { m_pAllocatorCallback = nullptr; }

  // IDSRendererPaintCallback
  void BeginRender();
  void RenderToTexture(DS_RENDER_LAYER layer);
  void EndRender();
  void Register(IDSRendererPaintCallback* pPaintCallback) { m_pPaintCallback = pPaintCallback; }
  void Unregister(IDSRendererPaintCallback* pPaintCallback) { m_pPaintCallback = nullptr; }

  // IMadvrSettingCallback
  void LoadSettings(int iSectionId);
  void RestoreSettings();
  void GetProfileActiveName(const std::string &path, std::string *profile);
  void OnSettingChanged(int iSectionId, CSettingsManager* settingsManager, const CSetting *setting);
  void AddDependencies(const std::string &xml, CSettingsManager *settingsManager, CSetting *setting);
  void ListSettings(const std::string &path);
  void Register(IMadvrSettingCallback* pSettingCallback) { m_pSettingCallback = pSettingCallback; }
  void Unregister(IMadvrSettingCallback* pSettingCallback) { m_pSettingCallback = nullptr; }

  IDSRendererAllocatorCallback* m_pAllocatorCallback;
  IMadvrSettingCallback* m_pSettingCallback;
  IDSRendererPaintCallback* m_pPaintCallback;
  bool m_renderOnDs;
  bool m_bPerformStop;
  int m_renderUnderCount;
  int m_renderOverCount;
  DS_RENDER_LAYER m_currentVideoLayer;
  DIRECTSHOW_RENDERER m_CurrentRenderer;
};

