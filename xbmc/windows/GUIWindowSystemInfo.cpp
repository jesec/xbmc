/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIWindowSystemInfo.h"
#include "ServiceBroker.h"
#include "GUIInfoManager.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIComponent.h"
#include "guilib/WindowIDs.h"
#include "guilib/LocalizeStrings.h"
#include "pvr/PVRManager.h"
#include "utils/SystemInfo.h"
#include "utils/StringUtils.h"
#include "storage/MediaManager.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "ServiceBroker.h"

#ifdef HAS_DS_PLAYER
#include "DSFilterVersion.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#endif

#define CONTROL_TB_POLICY   30
#define CONTROL_BT_STORAGE  94
#define CONTROL_BT_DEFAULT  95
#define CONTROL_BT_NETWORK  96
#define CONTROL_BT_VIDEO    97
#define CONTROL_BT_HARDWARE 98
#define CONTROL_BT_PVR      99
#define CONTROL_BT_POLICY   100

#define CONTROL_START       CONTROL_BT_STORAGE
#define CONTROL_END         CONTROL_BT_POLICY

CGUIWindowSystemInfo::CGUIWindowSystemInfo(void) :
    CGUIWindow(WINDOW_SYSTEM_INFORMATION, "SettingsSystemInfo.xml")
{
  m_section = CONTROL_BT_DEFAULT;
  m_loadType = KEEP_IN_MEMORY;
}

CGUIWindowSystemInfo::~CGUIWindowSystemInfo(void) = default;

bool CGUIWindowSystemInfo::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_WINDOW_INIT:
    {
      CGUIWindow::OnMessage(message);
      SET_CONTROL_LABEL(52, CSysInfo::GetAppName() + " " + CSysInfo::GetVersion());
      SET_CONTROL_LABEL(53, CSysInfo::GetBuildDate());
      CONTROL_ENABLE_ON_CONDITION(CONTROL_BT_PVR, CServiceBroker::GetPVRManager().IsStarted());
      return true;
    }
    break;

    case GUI_MSG_WINDOW_DEINIT:
    {
      CGUIWindow::OnMessage(message);
      m_diskUsage.clear();
      return true;
    }
    break;

    case GUI_MSG_FOCUSED:
    {
      CGUIWindow::OnMessage(message);
      int focusedControl = GetFocusedControlID();
      if (m_section != focusedControl && focusedControl >= CONTROL_START && focusedControl <= CONTROL_END)
      {
        ResetLabels();
        m_section = focusedControl;
      }
      if (m_section >= CONTROL_BT_STORAGE && m_section <= CONTROL_BT_PVR)
        SET_CONTROL_HIDDEN(CONTROL_TB_POLICY);
      else if (m_section == CONTROL_BT_POLICY)
      {
        SET_CONTROL_LABEL(CONTROL_TB_POLICY, CServiceBroker::GetGUI()->GetInfoManager().GetLabel(SYSTEM_PRIVACY_POLICY));
        SET_CONTROL_VISIBLE(CONTROL_TB_POLICY);
      }
      return true;
    }
    break;
  }
  return CGUIWindow::OnMessage(message);
}

void CGUIWindowSystemInfo::FrameMove()
{
  int i = 2;
  if (m_section == CONTROL_BT_DEFAULT)
  {
    SET_CONTROL_LABEL(40, g_localizeStrings.Get(20154));
    SetControlLabel(i++, "%s: %s", 158, SYSTEM_FREE_MEMORY);
    SetControlLabel(i++, "%s: %s", 150, NETWORK_IP_ADDRESS);
    SetControlLabel(i++, "%s %s", 13287, SYSTEM_SCREEN_RESOLUTION);
    SetControlLabel(i++, "%s %s", 13283, SYSTEM_OS_VERSION_INFO);
    SetControlLabel(i++, "%s: %s", 12390, SYSTEM_UPTIME);
    SetControlLabel(i++, "%s: %s", 12394, SYSTEM_TOTALUPTIME);
    SetControlLabel(i++, "%s: %s", 12395, SYSTEM_BATTERY_LEVEL);
  }

  else if (m_section == CONTROL_BT_STORAGE)
  {
    SET_CONTROL_LABEL(40, g_localizeStrings.Get(20155));
    if (m_diskUsage.empty())
      m_diskUsage = g_mediaManager.GetDiskUsage();

    for (size_t d = 0; d < m_diskUsage.size(); d++)
    {
      SET_CONTROL_LABEL(i++, m_diskUsage[d]);
    }
  }

  else if (m_section == CONTROL_BT_NETWORK)
  {
    SET_CONTROL_LABEL(40,g_localizeStrings.Get(20158));
    SET_CONTROL_LABEL(i++, CServiceBroker::GetGUI()->GetInfoManager().GetLabel(NETWORK_LINK_STATE));
    SetControlLabel(i++, "%s: %s", 149, NETWORK_MAC_ADDRESS);
    SetControlLabel(i++, "%s: %s", 150, NETWORK_IP_ADDRESS);
    SetControlLabel(i++, "%s: %s", 13159, NETWORK_SUBNET_MASK);
    SetControlLabel(i++, "%s: %s", 13160, NETWORK_GATEWAY_ADDRESS);
    SetControlLabel(i++, "%s: %s", 13161, NETWORK_DNS1_ADDRESS);
    SetControlLabel(i++, "%s: %s", 20307, NETWORK_DNS2_ADDRESS);
    SetControlLabel(i++, "%s %s", 13295, SYSTEM_INTERNET_STATE);
  }

  else if (m_section == CONTROL_BT_VIDEO)
  {
    SET_CONTROL_LABEL(40,g_localizeStrings.Get(20159));
    SET_CONTROL_LABEL(i++,CServiceBroker::GetGUI()->GetInfoManager().GetLabel(SYSTEM_VIDEO_ENCODER_INFO));
    SetControlLabel(i++, "%s %s", 13287, SYSTEM_SCREEN_RESOLUTION);
#ifndef HAS_DX
    SetControlLabel(i++, "%s %s", 22007, SYSTEM_RENDER_VENDOR);
    SetControlLabel(i++, "%s %s", 22009, SYSTEM_RENDER_VERSION);
#else
    SetControlLabel(i++, "%s %s", 22024, SYSTEM_RENDER_VERSION);
#endif
#if !defined(__arm__) && !defined(__aarch64__) && !defined(HAS_DX)
    SetControlLabel(i++, "%s %s", 22010, SYSTEM_GPU_TEMPERATURE);
#endif
#ifdef HAS_DS_PLAYER
    i++;  // empty line
    SetControlLabel(i++, "%s: %s", 55102, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::MADSHI_VIDEO_RENDERER));   
    SetControlLabel(i++, "%s: %s", 55131, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_SANEAR));
    if (CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVSPLITTER) == CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVVIDEO)
      && CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVSPLITTER) == CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVAUDIO))
    {
      SetControlLabel(i++, "%s: %s", 55132, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVSPLITTER));
    }
    else
    {
      SetControlLabel(i++, "%s: %s", 55097, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVSPLITTER));
      SetControlLabel(i++, "%s: %s", 55098, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVVIDEO));
      SetControlLabel(i++, "%s: %s", 55099, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_LAVAUDIO));
    }
    if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_VIDEORENDERER) == "madVR")
      SetControlLabel(i++, "%s: %s", 55100, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_XYSUBFILTER));
    else
      SetControlLabel(i++, "%s: %s", 55101, CDSFilterVersion::Get()->GetStringVersion(CGraphFilters::INTERNAL_XYVSFILTER));
#endif
  }

  else if (m_section == CONTROL_BT_HARDWARE)
  {
    SET_CONTROL_LABEL(40,g_localizeStrings.Get(20160));
    SET_CONTROL_LABEL(i++, g_sysinfo.GetCPUModel());
#if defined(__arm__) && defined(TARGET_LINUX)
    SET_CONTROL_LABEL(i++, g_sysinfo.GetCPUBogoMips());
    if (!g_sysinfo.GetCPUSoC().empty())
      SET_CONTROL_LABEL(i++, g_sysinfo.GetCPUSoC());
    SET_CONTROL_LABEL(i++, g_sysinfo.GetCPUHardware());
    SET_CONTROL_LABEL(i++, g_sysinfo.GetCPURevision());
    SET_CONTROL_LABEL(i++, g_sysinfo.GetCPUSerial());
#endif
    SetControlLabel(i++, "%s %s", 22011, SYSTEM_CPU_TEMPERATURE);
#if (!defined(__arm__) && !defined(__aarch64__)) || defined(TARGET_RASPBERRY_PI)
    SetControlLabel(i++, "%s %s", 13284, SYSTEM_CPUFREQUENCY);
#endif
#if !(defined(__arm__) && defined(TARGET_LINUX))
    SetControlLabel(i++, "%s %s", 13271, SYSTEM_CPU_USAGE);
#endif
    i++;  // empty line
    SetControlLabel(i++, "%s: %s", 22012, SYSTEM_TOTAL_MEMORY);
    SetControlLabel(i++, "%s: %s", 158, SYSTEM_FREE_MEMORY);
  }

  else if (m_section == CONTROL_BT_PVR)
  {
    SET_CONTROL_LABEL(40, g_localizeStrings.Get(19166));
    int i = 2;

    SetControlLabel(i++, "%s: %s", 19120, PVR_BACKEND_NUMBER);
    i++;  // empty line
    SetControlLabel(i++, "%s: %s", 19012, PVR_BACKEND_NAME);
    SetControlLabel(i++, "%s: %s", 19114, PVR_BACKEND_VERSION);
    SetControlLabel(i++, "%s: %s", 19115, PVR_BACKEND_HOST);
    SetControlLabel(i++, "%s: %s", 19116, PVR_BACKEND_DISKSPACE);
    SetControlLabel(i++, "%s: %s", 19019, PVR_BACKEND_CHANNELS);
    SetControlLabel(i++, "%s: %s", 19163, PVR_BACKEND_RECORDINGS);
    SetControlLabel(i++, "%s: %s", 19168, PVR_BACKEND_DELETED_RECORDINGS);  // Deleted and recoverable recordings
    SetControlLabel(i++, "%s: %s", 19025, PVR_BACKEND_TIMERS);
  }

  else if (m_section == CONTROL_BT_POLICY)
  {
    SET_CONTROL_LABEL(40, g_localizeStrings.Get(12389));
  }
  CGUIWindow::FrameMove();
}

void CGUIWindowSystemInfo::ResetLabels()
{
  for (int i = 2; i < 13; i++)
  {
    SET_CONTROL_LABEL(i, "");
  }
  SET_CONTROL_LABEL(CONTROL_TB_POLICY, "");
}

void CGUIWindowSystemInfo::SetControlLabel(int id, const char *format, int label, int info)
{
  std::string tmpStr = StringUtils::Format(format, g_localizeStrings.Get(label).c_str(),
      CServiceBroker::GetGUI()->GetInfoManager().GetLabel(info).c_str());
  SET_CONTROL_LABEL(id, tmpStr);
}

#ifdef HAS_DS_PLAYER
void CGUIWindowSystemInfo::SetControlLabel(int id, const char *format, int label, const std::string &info)
{
  std::string tmpStr = StringUtils::Format(format, g_localizeStrings.Get(label).c_str(),
    info.c_str());
  SET_CONTROL_LABEL(id, tmpStr);
}

#endif