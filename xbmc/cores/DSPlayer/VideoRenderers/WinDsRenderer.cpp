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
 
#ifdef HAS_DS_PLAYER

#include "WinDsRenderer.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "settings/Settings.h"
#include "guilib/Texture.h"
#include "rendering/dx/RenderContext.h"
#include "rendering/dx/DeviceResources.h"
#include "settings/AdvancedSettings.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "FileSystem/File.h"
#include "utils/MathUtils.h"
#include "StreamsManager.h"
#include "settings/DisplaySettings.h"
#include "settings/MediaSettings.h"
#include "windowing/GraphicContext.h"
#include "Application.h"

#include "DVDCodecs/Video/DVDVideoCodec.h"

CWinDsRenderer::CWinDsRenderer(): 
  m_bConfigured(false)
  , m_oldVideoRect(0, 0, 0, 0)
{
}

CWinDsRenderer::~CWinDsRenderer()
{
  UnInit();
}

void CWinDsRenderer::SetupScreenshot()
{
}

bool CWinDsRenderer::Configure(const VideoPicture &picture, float fps, unsigned int orientation)
{
  m_sourceWidth = picture.iWidth;
  m_sourceHeight = picture.iHeight;

  m_fps = fps;

  // calculate the input frame aspect ratio
  CalculateFrameAspectRatio(picture.iDisplayWidth, picture.iDisplayHeight);

  SetViewMode(g_application.GetAppPlayer().GetVideoSettings().m_ViewMode);
  ManageRenderArea();

  m_bConfigured = true;

  return true;
}

void CWinDsRenderer::Update()
{
  if (!m_bConfigured) 
    return;

  ManageRenderArea();
}

bool CWinDsRenderer::RenderCapture(CRenderCapture* capture)
{
  if (!m_bConfigured)
    return false;

  bool succeeded = false;

  ID3D11DeviceContext* pContext = DX::DeviceResources::Get()->GetD3DContext();

  CRect saveSize = m_destRect;
  saveRotatedCoords();//backup current m_rotatedDestCoords

  m_destRect.SetRect(0, 0, (float)capture->GetWidth(), (float)capture->GetHeight());
  syncDestRectToRotatedPoints();//syncs the changed destRect to m_rotatedDestCoords

  ID3D11DepthStencilView* oldDepthView;
  ID3D11RenderTargetView* oldSurface;
  pContext->OMGetRenderTargets(1, &oldSurface, &oldDepthView);

  capture->BeginRender();
  if (capture->GetState() != CAPTURESTATE_FAILED)
  {
    Render(0);
    capture->EndRender();
    succeeded = true;
  }

  pContext->OMSetRenderTargets(1, &oldSurface, oldDepthView);
  oldSurface->Release();
  SAFE_RELEASE(oldDepthView); // it can be nullptr

  m_destRect = saveSize;
  restoreRotatedCoords();//restores the previous state of the rotated dest coords

  return succeeded;
}

void CWinDsRenderer::RenderUpdate(int index, int index2, bool clear, unsigned int flags, unsigned int alpha)
{
  if (clear)
    CServiceBroker::GetWinSystem()->GetGfxContext().Clear(m_clearColour);

  if (alpha < 255)
    DX::Windowing()->SetAlphaBlendEnable(true);
  else
    DX::Windowing()->SetAlphaBlendEnable(false);

  if (!m_bConfigured)
    return;

  CSingleLock lock(CServiceBroker::GetWinSystem()->GetGfxContext());

  ManageRenderArea();

  Render(flags);
}

bool CWinDsRenderer::Flush(bool saveBuffers)
{
  SetViewMode(g_application.GetAppPlayer().GetVideoSettings().m_ViewMode);
  ManageRenderArea();

  m_bConfigured = true;

  return false;
}

void CWinDsRenderer::UnInit()
{
  m_bConfigured = false;
}

void CWinDsRenderer::Render(DWORD flags)
{
  // TODO: Take flags into account
  /*if( flags & RENDER_FLAG_NOOSD ) 
    return;*/

  CSingleLock lock(CServiceBroker::GetWinSystem()->GetGfxContext());
  if (m_oldVideoRect != m_destRect)
  {
    g_application.GetAppPlayer().SetPosition(m_sourceRect, m_destRect, m_viewRect);
    m_oldVideoRect = m_destRect;
  }

  
}

bool CWinDsRenderer::Supports(ESCALINGMETHOD method)
{
  if(method == VS_SCALINGMETHOD_NEAREST
  || method == VS_SCALINGMETHOD_LINEAR)
    return true;

  return false;
}

bool CWinDsRenderer::Supports( ERENDERFEATURE method )
{
  if ( method == RENDERFEATURE_CONTRAST
    || method == RENDERFEATURE_BRIGHTNESS
	|| method == RENDERFEATURE_ZOOM
	|| method == RENDERFEATURE_VERTICAL_SHIFT
	|| method == RENDERFEATURE_PIXEL_RATIO
	|| method == RENDERFEATURE_POSTPROCESS)
    return true;

  return false;
}

#endif
