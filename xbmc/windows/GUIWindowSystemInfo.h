/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

#include "guilib/GUIWindow.h"

class CGUIWindowSystemInfo : public CGUIWindow
{
public:
  CGUIWindowSystemInfo(void);
  ~CGUIWindowSystemInfo(void) override;
  bool OnMessage(CGUIMessage& message) override;
  void FrameMove() override;
private:
  int  m_section;
  void ResetLabels();
  void SetControlLabel(int id, const char *format, int label, int info);
#ifdef HAS_DS_PLAYER
  void SetControlLabel(int id, const char *format, int label, const std::string &info);
#endif
  std::vector<std::string> m_diskUsage;
};

