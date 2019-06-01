/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *		Copyright (C) 2010-2013 Eduard Kytmanov
 *		http://www.avmedia.su
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

#include "RendererSettings.h"
#include "ServiceBroker.h"
#include "filesystem/file.h"
#include "utils/log.h"
#include "util.h"
#include "profiles/ProfilesManager.h"
#include "settings/Settings.h"
#include "utils/XMLUtils.h"
#include "utils/SystemInfo.h"
#include "PixelShaderList.h"
#include "utils/StringUtils.h"

using namespace XFILE;
CDSSettings::CDSSettings(void)
{
  m_hD3DX9Dll = NULL;

  m_pDwmIsCompositionEnabled = NULL;
  m_pDwmEnableComposition = NULL;
  m_hDWMAPI = LoadLibraryA("dwmapi.dll");
  if (m_hDWMAPI)
  {
    (FARPROC &)m_pDwmIsCompositionEnabled = GetProcAddress(m_hDWMAPI, "DwmIsCompositionEnabled");
    (FARPROC &)m_pDwmEnableComposition = GetProcAddress(m_hDWMAPI, "DwmEnableComposition");
  }
}

void CDSSettings::Initialize()
{
  std::string videoRender;
  videoRender = CServiceBroker::GetSettings().GetString(CSettings::SETTING_DSPLAYER_VIDEORENDERER);

  // Create the pixel shader list
  pixelShaderList.reset(new CPixelShaderList());
  pixelShaderList->Load();
}

CDSSettings::~CDSSettings(void)
{
  if (m_hD3DX9Dll)
    FreeLibrary(m_hD3DX9Dll);

  if (m_hDWMAPI)
    FreeLibrary(m_hDWMAPI);
}

void CDSSettings::LoadConfig()
{

}

HINSTANCE CDSSettings::GetD3X9Dll()
{
if (m_hD3DX9Dll == NULL)
  {
    int min_ver = D3DX_SDK_VERSION;
    int max_ver = D3DX_SDK_VERSION;
    
    m_nDXSdkRelease = 0;

    if(D3DX_SDK_VERSION >= 42) {
      // August 2009 SDK (v42) is not compatible with older versions
      min_ver = 42;      
    } else {
      if(D3DX_SDK_VERSION > 33) {
        // versions between 34 and 41 have no known compatibility issues
        min_ver = 34;
      }  else {    
        // The minimum version that supports the functionality required by MPC is 24
        min_ver = 24;
  
        if(D3DX_SDK_VERSION == 33) {
          // The April 2007 SDK (v33) should not be used (crash sometimes during shader compilation)
          max_ver = 32;    
        }
      }
    }
    
    // load latest compatible version of the DLL that is available
    for (int i=max_ver; i>=min_ver; i--)
    {
      m_strD3DX9Version = StringUtils::Format("d3dx9_%d.dll", i);
      m_hD3DX9Dll = LoadLibraryA(m_strD3DX9Version.c_str());
      if (m_hD3DX9Dll) 
      {
        m_nDXSdkRelease = i;
        break;
      }
    }
  }

  return m_hD3DX9Dll;
}

CDSSettings g_dsSettings;
bool g_bNoDuration = false;
bool g_bExternalSubtitleTime = false;

#endif