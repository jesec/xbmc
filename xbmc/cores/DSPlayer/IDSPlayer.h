#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
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

#include "system.h" 
#include "guilib/Geometry.h"
#include "settings/lib/SettingsManager.h"
#include "settings/lib/SettingDefinitions.h"

enum DS_RENDER_LAYER
{
  RENDER_LAYER_ALL,
  RENDER_LAYER_UNDER,
  RENDER_LAYER_OVER
};

enum DIRECTSHOW_RENDERER
{
  DIRECTSHOW_RENDERER_MADVR = 3,
  DIRECTSHOW_RENDERER_UNDEF = 4
};

class IDSRendererAllocatorCallback
{
public:
  virtual ~IDSRendererAllocatorCallback() {};

  virtual CRect GetActiveVideoRect() { return CRect(0, 0, 0, 0); };
  virtual bool IsEnteringExclusive() { return false; };
  virtual void EnableExclusive(bool bEnable) {};
  virtual void SetPixelShader() {};
  virtual void SetResolution() {};
  virtual void SetPosition(CRect sourceRect, CRect videoRect, CRect viewRect) {};
  virtual bool ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM *wParam, LPARAM *lParam, LRESULT *ret) { return false; };
  virtual void Reset(bool bForceWindowed) {};
  virtual void DisplayChange(bool bExternalChange) {};
};

class IDSRendererPaintCallback
{
public:
  virtual ~IDSRendererPaintCallback() {};

  virtual void BeginRender() {};
  virtual void RenderToTexture(DS_RENDER_LAYER layer){};
  virtual void EndRender() {};
  virtual void IncRenderCount() {};
};

class IDSPlayer
{
public:
  virtual ~IDSPlayer() {};

  virtual bool Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags) { return false; };
  virtual bool UsingDS(DIRECTSHOW_RENDERER renderer = DIRECTSHOW_RENDERER_UNDEF) { return false; };
  virtual bool ReadyDS(DIRECTSHOW_RENDERER renderer = DIRECTSHOW_RENDERER_UNDEF) { return false; };
  virtual void Register(IDSRendererAllocatorCallback* pAllocatorCallback) {};
  virtual void Register(IDSRendererPaintCallback* pPaintCallback) {};
  virtual void Unregister(IDSRendererAllocatorCallback* pAllocatorCallback) {};
  virtual void Unregister(IDSRendererPaintCallback* pPaintCallback) {};

  virtual int  GetEditionsCount() { return 0; };
  virtual int  GetEdition() { return -1; }
  virtual void GetEditionInfo(int iEdition, std::string &strEditionName, REFERENCE_TIME *prt) {};
  virtual void SetEdition(int iEdition) {};
  virtual bool IsMatroskaEditions() { return false; };
  virtual void ShowEditionDlg(bool playStart) {};
};