/*
 *      Copyright (C) 2005-2010 Team XBMC
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

#include "StreamsManager.h"
#include "ServiceBroker.h"
#include "filesystem/SpecialProtocol.h"
#include "utils/StreamDetails.h"
#include "filesystem/SpecialProtocol.h"
#include "DVDSubtitles/DVDFactorySubtitle.h"
#include "settings/Settings.h"
#include "filtercorefactory/filtercorefactory.h"
#include "DSPlayer.h"
#include "FGFilter.h"
#include "DSUtil/SmartPtr.h"
#include "windowing/WinSystem.h"
#include "LangInfo.h"
#include "settings/SettingsComponent.h"
#include "settings/MediaSettings.h"
#include "Filters/RendererSettings.h"
#include "Utils/URIUtils.h"
#include "utils/Color.h"
#include "utils/StringUtils.h"
#include "utils/DSFileUtils.h"
#include "guilib/GUIComponent.h"
#include "guilib/LocalizeStrings.h"
#include "LangInfo.h"
#include "DSPlayerDatabase.h"
#include "guilib/StereoscopicsManager.h"
#include "DSPropertyPage.h"

CDSStreamDetail::CDSStreamDetail()
  : IAMStreamSelect_Index(0), flags(0), pObj(NULL), pUnk(NULL), lcid(0),
  group(0), displayname(""), connected(false)
{
}

CDSStreamDetailAudio::CDSStreamDetailAudio()
  : m_strCodecName(""), m_iBitRate(0), m_iSampleRate(0)
{
}

CDSStreamDetailVideo::CDSStreamDetailVideo()
  : m_strCodecName("")
{
}

CDSStreamDetailSubtitle::CDSStreamDetailSubtitle(SubtitleType type)
  : m_subType(type), encoding(""), isolang(""), trackname(""), offset(0), subtype(GUID_NULL)
{
}

CDSStreamDetailSubfilter::CDSStreamDetailSubfilter(SubtitleType type)
  : m_subType(type), encoding(""), isolang(""), trackname(""), offset(0), subtype(GUID_NULL)
{
}

CDSStreamDetailSubtitleExternal::CDSStreamDetailSubtitleExternal()
  : CDSStreamDetailSubtitle(EXTERNAL), path(""), substream(NULL)
{
}

CStreamsManager *CStreamsManager::m_pSingleton = NULL;

CStreamsManager *CStreamsManager::Get()
{
  return m_pSingleton;
}

CStreamsManager::CStreamsManager(void)
  : m_pIAMStreamSelect(NULL)
  , m_init(false)
  , m_readyEvent(true)
  , m_dvdStreamLoaded(false)
  , m_mkveditions(false)
  , m_pIAMStreamSelectSub(NULL)
  , m_pIDirectVobSub(NULL)
{
  m_readyEvent.Set();
  m_lastDelay = 0;
}

CStreamsManager::~CStreamsManager(void)
{

  while (!m_audioStreams.empty())
  {
    delete m_audioStreams.back();
    m_audioStreams.pop_back();
  }
  while (!m_editionStreams.empty())
  {
    delete m_editionStreams.back();
    m_editionStreams.pop_back();
  }
  while (!m_subfilterStreams.empty())
  {
    delete m_subfilterStreams.back();
    m_subfilterStreams.pop_back();
  }

  m_pSplitter = NULL;
  m_pGraphBuilder = NULL;
  m_pSubs = NULL;

  CLog::Log(LOGDEBUG, "%s Ressources released", __FUNCTION__);

}

std::vector<CDSStreamDetailAudio *>& CStreamsManager::GetAudios()
{
  return m_audioStreams;
}

int CStreamsManager::GetAudioStreamCount()
{
  return m_audioStreams.size();
}

int CStreamsManager::GetAudioStream()
{
  if (m_audioStreams.size() == 0)
    return -1;

  int i = 0;
  for (std::vector<CDSStreamDetailAudio *>::const_iterator it = m_audioStreams.begin();
    it != m_audioStreams.end(); ++it, i++)
  {
    if ((*it)->flags & AMSTREAMSELECTINFO_ENABLED)
      return i;
  }

  return -1;
}

void CStreamsManager::GetAudioStreamName(int iStream, std::string &strStreamName)
{
  if (m_audioStreams.size() == 0)
    return;

  int i = 0;
  for (std::vector<CDSStreamDetailAudio *>::const_iterator it = m_audioStreams.begin();
    it != m_audioStreams.end(); ++it, i++)
  {
    if (i == iStream)
    {
      strStreamName = (*it)->displayname;
    }
  }
}

void CStreamsManager::GetVideoStreamName(std::string &strStreamName)
{
    strStreamName = m_videoStream.displayname;
}

void CStreamsManager::SetAudioStream(int iStream)
{
  if (!m_init || m_audioStreams.size() == 0)
    return;

  if (CGraphFilters::Get()->IsDVD())
    return; // currently not implemented

  CSingleLock lock(m_lock);

  long disableIndex = GetAudioStream(), enableIndex = iStream;

  if (disableIndex == enableIndex
    || !m_audioStreams[disableIndex]->connected
    || m_audioStreams[enableIndex]->connected)
    return;

  m_readyEvent.Reset();
  CAutoSetEvent event(&m_readyEvent);

  if (m_pIAMStreamSelect)
  {
    m_audioStreams[disableIndex]->connected = false;
    m_audioStreams[disableIndex]->flags = 0;

    if (SUCCEEDED(m_pIAMStreamSelect->Enable(m_audioStreams[enableIndex]->IAMStreamSelect_Index, AMSTREAMSELECTENABLE_ENABLE)))
    {
      m_audioStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
      m_audioStreams[enableIndex]->connected = true;
      CLog::Log(LOGDEBUG, "%s Successfully selected audio stream index: %i", __FUNCTION__, m_audioStreams[enableIndex]->IAMStreamSelect_Index);
    }
  }
  else {

    Com::SmartPtr<IPin> connectedToPin = NULL;
    HRESULT hr = S_OK;
#if 0
    std::string filter = "";
    SVideoStreamIndexes indexes(0, iStream, GetSubtitle());

    if (SUCCEEDED(CFilterCoreFactory::GetAudioFilter(CDSPlayer::currentFileItem, filter,
      CFGLoader::IsUsingDXVADecoder(), &indexes)))
    {
      CFGFilterFile *f;
      if (!(f = CFilterCoreFactory::GetFilterFromName(filter, false)))
      {
        CLog::Log(LOGERROR, "%s The filter corresponding to the new rule doesn't exist. Using current audio decoder to decode audio.", __FUNCTION__);
        goto standardway;
      }
      else
      {
        if (CFGLoader::Filters.Audio.guid == f->GetCLSID())
          goto standardway; // Same filter

        // Where this audio decoder is connected ?
        Com::SmartPtr<IPin> audioDecoderOutputPin = GetFirstPin(CFGLoader::Filters.Audio.pBF, PINDIR_OUTPUT); // first output pin of audio decoder
        Com::SmartPtr<IPin> audioDecoderConnectedToPin = NULL;
        hr = audioDecoderOutputPin->ConnectedTo(&audioDecoderConnectedToPin);

        if (!audioDecoderConnectedToPin)
          goto standardway;

        audioDecoderOutputPin = NULL;

        m_pGraph->Stop(); // Current position is kept by the graph

        // Remove the old decoder from the graph
        hr = m_pGraphBuilder->RemoveFilter(CFGLoader::Filters.Audio.pBF);
        if (SUCCEEDED(hr))
          CLog::Log(LOGDEBUG, "%s Removed old audio decoder from the graph", __FUNCTION__);

        // Delete filter
        m_audioStreams[disableIndex]->pUnk = NULL;
        m_audioStreams[enableIndex]->pUnk = NULL;
        CFGLoader::Filters.Audio.pBF.FullRelease();

        // Create the new audio decoder
        if (FAILED(f->Create(&CFGLoader::Filters.Audio.pBF)))
          goto standardway;

        // Change references
        CFGLoader::Filters.Audio.guid = f->GetCLSID();
        g_charsetConverter.wToUTF8(f->GetName(), CFGLoader::Filters.Audio.osdname);

        // Add the new one
        if (FAILED(hr = m_pGraphBuilder->AddFilter(CFGLoader::Filters.Audio.pBF, f->GetName())))
        {
          CLog::Log(LOGERROR, "%s Failed to add the new audio decoder. No more sound!", __FUNCTION__);
          goto done;
        }

        // Make connections
        m_audioStreams[enableIndex]->pUnk = GetFirstPin(CFGLoader::Filters.Audio.pBF);
        hr = m_pGraphBuilder->ConnectDirect(m_audioStreams[enableIndex]->pObj, m_audioStreams[enableIndex]->pUnk, NULL);

        Com::SmartPtr<IPin> pin = NULL;
        pin = GetFirstPin(CFGLoader::Filters.Audio.pBF, PINDIR_OUTPUT);
        hr = m_pGraphBuilder->ConnectDirect(pin, audioDecoderConnectedToPin, NULL);

        m_audioStreams[disableIndex]->flags = 0;
        m_audioStreams[disableIndex]->connected = false;

        m_audioStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
        m_audioStreams[enableIndex]->connected = true;

        CLog::Log(LOGINFO, "%s New audio decoder \"%s\" inserted because of rules configuration.", __FUNCTION__, CFGLoader::Filters.Audio.osdname.c_str());

        // Connections done, run the graph!
        goto done;
      }

    }
#endif

    /* Disable filter */
    //standardway:

    // Stop the graph
    g_dsGraph->Stop();

    IPin *newAudioStreamPin = m_audioStreams[enableIndex]->pObj; // Splitter pin
    IPin *oldAudioStreamPin = m_audioStreams[disableIndex]->pObj; // Splitter pin

    connectedToPin = m_audioStreams[disableIndex]->pUnk; // Audio dec pin
    if (!connectedToPin) // This case shouldn't happened
      hr = oldAudioStreamPin->ConnectedTo(&connectedToPin);

    if (FAILED(hr))
      goto done;

    m_audioStreams[enableIndex]->pUnk = connectedToPin;

    hr = m_pGraphBuilder->Disconnect(connectedToPin);
    if (FAILED(hr))
      goto done;
    hr = m_pGraphBuilder->Disconnect(oldAudioStreamPin);
    if (FAILED(hr))
    {
      /* Reconnect pin */
      m_pGraphBuilder->ConnectDirect(oldAudioStreamPin, connectedToPin, NULL);
      goto done;
    }

    m_audioStreams[disableIndex]->flags = 0;
    m_audioStreams[disableIndex]->connected = false;

    /* Enable filter */
    hr = m_pGraphBuilder->ConnectDirect(newAudioStreamPin, connectedToPin, NULL);
    if (FAILED(hr))
    {
      /* Reconnect previous working pins */
      m_pGraphBuilder->ConnectDirect(oldAudioStreamPin, connectedToPin, NULL);
      m_audioStreams[disableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
      m_audioStreams[disableIndex]->connected = true;
      goto done;
    }

    m_audioStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
    m_audioStreams[enableIndex]->connected = true;

  done:
    g_dsGraph->Play();

    if (SUCCEEDED(hr))
      CLog::Log(LOGNOTICE, "%s Successfully changed audio stream", __FUNCTION__);
    else
      CLog::Log(LOGERROR, "%s Can't change audio stream", __FUNCTION__);
  }
}

int CStreamsManager::GetEditionsCount()
{
  return m_editionStreams.size();
}

int CStreamsManager::GetEdition()
{
  if (m_editionStreams.size() == 0)
    return -1;

  int i = 0;
  for (std::vector<CDSStreamDetailEdition *>::const_iterator it = m_editionStreams.begin();
    it != m_editionStreams.end(); ++it, i++)
  {
    if ((*it)->flags & AMSTREAMSELECTINFO_ENABLED)
      return i;
  }

  return -1;
}

void CStreamsManager::GetEditionInfo(int iEdition, std::string &strEditionName, REFERENCE_TIME *prt)
{
  if (m_editionStreams.size() == 0)
    return;

  int i = 0;
  for (std::vector<CDSStreamDetailEdition *>::const_iterator it = m_editionStreams.begin();
    it != m_editionStreams.end(); ++it, i++)
  {
    if (i == iEdition)
    {

      strEditionName = (*it)->displayname.c_str();
      if (prt)*prt = (*it)->m_rtDuration;
    }
  }
}

void CStreamsManager::SetEdition(int iEdition)
{
  if (!m_init)
    return;

  if (CGraphFilters::Get()->IsDVD())
    return; // currently not implemented

  CSingleLock lock(m_lock);

  long disableIndex = GetEdition(), enableIndex = iEdition;

  if (disableIndex == enableIndex)
    return;

  m_readyEvent.Reset();
  CAutoSetEvent event(&m_readyEvent);

  if (m_pIAMStreamSelect)
  {
    m_editionStreams[disableIndex]->flags = 0;
    m_editionStreams[disableIndex]->connected = false;

    if (SUCCEEDED(m_pIAMStreamSelect->Enable(m_editionStreams[enableIndex]->IAMStreamSelect_Index, AMSTREAMSELECTENABLE_ENABLE)))
    {
      // Reload Chapters
      if (!CChaptersManager::Get()->LoadChapters())
        CLog::Log(LOGNOTICE, "%s No chapters found!", __FUNCTION__);

      // Update displayed time
      g_dsGraph->UpdateTotalTime();

      m_editionStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
      m_editionStreams[enableIndex]->connected = true;
      CLog::Log(LOGDEBUG, "%s Successfully selected edition - index: %i", __FUNCTION__, m_editionStreams[enableIndex]->IAMStreamSelect_Index);
    }
  }
}

void CStreamsManager::LoadIAMStreamSelectStreamsInternal()
{
  DWORD nStreams = 0, flags = 0, group = 0;
  WCHAR* wname = NULL;
  CStreamDetail* infos = NULL;
  LCID lcid;
  IUnknown *pObj = NULL, *pUnk = NULL;
  m_mkveditions = false;
  m_pIAMStreamSelect->Count(&nStreams);

  AM_MEDIA_TYPE * mediaType = NULL;
  for (unsigned int i = 0; i < nStreams; i++)
  {
    m_pIAMStreamSelect->Info(i, &mediaType, &flags, &lcid, &group, &wname, &pObj, &pUnk);

    switch (group)
    {
    case CStreamDetail::VIDEO:
      infos = &m_videoStream;
      break;
    case CStreamDetail::AUDIO:
      infos = new CDSStreamDetailAudio();
      break;
    case CStreamDetail::SUBTITLE:
    {
      if (m_bHasSubsFilter)
        infos = new CDSStreamDetailSubfilter(INTERNAL);
      else
        infos = new CDSStreamDetailSubtitle();
      break;
    }
    default: continue;
    }

    CDSStreamDetail& pS = dynamic_cast<CDSStreamDetail&> (*infos);

    pS.IAMStreamSelect_Index = i;

    g_charsetConverter.wToUTF8(wname, pS.displayname);
    CoTaskMemFree(wname);

    pS.flags = flags; pS.lcid = lcid; pS.group = group; pS.pObj = (IPin *)pObj; pS.pUnk = (IPin *)pUnk;
    if (flags & AMSTREAMSELECTINFO_ENABLED)
      pS.connected = true;

    if (!m_mkveditions)
      MediaTypeToStreamDetail(mediaType, (CStreamDetail&)(*infos));

    if (group == CStreamDetail::AUDIO)
    {
      /* Audio stream */
      if (pS.displayname.find("Undetermined") != std::string::npos)
        pS.displayname = StringUtils::Format("A: Audio %02d", i + 1);

      m_audioStreams.push_back(static_cast<CDSStreamDetailAudio *>(infos));
      CLog::Log(LOGNOTICE, "%s Audio stream found : %s - index: %i", __FUNCTION__, pS.displayname.c_str(), pS.IAMStreamSelect_Index);
    }
    else if (group == CStreamDetail::SUBTITLE)
    {
      if (m_bHasSubsFilter)
        m_subfilterStreams.push_back(static_cast<CDSStreamDetailSubfilter *>(infos));
      else
        SubtitleManager->GetSubtitles().push_back(static_cast<CDSStreamDetailSubtitle *>(infos));
      CLog::Log(LOGNOTICE, "%s Subtitle stream found : %s - index: %i", __FUNCTION__, pS.displayname.c_str(), pS.IAMStreamSelect_Index);
    }

    DeleteMediaType(mediaType);
  }
}

void CStreamsManager::LoadStreamsInternal()
{
  // Enumerate output pins
  PIN_DIRECTION dir;
  int i = 0, j = 0;
  std::string pinName, guid = "";
  CStreamDetail *infos = NULL;
  bool audioPinAlreadyConnected = false;//, subtitlePinAlreadyConnected = FALSE;

  // If we're playing a DVD, the bottom method doesn't work
  if (CGraphFilters::Get()->IsDVD())
  { // The playback need to be started in order to get informations
    return;
  }

  int nIn = 0, nOut = 0, nInC = 0, nOutC = 0;
  CountPins(m_pSplitter, nIn, nOut, nInC, nOutC);
  CLog::Log(LOGDEBUG, "%s The splitter has %d output pins", __FUNCTION__, nOut);

  BeginEnumPins(m_pSplitter, pEP, pPin)
  {
    if (SUCCEEDED(pPin->QueryDirection(&dir)) && (dir == PINDIR_OUTPUT))
    {
      g_charsetConverter.wToUTF8(GetPinName(pPin), pinName);
      CLog::Log(LOGDEBUG, "%s Output pin found : %s", __FUNCTION__, pinName.c_str());

      BeginEnumMediaTypes(pPin, pET, pMediaType)
      {

        g_charsetConverter.wToUTF8(StringFromGUID(pMediaType->majortype), guid);
        CLog::Log(LOGDEBUG, "%s \tOutput pin major type : %s %s", __FUNCTION__, GuidNames[pMediaType->majortype], guid.c_str());

        g_charsetConverter.wToUTF8(StringFromGUID(pMediaType->subtype), guid);
        CLog::Log(LOGDEBUG, "%s \tOutput pin sub type : %s %s", __FUNCTION__, GuidNames[pMediaType->subtype], guid.c_str());

        g_charsetConverter.wToUTF8(StringFromGUID(pMediaType->formattype), guid);
        CLog::Log(LOGDEBUG, "%s \tOutput pin format type : %s %s", __FUNCTION__, GuidNames[pMediaType->formattype], guid.c_str());

        if (pMediaType->majortype == MEDIATYPE_Video)
          infos = &m_videoStream;
        else if (pMediaType->majortype == MEDIATYPE_Audio)
          infos = new CDSStreamDetailAudio();
        else if (pMediaType->majortype == MEDIATYPE_Subtitle)
          infos = new CDSStreamDetailSubtitle();
        else
          continue;

        CDSStreamDetail& pS = dynamic_cast<CDSStreamDetail&>(*infos);

        if (pS.displayname.empty())
          pS.displayname = pinName;

        MediaTypeToStreamDetail(pMediaType, *infos);

        pS.pObj = pPin;
        pPin->ConnectedTo(&pS.pUnk);

        if (pMediaType->majortype == MEDIATYPE_Audio)
        {
          if (pS.displayname.find("Undetermined") != std::string::npos)
            pS.displayname = StringUtils::Format("Audio %02d", i + 1);

          if (i == 0)
            pS.flags = AMSTREAMSELECTINFO_ENABLED;
          if (pS.pUnk)
            pS.connected = true;

          m_audioStreams.push_back(static_cast<CDSStreamDetailAudio *>(infos));
          CLog::Log(LOGNOTICE, "%s Audio stream found : %s", __FUNCTION__, pS.displayname.c_str());
          i++;

        }
        else if (pMediaType->majortype == MEDIATYPE_Subtitle)
        {
          if (pS.displayname.find("Undetermined") != std::string::npos)
            pS.displayname = StringUtils::Format("Subtitle %02d", j + 1);

          if (j == 0)
            pS.flags = AMSTREAMSELECTINFO_ENABLED;
          if (pS.pUnk)
            pS.connected = true;

          SubtitleManager->GetSubtitles().push_back(static_cast<CDSStreamDetailSubtitle *>(infos));
          CLog::Log(LOGNOTICE, "%s Subtitle stream found : %s", __FUNCTION__, pS.displayname.c_str());
          j++;
        }
        else if (pMediaType->majortype == MEDIATYPE_Video)
        {

        }

        break; // if the pin has multiple output type, get only the first one
      }
      EndEnumMediaTypes(pMediaType)
    }
  }
  EndEnumPins
}

void CStreamsManager::LoadStreams()
{
  if (!m_init)
    return;

  std::string splitterName;
  g_charsetConverter.wToUTF8(GetFilterName(m_pSplitter), splitterName);

  CLog::Log(LOGDEBUG, "%s Looking for streams in %s splitter", __FUNCTION__, splitterName.c_str());

  // Does the splitter support IAMStreamSelect?
  m_pIAMStreamSelect = NULL;
  HRESULT hr = m_pSplitter->QueryInterface(__uuidof(m_pIAMStreamSelect), (void **)&m_pIAMStreamSelect);
  if (SUCCEEDED(hr) && !(CGraphFilters::Get()->UsingMediaPortalTsReader()))
  {
    CLog::Log(LOGDEBUG, "%s Get IAMStreamSelect interface from %s", __FUNCTION__, splitterName.c_str());
    LoadIAMStreamSelectStreamsInternal();
  }
  else
    LoadStreamsInternal();

  if (m_bHasSubsFilter)
  {
    std::string subName;
    g_charsetConverter.wToUTF8(GetFilterName(m_pSubs), subName);
    m_pIAMStreamSelectSub = NULL;

    HRESULT hr = m_pSubs->QueryInterface(__uuidof(m_pIAMStreamSelectSub), (void **)&m_pIAMStreamSelectSub);
    if (SUCCEEDED(hr))
    {
      CLog::Log(LOGDEBUG, "%s Get IAMStreamSelect interface from %s", __FUNCTION__, subName.c_str());

      HRESULT hr = m_pSubs->QueryInterface(__uuidof(m_pIDirectVobSub), (void **)&m_pIDirectVobSub);
      if (SUCCEEDED(hr))
        m_bIsXYVSFilter = true;

      m_subfilterStreams_int = m_subfilterStreams;
      m_pIDirectVobSub->put_LoadSettings(1, true, false, true);
      m_InitialSubsDelay = GetSubTitleDelay();

      Com::SmartQIPtr<IDSPlayerCustom> pDPlayerCustom;
      hr = m_pSubs->QueryInterface(__uuidof(pDPlayerCustom), (void **)&pDPlayerCustom);
      if (SUCCEEDED(hr))
      {
        pDPlayerCustom->SetPropertyPageCallback(CDSPropertyPage::PropertyPageCallback);
      }

      SubInterface(ADD_EXTERNAL_SUB);
    }
  }

  SubtitleManager->Initialize();
  if (!SubtitleManager->Ready())
  {
    SubtitleManager->Unload();
    return;
  }

  /* We're done, internal audio & subtitles stream are loaded.
     We load external subtitle file */

  std::vector<std::string> subtitles;
  CUtil::ScanForExternalSubtitles(g_application.CurrentFile(), subtitles);

  for (std::vector<std::string>::const_iterator it = subtitles.begin(); it != subtitles.end(); ++it)
  {
    SubtitleManager->AddSubtitle(*it);
  }
  SubtitleManager->SelectBestSubtitle();

  SubtitleManager->SetSubtitleVisible(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn);
}

int CStreamsManager::GetSubtitle()
{
  if (!m_bHasSubsFilter)
  {
    return SubtitleManager->GetSubtitle();
  }

  if (m_subfilterStreams.size() == 0)
    return -1;

  int i = 0;
  for (std::vector<CDSStreamDetailSubfilter *>::const_iterator it = m_subfilterStreams.begin();
    it != m_subfilterStreams.end(); ++it, i++)
  {
    if ((*it)->flags & AMSTREAMSELECTINFO_ENABLED)
      return i;
  }
  return -1;
}

int CStreamsManager::GetSubtitleCount()
{
  if (!m_bHasSubsFilter)
  {
    return SubtitleManager->GetSubtitleCount();
  }

  return m_subfilterStreams.size();
}

void CStreamsManager::GetSubtitleName(int iStream, std::string &strStreamName)
{
  if (!m_bHasSubsFilter)
  {
    SubtitleManager->GetSubtitleName(iStream, strStreamName);
    return;
  }


  if (m_subfilterStreams.size() == 0)
    return;

  int i = 0;
  for (std::vector<CDSStreamDetailSubfilter *>::const_iterator it = m_subfilterStreams.begin();
    it != m_subfilterStreams.end(); ++it, i++)
  {
    if (i == iStream)
    {
      strStreamName = (*it)->displayname;
    }
  }
}

bool CStreamsManager::GetSubtitleVisible()
{
  if (!m_bHasSubsFilter)
  {
    return SubtitleManager->GetSubtitleVisible();
  }

  return m_bSubfilterVisible;
}

void CStreamsManager::SetSubtitleVisible(bool bVisible)
{
  if (!m_bHasSubsFilter)
  {
    SubtitleManager->SetSubtitleVisible(bVisible);
    return;
  }

  CVideoSettings vs = g_application.GetAppPlayer().GetVideoSettings();
  vs.m_SubtitleOn = bVisible;
  g_application.GetAppPlayer().SetVideoSettings(vs);
  m_bSubfilterVisible = bVisible;

  if (!m_bIsXYVSFilter)
    return;

  m_pIDirectVobSub->put_HideSubtitles(!m_bSubfilterVisible);
}

void CStreamsManager::resetDelayInterface()
{
  int iValueAudio = m_InitialAudioDelay * 1000;
  BOOL bValue = iValueAudio != 0;
  int iValueSubs = m_InitialSubsDelay * 1000;

  if (m_bIsXYVSFilter)
    m_pIDirectVobSub->put_SubtitleTiming(iValueSubs, 1000, 1000);

  if (m_bIsLavAudio)
    m_pILAVAudioSettings->SetAudioDelay(bValue, iValueAudio);
}

void CStreamsManager::SetAudioInterface()
{
  Com::SmartPtr<IBaseFilter> pAudio;
  pAudio = CGraphFilters::Get()->Audio.pBF;

  if (!pAudio)
    return;

  std::string audioName;
  HRESULT hraudio;
  g_charsetConverter.wToUTF8(GetFilterName(pAudio), audioName);

  hraudio = pAudio->QueryInterface(__uuidof(m_pILAVAudioSettings), (void **)&m_pILAVAudioSettings);
  if (SUCCEEDED(hraudio))
  {
    CLog::Log(LOGDEBUG, "%s Get LAVAudio Settings interface from %s", __FUNCTION__, audioName.c_str());
    m_bIsLavAudio = true;
  }

  m_InitialAudioDelay = GetAVDelay();
}

DWORD CStreamsManager::GetHWAccel()
{
  Com::SmartQIPtr<ILAVVideoSettings> pLAVVideoSettings = CGraphFilters::Get()->Video.pBF;
  if (!pLAVVideoSettings)
    return 0;

  return pLAVVideoSettings->GetHWAccel();
}

void CStreamsManager::SetAVDelay(float fValue, int iDisplayerLatency)
{
  //delay float secs to int msecs
  int iValue = round(fValue * 1000.0f);

  //invert the sign displaylatency because kodi interface
  iDisplayerLatency = -iDisplayerLatency;
  iValue = iValue + iDisplayerLatency;

  //get delay and invert the sign because kodi interface
  iValue = -iValue;
  BOOL bValue = iValue != 0;

  if (m_bIsLavAudio)
    m_pILAVAudioSettings->SetAudioDelay(bValue, iValue);

  m_lastDelay = iValue;
}

float CStreamsManager::GetAVDelay()
{
  float fValue = 0.0f;
  int iValue;
  BOOL bValue;

  if (!m_bIsLavAudio)
    return 0;

  if (m_bIsLavAudio)
    m_pILAVAudioSettings->GetAudioDelay(&bValue, &iValue);

  fValue = (float)iValue / 1000.0f;

  return -fValue;
}

void CStreamsManager::SetSubTitleDelay(float fValue)
{
  if (!m_bHasSubsFilter)
  {
    SubtitleManager->SetSubtitleDelay(fValue);
    return;
  }

  int iValue = round(fValue * 1000.0f);

  //get delay and invert the sign because kodi interface
  iValue = -iValue;

  if (m_bIsXYVSFilter)
    m_pIDirectVobSub->put_SubtitleTiming(iValue, 1000, 1000);
}

float CStreamsManager::GetSubTitleDelay()
{
  if (!m_bHasSubsFilter)
  {
    return SubtitleManager->GetSubtitleDelay();
  }

  float fValue = 0.0f;
  int iValue, a, b;

  if (!m_bIsXYVSFilter)
    return 0;

  if (m_bIsXYVSFilter)
    m_pIDirectVobSub->get_SubtitleTiming(&iValue, &a, &b);

  fValue = (float)iValue / 1000.0f;
  return fValue;
}

int CStreamsManager::AddSubtitle(const std::string& subFilePath)
{
  if (!m_bHasSubsFilter)
  {
    return SubtitleManager->AddSubtitle(subFilePath);
  }

  if (!m_bIsXYVSFilter)
    return -1;

  std::wstring subFileW;
  std::string subFile;

  subFile = CDSFile::SmbToUncPath(subFilePath);

  if (StringUtils::StartsWithNoCase(subFile, "special://"))
    subFile = CSpecialProtocol::TranslatePath(subFile);

  g_charsetConverter.utf8ToW(subFile, subFileW);

  CLog::Log(LOGDEBUG, "%s Successfully loaded external subtitle name  \"%s\" ", __FUNCTION__, subFile.c_str());

  m_pIDirectVobSub->put_FileName(const_cast<wchar_t*>(subFileW.c_str()));

  m_subfilterStreams = m_subfilterStreams_int;

  SubInterface(ADD_EXTERNAL_SUB);

  return GetSubtitleCount() - 1;
}

bool CStreamsManager::SetSubtitle(const std::string &sTrackName)
{
  if (!m_init || sTrackName.empty())
    return false;

  CSingleLock lock(m_lock);

  if (GetSubtitleCount() <= 0)
    return false;

  int iStream = 0;
  for (const auto &sub : m_subfilterStreams)
  {
    if (sub->displayname == sTrackName)
    {
      SetSubtitle(iStream);
      return true;
    }
    iStream++;
  }

  return false;
}

void CStreamsManager::SetSubtitle(int iStream)
{
  if (!m_bHasSubsFilter)
  {
    SubtitleManager->SetSubtitle(iStream);
    return;
  }

  if (!m_init)
    return;

  CSingleLock lock(m_lock);

  if (GetSubtitleCount() <= 0)
    return;

  if (iStream > GetSubtitleCount()-1)
    return;

  long disableIndex = GetSubtitle(), enableIndex = iStream;

  m_readyEvent.Reset();
  CAutoSetEvent event(&m_readyEvent);

  CVideoSettings vs = g_application.GetAppPlayer().GetVideoSettings();
  vs.m_SubtitleStream = enableIndex;
  g_application.GetAppPlayer().SetVideoSettings(vs);

  if (m_subfilterStreams[enableIndex]->m_subType == EXTERNAL)
  {    
    g_application.GetAppPlayer().GetVideoSettings().m_SubtitleExtTrackName = m_subfilterStreams[enableIndex]->displayname;
    
    DisconnectCurrentSubtitlePins();

    CDSStreamDetailSubfilter *s = reinterpret_cast<CDSStreamDetailSubfilter *>(m_subfilterStreams[enableIndex]);

    m_pIAMStreamSelectSub->Enable(s->IAMStreamSelect_Index, AMSTREAMSELECTENABLE_ENABLE);
    s->flags = AMSTREAMSELECTINFO_ENABLED; // for gui
    s->connected = true;
  }

  else if (m_pIAMStreamSelect)
  {
    g_application.GetAppPlayer().GetVideoSettings().m_SubtitleExtTrackName = "";

    if (disableIndex >= 0 && m_subfilterStreams[disableIndex]->connected)
      DisconnectCurrentSubtitlePins();

    SubInterface(SELECT_INTERNAL_SUB);

    if (SUCCEEDED(m_pIAMStreamSelect->Enable(m_subfilterStreams[enableIndex]->IAMStreamSelect_Index, AMSTREAMSELECTENABLE_ENABLE)))
    {
      m_subfilterStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
      m_subfilterStreams[enableIndex]->connected = true;
    }
  }
  CLog::Log(LOGDEBUG, "%s Successfully selected subfilter stream number %i", __FUNCTION__, enableIndex);
  SetSubtitleVisible(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn);
}

void CStreamsManager::SubInterface(SelectSubType action)
{
  if (!m_bIsXYVSFilter)
    return;

  DWORD nCount;
  DWORD group;
  if (FAILED(m_pIAMStreamSelectSub->Count(&nCount))) { nCount = 0; }
  for (DWORD i = 0; i < nCount; i++)
  {
    WCHAR* pszName = nullptr;
    if (SUCCEEDED(m_pIAMStreamSelectSub->Info(i, nullptr, nullptr, nullptr, &group, &pszName, nullptr, nullptr)))
    {
      std::string fileName;
      std::string langName;
      g_charsetConverter.wToUTF8(pszName, fileName);
      if (action == ADD_EXTERNAL_SUB)
      {
        if (group == XYVSFILTER_SUB_EXTERNAL)
        {
          std::auto_ptr<CDSStreamDetailSubfilter> s(new CDSStreamDetailSubfilter(EXTERNAL));

          langName = ISOToLanguage(fileName);
          if (!langName.empty() && !fileName.empty())
            s->displayname = StringUtils::Format("%s (%s)", langName.c_str(), fileName.c_str());
          else if (!fileName.empty())
            s->displayname = fileName;
          else
            s->displayname = "Unknown";

          s->displayname += " " + g_localizeStrings.Get(21602);

          s->isolang = fileName;
          s->IAMStreamSelect_Index = i;
          m_subfilterStreams.push_back(s.release());
          CLog::Log(LOGNOTICE, "%s Successfully loaded external subtitle name  \"%s\" ", __FUNCTION__, fileName.c_str());
        }
      }
      if (action == SELECT_INTERNAL_SUB)
      {
        if (group == XYVSFILTER_SUB_INTERNAL)
          m_pIAMStreamSelectSub->Enable(i, AMSTREAMSELECTENABLE_ENABLE);
      }
    }
    CoTaskMemFree(pszName);
  }
}

void CStreamsManager::SelectBestAudio()
{
  std::string sPrefCodec = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_PREFAUDIOCODEC);
  int iLibrary = g_application.GetAppPlayer().GetVideoSettings().m_AudioStream;
  if ((iLibrary < GetAudioStreamCount()) && !(iLibrary < 0))
  {
    SetAudioStream(iLibrary);
  }
  else if (!m_audioStreams.empty() && !sPrefCodec.empty())
  {
    std::vector<std::string> codecs = StringUtils::Split(sPrefCodec, "|");

    std::string sPrefLang = g_langInfo.GetLocale().GetLanguageCode();

    int i = 0;
    for (const auto &it : m_audioStreams)
    {     
      char buffer[64];
      std::string audioLang;
      std::string audioCodec = it->m_strCodecName;
      StringUtils::ToLower(audioCodec);

      if (it->lcid)
      {
        int len = 0;
        if (len = GetLocaleInfoA(it->lcid, LOCALE_SISO639LANGNAME, buffer, 64))
        {
          audioLang = buffer;
          audioLang.resize(len - 1); //get rid of last \0
        }
        
      }
      
      for (const auto &codec : codecs)
      {
        if (audioCodec.find(codec) != std::string::npos && audioLang == sPrefLang)
        {
          SetAudioStream(i);
          return;
        }
      }

      i++;
    }
  }
}

void CStreamsManager::SelectBestSubtitle(const std::string &fileName /* ="" */)
{
  if (!m_bHasSubsFilter)
  {
    SubtitleManager->SelectBestSubtitle();
    return;
  }

  int selectFirst = -1;
  int select = -1;
  int iLibrary;
  std::string exSubTrackName = "";

  CDSPlayerDatabase db;
  if (db.Open())
  {
    exSubTrackName = db.GetSubtitleExtTrackName(fileName);
    db.Close();
    if (!exSubTrackName.empty())
    {
      if (SetSubtitle(exSubTrackName))
        return;
    }
  }

  // set previuos selected stream
  iLibrary = g_application.GetAppPlayer().GetVideoSettings().m_SubtitleStream;
  if ((iLibrary < g_application.GetAppPlayer().GetSubtitleCount()) && !(iLibrary < 0))
    select = iLibrary;
    
  // set prefered external sub or first external sub
  else if (!m_subfilterStreams.empty())
  {
    int i = 0;
    for (std::vector<CDSStreamDetailSubfilter *>::const_iterator it = m_subfilterStreams.begin();
      it != m_subfilterStreams.end(); ++it, i++)
    {
      if ((*it)->m_subType & EXTERNAL)
      {
        if (selectFirst == -1)
          selectFirst = i;

        if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_EXSUBTITLELANGUAGE) != "original")
        {

          std::string sPref;
          if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_EXSUBTITLELANGUAGE) == "default")
          {
            sPref = g_langInfo.GetLocale().GetLanguageCode();
            sPref = ProbeLangForLanguage(sPref.c_str());
          }
          else
            sPref = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_EXSUBTITLELANGUAGE);

          std::string sName = ProbeLangForLanguage((*it)->isolang.c_str());
          if (StringUtils::EqualsNoCase(sName, sPref))
          {
            select = i;
            break;
          }
        }
      }
      if (select == -1)
        select = selectFirst;
    }
  }

  if (select != -1)
    SetSubtitle(select);
}

void CStreamsManager::DisconnectCurrentSubtitlePins()
{
  int i = GetSubtitle();
  if (i == -1)
    return;

  m_subfilterStreams[i]->connected = false;
  m_subfilterStreams[i]->flags = 0;
}


bool CStreamsManager::InitManager()
{
  if (!g_dsGraph)
    return false;

  m_pSplitter = CGraphFilters::Get()->Splitter.pBF;  
  m_pSubs = CGraphFilters::Get()->Subs.pBF;
  m_pGraphBuilder = g_dsGraph->pFilterGraph;

  // Create subtitle manager
  SubtitleManager.reset(new CSubtitleManager(this));

  m_bHasSubsFilter = m_pSubs != nullptr;
  m_init = true;
  m_bIsXYVSFilter = false;
  m_bIsLavAudio = false;
  m_InitialAudioDelay = 0.0f;
  m_InitialSubsDelay = 0.0f;

  return true;
}

bool CStreamsManager::Create()
{
  if (CDSPlayer::PlayerState == DSPLAYER_CLOSING || CDSPlayer::PlayerState == DSPLAYER_CLOSED)
    return false;

  if ((m_pSingleton = new CStreamsManager()))
    return true;

  return false;
}

void CStreamsManager::Destroy()
{
  SAFE_DELETE(m_pSingleton);
}

void CStreamsManager::WaitUntilReady()
{
  // If the event has not been Reset(), Wait() will return immediately
  m_readyEvent.Wait();
}

int CStreamsManager::GetChannels()
{
  int i = GetAudioStream();
  return (i == -1) ? 0 : m_audioStreams[i]->m_iChannels;
}

int CStreamsManager::GetChannels(int istream)
{
  return (istream == -1) ? 0 : m_audioStreams[istream]->m_iChannels;
}

int CStreamsManager::GetBitsPerSample(int istream)
{
  return (istream == -1) ? 0 : m_audioStreams[istream]->m_iBitRate;
}

int CStreamsManager::GetSampleRate(int istream)
{
  return (istream == -1) ? 0 : m_audioStreams[istream]->m_iSampleRate;
}

void CStreamsManager::ExtractCodecDetail(CStreamDetail& s, std::string& codecInfos)
{
  if (s.m_eType == CStreamDetail::SUBTITLE)
    return;

  std::vector<std::string> tokens;
  StringUtils::Tokenize(codecInfos, tokens, "|");

  if (tokens.empty() || tokens.size() != 2)
  {
    switch (s.m_eType)
    {
    case CStreamDetail::AUDIO:
    {
      CDSStreamDetailAudio& pS = reinterpret_cast<CDSStreamDetailAudio &>(s);
      pS.m_strCodec = pS.m_strCodecName = codecInfos;
      return;
    }
    case CStreamDetail::VIDEO:
    {
      CDSStreamDetailVideo& pS = reinterpret_cast<CDSStreamDetailVideo &>(s);
      pS.m_strCodec = pS.m_strCodecName = codecInfos;
      return;
    }
    }
  }

  switch (s.m_eType)
  {
  case CStreamDetail::AUDIO:
  {
    CDSStreamDetailAudio& pS = reinterpret_cast<CDSStreamDetailAudio &>(s);
    pS.m_strCodecName = tokens[0];
    pS.m_strCodec = tokens[1];
    return;
  }
  case CStreamDetail::VIDEO:
  {
    CDSStreamDetailVideo& pS = reinterpret_cast<CDSStreamDetailVideo &>(s);
    pS.m_strCodecName = tokens[0];
    pS.m_strCodec = tokens[1];
    return;
  }
  }
}

void CStreamsManager::MediaTypeToStreamDetail(AM_MEDIA_TYPE *pMediaType, CStreamDetail& s)
{
  if (pMediaType->majortype == MEDIATYPE_Audio)
  {
    CDSStreamDetailAudio& infos = static_cast<CDSStreamDetailAudio&>(s);
    if (pMediaType->formattype == FORMAT_WaveFormatEx)
    {
      if (pMediaType->cbFormat >= sizeof(WAVEFORMATEX))
      {
        WAVEFORMATEX *f = reinterpret_cast<WAVEFORMATEX *>(pMediaType->pbFormat);
        infos.m_iChannels = f->nChannels;
        infos.m_iSampleRate = f->nSamplesPerSec;
        infos.m_iBitRate = f->wBitsPerSample;
        ExtractCodecDetail(infos, CMediaTypeEx::GetAudioCodecName(pMediaType->subtype, f->wFormatTag));
      }
    }
    else if (pMediaType->formattype == FORMAT_VorbisFormat2)
    {
      if (pMediaType->cbFormat >= sizeof(VORBISFORMAT2))
      {
        VORBISFORMAT2 *v = reinterpret_cast<VORBISFORMAT2 *>(pMediaType->pbFormat);
        infos.m_iChannels = v->Channels;
        infos.m_iSampleRate = v->SamplesPerSec;
        infos.m_iBitRate = v->BitsPerSample;
        ExtractCodecDetail(infos, CMediaTypeEx::GetAudioCodecName(pMediaType->subtype, 0));
      }
    }
    else if (pMediaType->formattype == FORMAT_VorbisFormat)
    {
      if (pMediaType->cbFormat >= sizeof(VORBISFORMAT))
      {
        VORBISFORMAT *v = reinterpret_cast<VORBISFORMAT *>(pMediaType->pbFormat);
        infos.m_iChannels = v->nChannels;
        infos.m_iSampleRate = v->nSamplesPerSec;
        infos.m_iBitRate = v->nAvgBitsPerSec;
        ExtractCodecDetail(infos, CMediaTypeEx::GetAudioCodecName(pMediaType->subtype, 0));
      }
    }
  }
  else if (pMediaType->majortype == MEDIATYPE_Video)
  {
    CDSStreamDetailVideo& infos = static_cast<CDSStreamDetailVideo&>(s);

    // GET STEREO FLAG FROM KODI
    CFileItem fileItem = g_application.CurrentFileItem();
    if (fileItem.HasVideoInfoTag() && fileItem.GetVideoInfoTag()->HasStreamDetails())
    {
      infos.m_strStereoMode = fileItem.GetVideoInfoTag()->m_streamDetails.GetStereoMode();
    }
    if (infos.m_strStereoMode.empty())
      infos.m_strStereoMode = CServiceBroker::GetGUI()->GetStereoscopicsManager().DetectStereoModeByString(g_application.CurrentFileItem().GetPath());

    if (pMediaType->formattype == FORMAT_VideoInfo)
    {
      if (pMediaType->cbFormat >= sizeof(VIDEOINFOHEADER))
      {
        VIDEOINFOHEADER *v = reinterpret_cast<VIDEOINFOHEADER *>(pMediaType->pbFormat);
        infos.m_iWidth = v->bmiHeader.biWidth;
        infos.m_iHeight = v->bmiHeader.biHeight;
        v->AvgTimePerFrame > 0 ? infos.m_fps = (10000000.0 / v->AvgTimePerFrame) : infos.m_fps = 0.0f;
        ExtractCodecDetail(infos, CMediaTypeEx::GetVideoCodecName(pMediaType->subtype, v->bmiHeader.biCompression, &infos.m_iFourcc));
      }
    }
    else if (pMediaType->formattype == FORMAT_MPEG2Video)
    {
      if (pMediaType->cbFormat >= (sizeof(MPEG2VIDEOINFO) - 4))
      {
        MPEG2VIDEOINFO *m = reinterpret_cast<MPEG2VIDEOINFO *>(pMediaType->pbFormat);
        infos.m_iWidth = m->hdr.bmiHeader.biWidth;
        infos.m_iHeight = m->hdr.bmiHeader.biHeight;
        m->hdr.AvgTimePerFrame > 0 ? infos.m_fps = (10000000.0 / m->hdr.AvgTimePerFrame) : infos.m_fps = 0.0f;
        ExtractCodecDetail(infos, CMediaTypeEx::GetVideoCodecName(pMediaType->subtype, m->hdr.bmiHeader.biCompression, &infos.m_iFourcc));
        if (infos.m_iFourcc == 0)
        {
          infos.m_iFourcc = 'MPG2';
          infos.m_strCodecName = "MPEG2 Video";
          infos.m_strCodec = "mpeg2";
        }
      }
    }
    else if (pMediaType->formattype == FORMAT_VideoInfo2)
    {
      if (pMediaType->cbFormat >= sizeof(VIDEOINFOHEADER2))
      {
        VIDEOINFOHEADER2 *v = reinterpret_cast<VIDEOINFOHEADER2 *>(pMediaType->pbFormat);
        infos.m_iWidth = v->bmiHeader.biWidth;
        infos.m_iHeight = v->bmiHeader.biHeight;
        v->AvgTimePerFrame > 0 ? infos.m_fps = (10000000.0 / v->AvgTimePerFrame) : infos.m_fps = 0.0f;
        ExtractCodecDetail(infos, CMediaTypeEx::GetVideoCodecName(pMediaType->subtype, v->bmiHeader.biCompression, &infos.m_iFourcc));
      }
    }
    else if (pMediaType->formattype == FORMAT_MPEGVideo)
    {
      if (pMediaType->cbFormat >= sizeof(MPEG1VIDEOINFO))
      {
        MPEG1VIDEOINFO *m = reinterpret_cast<MPEG1VIDEOINFO *>(pMediaType->pbFormat);
        infos.m_iWidth = m->hdr.bmiHeader.biWidth;
        infos.m_iHeight = m->hdr.bmiHeader.biHeight;
        m->hdr.AvgTimePerFrame > 0 ? infos.m_fps = (10000000.0 / m->hdr.AvgTimePerFrame) : infos.m_fps = 0.0f;
        ExtractCodecDetail(infos, CMediaTypeEx::GetVideoCodecName(pMediaType->subtype, m->hdr.bmiHeader.biCompression, &infos.m_iFourcc));
      }
    }
    infos.m_fAspect = (float)infos.m_iWidth / infos.m_iHeight;
  }
  else if (pMediaType->majortype == MEDIATYPE_Subtitle)
  {
    CDSStreamDetailSubtitle& infos = static_cast<CDSStreamDetailSubtitle&>(s);

    if (pMediaType->formattype == FORMAT_SubtitleInfo
      && pMediaType->cbFormat >= sizeof(SUBTITLEINFO))
    {
      SUBTITLEINFO *i = reinterpret_cast<SUBTITLEINFO *>(pMediaType->pbFormat);
      infos.isolang = i->IsoLang;
      infos.offset = i->dwOffset;
      g_charsetConverter.wToUTF8(i->TrackName, infos.trackname);
      if (!infos.lcid)
        infos.lcid = ISO6392ToLcid(i->IsoLang);
    }

    infos.subtype = pMediaType->subtype;
  }

  if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_SHOWSPLITTERDETAIL) ||
      CGraphFilters::Get()->UsingMediaPortalTsReader())
    FormatStreamName(s);
  else
    FormatStreamNameBySplitter(s);
}

int CStreamsManager::GetPictureWidth()
{
  return m_videoStream.m_iWidth;
}

int CStreamsManager::GetPictureHeight()
{
  return m_videoStream.m_iHeight;
}

std::string CStreamsManager::GetAudioCodecName(int istream)
{
  if (m_audioStreams.size() == 0 || istream == -1)
    return "";

  return m_audioStreams[istream]->m_strCodec;
}

std::string CStreamsManager::GetAudioCodecDisplayName()
{ 
  int i = GetAudioStream(); return (i == -1) ? "" : m_audioStreams[i]->m_strCodecName; 
}
std::string CStreamsManager::GetAudioCodecDisplayName(int istream)
{ 
  if (m_audioStreams.size() == 0 || istream == -1)
    return "";

  return m_audioStreams[istream]->m_strCodecName; 
}

std::string CStreamsManager::GetVideoCodecName()
{
  return m_videoStream.m_strCodec;
}

std::string CStreamsManager::GetStereoMode()
{
  return m_videoStream.m_strStereoMode;
}

CDSStreamDetailVideo * CStreamsManager::GetVideoStreamDetail(unsigned int iIndex /*= 0*/)
{
  /* We currently supports only one video stream */
  if (iIndex != 0)
    return NULL;

  return &m_videoStream;
}

CDSStreamDetailAudio * CStreamsManager::GetAudioStreamDetail(unsigned int iIndex /*= 0*/)
{
  if (iIndex > m_audioStreams.size())
    return NULL;

  return m_audioStreams[iIndex];
}

void CStreamsManager::FormatStreamNameBySplitter(CStreamDetail& s)
{
  CDSStreamDetail& pS = dynamic_cast<CDSStreamDetail&> (s);

  StringUtils::Replace(pS.displayname, "A:","");
  StringUtils::Replace(pS.displayname, "V:", "");
  StringUtils::Replace(pS.displayname, "S:", "");
  StringUtils::Replace(pS.displayname, "E:", "");

  StringUtils::Replace(pS.displayname, "Audio -", "");
  StringUtils::Replace(pS.displayname, "Video -", "");
  StringUtils::Replace(pS.displayname, "Subtitle -", "");
  StringUtils::Replace(pS.displayname, "Edition -", "");
  
  

  CRegExp regExp;
  regExp.RegComp(".*(\\[.*\\])");
  if (regExp.RegFind(pS.displayname, 0) != -1)
    StringUtils::TrimRight(pS.displayname, regExp.GetMatch(1).c_str());

  StringUtils::Trim(pS.displayname);
}

void CStreamsManager::FormatStreamName(CStreamDetail& s)
{
  std::vector<std::shared_ptr<CRegExp>> regex;

  std::shared_ptr<CRegExp> reg(new CRegExp(true));
  reg->RegComp("(.*?)(\\(audio.*?\\)|\\(subtitle.*?\\))"); // mkv source audio / subtitle
  regex.push_back(reg);

  reg.reset(new CRegExp(true));
  reg->RegComp(".* - (.*),.*\\(.*\\)"); // mpeg source audio / subtitle
  regex.push_back(reg);

  CDSStreamDetail& pS = dynamic_cast<CDSStreamDetail&> (s);

  for (std::vector<std::shared_ptr<CRegExp>>::iterator
    it = regex.begin(); it != regex.end(); ++it)
  {
    if ((*it)->RegFind(pS.displayname) > -1)
    {
      pS.displayname = (*it)->GetMatch(1);
      break;
    }
  }

  regex.clear();

  // First, if lcid isn't 0, try GetLocalInfo
  if (pS.lcid)
  {
    wchar_t buffer[64];
    std::wstring _name; int len = 0;
    if (len = GetLocaleInfoW(pS.lcid, LOCALE_SLANGUAGE, buffer, 64))
    {
      _name = buffer;
      _name.resize(len - 1); //get rid of last \0
      std::string name; g_charsetConverter.wToUTF8(_name, name);
      if (s.m_eType == CStreamDetail::SUBTITLE && !((CDSStreamDetailSubtitle&)pS).trackname.empty())
        pS.displayname = StringUtils::Format("%s (%s)", name.c_str(), ((CDSStreamDetailSubtitle&)pS).trackname.c_str());
      else
        pS.displayname = name;
    }
  }

  if (s.m_eType == CStreamDetail::SUBTITLE && pS.displayname.empty())
  {
    CDSStreamDetailSubtitle& c = (CDSStreamDetailSubtitle&)pS;
    std::string name = ISOToLanguage(c.isolang);
    if (!name.empty() && !c.trackname.empty())
      c.displayname = StringUtils::Format("%s (%s)", name.c_str(), ((CDSStreamDetailSubtitle&)pS).trackname.c_str());
    else if (!c.trackname.empty())
      c.displayname = c.trackname;
    else
      c.displayname = "Unknown";
  }
}

CSubtitleManager::CSubtitleManager(CStreamsManager* pStreamManager) :
m_dll(), m_pStreamManager(pStreamManager), m_rtSubtitleDelay(0)
{
  m_dll.Load();

  memset(&m_subtitleMediaType, 0, sizeof(AM_MEDIA_TYPE));
  m_subtitleMediaType.majortype = MEDIATYPE_Subtitle;

  m_bSubtitlesVisible = g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn;
  SetSubtitleDelay(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleDelay);

  m_pManager.reset();
}

CSubtitleManager::~CSubtitleManager()
{
  Unload();
  m_dll.Unload();
}

#define FONT_STYLE_NORMAL       0
#define FONT_STYLE_BOLD         1
#define FONT_STYLE_ITALICS      2
#define FONT_STYLE_BOLD_ITALICS 3

static UTILS::Color color[8] = { 0x0000FFFF, 0x00FFFFFF, 0x00FF9900, 0x0000FF00, 0x00FFCC, 0x00FFFF00, 0x00E5E5E5, 0x00C0C0C0 };

void CSubtitleManager::Initialize()
{
  // Initialize subtitles
  // 1. Create Subtitle Manager

  SIZE s; s.cx = 0; s.cy = 0;
  ISubManager *pManager = NULL;

  if (CGraphFilters::Get()->Subs.pBF != nullptr) {
    CLog::Log(LOGNOTICE, "%s disabled libsubs.dll", __FUNCTION__);
    return;
  }
  if (!m_dll.IsLoaded())
    return;

  CLog::Log(LOGNOTICE, "%s enabled libsubs.dll", __FUNCTION__);
  // Log manager for the DLL
  m_Log.reset(new ILogImpl());

  m_dll.CreateSubtitleManager(CGraphFilters::Get()->GetD3DDevice(), s, m_Log.get(), &pManager);

  if (!pManager)
    return;

  m_pManager.reset(pManager, std::bind2nd(std::ptr_fun(DeleteSubtitleManager), m_dll));

  SSubStyle style; //auto default on constructor

  // Build style based on XBMC settings
  style.colors[0] = color[CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_SUBTITLES_COLOR)];
  style.alpha[0] = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt("subtitles.alpha");

  CServiceBroker::GetWinSystem()->GetGfxContext().SetScalingResolution(10, true);
  style.fontSize = (float)(CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_SUBTITLES_HEIGHT)) * 50.0 / 72.0;

  int fontStyle = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_SUBTITLES_STYLE);
  switch (fontStyle)
  {
  case FONT_STYLE_NORMAL:
  default:
    style.fItalic = false;
    style.fontWeight = FW_NORMAL;
    break;
  case FONT_STYLE_BOLD:
    style.fItalic = false;
    style.fontWeight = FW_BOLD;
    break;
  case FONT_STYLE_ITALICS:
    style.fontWeight = FW_NORMAL;
    style.fItalic = true;
    break;
  case FONT_STYLE_BOLD_ITALICS:
    style.fItalic = true;
    style.fontWeight = FW_BOLD;
    break;
  }

  style.charSet = CDSCharsetConverter::getCharsetIdByName(g_langInfo.GetSubtitleCharSet());

  style.borderStyle = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt("subtitles.border");
  style.shadowDepthX = style.shadowDepthY = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt("subtitles.shadowdepth");
  style.outlineWidthX = style.outlineWidthY = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt("subtitles.outlinewidth");

  std::wstring fontName;
  g_charsetConverter.utf8ToW(CServiceBroker::GetSettingsComponent()->GetSettings()->GetString("subtitles.dsfont"), fontName);
  style.fontName = (wchar_t *)CoTaskMemAlloc(fontName.length() * sizeof(wchar_t) + 2);
  if (style.fontName)
    wcscpy_s(style.fontName, fontName.length() + 1, fontName.c_str());

  bool override = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_SUBTITLES_OVERRIDEASSFONTS);
  m_pManager->SetStyle(&style, override);

  if (FAILED(m_pManager->InsertPassThruFilter(g_dsGraph->pFilterGraph)))
  {
    // No internal subs. May have some externals!
  }

  m_pManager->SetEnable(true);
}

bool CSubtitleManager::Ready()
{
  return (!!m_pManager);
}

void CSubtitleManager::StopThread()
{
  if (!m_subtitleStreams.empty() && m_pManager)
    m_pManager->StopThread();
}

void CSubtitleManager::StartThread()
{
  if (!m_subtitleStreams.empty() && m_pManager)
    m_pManager->StartThread(CGraphFilters::Get()->GetD3DDevice());
}

void CSubtitleManager::Unload()
{
  if (m_pManager)
  {
    m_pManager->SetSubPicProvider(NULL);
    m_pManager->Free();
  }
  while (!m_subtitleStreams.empty())
  {
    if (m_subtitleStreams.back()->m_subType == EXTERNAL)
      ((CDSStreamDetailSubtitleExternal *)m_subtitleStreams.back())->substream.FullRelease();
    delete m_subtitleStreams.back();
    m_subtitleStreams.pop_back();
  }
  m_pManager.reset();
}

void CSubtitleManager::SetTimePerFrame(REFERENCE_TIME iTimePerFrame)
{
  if (m_pManager)
    m_pManager->SetTimePerFrame(iTimePerFrame);
}
void CSubtitleManager::SetTime(REFERENCE_TIME rtNow)
{
  if (m_pManager)
    m_pManager->SetTime(rtNow - m_rtSubtitleDelay);
}

HRESULT CSubtitleManager::GetTexture(Com::SmartPtr<IDirect3DTexture9>& pTexture, Com::SmartRect& pSrc, Com::SmartRect& pDest, Com::SmartRect& renderRect)
{
  if (m_pManager)
    return m_pManager->GetTexture(pTexture, pSrc, pDest, renderRect);
  return E_FAIL;
}

void CSubtitleManager::SetSizes(Com::SmartRect window, Com::SmartRect video)
{
  if (m_pManager)
    m_pManager->SetSizes(window, video);
}

std::vector<CDSStreamDetailSubtitle *>& CSubtitleManager::GetSubtitles()
{
  return m_subtitleStreams;
}

int CSubtitleManager::GetSubtitleCount()
{
  return m_subtitleStreams.size();
}

int CSubtitleManager::GetSubtitle()
{
  if (m_subtitleStreams.size() == 0)
    return -1;

  int i = 0;
  for (std::vector<CDSStreamDetailSubtitle *>::const_iterator it = m_subtitleStreams.begin();
    it != m_subtitleStreams.end(); ++it, i++)
  {
    if ((*it)->flags & AMSTREAMSELECTINFO_ENABLED)
      return i;
  }

  return -1;
}

void CSubtitleManager::GetSubtitleName(int iStream, std::string &strStreamName)
{
  if (m_subtitleStreams.size() == 0)
    return;

  int i = 0;
  for (std::vector<CDSStreamDetailSubtitle *>::const_iterator it = m_subtitleStreams.begin();
    it != m_subtitleStreams.end(); ++it, i++)
  {
    if (i == iStream)
    {
      strStreamName = (*it)->displayname;
    }
  }
}

void CSubtitleManager::SetSubtitle(int iStream)
{
  if (CGraphFilters::Get()->IsDVD())
    return; // currently not implemented

  CSingleLock lock(m_pStreamManager->m_lock);

  //If no subtitles just return
  if (GetSubtitleCount() <= 0)
    return;

  if (iStream > GetSubtitleCount())
    return;

  long disableIndex = GetSubtitle(), enableIndex = iStream;

  m_pStreamManager->m_readyEvent.Reset();
  CAutoSetEvent event(&m_pStreamManager->m_readyEvent);

  bool stopped = false;
  std::string subtitlePath = "";
  Com::SmartPtr<IPin> newAudioStreamPin;

  CVideoSettings vs = g_application.GetAppPlayer().GetVideoSettings();
  vs.m_SubtitleStream = enableIndex;
  g_application.GetAppPlayer().SetVideoSettings(vs);

  if (m_subtitleStreams[enableIndex]->m_subType == EXTERNAL)
  {
    /* External subtitle */
    DisconnectCurrentSubtitlePins();

    CDSStreamDetailSubtitleExternal *s = reinterpret_cast<CDSStreamDetailSubtitleExternal *>(m_subtitleStreams[enableIndex]);

    m_pManager->SetSubPicProvider(s->substream);

    s->flags = AMSTREAMSELECTINFO_ENABLED; // for gui
    s->connected = true;

    SetSubtitleVisible(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn);
    return;
  }

  if (m_pStreamManager->m_pIAMStreamSelect)
  {
    if (disableIndex >= 0 && m_subtitleStreams[disableIndex]->connected)
      DisconnectCurrentSubtitlePins();

    if (SUCCEEDED(m_pStreamManager->m_pIAMStreamSelect->Enable(m_subtitleStreams[enableIndex]->IAMStreamSelect_Index, AMSTREAMSELECTENABLE_ENABLE)))
    {
      m_pManager->SetSubPicProviderToInternal();
      m_subtitleStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
      m_subtitleStreams[enableIndex]->connected = true;
      SetSubtitleVisible(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn);
      CLog::Log(LOGDEBUG, "%s Successfully selected subtitle stream", __FUNCTION__);
    }
  }
  else
  {

    Com::SmartPtr<IPin> connectedToPin = NULL;
    Com::SmartPtr<IPin> oldAudioStreamPin = NULL;
    HRESULT hr = S_OK;

    g_dsGraph->Stop(); // Current position is kept by the graph
    stopped = true;

    /* Disconnect pins */
    if (m_subtitleStreams[disableIndex]->m_subType != EXTERNAL)
    {

      oldAudioStreamPin = m_subtitleStreams[disableIndex]->pObj;
      hr = oldAudioStreamPin->ConnectedTo(&connectedToPin);

      if (connectedToPin)
        hr = m_pStreamManager->m_pGraphBuilder->Disconnect(connectedToPin);

      m_pStreamManager->m_pGraphBuilder->Disconnect(oldAudioStreamPin);
    }

    m_subtitleStreams[disableIndex]->flags = 0;
    m_subtitleStreams[disableIndex]->connected = false;

    if (!connectedToPin)
    {
      m_subtitleMediaType.subtype = m_subtitleStreams[enableIndex]->subtype;
      connectedToPin = GetFirstSubtitlePin(); // Find where connect the subtitle
    }

    if (!connectedToPin)
    {
      /* Nothing support subtitles! */
      CLog::Log(LOGERROR, "%s You're trying to use subtitle, but no filters in the graph seems to accept subtitles!", __FUNCTION__);
      hr = E_FAIL;
      goto done;
    }

    /* Enable filter */
    newAudioStreamPin = m_subtitleStreams[enableIndex]->pObj;

    hr = m_pStreamManager->m_pGraphBuilder->ConnectDirect(newAudioStreamPin, connectedToPin, NULL);
    if (FAILED(hr))
    {
      /* Reconnect previous working pins */
      if (m_subtitleStreams[disableIndex]->m_subType == INTERNAL)
        m_pStreamManager->m_pGraphBuilder->ConnectDirect(oldAudioStreamPin, connectedToPin, NULL);

      m_subtitleStreams[disableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
      m_subtitleStreams[disableIndex]->connected = true;

      goto done;
    }

    m_subtitleStreams[enableIndex]->flags = AMSTREAMSELECTINFO_ENABLED;
    m_subtitleStreams[enableIndex]->connected = true;

    m_pManager->SetSubPicProviderToInternal();

  done:
    if (stopped)
      g_dsGraph->Play();

    if (m_bSubtitlesVisible)
    {
      if (SUCCEEDED(hr))
        CLog::Log(LOGNOTICE, "%s Successfully changed subtitle stream", __FUNCTION__);
      else
        CLog::Log(LOGERROR, "%s Can't change subtitle stream", __FUNCTION__);
    }
  }
}

bool CSubtitleManager::GetSubtitleVisible()
{
  return m_bSubtitlesVisible;
}

void CSubtitleManager::SetSubtitleVisible(bool bVisible)
{
  CVideoSettings vs = g_application.GetAppPlayer().GetVideoSettings();
  vs.m_SubtitleOn = bVisible;
  g_application.GetAppPlayer().SetVideoSettings(vs);
  m_bSubtitlesVisible = bVisible;
  if (m_pManager)
    m_pManager->SetEnable(bVisible);
}

int CSubtitleManager::AddSubtitle(const std::string& subFilePath)
{
  if (!m_pManager)
    return -1;

  std::auto_ptr<CDSStreamDetailSubtitleExternal> s(new CDSStreamDetailSubtitleExternal());

  if (m_subtitleStreams.empty())
    s->flags = AMSTREAMSELECTINFO_ENABLED;

  s->path = CSpecialProtocol::TranslatePath(subFilePath);
  if (!XFILE::CFile::Exists(s->path))
    return -1;

  // Try to detect isolang of subtitle
  CRegExp regex(true);
  regex.RegComp("^.*\\.(.*)\\.[^\\.]*.*$");
  if (regex.RegFind(s->path) > -1)
    s->isolang = regex.GetMatch(1);

  if (!s->isolang.empty())
  {
    s->lcid = ISO6392ToLcid(s->isolang.c_str());
    if (!s->lcid)
      s->lcid = ISO6391ToLcid(s->isolang.c_str());

    m_pStreamManager->FormatStreamName(*(s.get()));
  }

  if (s->displayname.empty())
    s->displayname = URIUtils::GetFileName(s->path);
  else
    s->displayname += " [External]";

  // Load subtitle file
  Com::SmartPtr<ISubStream> pSubStream;

  std::wstring unicodePath; g_charsetConverter.utf8ToW(s->path, unicodePath);

  if (SUCCEEDED(m_pManager->LoadExternalSubtitle(unicodePath.c_str(), &pSubStream)))
  {
    s->substream = pSubStream;
    // if the sub is a vob, it has an internal title, grab it
    wchar_t* title = NULL;
    if (SUCCEEDED(m_pManager->GetStreamTitle(pSubStream, &title)))
    {
      g_charsetConverter.wToUTF8(std::wstring(title), s->displayname);
      s->displayname += " [External]";
      CoTaskMemFree(title);
    }

    CLog::Log(LOGNOTICE, "%s Successfully loaded subtitle file \"%s\"", __FUNCTION__, s->path.c_str());
    m_subtitleStreams.push_back(s.release());
    return m_subtitleStreams.size() - 1;
  }

  return -1;
}

void CSubtitleManager::DisconnectCurrentSubtitlePins(void)
{
  int i = GetSubtitle();
  if (i == -1)
    return;

  if (m_pStreamManager->m_pIAMStreamSelect)
  {
    m_subtitleStreams[i]->connected = false;
    m_subtitleStreams[i]->flags = 0;
  }
  else
  {
    if (m_subtitleStreams[i]->connected && m_subtitleStreams[i]->m_subType == INTERNAL)
    {
      Com::SmartPtr<IPin> pin = NULL;

      if (VFW_E_NOT_CONNECTED == m_subtitleStreams[i]->pObj->ConnectedTo(&pin))
      {
        // Shouldn't happened!
        m_subtitleStreams[i]->connected = false;
        m_subtitleStreams[i]->flags = 0;
        return;
      }

      g_dsGraph->Stop();
      m_pStreamManager->m_pGraphBuilder->Disconnect(m_subtitleStreams[i]->pObj); // Splitter's output pin
      m_pStreamManager->m_pGraphBuilder->Disconnect(pin); // Filter's input pin
      g_dsGraph->Play();
    }
    m_subtitleStreams[i]->connected = false;
    m_subtitleStreams[i]->flags = 0;
  }
  //m_bSubtitlesUnconnected = true;
}

void CSubtitleManager::SetSubtitleDelay(float fValue)
{
  m_rtSubtitleDelay = (int)(-fValue * 10000000i64); // second to directshow timebase
}

float CSubtitleManager::GetSubtitleDelay(void)
{
  return (float)m_rtSubtitleDelay / 10000000i64;
}

IPin *CSubtitleManager::GetFirstSubtitlePin(void)
{
  PIN_DIRECTION  pindir;

  BeginEnumFilters(g_dsGraph->pFilterGraph, pEF, pBF)
  {
    BeginEnumPins(pBF, pEP, pPin)
    {
      Com::SmartPtr<IPin> pFellow;

      if (SUCCEEDED(pPin->QueryDirection(&pindir)) &&
        pindir == PINDIR_INPUT && pPin->ConnectedTo(&pFellow) == VFW_E_NOT_CONNECTED)
      {
        if (pPin->QueryAccept(&m_subtitleMediaType) == S_OK)
          return (pPin);
      }
    }
    EndEnumPins
  }
  EndEnumFilters

    return NULL;
}

std::string CStreamsManager::ISOToLanguage(std::string code)
{
  if (code.length() == 2)
    return ISO6391ToLanguage(code.c_str());
  else
    return ISO6392ToLanguage(code.c_str());
}

void CStreamsManager::LoadDVDStreams()
{
  Com::SmartPtr<IDvdInfo2> pDvdI = CGraphFilters::Get()->DVD.dvdInfo;
  Com::SmartPtr<IDvdControl2> pDvdC = CGraphFilters::Get()->DVD.dvdControl;

  DVD_VideoAttributes vid;
  // First, video
  HRESULT hr = pDvdI->GetCurrentVideoAttributes(&vid);
  if (SUCCEEDED(hr))
  {
    m_videoStream.m_iWidth = vid.ulSourceResolutionX;
    m_videoStream.m_iHeight = vid.ulSourceResolutionY;
    switch (vid.Compression)
    {
    case DVD_VideoCompression_MPEG1:
      m_videoStream.m_strCodecName = "MPEG1";
      break;
    case DVD_VideoCompression_MPEG2:
      m_videoStream.m_strCodecName = "MPEG2";
      break;
    default:
      m_videoStream.m_strCodecName = "Unknown";
    }
  }

  UpdateDVDStream();

}

void CStreamsManager::UpdateDVDStream()
{
  if (m_dvdStreamLoaded)
    return;

  // Second, audio
  unsigned long nbrStreams = 0, currentStream = 0;
  HRESULT hr = CGraphFilters::Get()->DVD.dvdInfo->GetCurrentAudio(&nbrStreams, &currentStream);
  if (FAILED(hr))
    return;

  CSingleLock lock(m_lock);

  m_audioStreams.clear();
  for (unsigned int i = 0; i < nbrStreams; i++)
  {
    std::auto_ptr<CDSStreamDetailAudio> s(new CDSStreamDetailAudio());
    DVD_AudioAttributes audio;
    hr = CGraphFilters::Get()->DVD.dvdInfo->GetAudioAttributes(i, &audio);

    s->m_iChannels = audio.bNumberOfChannels;
    s->m_iSampleRate = audio.dwFrequency;
    s->m_iBitRate = 0;
    s->lcid = audio.Language;
    if (i == currentStream)
    {
      s->flags = AMSTREAMSELECTINFO_ENABLED;
      s->connected = true;
    }
    switch (audio.AudioFormat)
    {
    case DVD_AudioFormat_AC3:
      s->m_strCodecName = "Dolby AC3";
      break;
    case DVD_AudioFormat_MPEG1:
      s->m_strCodecName = "MPEG1";
      break;
    case DVD_AudioFormat_MPEG1_DRC:
      s->m_strCodecName = "MPEG1 (DCR)";
      break;
    case DVD_AudioFormat_MPEG2:
      s->m_strCodecName = "MPEG2";
      break;
    case DVD_AudioFormat_MPEG2_DRC:
      s->m_strCodecName = "MPEG2 (DCR)";
      break;
    case DVD_AudioFormat_LPCM:
      s->m_strCodecName = "LPCM";
      break;
    case DVD_AudioFormat_DTS:
      s->m_strCodecName = "DTS";
      break;
    case DVD_AudioFormat_SDDS:
      s->m_strCodecName = "SDDS";
      break;
    default:
      s->m_strCodecName = "Unknown";
      break;
    }
    FormatStreamName(*(s.get()));

    m_audioStreams.push_back(s.release());
  }

  BOOL isDisabled = false;
  hr = CGraphFilters::Get()->DVD.dvdInfo->GetCurrentSubpicture(&nbrStreams, &currentStream, &isDisabled);
  if (FAILED(hr))
    return;

  for (unsigned int i = 0; i < nbrStreams; i++)
  {
    DVD_SubpictureAttributes subpic;
    hr = CGraphFilters::Get()->DVD.dvdInfo->GetSubpictureAttributes(i, &subpic);
    if (subpic.Type != DVD_SPType_Language)
      continue;

    std::auto_ptr<CDSStreamDetailSubtitle> s(new CDSStreamDetailSubtitle());
    std::auto_ptr<CDSStreamDetailSubfilter> s2(new CDSStreamDetailSubfilter(INTERNAL));
    s->lcid = subpic.Language;
    s2->lcid = subpic.Language;
    FormatStreamName(*(s.get()));
    FormatStreamName(*(s2.get()));
    SubtitleManager->GetSubtitles().push_back(s.release());
    m_subfilterStreams.push_back(s2.release());
  }

  m_dvdStreamLoaded = true;
}
CDSStreamDetailSubtitle * CSubtitleManager::GetSubtitleStreamDetail(unsigned int iIndex /*= 0*/)
{
  if (iIndex > m_subtitleStreams.size())
    return NULL;

  if (m_subtitleStreams[iIndex]->m_subType != INTERNAL)
    return NULL;

  return m_subtitleStreams[iIndex];
}

CDSStreamDetailSubtitleExternal* CSubtitleManager::GetExternalSubtitleStreamDetail(unsigned int iIndex /*= 0*/)
{
  if (iIndex > m_subtitleStreams.size())
    return NULL;

  if (m_subtitleStreams[iIndex]->m_subType != EXTERNAL)
    return NULL;

  return reinterpret_cast<CDSStreamDetailSubtitleExternal *>(m_subtitleStreams[iIndex]);
}

void CSubtitleManager::DeleteSubtitleManager(ISubManager* pManager, DllLibSubs dll)
{
  dll.DeleteSubtitleManager(pManager);
}

void CSubtitleManager::SelectBestSubtitle()
{
  int select = -1;
  int iLibrary;

  // set previuos selected stream
  iLibrary = g_application.GetAppPlayer().GetVideoSettings().m_SubtitleStream;
  if ((iLibrary < g_application.GetAppPlayer().GetSubtitleCount()) && !(iLibrary < 0))
    select = iLibrary;

  // set first external sub
  else if (m_subtitleStreams.size() != 0)
  {
    int i = 0;
    for (std::vector<CDSStreamDetailSubtitle *>::const_iterator it = m_subtitleStreams.begin();
      it != m_subtitleStreams.end(); ++it, i++)
    {
      if ((*it)->m_subType & EXTERNAL)
      {
        select = i;
        break;
      }
    }
  }

  if (select != -1)
    SetSubtitle(select);

  SetSubtitleVisible(g_application.GetAppPlayer().GetVideoSettings().m_SubtitleOn);
}

#endif
