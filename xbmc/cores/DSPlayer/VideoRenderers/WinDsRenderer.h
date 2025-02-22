#pragma once

/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#if !defined(_LINUX) && !defined(HAS_GL) && defined(HAS_DS_PLAYER)

#include "threads/CriticalSection.h"
#include "guilib/D3DResource.h"
#include "../VideoPlayer/VideoRenderers/RenderCapture.h"
#include "cores/VideoSettings.h"
#include "../VideoPlayer/VideoRenderers/BaseRenderer.h"

#define AUTOSOURCE -1

class CBaseTexture;

class CWinDsRenderer : public CBaseRenderer
{
public:
  CWinDsRenderer();
  ~CWinDsRenderer();

  bool CWinDsRenderer::RenderCapture(CRenderCapture* capture);

  virtual void         Update();
  virtual void         SetupScreenshot();
  void                 CreateThumbnail(CBaseTexture *texture, unsigned int width, unsigned int height){};

  // Player functions
  virtual bool         Configure(const VideoPicture &picture, float fps, unsigned int orientation);
  virtual void         AddVideoPicture(const VideoPicture &picture, int index) {};
  virtual void         FlipPage(int source) {};
  virtual void         UnInit();
  virtual void         Reset() {};
  virtual bool         IsConfigured() { return m_bConfigured; }
  virtual bool         Flush(bool saveBuffers);

  // Feature support
  virtual bool         SupportsMultiPassRendering() { return false; }
  virtual bool         Supports(ERENDERFEATURE feature);
  virtual bool         Supports(ESCALINGMETHOD method);

  void                 RenderUpdate(int index, int index2, bool clear, unsigned int flags, unsigned int alpha);
  virtual bool         ConfigChanged(const VideoPicture &picture) { return false; };

protected:
  virtual void         Render(DWORD flags);

  bool                 m_bConfigured;
  DWORD                m_clearColour;
  CRect                m_oldVideoRect;
};

#else
#include "LinuxRenderer.h"
#endif