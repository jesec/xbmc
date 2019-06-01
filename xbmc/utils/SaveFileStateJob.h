/*
 *      Copyright (C) 2010-2013 Team XBMC
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
#pragma once

#include "Job.h"
#include "FileItem.h"
#include "video/Bookmark.h"
#include "settings/VideoSettings.h"

#ifdef HAS_DS_PLAYER
#include "cores/DSplayer/Filters/MadvrSettings.h"
#endif

class CSaveFileStateJob : public CJob
{
  CFileItem m_item;
  CFileItem m_item_discstack;
  CBookmark m_bookmark;
  bool      m_updatePlayCount;
  CVideoSettings m_videoSettings;
#ifdef HAS_DS_PLAYER
  CMadvrSettings m_madvrSettings;
#endif
public:
                CSaveFileStateJob(const CFileItem& item,
                                  const CFileItem& item_discstack,
                                  const CBookmark& bookmark,
                                  bool updatePlayCount,
                                  const CVideoSettings &videoSettings
#ifdef HAS_DS_PLAYER
                                  , const CMadvrSettings &madvrSettings
#endif
                                  )
                  : m_item(item),
                    m_item_discstack(item_discstack),
                    m_bookmark(bookmark),
                    m_updatePlayCount(updatePlayCount),
                    m_videoSettings(videoSettings)
#ifdef HAS_DS_PLAYER
                    , m_madvrSettings(madvrSettings)
#endif
                {}
        ~CSaveFileStateJob() override = default;
  bool  DoWork() override;
};

