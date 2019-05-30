/*
 *      Copyright (C) 2005-2017 Team Kodi
 *      http://kodi.tv
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
 *  along with KODI; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "General.h"
#include "DialogContextMenu.h"
#include "DialogExtendedProgressBar.h"
#include "DialogFileBrowser.h"
#include "DialogKeyboard.h"
#include "DialogNumeric.h"
#include "DialogOK.h"
#include "DialogProgress.h"
#include "DialogSelect.h"
#include "DialogTextViewer.h"
#include "DialogYesNo.h"
#include "addons/kodi-addon-dev-kit/include/kodi/gui/General.h"

#include "addons/AddonDll.h"
#include "input/Key.h"
#include "guilib/GUIWindowManager.h"
#include "utils/log.h"

using namespace kodi; // addon-dev-kit namespace
using namespace kodi::gui; // addon-dev-kit namespace

namespace ADDON
{
int Interface_GUIGeneral::m_iAddonGUILockRef = 0;
};

extern "C"
{

namespace ADDON
{

void Interface_GUIGeneral::Init(AddonGlobalInterface* addonInterface)
{
  addonInterface->toKodi->kodi_gui = static_cast<AddonToKodiFuncTable_kodi_gui*>(malloc(sizeof(AddonToKodiFuncTable_kodi_gui)));

  Interface_GUIDialogContextMenu::Init(addonInterface);
  Interface_GUIDialogExtendedProgress::Init(addonInterface);
  Interface_GUIDialogFileBrowser::Init(addonInterface);
  Interface_GUIDialogKeyboard::Init(addonInterface);
  Interface_GUIDialogNumeric::Init(addonInterface);
  Interface_GUIDialogOK::Init(addonInterface);
  Interface_GUIDialogProgress::Init(addonInterface);
  Interface_GUIDialogSelect::Init(addonInterface);
  Interface_GUIDialogTextViewer::Init(addonInterface);
  Interface_GUIDialogYesNo::Init(addonInterface);

  addonInterface->toKodi->kodi_gui->general = static_cast<AddonToKodiFuncTable_kodi_gui_general*>(malloc(sizeof(AddonToKodiFuncTable_kodi_gui_general)));

  addonInterface->toKodi->kodi_gui->general->lock = lock;
  addonInterface->toKodi->kodi_gui->general->unlock = unlock;
  addonInterface->toKodi->kodi_gui->general->get_screen_height = get_screen_height;
  addonInterface->toKodi->kodi_gui->general->get_screen_width = get_screen_width;
  addonInterface->toKodi->kodi_gui->general->get_video_resolution = get_video_resolution;
  addonInterface->toKodi->kodi_gui->general->get_current_window_dialog_id = get_current_window_dialog_id;
  addonInterface->toKodi->kodi_gui->general->get_current_window_id = get_current_window_id;
}

void Interface_GUIGeneral::DeInit(AddonGlobalInterface* addonInterface)
{
  if (addonInterface->toKodi && /* <-- needed as long as the old addon way is used */
      addonInterface->toKodi->kodi_gui)
  {
    Interface_GUIDialogContextMenu::DeInit(addonInterface);
    Interface_GUIDialogExtendedProgress::DeInit(addonInterface);
    Interface_GUIDialogFileBrowser::DeInit(addonInterface);
    Interface_GUIDialogKeyboard::DeInit(addonInterface);
    Interface_GUIDialogNumeric::DeInit(addonInterface);
    Interface_GUIDialogOK::DeInit(addonInterface);
    Interface_GUIDialogProgress::DeInit(addonInterface);
    Interface_GUIDialogSelect::DeInit(addonInterface);
    Interface_GUIDialogTextViewer::DeInit(addonInterface);
    Interface_GUIDialogYesNo::DeInit(addonInterface);

    free(addonInterface->toKodi->kodi_gui->general);
    free(addonInterface->toKodi->kodi_gui);
    addonInterface->toKodi->kodi_gui = nullptr;
  }
}

//@{
void Interface_GUIGeneral::lock()
{
  if (m_iAddonGUILockRef == 0)
    g_graphicsContext.Lock();
  ++m_iAddonGUILockRef;
}

void Interface_GUIGeneral::unlock()
{
  if (m_iAddonGUILockRef > 0)
  {
    --m_iAddonGUILockRef;
    if (m_iAddonGUILockRef == 0)
      g_graphicsContext.Unlock();
  }
}
//@}

//@{
int Interface_GUIGeneral::get_screen_height(void* kodiBase)
{
  CAddonDll* addon = static_cast<CAddonDll*>(kodiBase);
  if (!addon)
  {
    CLog::Log(LOGERROR, "kodi::gui::%s - invalid data", __FUNCTION__);
    return -1;
  }

  return g_graphicsContext.GetHeight();
}

int Interface_GUIGeneral::get_screen_width(void* kodiBase)
{
  CAddonDll* addon = static_cast<CAddonDll*>(kodiBase);
  if (!addon)
  {
    CLog::Log(LOGERROR, "kodi::gui::%s - invalid data", __FUNCTION__);
    return -1;
  }

  return g_graphicsContext.GetWidth();
}

int Interface_GUIGeneral::get_video_resolution(void* kodiBase)
{
  CAddonDll* addon = static_cast<CAddonDll*>(kodiBase);
  if (!addon)
  {
    CLog::Log(LOGERROR, "kodi::gui::%s - invalid data", __FUNCTION__);
    return -1;
  }

  return (int)g_graphicsContext.GetVideoResolution();
}
//@}

//@{
int Interface_GUIGeneral::get_current_window_dialog_id(void* kodiBase)
{
  CAddonDll* addon = static_cast<CAddonDll*>(kodiBase);
  if (!addon)
  {
    CLog::Log(LOGERROR, "kodi::gui::%s - invalid data", __FUNCTION__);
    return -1;
  }

  CSingleLock gl(g_graphicsContext);
  return g_windowManager.GetTopMostModalDialogID();
}

int Interface_GUIGeneral::get_current_window_id(void* kodiBase)
{
  CAddonDll* addon = static_cast<CAddonDll*>(kodiBase);
  if (!addon)
  {
    CLog::Log(LOGERROR, "kodi::gui::%s - invalid data", __FUNCTION__);
    return -1;
  }

  CSingleLock gl(g_graphicsContext);
  return g_windowManager.GetActiveWindow();
}

//@}

} /* namespace ADDON */
} /* extern "C" */
