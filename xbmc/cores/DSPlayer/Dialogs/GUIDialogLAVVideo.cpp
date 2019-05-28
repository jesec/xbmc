/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *      Copyright (C) 2014-2015 Aracnoz
 *      http://github.com/aracnoz/xbmc
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "GUIDialogLAVVideo.h"
#include "Application.h"
#include "URL.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/File.h"
#include "guilib/LocalizeStrings.h"
#include "profiles/ProfilesManager.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "utils/LangCodeExpander.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "input/Key.h"
#include "utils/XMLUtils.h"
#include "Filters/RendererSettings.h"
#include "PixelShaderList.h"
#include "cores/playercorefactory/PlayerCoreFactory.h"
#include "Filters/LAVAudioSettings.h"
#include "Filters/LAVVideoSettings.h"
#include "Filters/LAVSplitterSettings.h"
#include "utils/CharsetConverter.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "addons/Skin.h"
#include "GraphFilters.h"

#define LAVVIDEO_PROPERTYPAGE      "lavvideo.propertypage"
#define LAVVIDEO_HWACCEL           "lavvideo.hwaccel"
#define LAVVIDEO_NUMTHREADS        "lavvideo.dwnumthreads"
#define LAVVIDEO_TRAYICON          "lavvideo.btrayicon"
#define LAVVIDEO_STREAMAR          "lavvideo.dwstreamar"
#define LAVVIDEO_DEINTFILEDORDER   "lavvideo.dwdeintfieldorder"
#define LAVVIDEO_DEINTMODE         "lavvideo.deintmode"
#define LAVVIDEO_RGBRANGE          "lavvideo.dwrgbrange"
#define LAVVIDEO_DITHERMODE        "lavvideo.dwdithermode"
#define LAVVIDEO_HWDEINTMODE       "lavvideo.dwhwdeintmode"
#define LAVVIDEO_HWDEINTOUT        "lavvideo.dwhwdeintoutput"
#define LAVVIDEO_SWDEINTMODE       "lavvideo.dwswdeintmode"
#define LAVVIDEO_SWDEINTOUT        "lavvideo.dwswdeintoutput"
#define LAVVIDEO_RESET             "lavvideo.reset"

using namespace std;

CGUIDialogLAVVideo::CGUIDialogLAVVideo()
  : CGUIDialogSettingsManualBase(WINDOW_DIALOG_LAVVIDEO, "DialogSettings.xml")
{
  m_allowchange = true;
}


CGUIDialogLAVVideo::~CGUIDialogLAVVideo()
{ }

void CGUIDialogLAVVideo::OnInitWindow()
{
  CGUIDialogSettingsManualBase::OnInitWindow();

  HideUnused();
}

void CGUIDialogLAVVideo::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();

  SetHeading(55077);

  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_OKAY_BUTTON);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 15067);
}

void CGUIDialogLAVVideo::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  CSettingCategory *category = AddCategory("dsplayerlavvideo", -1);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupProperty = AddGroup(category);
  if (groupProperty == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  CSettingGroup *group = AddGroup(category);
  if (group == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  CSettingGroup *groupHW = AddGroup(category);
  if (groupHW == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupSettings = AddGroup(category);
  if (groupSettings == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupOutput = AddGroup(category);
  if (groupOutput == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupDeintSW = AddGroup(category);
  if (groupDeintSW == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupDeintHW = AddGroup(category);
  if (groupDeintHW == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVVideo: unable to setup settings");
    return;
  }
  CSettingGroup *groupReset = AddGroup(category);
  if (groupReset == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLAVAudio: unable to setup settings");
    return;
  }

  // Get settings from the current running filter
  IBaseFilter *pBF;
  CGraphFilters::Get()->GetInternalFilter(CGraphFilters::INTERNAL_LAVVIDEO, &pBF);
  CGraphFilters::Get()->GetLavSettings(CGraphFilters::INTERNAL_LAVVIDEO, pBF);

  StaticIntegerSettingOptions entries;
  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

  // BUTTON
  AddButton(groupProperty, LAVVIDEO_PROPERTYPAGE, 80013, 0);

  // TRAYICON
  AddToggle(group, LAVVIDEO_TRAYICON, 80001, 0, lavSettings.video_bTrayIcon);

  // HW ACCELERATION
  entries.clear();
  for (unsigned int i = 0; i < 5; i++)
    entries.emplace_back(80200 + i, i);
  AddList(groupHW, LAVVIDEO_HWACCEL, 80005, 0, lavSettings.video_dwHWAccel, entries, 80005);

  entries.clear();
  for (unsigned int i = 0; i < 17; i++)
    entries.emplace_back(80100 + i, i);
  AddList(groupHW, LAVVIDEO_NUMTHREADS, 80003, 0, lavSettings.video_dwNumThreads, entries, 80003);

  // SETTINGS
  AddToggle(groupSettings, LAVVIDEO_STREAMAR, 80002, 0, lavSettings.video_dwStreamAR);

  entries.clear();
  entries.emplace_back(80100, 0);
  entries.emplace_back(80205, 1);
  entries.emplace_back(80206, 2);
  AddList(groupSettings, LAVVIDEO_DEINTFILEDORDER, 80009, 0, lavSettings.video_dwDeintFieldOrder, entries, 80009);

  entries.clear();
  entries.emplace_back(80100, 0);
  entries.emplace_back(80207, 1);
  entries.emplace_back(80208, 2);
  entries.emplace_back(80209, 3);
  AddList(groupSettings, LAVVIDEO_DEINTMODE, 80010, 0, (LAVDeintMode)lavSettings.video_deintMode, entries, 80010);

  // OUTPUT RANGE
  entries.clear();
  entries.emplace_back(80214, 1);
  entries.emplace_back(80215, 2);
  entries.emplace_back(80216, 0);
  AddList(groupOutput, LAVVIDEO_RGBRANGE, 80004, 0, lavSettings.video_dwRGBRange, entries, 80004);

  entries.clear();
  entries.emplace_back(80212, 0);
  entries.emplace_back(80213, 1);
  AddList(groupOutput, LAVVIDEO_DITHERMODE, 80012, 0, lavSettings.video_dwDitherMode, entries, 80012);

  // DEINT HW/SW
  AddToggle(groupDeintHW, LAVVIDEO_HWDEINTMODE, 80006, 0, lavSettings.video_dwHWDeintMode);
  entries.clear();
  entries.emplace_back(80210, 1);
  entries.emplace_back(80211, 0);
  AddList(groupDeintHW, LAVVIDEO_HWDEINTOUT, 80007, 0, lavSettings.video_dwHWDeintOutput, entries, 80007);

  entries.clear();
  entries.emplace_back(70117, 0);
  entries.emplace_back(80217, 1);
  entries.emplace_back(80218, 2);
  entries.emplace_back(80219, 3);
  AddList(groupDeintSW, LAVVIDEO_SWDEINTMODE, 80011, 0, lavSettings.video_dwSWDeintMode, entries, 800011);
  entries.clear();
  entries.emplace_back(80210, 1);
  entries.emplace_back(80211, 0);
  AddList(groupDeintSW, LAVVIDEO_SWDEINTOUT, 80007, 0, lavSettings.video_dwSWDeintOutput, entries, 80007);

  // BUTTON RESET
  if (!g_application.m_pPlayer->IsPlayingVideo())
    AddButton(groupReset, LAVVIDEO_RESET, 10041, 0);
}

void CGUIDialogLAVVideo::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);
  const std::string &settingId = setting->GetId();

  if (settingId == LAVVIDEO_HWACCEL)
    lavSettings.video_dwHWAccel = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_NUMTHREADS)
    lavSettings.video_dwNumThreads = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_TRAYICON)
    lavSettings.video_bTrayIcon = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVVIDEO_STREAMAR)
    lavSettings.video_dwStreamAR = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVVIDEO_DEINTFILEDORDER)
    lavSettings.video_dwDeintFieldOrder = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_DEINTMODE)
    lavSettings.video_deintMode = (LAVDeintMode)static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_RGBRANGE)
    lavSettings.video_dwRGBRange = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_DITHERMODE)
    lavSettings.video_dwDitherMode = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_HWDEINTMODE)
    lavSettings.video_dwHWDeintMode = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVVIDEO_HWDEINTOUT)
    lavSettings.video_dwHWDeintOutput = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
  if (settingId == LAVVIDEO_SWDEINTMODE)
    lavSettings.video_dwSWDeintMode = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
  if (settingId == LAVVIDEO_SWDEINTOUT)
    lavSettings.video_dwSWDeintOutput = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());

  HideUnused();

  // Get current running filter
  IBaseFilter *pBF;
  CGraphFilters::Get()->GetInternalFilter(CGraphFilters::INTERNAL_LAVVIDEO, &pBF);

  // Set settings changes into the running filter
  CGraphFilters::Get()->SetLavSettings(CGraphFilters::INTERNAL_LAVVIDEO, pBF);

  // Save new settings into DSPlayer DB
  CGraphFilters::Get()->SaveLavSettings(CGraphFilters::INTERNAL_LAVVIDEO);
}

void CGUIDialogLAVVideo::OnSettingAction(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingAction(setting);
  const std::string &settingId = setting->GetId();

  if (settingId == LAVVIDEO_PROPERTYPAGE)
  {
    CGraphFilters::Get()->ShowInternalPPage(CGraphFilters::INTERNAL_LAVVIDEO, true);
    this->Close();
  }

  if (settingId == LAVVIDEO_RESET)
  {
    if (!CGUIDialogYesNo::ShowAndGetInput(10041, 10042, 0, 0))
      return;

    CGraphFilters::Get()->EraseLavSetting(CGraphFilters::INTERNAL_LAVVIDEO);
    this->Close();
  }
}

void CGUIDialogLAVVideo::HideUnused()
{
  if (!m_allowchange)
    return;

  m_allowchange = false;

  bool bValue;

  CSetting *setting;

  // HIDE / SHOW

  // HWDEINT
  setting = m_settingsManager->GetSetting(LAVVIDEO_HWDEINTMODE);
  bValue = static_cast<const CSettingBool*>(setting)->GetValue();
  SetVisible(LAVVIDEO_HWDEINTOUT, bValue);

  // SWDEINT
  setting = m_settingsManager->GetSetting(LAVVIDEO_SWDEINTMODE);
  bValue = static_cast<const CSettingBool*>(setting)->GetValue();
  SetVisible(LAVVIDEO_SWDEINTOUT, bValue);

  m_allowchange = true;
}

void CGUIDialogLAVVideo::SetVisible(std::string id, bool visible)
{
  CSetting *setting = m_settingsManager->GetSetting(id);
  if (setting->IsEnabled() && visible)
    return;

  setting->SetVisible(true);
  setting->SetEnabled(visible);
}

