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

#include "GUIDialogSanear.h"
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
#include "FGLoader.h"

#define SANEAR_PROPERTYPAGE      "sanear.propertypage"
#define SANEAR_DEVICES           "sanear.devices"
#define SANEAR_EXCLUSIVE         "sanear.exclusive"
#define SANEAR_ALLOWBITSTREAM    "sanear.allowbitstream"

#define SANEAR_CROSSFEED         "sanear.crossfeed"
#define SANEAR_CMOY              "sanear.cmoy"
#define SANEAR_JMEIER            "sanear.jmeier"
#define SANEAR_CUTOFF            "sanear.cutoff"
#define SANEAR_LEVEL             "sanear.level"

using namespace std;

CGUIDialogSanear::CGUIDialogSanear()
  : CGUIDialogSettingsManualBase(WINDOW_DIALOG_SANEAR, "DialogSettings.xml")
{
  m_allowchange = true;
}


CGUIDialogSanear::~CGUIDialogSanear()
{ }

void CGUIDialogSanear::OnInitWindow()
{
  CGUIDialogSettingsManualBase::OnInitWindow();

  HideUnused();
}

void CGUIDialogSanear::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();

  SetHeading(55123);

  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_OKAY_BUTTON);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 15067);
}

void CGUIDialogSanear::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  CSettingCategory *category = AddCategory("dsplayerlavaudio", -1);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogSanear: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupProperty = AddGroup(category);
  if (groupProperty == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogSanear: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *group = AddGroup(category);
  if (group == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogSanear: unable to setup settings");
    return;
  }
  CSettingGroup *groupBitstream = AddGroup(category);
  if (groupBitstream == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogSanear: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CSettingGroup *groupOptions = AddGroup(category);
  if (groupOptions == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogSanear: unable to setup settings");
    return;
  }

  std::string sValue;
  bool bValue;
  int iValue;

  // BUTTON
  AddButton(groupProperty, SANEAR_PROPERTYPAGE, 80013, 0);

  // DEVICES
  sValue = CSettings::GetInstance().GetString(CSettings::SETTING_DSPLAYER_SANEARDEVICES);
  AddList(groupBitstream, SANEAR_DEVICES, 55120, 0, sValue, CFGLoader::SettingOptionsSanearDevicesFiller, 81027);
 
  // BITSTREAM
  bValue = CSettings::GetInstance().GetBool(CSettings::SETTING_DSPLAYER_SANEAREXCLUSIVE);
  AddToggle(groupBitstream, SANEAR_EXCLUSIVE, 55121, 0, bValue);
  bValue = CSettings::GetInstance().GetBool(CSettings::SETTING_DSPLAYER_SANEARALLOWBITSTREAM);
  AddToggle(groupBitstream, SANEAR_ALLOWBITSTREAM, 55122, 0, bValue);

  // STEREOCROSSFEED
  bValue = CSettings::GetInstance().GetBool(CSettings::SETTING_DSPLAYER_SANEARSTEREOCROSSFEED);
  AddToggle(groupOptions, SANEAR_CROSSFEED, 55124, 0, bValue);

  AddButton(groupOptions, SANEAR_CMOY, 55125, 0);
  AddButton(groupOptions, SANEAR_JMEIER, 55126, 0);

  iValue = CSettings::GetInstance().GetInt(CSettings::SETTING_DSPLAYER_SANEARCUTOFF);
  AddSlider(groupOptions, SANEAR_CUTOFF, 55127, 0, iValue, "%i Hz", SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_MIN, 1, SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_MAX);

  iValue = CSettings::GetInstance().GetInt(CSettings::SETTING_DSPLAYER_SANEARLEVEL);
  AddSlider(groupOptions, SANEAR_LEVEL, 55128, 0, iValue, "%i dB", SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_MIN, 1, SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_MAX);

}

void CGUIDialogSanear::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);
  const std::string &settingId = setting->GetId();

  std::string sValue;
  bool bValue;
  int iValue;

  if (settingId == SANEAR_DEVICES)
  {
    sValue = static_cast<std::string>(static_cast<const CSettingString*>(setting)->GetValue());
    CSettings::GetInstance().SetString(CSettings::SETTING_DSPLAYER_SANEARDEVICES, sValue);
  }
  if (settingId == SANEAR_EXCLUSIVE)
  {
    bValue = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
    CSettings::GetInstance().SetBool(CSettings::SETTING_DSPLAYER_SANEAREXCLUSIVE, bValue);
  }
  if (settingId == SANEAR_ALLOWBITSTREAM)
  {
    bValue = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
    CSettings::GetInstance().SetBool(CSettings::SETTING_DSPLAYER_SANEARALLOWBITSTREAM, bValue);
  }
  if (settingId == SANEAR_CROSSFEED)
  {
    bValue = static_cast<BOOL>(static_cast<const CSettingBool*>(setting)->GetValue());
    CSettings::GetInstance().SetBool(CSettings::SETTING_DSPLAYER_SANEARALLOWBITSTREAM, bValue);
  }
  if (settingId == SANEAR_CUTOFF)
  {
    iValue = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
    CSettings::GetInstance().SetInt(CSettings::SETTING_DSPLAYER_SANEARCUTOFF, iValue);
  }
  if (settingId == SANEAR_LEVEL)
  {
    iValue = static_cast<int>(static_cast<const CSettingInt*>(setting)->GetValue());
    CSettings::GetInstance().SetInt(CSettings::SETTING_DSPLAYER_SANEARLEVEL, iValue);;
  }

  /*
  if (settingId == LAVAUDIO_MIXINGLFE)
    lavSettings.audio_dwMixingLFELevel = FloatToDw(static_cast<float>(static_cast<const CSettingNumber*>(setting)->GetValue()));
  */
  HideUnused();
}

void CGUIDialogSanear::OnSettingAction(const CSetting *setting)
{
  if (setting == NULL)
    return;

  int iValue;

  CGUIDialogSettingsManualBase::OnSettingAction(setting);
  const std::string &settingId = setting->GetId();

  if (settingId == SANEAR_PROPERTYPAGE)
  {
    CGraphFilters::Get()->ShowInternalPPage(CGraphFilters::INTERNAL_SANEAR, true);
    this->Close();
  }
  else if (settingId == SANEAR_CMOY)
  {
    m_settingsManager->SetInt(SANEAR_CUTOFF, SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_CMOY);
    m_settingsManager->SetInt(SANEAR_LEVEL, SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_CMOY);
  }
  else if (settingId == SANEAR_JMEIER)
  {
    m_settingsManager->SetInt(SANEAR_CUTOFF, SaneAudioRenderer::ISettings::CROSSFEED_CUTOFF_FREQ_JMEIER);
    m_settingsManager->SetInt(SANEAR_LEVEL, SaneAudioRenderer::ISettings::CROSSFEED_LEVEL_JMEIER);
  }
}

void CGUIDialogSanear::HideUnused()
{
  if (!m_allowchange)
    return;

  m_allowchange = false;

  bool bValue;

  CSetting *setting;

  // HIDE / SHOW

  // BITSTREAM
  setting = m_settingsManager->GetSetting(SANEAR_EXCLUSIVE);
  bValue = static_cast<const CSettingBool*>(setting)->GetValue();
  SetVisible(SANEAR_ALLOWBITSTREAM, bValue);

  // STEERO CROSSFEED
  setting = m_settingsManager->GetSetting(SANEAR_CROSSFEED);
  bValue = static_cast<const CSettingBool*>(setting)->GetValue();
  SetVisible(SANEAR_CMOY, bValue);
  SetVisible(SANEAR_JMEIER, bValue);
  SetVisible(SANEAR_CUTOFF, bValue);
  SetVisible(SANEAR_LEVEL, bValue);

  m_allowchange = true;
}

void CGUIDialogSanear::SetVisible(std::string id, bool visible)
{
  CSetting *setting = m_settingsManager->GetSetting(id);
  if (setting->IsEnabled() && visible)
    return;

  setting->SetVisible(true);
  setting->SetEnabled(visible);
}


