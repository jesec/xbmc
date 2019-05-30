/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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

#include "Addon.h"

#include <algorithm>
#include <string.h>
#include <ostream>
#include <utility>
#include <vector>

#include "AddonManager.h"
#include "addons/Service.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "guilib/LocalizeStrings.h"
#include "RepositoryUpdater.h"
#include "settings/Settings.h"
#include "ServiceBroker.h"
#include "system.h"
#include "URL.h"
#include "Util.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/XMLUtils.h"

#ifdef HAS_DS_PLAYER
#include "settings/MediaSettings.h"
#endif

#ifdef HAS_PYTHON
#include "interfaces/python/XBPython.h"
#endif
#if defined(TARGET_DARWIN)
#include "../platform/darwin/OSXGNUReplacements.h"
#endif
#ifdef TARGET_FREEBSD
#include "freebsd/FreeBSDGNUReplacements.h"
#endif

using XFILE::CDirectory;
using XFILE::CFile;

namespace ADDON
{

/**
 * helper functions 
 *
 */

typedef struct
{
  const char* name;
  TYPE        type;
  int         pretty;
  const char* icon;
} TypeMapping;

static const TypeMapping types[] =
  {{"unknown",                           ADDON_UNKNOWN,                 0, "" },
   {"xbmc.metadata.scraper.albums",      ADDON_SCRAPER_ALBUMS,      24016, "DefaultAddonAlbumInfo.png" },
   {"xbmc.metadata.scraper.artists",     ADDON_SCRAPER_ARTISTS,     24017, "DefaultAddonArtistInfo.png" },
   {"xbmc.metadata.scraper.movies",      ADDON_SCRAPER_MOVIES,      24007, "DefaultAddonMovieInfo.png" },
   {"xbmc.metadata.scraper.musicvideos", ADDON_SCRAPER_MUSICVIDEOS, 24015, "DefaultAddonMusicVideoInfo.png" },
   {"xbmc.metadata.scraper.tvshows",     ADDON_SCRAPER_TVSHOWS,     24014, "DefaultAddonTvInfo.png" },
   {"xbmc.metadata.scraper.library",     ADDON_SCRAPER_LIBRARY,     24083, "DefaultAddonInfoLibrary.png" },
   {"xbmc.ui.screensaver",               ADDON_SCREENSAVER,         24008, "DefaultAddonScreensaver.png" },
   {"xbmc.player.musicviz",              ADDON_VIZ,                 24010, "DefaultAddonVisualization.png" },
   {"xbmc.python.pluginsource",          ADDON_PLUGIN,              24005, "" },
   {"xbmc.python.script",                ADDON_SCRIPT,              24009, "" },
   {"xbmc.python.weather",               ADDON_SCRIPT_WEATHER,      24027, "DefaultAddonWeather.png" },
   {"xbmc.python.lyrics",                ADDON_SCRIPT_LYRICS,       24013, "DefaultAddonLyrics.png" },
   {"xbmc.python.library",               ADDON_SCRIPT_LIBRARY,      24081, "DefaultAddonHelper.png" },
   {"xbmc.python.module",                ADDON_SCRIPT_MODULE,       24082, "DefaultAddonLibrary.png" },
   {"xbmc.subtitle.module",              ADDON_SUBTITLE_MODULE,     24012, "DefaultAddonSubtitles.png" },
   {"kodi.context.item",                 ADDON_CONTEXT_ITEM,        24025, "DefaultAddonContextItem.png" },
   {"kodi.game.controller",              ADDON_GAME_CONTROLLER,     35050, "DefaultAddonGame.png" },
   {"xbmc.gui.skin",                     ADDON_SKIN,                  166, "DefaultAddonSkin.png" },
   {"xbmc.webinterface",                 ADDON_WEB_INTERFACE,         199, "DefaultAddonWebSkin.png" },
   {"xbmc.addon.repository",             ADDON_REPOSITORY,          24011, "DefaultAddonRepository.png" },
   {"xbmc.pvrclient",                    ADDON_PVRDLL,              24019, "DefaultAddonPVRClient.png" },
   {"kodi.gameclient",                   ADDON_GAMEDLL,             35049, "DefaultAddonGame.png" },
   {"kodi.peripheral",                   ADDON_PERIPHERALDLL,       35010, "DefaultAddonPeripheral.png" },
   {"xbmc.addon.video",                  ADDON_VIDEO,                1037, "DefaultAddonVideo.png" },
   {"xbmc.addon.audio",                  ADDON_AUDIO,                1038, "DefaultAddonMusic.png" },
   {"xbmc.addon.image",                  ADDON_IMAGE,                1039, "DefaultAddonPicture.png" },
   {"xbmc.addon.executable",             ADDON_EXECUTABLE,           1043, "DefaultAddonProgram.png" },
   {"kodi.addon.game",                   ADDON_GAME,                35049, "DefaultAddonGame.png" },
   {"xbmc.audioencoder",                 ADDON_AUDIOENCODER,         200,  "DefaultAddonAudioEncoder.png" },
   {"kodi.audiodecoder",                 ADDON_AUDIODECODER,         201,  "DefaultAddonAudioDecoder.png" },
   {"xbmc.service",                      ADDON_SERVICE,             24018, "DefaultAddonService.png" },
   {"kodi.resource.images",              ADDON_RESOURCE_IMAGES,     24035, "DefaultAddonImages.png" },
   {"kodi.resource.language",            ADDON_RESOURCE_LANGUAGE,   24026, "DefaultAddonLanguage.png" },
   {"kodi.resource.uisounds",            ADDON_RESOURCE_UISOUNDS,   24006, "DefaultAddonUISounds.png" },
   {"kodi.resource.games",               ADDON_RESOURCE_GAMES,      35209, "DefaultAddonGame.png" },
   {"kodi.adsp",                         ADDON_ADSPDLL,             24135, "DefaultAddonAudioDSP.png" },
   {"kodi.inputstream",                  ADDON_INPUTSTREAM,         24048, "DefaultAddonInputstream.png" },
   {"kodi.vfs",                          ADDON_VFS,                 39013, "DefaultAddonVfs.png" },
   {"kodi.imagedecoder",                 ADDON_IMAGEDECODER,        39015, "DefaultAddonImageDecoder.png" },
  };

std::string TranslateType(ADDON::TYPE type, bool pretty/*=false*/)
{
  for (unsigned int index=0; index < ARRAY_SIZE(types); ++index)
  {
    const TypeMapping &map = types[index];
    if (type == map.type)
    {
      if (pretty && map.pretty)
        return g_localizeStrings.Get(map.pretty);
      else
        return map.name;
    }
  }
  return "";
}

TYPE TranslateType(const std::string &string)
{
  for (unsigned int index=0; index < ARRAY_SIZE(types); ++index)
  {
    const TypeMapping &map = types[index];
    if (string == map.name)
      return map.type;
  }

  return ADDON_UNKNOWN;
}

std::string GetIcon(ADDON::TYPE type)
{
  for (unsigned int index=0; index < ARRAY_SIZE(types); ++index)
  {
    const TypeMapping &map = types[index];
    if (type == map.type)
      return map.icon;
  }
  return "";
}

CAddon::CAddon(AddonProps props)
  : m_props(std::move(props))
  , m_userSettingsPath()
  , m_loadSettingsFailed(false)
  , m_hasUserSettings(false)
  , m_profilePath(StringUtils::Format("special://profile/addon_data/%s/", m_props.id.c_str()))
  , m_settings(nullptr)
{
  m_userSettingsPath = URIUtils::AddFileToFolder(m_profilePath, "settings.xml");
}

bool CAddon::MeetsVersion(const AddonVersion &version) const
{
  return m_props.minversion <= version && version <= m_props.version;
}

/**
 * Settings Handling
 */
bool CAddon::HasSettings()
{
  return LoadSettings();
}

bool CAddon::SettingsInitialized() const
{
  return m_settings != nullptr && m_settings->IsInitialized();
}

bool CAddon::SettingsLoaded() const
{
  return m_settings != nullptr && m_settings->IsLoaded();
}

bool CAddon::LoadSettings(bool bForce /* = false */)
{
  if (SettingsInitialized() && !bForce)
    return true;

  if (m_loadSettingsFailed)
    return false;

  // assume loading settings fails
  m_loadSettingsFailed = true;

  // reset the settings if we are forced to
  if (SettingsInitialized() && bForce)
    GetSettings()->Uninitialize();

  // load the settings definition XML file
  auto addonSettingsDefinitionFile = URIUtils::AddFileToFolder(m_props.path, "resources", "settings.xml");
  CXBMCTinyXML addonSettingsDefinitionDoc;
  if (!addonSettingsDefinitionDoc.LoadFile(addonSettingsDefinitionFile))
  {
    if (CFile::Exists(addonSettingsDefinitionFile))
    {
      CLog::Log(LOGERROR, "CAddon[%s]: unable to load: %s, Line %d\n%s",
        ID().c_str(), addonSettingsDefinitionFile.c_str(), addonSettingsDefinitionDoc.ErrorRow(), addonSettingsDefinitionDoc.ErrorDesc());
    }

    return false;
  }

  // initialize the settings definition
  if (!GetSettings()->Initialize(addonSettingsDefinitionDoc))
  {
    CLog::Log(LOGERROR, "CAddon[%s]: failed to initialize addon settings", ID().c_str());
    return false;
  }

  // loading settings didn't fail
  m_loadSettingsFailed = false;

  // load user settings / values
  LoadUserSettings();

  return true;
}

bool CAddon::HasUserSettings()
{
  if (!LoadSettings())
    return false;

  return SettingsLoaded() && m_hasUserSettings;
}

bool CAddon::ReloadSettings()
{
  return LoadSettings(true);
}

bool CAddon::LoadUserSettings()
{
  if (!SettingsInitialized())
    return false;

  m_hasUserSettings = false;

  // there are no user settings
  if (!CFile::Exists(m_userSettingsPath))
  {
    // mark the settings as loaded
    GetSettings()->SetLoaded();
    return true;
  }

  CXBMCTinyXML doc;
  if (!doc.LoadFile(m_userSettingsPath))
  {
    CLog::Log(LOGERROR, "CAddon[%s]: failed to load addon settings from %s", ID().c_str(), m_userSettingsPath.c_str());
    return false;
  }

  return SettingsFromXML(doc);
}

bool CAddon::HasSettingsToSave() const
{
  return SettingsLoaded();
}

void CAddon::SaveSettings(void)
{
  if (!HasSettingsToSave())
    return; // no settings to save

  // break down the path into directories
  std::string strAddon = URIUtils::GetDirectory(m_userSettingsPath);
  URIUtils::RemoveSlashAtEnd(strAddon);
  std::string strRoot = URIUtils::GetDirectory(strAddon);
  URIUtils::RemoveSlashAtEnd(strRoot);

  // create the individual folders
  if (!CDirectory::Exists(strRoot))
    CDirectory::Create(strRoot);
  if (!CDirectory::Exists(strAddon))
    CDirectory::Create(strAddon);

  // create the XML file
  CXBMCTinyXML doc;
  if (SettingsToXML(doc))
    doc.SaveFile(m_userSettingsPath);

  m_hasUserSettings = true;
  
  //push the settings changes to the running addon instance
  CAddonMgr::GetInstance().ReloadSettings(ID());
#ifdef HAS_PYTHON
  g_pythonParser.OnSettingsChanged(ID());
#endif
}

std::string CAddon::GetSetting(const std::string& key)
{
  if (key.empty() || !LoadSettings())
    return ""; // no settings available

  auto setting = m_settings->GetSetting(key);
  if (setting != nullptr)
    return setting->ToString();

  return "";
}

void CAddon::UpdateSetting(const std::string& key, const std::string& value)
{
  if (key.empty() || !LoadSettings())
    return;

  // try to get the setting
  auto setting = m_settings->GetSetting(key);

  // if the setting doesn't exist, try to add it
  if (setting == nullptr)
  {
    setting = m_settings->AddSetting(key, value);
    if (setting == nullptr)
    {
      CLog::Log(LOGERROR, "CAddon[%s]: failed to add undefined setting \"%s\"", ID().c_str(), key.c_str());
      return;
    }
  }

  setting->FromString(value);
}

bool CAddon::SettingsFromXML(const CXBMCTinyXML &doc, bool loadDefaults /* = false */)
{
  if (doc.RootElement() == nullptr)
    return false;

  // if the settings haven't been initialized yet, try it from the given XML
  if (!SettingsInitialized())
  {
    if (!GetSettings()->Initialize(doc))
    {
      CLog::Log(LOGERROR, "CAddon[%s]: failed to initialize addon settings", ID().c_str());
      return false;
    }
  }

  // reset all setting values to their default value
  if (loadDefaults)
    GetSettings()->SetDefaults();

  // try to load the setting's values from the given XML
  if (!GetSettings()->Load(doc))
  {
    CLog::Log(LOGERROR, "CAddon[%s]: failed to load user settings", ID().c_str());
    return false;
  }

  m_hasUserSettings = true;

  return true;
}

bool CAddon::SettingsToXML(CXBMCTinyXML &doc) const
{
  if (!SettingsInitialized())
    return false;

  if (!m_settings->Save(doc))
  {
    CLog::Log(LOGERROR, "CAddon[%s]: failed to save addon settings", ID().c_str());
    return false;
  }

  return true;
}

CAddonSettings* CAddon::GetSettings() const
{
  // initialize addon settings if necessary
  if (m_settings == nullptr)
    m_settings = std::make_shared<CAddonSettings>(enable_shared_from_this::shared_from_this());

  return m_settings.get();
}

std::string CAddon::LibPath() const
{
  if (m_props.libname.empty())
    return "";
  return URIUtils::AddFileToFolder(m_props.path, m_props.libname);
}

AddonVersion CAddon::GetDependencyVersion(const std::string &dependencyID) const
{
  const ADDON::ADDONDEPS &deps = GetDeps();
  ADDONDEPS::const_iterator it = deps.find(dependencyID);
  if (it != deps.end())
    return it->second.first;
  return AddonVersion("0.0.0");
}

void OnEnabled(const std::string& id)
{
  // If the addon is a special, call enabled handler
  AddonPtr addon;
  if (CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_PVRDLL) ||
      CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_ADSPDLL))
    return addon->OnEnabled();

  if (CAddonMgr::GetInstance().ServicesHasStarted())
  {
    if (CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_SERVICE))
      std::static_pointer_cast<CService>(addon)->Start();
  }

  if (CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_REPOSITORY))
    CRepositoryUpdater::GetInstance().ScheduleUpdate(); //notify updater there is a new addon
}

void OnDisabled(const std::string& id)
{

  AddonPtr addon;
  if (CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_PVRDLL, false) ||
      CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_ADSPDLL, false))
    return addon->OnDisabled();

  if (CAddonMgr::GetInstance().ServicesHasStarted())
  {
    if (CAddonMgr::GetInstance().GetAddon(id, addon, ADDON_SERVICE, false))
      std::static_pointer_cast<CService>(addon)->Stop();
  }
}

void OnPreInstall(const AddonPtr& addon)
{
  //Before installing we need to stop/unregister any local addon
  //that have this id, regardless of what the 'new' addon is.
  AddonPtr localAddon;

  if (CAddonMgr::GetInstance().ServicesHasStarted())
  {
    if (CAddonMgr::GetInstance().GetAddon(addon->ID(), localAddon, ADDON_SERVICE))
      std::static_pointer_cast<CService>(localAddon)->Stop();
  }

  //Fallback to the pre-install callback in the addon.
  //! @bug If primary extension point have changed we're calling the wrong method.
  addon->OnPreInstall();
}

void OnPostInstall(const AddonPtr& addon, bool update, bool modal)
{
  AddonPtr localAddon;
  if (CAddonMgr::GetInstance().ServicesHasStarted())
  {
    if (CAddonMgr::GetInstance().GetAddon(addon->ID(), localAddon, ADDON_SERVICE))
      std::static_pointer_cast<CService>(localAddon)->Start();
  }

  if (CAddonMgr::GetInstance().GetAddon(addon->ID(), localAddon, ADDON_REPOSITORY))
    CRepositoryUpdater::GetInstance().ScheduleUpdate(); //notify updater there is a new addon or version

#ifdef HAS_DS_PLAYER
  if (addon->ID() == "script.madvrsettings") 
  {
    CMediaSettings::GetInstance().GetCurrentMadvrSettings().UpdateSettings();
  }
#endif

  addon->OnPostInstall(update, modal);
}

void OnPreUnInstall(const AddonPtr& addon)
{
  AddonPtr localAddon;

  if (CAddonMgr::GetInstance().ServicesHasStarted())
  {
    if (CAddonMgr::GetInstance().GetAddon(addon->ID(), localAddon, ADDON_SERVICE))
      std::static_pointer_cast<CService>(localAddon)->Stop();
  }

  addon->OnPreUnInstall();
}

void OnPostUnInstall(const AddonPtr& addon)
{
  addon->OnPostUnInstall();
}

} /* namespace ADDON */

