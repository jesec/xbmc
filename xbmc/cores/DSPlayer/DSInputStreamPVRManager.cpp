/*
*  Copyright (C) 2010-2013 Eduard Kytmanov
*  http://www.avmedia.su
*
*  Copyright (C) 2015 Romank
*  https://github.com/Romank1
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
*  along with GNU Make; see the file COPYING.  If not, write to
*  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*  http://www.gnu.org/copyleft/gpl.html
*
*/

#ifdef HAS_DS_PLAYER

#include "DSInputStreamPVRManager.h"
#include "ServiceBroker.h"
#include "URL.h"

#include "pvr/addons/PVRClients.h"
#include "pvr/PVRManager.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "settings/AdvancedSettings.h"
#include "utils/StringUtils.h"
#include "pvr/recordings/PVRRecordingsPath.h"


CDSInputStreamPVRManager* g_pPVRStream = NULL;

CDSInputStreamPVRManager::CDSInputStreamPVRManager(CDSPlayer *pPlayer)
  : m_pPlayer(pPlayer)
  , m_pPVRBackend(NULL)
{
}

CDSInputStreamPVRManager::~CDSInputStreamPVRManager(void)
{
  Close();
}

void CDSInputStreamPVRManager::Close()
{
  CServiceBroker::GetPVRManager().CloseStream();
  SAFE_DELETE(m_pPVRBackend);
}

bool CDSInputStreamPVRManager::CloseAndOpenFile(const CURL& url)
{
  bool bReturnVal = false;
  // TODO
  return bReturnVal;
}

bool CDSInputStreamPVRManager::Open(const CFileItem& file)
{
  Close();
  bool bReturnVal = false;
  // TODO
  return bReturnVal;
}

bool  CDSInputStreamPVRManager::PrepareForChannelSwitch(const CPVRChannelPtr &channel)
{
  bool bReturnVal = true;
  // TODO
  return bReturnVal;
}

bool CDSInputStreamPVRManager::PerformChannelSwitch()
{
  bool bResult = false;
  // TODO
  return bResult;
}

bool CDSInputStreamPVRManager::GetNewChannel(CFileItem& fileItem)
{
  bool bResult = false;
  // TODO
  return bResult;
}

bool CDSInputStreamPVRManager::SelectChannel(const CPVRChannelPtr &channel)
{
  bool bResult = false;
  // TODO
  return bResult;
}

bool CDSInputStreamPVRManager::NextChannel(bool preview /* = false */)
{
  bool bResult = false;
  // TODO
  return bResult;
}

bool CDSInputStreamPVRManager::PrevChannel(bool preview/* = false*/)
{
  bool bResult = false;
  // TODO
  return bResult;
}

bool CDSInputStreamPVRManager::SelectChannelByNumber(unsigned int iChannelNumber)
{
  bool bResult = false;
  // TODO
  return bResult;
}

bool CDSInputStreamPVRManager::SupportsChannelSwitch()const
{
  if (!m_pPVRBackend || !g_advancedSettings.m_bDSPlayerFastChannelSwitching)
    return false;

  PVR_CLIENT pvrClient;
  if (!CServiceBroker::GetPVRManager().Clients()->GetPlayingClient(pvrClient))
    return false;

  // Check if active PVR Backend addon has changed or PVR Backend addon does not support channel switching
  if (pvrClient->GetBackendName() != m_pPVRBackend->GetBackendName() || !m_pPVRBackend->SupportsFastChannelSwitch())
    return false;

  return pvrClient->GetClientCapabilities().HandlesInputStream();
}

CDSPVRBackend* CDSInputStreamPVRManager::GetPVRBackend()
{
  PVR_CLIENT pvrClient;
  if (!CServiceBroker::GetPVRManager().Clients()->GetPlayingClient(pvrClient))
  {
    CLog::Log(LOGERROR, "%s - Failed to get PVR Client", __FUNCTION__);
    return NULL;
  }

  CDSPVRBackend* pPVRBackend = NULL;
  if (pvrClient->GetBackendName().find("MediaPortal TV-server") != std::string::npos)
  {
    pPVRBackend = new CDSMediaPortal(pvrClient->GetConnectionString(), pvrClient->GetBackendName());
  }
  else if (pvrClient->GetBackendName().find("ARGUS TV") != std::string::npos)
  {
    pPVRBackend = new CDSArgusTV(pvrClient->GetConnectionString(), pvrClient->GetBackendName());
  }

  return pPVRBackend;
}

uint64_t CDSInputStreamPVRManager::GetTotalTime()
{
  if (!m_isRecording)
    return CServiceBroker::GetPVRManager().GetTotalTime();
  return 0;
}

uint64_t CDSInputStreamPVRManager::GetTime()
{
  if (!m_isRecording)
    return CServiceBroker::GetPVRManager().GetStartTime();
  return 0;
}

#endif
