/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogAudioSettings.h"

#include <string>
#include <vector>

#include "addons/Skin.h"
#include "Application.h"
#include "ServiceBroker.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "cores/IPlayer.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "GUIPassword.h"
#include "profiles/ProfileManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "URL.h"
#include "utils/LangCodeExpander.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"

#ifdef HAS_DS_PLAYER
#include "guilib/GUIWindowManager.h"
#include "guilib/GUIComponent.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "GUIInfoManager.h"
#include "input/Key.h"
#include "cores/DSPlayer/dsgraph.h"
#endif

#define SETTING_AUDIO_VOLUME                   "audio.volume"
#define SETTING_AUDIO_VOLUME_AMPLIFICATION     "audio.volumeamplification"
#define SETTING_AUDIO_CENTERMIXLEVEL           "audio.centermixlevel"
#define SETTING_AUDIO_DELAY                    "audio.delay"
#define SETTING_AUDIO_STREAM                   "audio.stream"
#define SETTING_AUDIO_PASSTHROUGH              "audio.digitalanalog"
#define SETTING_AUDIO_MAKE_DEFAULT             "audio.makedefault"

CGUIDialogAudioSettings::CGUIDialogAudioSettings()
  : CGUIDialogSettingsManualBase(WINDOW_DIALOG_AUDIO_OSD_SETTINGS, "DialogSettings.xml")
{ }

CGUIDialogAudioSettings::~CGUIDialogAudioSettings() = default;

void CGUIDialogAudioSettings::FrameMove()
{
  // update the volume setting if necessary
  float newVolume = g_application.GetVolume(false);
  if (newVolume != m_volume)
    GetSettingsManager()->SetNumber(SETTING_AUDIO_VOLUME, newVolume);

  if (g_application.GetAppPlayer().HasPlayer())
  {
    const CVideoSettings videoSettings = g_application.GetAppPlayer().GetVideoSettings();

    // these settings can change on the fly
    //! @todo (needs special handling): m_settingsManager->SetInt(SETTING_AUDIO_STREAM, g_application.GetAppPlayer().GetAudioStream());
#ifdef HAS_DS_PLAYER
    if (m_bIsDSPlayer)
      GetSettingsManager()->SetNumber(SETTING_AUDIO_DELAY, -videoSettings.m_AudioDelay);
    else
#endif
    GetSettingsManager()->SetNumber(SETTING_AUDIO_DELAY, videoSettings.m_AudioDelay);
    GetSettingsManager()->SetBool(SETTING_AUDIO_PASSTHROUGH, CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_AUDIOOUTPUT_PASSTHROUGH));
  }

  CGUIDialogSettingsManualBase::FrameMove();
}

std::string CGUIDialogAudioSettings::FormatDelay(float value, float interval)
{
  if (fabs(value) < 0.5f * interval)
    return StringUtils::Format(g_localizeStrings.Get(22003).c_str(), 0.0);
  if (value < 0)
    return StringUtils::Format(g_localizeStrings.Get(22004).c_str(), fabs(value));

  return StringUtils::Format(g_localizeStrings.Get(22005).c_str(), value);
}

std::string CGUIDialogAudioSettings::FormatDecibel(float value)
{
  return StringUtils::Format(g_localizeStrings.Get(14054).c_str(), value);
}

std::string CGUIDialogAudioSettings::FormatPercentAsDecibel(float value)
{
  return StringUtils::Format(g_localizeStrings.Get(14054).c_str(), CAEUtil::PercentToGain(value));
}

void CGUIDialogAudioSettings::OnSettingChanged(std::shared_ptr<const CSetting> setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);

  const std::string &settingId = setting->GetId();
  if (settingId == SETTING_AUDIO_VOLUME)
  {
    m_volume = static_cast<float>(std::static_pointer_cast<const CSettingNumber>(setting)->GetValue());
    g_application.SetVolume(m_volume, false); // false - value is not in percent
  }
  else if (settingId == SETTING_AUDIO_VOLUME_AMPLIFICATION)
  {
    float value = static_cast<float>(std::static_pointer_cast<const CSettingNumber>(setting)->GetValue());
    g_application.GetAppPlayer().SetDynamicRangeCompression((long)(value * 100));
  }
  else if (settingId == SETTING_AUDIO_CENTERMIXLEVEL)
  {
    CVideoSettings vs = g_application.GetAppPlayer().GetVideoSettings();
    vs.m_CenterMixLevel = std::static_pointer_cast<const CSettingInt>(setting)->GetValue();
    g_application.GetAppPlayer().SetVideoSettings(vs);
  }
  else if (settingId == SETTING_AUDIO_DELAY)
  {
    float value = static_cast<float>(std::static_pointer_cast<const CSettingNumber>(setting)->GetValue());
#ifdef HAS_DS_PLAYER
if (m_bIsDSPlayer) value = -value;
#endif
    g_application.GetAppPlayer().SetAVDelay(value);
  }
  else if (settingId == SETTING_AUDIO_STREAM)
  {
    m_audioStream = std::static_pointer_cast<const CSettingInt>(setting)->GetValue();
    // only change the audio stream if a different one has been asked for
    if (g_application.GetAppPlayer().GetAudioStream() != m_audioStream)
    {
      g_application.GetAppPlayer().SetAudioStream(m_audioStream);    // Set the audio stream to the one selected
    }
  }
  else if (settingId == SETTING_AUDIO_PASSTHROUGH)
  {
    m_passthrough = std::static_pointer_cast<const CSettingBool>(setting)->GetValue();
    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::SETTING_AUDIOOUTPUT_PASSTHROUGH, m_passthrough);
  }
}

void CGUIDialogAudioSettings::OnSettingAction(std::shared_ptr<const CSetting> setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingAction(setting);

  const std::string &settingId = setting->GetId();
  if (settingId == SETTING_AUDIO_MAKE_DEFAULT)
    Save();
}

void CGUIDialogAudioSettings::Save()
{
  const std::shared_ptr<CProfileManager> profileManager = CServiceBroker::GetSettingsComponent()->GetProfileManager();

  if (!g_passwordManager.CheckSettingLevelLock(SettingLevel::Expert) &&
      profileManager->GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE)
    return;

  // prompt user if they are sure
  if (!CGUIDialogYesNo::ShowAndGetInput(CVariant{12376}, CVariant{12377}))
    return;

  // reset the settings
  CVideoDatabase db;
  if (!db.Open())
    return;

  db.EraseAllVideoSettings();
  db.Close();

  CMediaSettings::GetInstance().GetDefaultVideoSettings() = g_application.GetAppPlayer().GetVideoSettings();
  CMediaSettings::GetInstance().GetDefaultVideoSettings().m_AudioStream = -1;
  CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
}

void CGUIDialogAudioSettings::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();

  SetHeading(13396);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_OKAY_BUTTON);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 15067);
}

void CGUIDialogAudioSettings::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

#ifdef HAS_DS_PLAYER
  m_bIsDSPlayer = (g_application.GetCurrentPlayer() == "DSPlayer");
#endif

  const std::shared_ptr<CSettingCategory> category = AddCategory("audiosubtitlesettings", -1);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogAudioSettings: unable to setup settings");
    return;
  }

  // get all necessary setting groups
  const std::shared_ptr<CSettingGroup> groupAudio = AddGroup(category);
  if (groupAudio == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogAudioSettings: unable to setup settings");
    return;
  }
  const std::shared_ptr<CSettingGroup> groupSubtitles = AddGroup(category);
  if (groupSubtitles == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogAudioSettings: unable to setup settings");
    return;
  }

  const std::shared_ptr<CSettingGroup> groupSaveAsDefault = AddGroup(category);
  if (groupSaveAsDefault == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogAudioSettings: unable to setup settings");
    return;
  }


  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  const CVideoSettings videoSettings = g_application.GetAppPlayer().GetVideoSettings();

  if (g_application.GetAppPlayer().HasPlayer())
  {
    g_application.GetAppPlayer().GetAudioCapabilities(m_audioCaps);
  }

  // register IsPlayingPassthrough condition
  GetSettingsManager()->AddDynamicCondition("IsPlayingPassthrough", IsPlayingPassthrough);

  CSettingDependency dependencyAudioOutputPassthroughDisabled(SettingDependencyType::Enable, GetSettingsManager());
  dependencyAudioOutputPassthroughDisabled.Or()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(SETTING_AUDIO_PASSTHROUGH, "false", SettingDependencyOperator::Equals, false, GetSettingsManager())))
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition("IsPlayingPassthrough", "", "", true, GetSettingsManager())));
  SettingDependencies depsAudioOutputPassthroughDisabled;
  depsAudioOutputPassthroughDisabled.push_back(dependencyAudioOutputPassthroughDisabled);

  // audio settings
  // audio volume setting
  m_volume = g_application.GetVolume(false);
  std::shared_ptr<CSettingNumber> settingAudioVolume = AddSlider(groupAudio, SETTING_AUDIO_VOLUME, 13376, SettingLevel::Basic, m_volume, 14054, VOLUME_MINIMUM, VOLUME_MAXIMUM / 100.0f, VOLUME_MAXIMUM);
  settingAudioVolume->SetDependencies(depsAudioOutputPassthroughDisabled);
  std::static_pointer_cast<CSettingControlSlider>(settingAudioVolume->GetControl())->SetFormatter(SettingFormatterPercentAsDecibel);

  // audio volume amplification setting
  if (SupportsAudioFeature(IPC_AUD_AMP))
  {
    std::shared_ptr<CSettingNumber> settingAudioVolumeAmplification = AddSlider(groupAudio, SETTING_AUDIO_VOLUME_AMPLIFICATION, 660, SettingLevel::Basic, videoSettings.m_VolumeAmplification, 14054, VOLUME_DRC_MINIMUM * 0.01f, (VOLUME_DRC_MAXIMUM - VOLUME_DRC_MINIMUM) / 6000.0f, VOLUME_DRC_MAXIMUM * 0.01f);
    settingAudioVolumeAmplification->SetDependencies(depsAudioOutputPassthroughDisabled);
  }

  // downmix: center mix level
  {
    AddSlider(groupAudio, SETTING_AUDIO_CENTERMIXLEVEL, 39112, SettingLevel::Basic,
              videoSettings.m_CenterMixLevel, 14050, -10, 1, 30,
              -1, false, false, true, 39113);
  }

  // audio delay setting
  if (SupportsAudioFeature(IPC_AUD_OFFSET))
  {
    std::shared_ptr<CSettingNumber> settingAudioDelay = AddSlider(groupAudio, SETTING_AUDIO_DELAY, 297, SettingLevel::Basic, videoSettings.m_AudioDelay, 0, -CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_videoAudioDelayRange, 0.025f, CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_videoAudioDelayRange, 297, usePopup);
    std::static_pointer_cast<CSettingControlSlider>(settingAudioDelay->GetControl())->SetFormatter(SettingFormatterDelay);
  }

  // audio stream setting
  if (SupportsAudioFeature(IPC_AUD_SELECT_STREAM))
    AddAudioStreams(groupAudio, SETTING_AUDIO_STREAM);

  // audio digital/analog setting
  if (SupportsAudioFeature(IPC_AUD_SELECT_OUTPUT))
  {
    m_passthrough = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_AUDIOOUTPUT_PASSTHROUGH);
    AddToggle(groupAudio, SETTING_AUDIO_PASSTHROUGH, 348, SettingLevel::Basic, m_passthrough);
  }

  // subtitle stream setting
  AddButton(groupSaveAsDefault, SETTING_AUDIO_MAKE_DEFAULT, 12376, SettingLevel::Basic);
}

bool CGUIDialogAudioSettings::SupportsAudioFeature(int feature)
{
  for (Features::iterator itr = m_audioCaps.begin(); itr != m_audioCaps.end(); ++itr)
  {
    if (*itr == feature || *itr == IPC_AUD_ALL)
      return true;
  }

  return false;
}

void CGUIDialogAudioSettings::AddAudioStreams(std::shared_ptr<CSettingGroup> group, const std::string &settingId)
{
  if (group == NULL || settingId.empty())
    return;

  m_audioStream = g_application.GetAppPlayer().GetAudioStream();
  if (m_audioStream < 0)
    m_audioStream = 0;

  AddList(group, settingId, 460, SettingLevel::Basic, m_audioStream, AudioStreamsOptionFiller, 460);
}

bool CGUIDialogAudioSettings::IsPlayingPassthrough(const std::string &condition, const std::string &value, SettingConstPtr setting, void *data)
{
  return g_application.GetAppPlayer().IsPassthrough();
}

void CGUIDialogAudioSettings::AudioStreamsOptionFiller(SettingConstPtr setting, std::vector< std::pair<std::string, int> > &list, int &current, void *data)
{
  int audioStreamCount = g_application.GetAppPlayer().GetAudioStreamCount();

  std::string strFormat = "%s - %s - %d " + g_localizeStrings.Get(10127);
  std::string strUnknown = "[" + g_localizeStrings.Get(13205) + "]";

  // cycle through each audio stream and add it to our list control
  for (int i = 0; i < audioStreamCount; ++i)
  {
    std::string strItem;
    std::string strLanguage;

    AudioStreamInfo info;
    g_application.GetAppPlayer().GetAudioStreamInfo(i, info);

    if (!g_LangCodeExpander.Lookup(info.language, strLanguage))
      strLanguage = strUnknown;

    if (info.name.length() == 0)
      info.name = strUnknown;

    strItem = StringUtils::Format(strFormat, strLanguage.c_str(), info.name.c_str(), info.channels);

#ifdef HAS_DS_PLAYER
    if (g_application.GetCurrentPlayer() == "DSPlayer")
      strItem = info.name;
#endif

    strItem += FormatFlags(info.flags);
    strItem += StringUtils::Format(" (%i/%i)", i + 1, audioStreamCount);
    list.push_back(make_pair(strItem, i));
  }

  if (list.empty())
  {
    list.push_back(make_pair(g_localizeStrings.Get(231), -1));
    current = -1;
  }
}

std::string CGUIDialogAudioSettings::SettingFormatterDelay(std::shared_ptr<const CSettingControlSlider> control, const CVariant &value, const CVariant &minimum, const CVariant &step, const CVariant &maximum)
{
  if (!value.isDouble())
    return "";

  float fValue = value.asFloat();
  float fStep = step.asFloat();

  if (fabs(fValue) < 0.5f * fStep)
    return StringUtils::Format(g_localizeStrings.Get(22003).c_str(), 0.0);

#ifdef HAS_DS_PLAYER
  if (g_application.GetCurrentPlayer() == "DSPlayer")
  {
    if (fValue < 0)
      return StringUtils::Format(g_localizeStrings.Get(22005).c_str(), fabs(fValue));

    return StringUtils::Format(g_localizeStrings.Get(22004).c_str(), fValue);
  }
  else
  {
#endif
    if (fValue < 0)
      return StringUtils::Format(g_localizeStrings.Get(22004).c_str(), fabs(fValue));

    return StringUtils::Format(g_localizeStrings.Get(22005).c_str(), fValue);
#ifdef HAS_DS_PLAYER
  }
#endif
}

std::string CGUIDialogAudioSettings::SettingFormatterPercentAsDecibel(std::shared_ptr<const CSettingControlSlider> control, const CVariant &value, const CVariant &minimum, const CVariant &step, const CVariant &maximum)
{
  if (control == NULL || !value.isDouble())
    return "";

  std::string formatString = control->GetFormatString();
  if (control->GetFormatLabel() > -1)
    formatString = g_localizeStrings.Get(control->GetFormatLabel());

  return StringUtils::Format(formatString.c_str(), CAEUtil::PercentToGain(value.asFloat()));
}

std::string CGUIDialogAudioSettings::FormatFlags(StreamFlags flags)
{
  std::vector<std::string> localizedFlags;
  if (flags & StreamFlags::FLAG_DEFAULT)
    localizedFlags.emplace_back(g_localizeStrings.Get(39105));
  if (flags & StreamFlags::FLAG_FORCED)
    localizedFlags.emplace_back(g_localizeStrings.Get(39106));
  if (flags & StreamFlags::FLAG_HEARING_IMPAIRED)
    localizedFlags.emplace_back(g_localizeStrings.Get(39107));
  if (flags &  StreamFlags::FLAG_VISUAL_IMPAIRED)
    localizedFlags.emplace_back(g_localizeStrings.Get(39108));
  if (flags & StreamFlags::FLAG_ORIGINAL)
    localizedFlags.emplace_back(g_localizeStrings.Get(39111));

  std::string formated = StringUtils::Join(localizedFlags, ", ");

  if (!formated.empty())
    formated = StringUtils::Format(" [%s]", formated);

  return formated;
}

#ifdef HAS_DS_PLAYER
void CGUIDialogAudioSettings::ShowAudioSelector()
{
  int count = g_application.GetAppPlayer().GetAudioStreamCount();

  if (count <= 0)
  {
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(460), g_localizeStrings.Get(55059), 2000, false, 300);
    return;
  }

  CGUIDialogSelect *pDlg = (CGUIDialogSelect *)CServiceBroker::GetGUI()->GetWindowManager().GetWindow(WINDOW_DIALOG_SELECT);
  if (!pDlg)
    return;

  for (int i = 0; i < count; ++i)
  {
    std::string strName;
    AudioStreamInfo audio;
    g_application.GetAppPlayer().GetAudioStreamInfo(i, audio);
    strName = audio.name;
    if (strName.length() == 0)
      strName = "Unnamed";

    strName += StringUtils::Format(" (%i/%i)", i + 1, count);
    pDlg->Add(strName);
  }

  int selected = g_application.GetAppPlayer().GetAudioStream();

  if (selected < 0) selected = 0;

  pDlg->SetHeading(460);
  pDlg->SetSelected(selected);
  pDlg->Open();

  selected = pDlg->GetSelectedItem();

  if (selected != -1 && g_application.GetAppPlayer().GetAudioStream() != selected)
  {
    g_application.GetAppPlayer().SetAudioStream(selected);    // Set the audio stream to the one selected
  }
}
#endif
