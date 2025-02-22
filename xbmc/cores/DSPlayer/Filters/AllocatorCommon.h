/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  Copyright (C) 2005-2010 Team XBMC
 *  http://www.xbmc.org
 *
 *  Copyright (C) 2010-2013 Eduard Kytmanov
 *  http://www.avmedia.su
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
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "streams.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <Vmr9.h>
#include "Subtitles/subpic/ISubPic.h"
#include "Subtitles/subpic/DX9SubPic.h"
#include "PixelShaderCompiler.h"
#include "DSGraph.h"
#include "cores/VideoPlayer/VideoRenderers/RenderFlags.h"

// {C7ED3100-9002-4595-9DCA-B30B30413429}
DEFINE_GUID(CLSID_madVRAllocatorPresenter,
  0xc7ed3100, 0x9002, 0x4595, 0x9d, 0xca, 0xb3, 0xb, 0x30, 0x41, 0x34, 0x29);

extern std::string GetWindowsErrorMessage(HRESULT _Error, HMODULE _Module);
extern const char *GetD3DFormatStr(D3DFORMAT Format);

// extern HRESULT CreateAP9(const CLSID& clsid, HWND hWnd, ISubPicAllocatorPresenter** ppAP);
// extern HRESULT CreateEVR(const CLSID& clsid, HWND hWnd, ISubPicAllocatorPresenter** ppAP);
extern HRESULT CreateMadVR(const CLSID& clsid, HWND hWnd, ISubPicAllocatorPresenter** ppAP);
