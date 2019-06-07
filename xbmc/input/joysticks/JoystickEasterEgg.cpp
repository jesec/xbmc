/*
 *      Copyright (C) 2016-2017 Team Kodi
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
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "JoystickEasterEgg.h"
#include "ServiceBroker.h"
#include "games/controllers/ControllerIDs.h"
#include "games/GameServices.h"
#include "games/GameSettings.h"
#include "guilib/GUIAudioManager.h"
#include "guilib/WindowIDs.h"

using namespace KODI;
using namespace JOYSTICK;

const std::map<std::string, std::vector<FeatureName>> CJoystickEasterEgg::m_sequence =
{
  {
    DEFAULT_CONTROLLER_ID,
    {
      "up",
      "up",
      "down",
      "down",
      "left",
      "right",
      "left",
      "right",
      "b",
      "a",
    },
  },
  {
    DEFAULT_REMOTE_ID,
    {
      "up",
      "up",
      "down",
      "down",
      "left",
      "right",
      "left",
      "right",
      "back",
      "ok",
    },
  },
};

CJoystickEasterEgg::CJoystickEasterEgg(const std::string& controllerId) :
  m_controllerId(controllerId),
  m_state(0)
{
}

bool CJoystickEasterEgg::OnButtonPress(const FeatureName& feature)
{
  bool bHandled = false;

  auto it = m_sequence.find(m_controllerId);
  if (it != m_sequence.end())
  {
    const auto& sequence = it->second;

    // Reset state if it previously finished
    if (m_state >= sequence.size())
      m_state = 0;

    if (feature == sequence[m_state])
      m_state++;
    else
      m_state = 0;

    if (IsCapturing())
    {
      bHandled = true;

      if (m_state >= sequence.size())
        OnFinish();
    }
  }

  return bHandled;
}

bool CJoystickEasterEgg::IsCapturing()
{
  // Capture input when finished with arrows (2 x up/down/left/right)
  return m_state > 8;
}

void CJoystickEasterEgg::OnFinish(void)
{
  GAME::CGameSettings &gameSettings = CServiceBroker::GetGameServices().GameSettings();
  gameSettings.ToggleGames();

  WINDOW_SOUND sound = gameSettings.GamesEnabled() ? SOUND_INIT : SOUND_DEINIT;
  CGUIComponent* gui = CServiceBroker::GetGUI();
  if (gui)
    gui->GetAudioManager().PlayWindowSound(WINDOW_DIALOG_KAI_TOAST, sound);

  //! @todo Shake screen
}
