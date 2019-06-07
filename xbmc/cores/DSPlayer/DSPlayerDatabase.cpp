/*
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
#ifdef HAS_DS_PLAYER
#include "DSPlayerDatabase.h"
#include "ServiceBroker.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "dbwrappers/dataset.h"
#include "filesystem/StackDirectory.h"
#include "video/VideoInfoTag.h "
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "Filters/LAVAudioSettings.h"
#include "Filters/LAVVideoSettings.h"
#include "Filters/LAVSplitterSettings.h"
#include "utils/CharsetConverter.h"
#include "utils/JSONVariantWriter.h"
#include "utils/JSONVariantParser.h"
#include "settings/MediaSettings.h"

using namespace XFILE;

CEdition::CEdition()
  :editionNumber(0)
  , editionName("")
{
}

bool CEdition::IsSet() const
{
  return (!editionName.empty() && editionNumber >= 0);
}

CDSPlayerDatabase::CDSPlayerDatabase(void)
{
}


CDSPlayerDatabase::~CDSPlayerDatabase(void)
{
}

bool CDSPlayerDatabase::Open()
{
  return CDatabase::Open(CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseDSPlayer);
}

void CDSPlayerDatabase::CreateTables()
{
  CLog::Log(LOGINFO, "create edition table");
  m_pDS->exec("CREATE TABLE edition (idEdition integer primary key, file text, editionName text, editionNumber integer)\n");

  std::string strSQL = "CREATE TABLE lavvideoSettings (id integer, bTrayIcon integer, dwStreamAR integer, dwNumThreads integer, ";
  for (int i = 0; i < LAVOutPixFmt_NB; ++i)
    strSQL += PrepareSQL("bPixFmts%i integer, ", i);
  strSQL += "dwRGBRange integer, dwHWAccel integer, ";
  for (int i = 0; i < HWAccel_NB; ++i)
    strSQL += PrepareSQL("dwHWAccelDeviceIndex%i integer, ", i);
  for (int i = 0; i < HWCodec_NB; ++i)
    strSQL += PrepareSQL("bHWFormats%i integer, ", i);
  for (int i = 0; i < Codec_VideoNB; ++i)
    strSQL += PrepareSQL("bVideoFormats%i integer, ", i);
  strSQL += "dwHWAccelResFlags integer, dwHWDeintMode integer, dwHWDeintOutput integer, dwDeintFieldOrder integer, deintMode integer, dwSWDeintMode integer, "
    "dwSWDeintOutput integer, dwDitherMode integer, bUseMSWMV9Decoder integer, bDVDVideoSupport integer"
    ")\n";

  CLog::Log(LOGINFO, "create lavvideo setting table");
  m_pDS->exec(strSQL);

  strSQL = "CREATE TABLE lavaudioSettings (id integer, bTrayIcon integer, bDRCEnabled integer, iDRCLevel integer, ";
  for (int i = 0; i < Bitstream_NB; ++i)
    strSQL += PrepareSQL("bBitstream%i integer, ", i);
  strSQL += "bDTSHDFraming integer, bAutoAVSync integer, bExpandMono integer, bExpand61 integer, bOutputStandardLayout integer, b51Legacy integer, bAllowRawSPDIF integer, ";
  for (int i = 0; i < SampleFormat_NB; ++i)
    strSQL += PrepareSQL("bSampleFormats%i integer, ", i);
  for (int i = 0; i < Codec_AudioNB; ++i)
    strSQL += PrepareSQL("bAudioFormats%i integer, ", i);
  strSQL += "bSampleConvertDither integer, bAudioDelayEnabled integer, iAudioDelay integer, bMixingEnabled integer, dwMixingLayout integer, dwMixingFlags integer, dwMixingMode integer, "
    "dwMixingCenterLevel integer, dwMixingSurroundLevel integer, dwMixingLFELevel integer"
    ")\n";

  CLog::Log(LOGINFO, "create lavaudio setting table");
  m_pDS->exec(strSQL);

  strSQL = "CREATE TABLE lavsplitterSettings (id integer, bTrayIcon integer, prefAudioLangs txt, prefSubLangs txt, subtitleAdvanced txt, subtitleMode integer, bPGSForcedStream integer, bPGSOnlyForced integer, "
    "iVC1Mode integer, bSubstreams integer, bMatroskaExternalSegments integer, bStreamSwitchRemoveAudio integer, bImpairedAudio integer, bPreferHighQualityAudio integer, dwQueueMaxSize integer, dwQueueMaxPacketsSize integer, dwNetworkAnalysisDuration integer"
    ")\n";

  CLog::Log(LOGINFO, "create lavsplitter setting table");
  m_pDS->exec(strSQL);

  m_pDS->exec("CREATE TABLE lastTvId (lastPlayed integer, lastWatched integer)\n");
  m_pDS->exec("INSERT INTO lastTvId (lastPlayed, lastWatched) VALUES (-1,-1)\n");
  CLog::Log(LOGINFO, "create lastTvId setting table");

  m_pDS->exec("CREATE TABLE settings (file text, extSubTrackName text)\n");
  CLog::Log(LOGINFO, "create settings setting table");
}

void CDSPlayerDatabase::CreateAnalytics()
{
  m_pDS->exec("CREATE INDEX idxEdition ON edition (file)");
  m_pDS->exec("CREATE INDEX idxSettings ON settings (file)");
  m_pDS->exec("CREATE INDEX idxLavVideo ON lavvideoSettings(id)");
  m_pDS->exec("CREATE INDEX idxLavAudio ON lavaudioSettings (id)");
  m_pDS->exec("CREATE INDEX idxlavSplitter ON lavsplitterSettings (id)");
}

int CDSPlayerDatabase::GetSchemaVersion() const
{ 
  return 1; 
}

void CDSPlayerDatabase::UpdateTables(int version)
{

}

bool CDSPlayerDatabase::GetResumeEdition(const std::string& strFilenameAndPath, CEdition &edition)
{
  VECEDITIONS editions;
  GetEditionForFile(strFilenameAndPath, editions);
  if (editions.size() > 0)
  {
    edition = editions[0];
    return true;
  }
  return false;
}

bool CDSPlayerDatabase::GetResumeEdition(const CFileItem *item, CEdition &edition)
{
  std::string strPath = item->GetPath();
  if ((item->IsVideoDb() || item->IsDVD()) && item->HasVideoInfoTag())
    strPath = item->GetVideoInfoTag()->m_strFileNameAndPath;

  return GetResumeEdition(strPath, edition);
}

void CDSPlayerDatabase::GetEditionForFile(const std::string& strFilenameAndPath, VECEDITIONS& editions)
{
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    std::string strSQL = PrepareSQL("select * from edition where file='%s' order by editionNumber", strFilenameAndPath.c_str());
    m_pDS->query(strSQL.c_str());
    while (!m_pDS->eof())
    {
      CEdition edition;
      edition.editionName = m_pDS->fv("editionName").get_asString();
      edition.editionNumber = m_pDS->fv("editionNumber").get_asInt();

      editions.push_back(edition);
      m_pDS->next();
    }

    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

void CDSPlayerDatabase::AddEdition(const std::string& strFilenameAndPath, const CEdition &edition)
{
  try
  {
    if (!edition.IsSet())		return;
    if (NULL == m_pDB.get())    return;
    if (NULL == m_pDS.get())    return;

    std::string strSQL;
    int idEdition = -1;

    strSQL = PrepareSQL("select idEdition from edition where file='%s'", strFilenameAndPath.c_str());

    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() != 0)
      idEdition = m_pDS->get_field_value("idEdition").get_asInt();
    m_pDS->close();

    if (idEdition >= 0)
      strSQL = PrepareSQL("update edition set  editionName = '%s', editionNumber = '%i' where idEdition = %i", edition.editionName.c_str(), edition.editionNumber, idEdition);
    else
      strSQL = PrepareSQL("insert into edition (idEdition, file, editionName, editionNumber) values(NULL, '%s', '%s', %i)", strFilenameAndPath.c_str(), edition.editionName.c_str(), edition.editionNumber);


    m_pDS->exec(strSQL.c_str());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

void CDSPlayerDatabase::ClearEditionOfFile(const std::string& strFilenameAndPath)
{
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    std::string strSQL = PrepareSQL("delete from edition where file='%s'", strFilenameAndPath.c_str());
    m_pDS->exec(strSQL.c_str());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

bool CDSPlayerDatabase::GetLAVVideoSettings(CLavSettings &settings)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    // ok, now obtain the settings for this file
    std::string strSQL = PrepareSQL("select * from lavvideoSettings where id = 0");

    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    { // get the lavvideo settings info

      settings.video_bTrayIcon = m_pDS->fv("bTrayIcon").get_asInt();
      settings.video_dwStreamAR = m_pDS->fv("dwStreamAR").get_asInt();
      settings.video_dwNumThreads = m_pDS->fv("dwNumThreads").get_asInt();
      for (int i = 0; i < LAVOutPixFmt_NB; ++i)
        settings.video_bPixFmts[i] = m_pDS->fv(PrepareSQL("bPixFmts%i", i).c_str()).get_asInt();
      settings.video_dwRGBRange = m_pDS->fv("dwRGBRange").get_asInt();
      settings.video_dwHWAccel = m_pDS->fv("dwHWAccel").get_asInt();
      for (int i = 0; i < HWAccel_NB; ++i)
        settings.video_dwHWAccelDeviceIndex[i] = m_pDS->fv(PrepareSQL("dwHWAccelDeviceIndex%i", i).c_str()).get_asInt();
      for (int i = 0; i < HWCodec_NB; ++i)
        settings.video_bHWFormats[i] = m_pDS->fv(PrepareSQL("bHWFormats%i", i).c_str()).get_asInt();
      for (int i = 0; i < Codec_VideoNB; ++i)
        settings.video_bVideoFormats[i] = m_pDS->fv(PrepareSQL("bVideoFormats%i", i).c_str()).get_asInt();
      settings.video_dwHWAccelResFlags = m_pDS->fv("dwHWAccelResFlags").get_asInt();
      settings.video_dwHWDeintMode = m_pDS->fv("dwHWDeintMode").get_asInt();
      settings.video_dwHWDeintOutput = m_pDS->fv("dwHWDeintOutput").get_asInt();
      settings.video_dwDeintFieldOrder = m_pDS->fv("dwDeintFieldOrder").get_asInt();
      settings.video_deintMode = (LAVDeintMode)m_pDS->fv("deintMode").get_asInt();
      settings.video_dwSWDeintMode = m_pDS->fv("dwSWDeintMode").get_asInt();
      settings.video_dwSWDeintOutput = m_pDS->fv("dwSWDeintOutput").get_asInt();
      settings.video_dwDitherMode = m_pDS->fv("dwDitherMode").get_asInt();
      settings.video_bUseMSWMV9Decoder = m_pDS->fv("bUseMSWMV9Decoder").get_asInt();
      settings.video_bDVDVideoSupport = m_pDS->fv("bDVDVideoSupport").get_asInt();

      m_pDS->close();
      return true;
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CDSPlayerDatabase::GetLAVAudioSettings(CLavSettings &settings)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    // ok, now obtain the settings for this file
    std::string strSQL = PrepareSQL("select * from lavaudioSettings where id = 0");

    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    { // get the lavaudio settings info

      settings.audio_bTrayIcon = m_pDS->fv("bTrayIcon").get_asInt();
      settings.audio_bDRCEnabled = m_pDS->fv("bDRCEnabled").get_asInt();
      settings.audio_iDRCLevel = m_pDS->fv("iDRCLevel").get_asInt();
      for (int i = 0; i < Bitstream_NB; ++i)
        settings.audio_bBitstream[i] = m_pDS->fv(PrepareSQL("bBitstream%i", i).c_str()).get_asInt();
      settings.audio_bDTSHDFraming = m_pDS->fv("bDTSHDFraming").get_asInt();
      settings.audio_bAutoAVSync = m_pDS->fv("bAutoAVSync").get_asInt();
      settings.audio_bExpandMono = m_pDS->fv("bExpandMono").get_asInt();
      settings.audio_bExpand61 = m_pDS->fv("bExpand61").get_asInt();
      settings.audio_bOutputStandardLayout = m_pDS->fv("bOutputStandardLayout").get_asInt();
      settings.audio_b51Legacy = m_pDS->fv("b51Legacy").get_asInt();
      settings.audio_bAllowRawSPDIF = m_pDS->fv("bAllowRawSPDIF").get_asInt();
      for (int i = 0; i < SampleFormat_NB; ++i)
        settings.audio_bSampleFormats[i] = m_pDS->fv(PrepareSQL("bSampleFormats%i", i).c_str()).get_asInt();
      for (int i = 0; i < Codec_AudioNB; ++i)
        settings.audio_bAudioFormats[i] = m_pDS->fv(PrepareSQL("bAudioFormats%i", i).c_str()).get_asInt();
      settings.audio_bSampleConvertDither = m_pDS->fv("bSampleConvertDither").get_asInt();
      settings.audio_bAudioDelayEnabled = m_pDS->fv("bAudioDelayEnabled").get_asInt();
      settings.audio_iAudioDelay = m_pDS->fv("iAudioDelay").get_asInt();
      settings.audio_bMixingEnabled = m_pDS->fv("bMixingEnabled").get_asInt();
      settings.audio_dwMixingLayout = m_pDS->fv("dwMixingLayout").get_asInt();
      settings.audio_dwMixingFlags = m_pDS->fv("dwMixingFlags").get_asInt();
      settings.audio_dwMixingMode = m_pDS->fv("dwMixingMode").get_asInt();
      settings.audio_dwMixingCenterLevel = m_pDS->fv("dwMixingCenterLevel").get_asInt();
      settings.audio_dwMixingSurroundLevel = m_pDS->fv("dwMixingSurroundLevel").get_asInt();
      settings.audio_dwMixingLFELevel = m_pDS->fv("dwMixingLFELevel").get_asInt();

      m_pDS->close();
      return true;
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CDSPlayerDatabase::GetLAVSplitterSettings(CLavSettings &settings)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    // ok, now obtain the settings for this file
    std::string strSQL = PrepareSQL("select * from lavsplitterSettings where id = 0");

    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    { // get the lavsplitter settings info

      std::wstring strW;
      settings.splitter_bTrayIcon = m_pDS->fv("bTrayIcon").get_asInt();     
      g_charsetConverter.utf8ToW(m_pDS->fv("prefAudioLangs").get_asString(), strW, false);
      settings.splitter_prefAudioLangs = strW;
      g_charsetConverter.utf8ToW(m_pDS->fv("prefSubLangs").get_asString(), strW, false);
      settings.splitter_prefSubLangs = strW;
      g_charsetConverter.utf8ToW(m_pDS->fv("subtitleAdvanced").get_asString(), strW, false);
      settings.splitter_subtitleAdvanced = strW;
      settings.splitter_subtitleMode = (LAVSubtitleMode)m_pDS->fv("subtitleMode").get_asInt();
      settings.splitter_bPGSForcedStream = m_pDS->fv("bPGSForcedStream").get_asInt();
      settings.splitter_bPGSOnlyForced = m_pDS->fv("bPGSOnlyForced").get_asInt();
      settings.splitter_iVC1Mode = m_pDS->fv("iVC1Mode").get_asInt();
      settings.splitter_bSubstreams = m_pDS->fv("bSubstreams").get_asInt();
      settings.splitter_bMatroskaExternalSegments = m_pDS->fv("bMatroskaExternalSegments").get_asInt();
      settings.splitter_bStreamSwitchRemoveAudio = m_pDS->fv("bStreamSwitchRemoveAudio").get_asInt();
      settings.splitter_bImpairedAudio = m_pDS->fv("bImpairedAudio").get_asInt();
      settings.splitter_bPreferHighQualityAudio = m_pDS->fv("bPreferHighQualityAudio").get_asInt();
      settings.splitter_dwQueueMaxSize = m_pDS->fv("dwQueueMaxSize").get_asInt();
      settings.splitter_dwQueueMaxPacketsSize = m_pDS->fv("dwQueueMaxPacketsSize").get_asInt();
      settings.splitter_dwNetworkAnalysisDuration = m_pDS->fv("dwNetworkAnalysisDuration").get_asInt();

      m_pDS->close();
      return true;
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}


/// \brief Sets the settings for a particular video file
void CDSPlayerDatabase::SetLAVVideoSettings(CLavSettings &settings)
{
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    std::string strSQL = PrepareSQL("select * from lavvideoSettings where id = 0");
    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    {
      m_pDS->close();

      // update the item
      strSQL = "update lavvideoSettings set ";
      strSQL += PrepareSQL("bTrayIcon=%i, ", settings.video_bTrayIcon);
      strSQL += PrepareSQL("dwStreamAR=%i, ", settings.video_dwStreamAR);
      strSQL += PrepareSQL("dwNumThreads=%i, ", settings.video_dwNumThreads);
      for (int i = 0; i < LAVOutPixFmt_NB; ++i)
        strSQL += PrepareSQL("bPixFmts%i=%i, ", i, settings.video_bPixFmts[i]);
      strSQL += PrepareSQL("dwRGBRange=%i, ", settings.video_dwRGBRange);
      strSQL += PrepareSQL("dwHWAccel=%i, ", settings.video_dwHWAccel);
      for (int i = 0; i < HWAccel_NB; ++i)
        strSQL += PrepareSQL("dwHWAccelDeviceIndex%i=%i, ", i, settings.video_dwHWAccelDeviceIndex[i]);
      for (int i = 0; i < HWCodec_NB; ++i)
        strSQL += PrepareSQL("bHWFormats%i=%i, ", i, settings.video_bHWFormats[i]);
      for (int i = 0; i < Codec_VideoNB; ++i)
        strSQL += PrepareSQL("bVideoFormats%i=%i, ", i, settings.video_bVideoFormats[i]);
      strSQL += PrepareSQL("dwHWAccelResFlags=%i, ", settings.video_dwHWAccelResFlags);
      strSQL += PrepareSQL("dwHWDeintMode=%i, ", settings.video_dwHWDeintMode);
      strSQL += PrepareSQL("dwHWDeintOutput=%i, ", settings.video_dwHWDeintOutput);
      strSQL += PrepareSQL("dwDeintFieldOrder=%i, ", settings.video_dwDeintFieldOrder);
      strSQL += PrepareSQL("deintMode=%i, ", settings.video_deintMode);
      strSQL += PrepareSQL("dwSWDeintMode=%i, ", settings.video_dwSWDeintMode);
      strSQL += PrepareSQL("dwSWDeintOutput=%i, ", settings.video_dwSWDeintOutput);
      strSQL += PrepareSQL("dwDitherMode=%i, ", settings.video_dwDitherMode);
      strSQL += PrepareSQL("bUseMSWMV9Decoder=%i, ", settings.video_bUseMSWMV9Decoder);
      strSQL += PrepareSQL("bDVDVideoSupport=%i ", settings.video_bDVDVideoSupport);
      strSQL += "where id=0";
      m_pDS->exec(strSQL.c_str());
      return;
    }
    else
    { // add the items
      m_pDS->close();

      strSQL = "INSERT INTO lavvideoSettings (id, bTrayIcon, dwStreamAR, dwNumThreads, ";
      for (int i = 0; i < LAVOutPixFmt_NB; ++i)
        strSQL += PrepareSQL("bPixFmts%i, ", i);
      strSQL += "dwRGBRange, dwHWAccel, ";
      for (int i = 0; i < HWAccel_NB; ++i)
        strSQL += PrepareSQL("dwHWAccelDeviceIndex%i, ", i);
      for (int i = 0; i < HWCodec_NB; ++i)
        strSQL += PrepareSQL("bHWFormats%i, ", i);
      for (int i = 0; i < Codec_VideoNB; ++i)
        strSQL += PrepareSQL("bVideoFormats%i, ", i);
      strSQL += "dwHWAccelResFlags, dwHWDeintMode, dwHWDeintOutput, dwDeintFieldOrder, deintMode, dwSWDeintMode, "
        "dwSWDeintOutput, dwDitherMode, bUseMSWMV9Decoder, bDVDVideoSupport"
        ") VALUES (0, ";

      strSQL += PrepareSQL("%i, ", settings.video_bTrayIcon);
      strSQL += PrepareSQL("%i, ", settings.video_dwStreamAR);
      strSQL += PrepareSQL("%i, ", settings.video_dwNumThreads);
      for (int i = 0; i < LAVOutPixFmt_NB; ++i)
        strSQL += PrepareSQL("%i, ", settings.video_bPixFmts[i]);
      strSQL += PrepareSQL("%i, ", settings.video_dwRGBRange);
      strSQL += PrepareSQL("%i, ", settings.video_dwHWAccel);
      for (int i = 0; i < HWAccel_NB; ++i)
        strSQL += PrepareSQL("%i, ", settings.video_dwHWAccelDeviceIndex[i]);
      for (int i = 0; i < HWCodec_NB; ++i)
        strSQL += PrepareSQL("%i, ", settings.video_bHWFormats[i]);
      for (int i = 0; i < Codec_VideoNB; ++i)
        strSQL += PrepareSQL("%i, ", settings.video_bVideoFormats[i]);
      strSQL += PrepareSQL("%i, ", settings.video_dwHWAccelResFlags);
      strSQL += PrepareSQL("%i, ", settings.video_dwHWDeintMode);
      strSQL += PrepareSQL("%i, ", settings.video_dwHWDeintOutput);
      strSQL += PrepareSQL("%i, ", settings.video_dwDeintFieldOrder);
      strSQL += PrepareSQL("%i, ", settings.video_deintMode);
      strSQL += PrepareSQL("%i, ", settings.video_dwSWDeintMode);
      strSQL += PrepareSQL("%i, ", settings.video_dwSWDeintOutput);
      strSQL += PrepareSQL("%i, ", settings.video_dwDitherMode);
      strSQL += PrepareSQL("%i, ", settings.video_bUseMSWMV9Decoder);
      strSQL += PrepareSQL("%i ", settings.video_bDVDVideoSupport);
      strSQL += ")";

      m_pDS->exec(strSQL.c_str());
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CDSPlayerDatabase::SetLAVAudioSettings(CLavSettings &settings)
{
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    std::string strSQL = PrepareSQL("select * from lavaudioSettings where id = 0");
    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    {
      m_pDS->close();

      // update the item
      strSQL = "update lavaudioSettings set ";
      strSQL += PrepareSQL("bTrayIcon=%i, ", settings.audio_bTrayIcon);
      strSQL += PrepareSQL("bDRCEnabled=%i, ", settings.audio_bDRCEnabled);
      strSQL += PrepareSQL("iDRCLevel=%i, ", settings.audio_iDRCLevel);
      for (int i = 0; i < Bitstream_NB; ++i)
        strSQL += PrepareSQL("bBitstream%i=%i, ", i, settings.audio_bBitstream[i]);
      strSQL += PrepareSQL("bDTSHDFraming=%i, ", settings.audio_bDTSHDFraming);
      strSQL += PrepareSQL("bAutoAVSync=%i, ", settings.audio_bAutoAVSync);
      strSQL += PrepareSQL("bExpandMono=%i, ", settings.audio_bExpandMono);
      strSQL += PrepareSQL("bExpand61=%i, ", settings.audio_bExpand61);
      strSQL += PrepareSQL("bOutputStandardLayout=%i, ", settings.audio_bOutputStandardLayout);
      strSQL += PrepareSQL("b51Legacy=%i, ", settings.audio_b51Legacy);
      strSQL += PrepareSQL("bAllowRawSPDIF=%i, ", settings.audio_bAllowRawSPDIF);
      for (int i = 0; i < SampleFormat_NB; ++i)
        strSQL += PrepareSQL("bSampleFormats%i=%i, ", i, settings.audio_bSampleFormats[i]);
      for (int i = 0; i < Codec_AudioNB; ++i)
        strSQL += PrepareSQL("bAudioFormats%i=%i, ", i, settings.audio_bAudioFormats[i]);
      strSQL += PrepareSQL("bSampleConvertDither=%i, ", settings.audio_bSampleConvertDither);
      strSQL += PrepareSQL("bAudioDelayEnabled=%i, ", settings.audio_bAudioDelayEnabled);
      strSQL += PrepareSQL("iAudioDelay=%i, ", settings.audio_iAudioDelay);
      strSQL += PrepareSQL("bMixingEnabled=%i, ", settings.audio_bMixingEnabled);
      strSQL += PrepareSQL("dwMixingLayout=%i, ", settings.audio_dwMixingLayout);
      strSQL += PrepareSQL("dwMixingFlags=%i, ", settings.audio_dwMixingFlags);
      strSQL += PrepareSQL("dwMixingMode=%i, ", settings.audio_dwMixingMode);
      strSQL += PrepareSQL("dwMixingCenterLevel=%i, ", settings.audio_dwMixingCenterLevel);
      strSQL += PrepareSQL("dwMixingSurroundLevel=%i, ", settings.audio_dwMixingSurroundLevel);
      strSQL += PrepareSQL("dwMixingLFELevel=%i ", settings.audio_dwMixingLFELevel);
      strSQL += "where id=0";
      m_pDS->exec(strSQL.c_str());
      return;
    }
    else
    { // add the items
      m_pDS->close();

      strSQL = "INSERT INTO lavaudioSettings (id, bTrayIcon, bDRCEnabled, iDRCLevel, ";
      for (int i = 0; i < Bitstream_NB; ++i)
        strSQL += PrepareSQL("bBitstream%i, ", i);
      strSQL += "bDTSHDFraming, bAutoAVSync, bExpandMono, bExpand61, bOutputStandardLayout, b51Legacy, bAllowRawSPDIF, ";
      for (int i = 0; i < SampleFormat_NB; ++i)
        strSQL += PrepareSQL("bSampleFormats%i, ", i);
      for (int i = 0; i < Codec_AudioNB; ++i)
        strSQL += PrepareSQL("bAudioFormats%i, ", i);
      strSQL += "bSampleConvertDither, bAudioDelayEnabled, iAudioDelay, bMixingEnabled, dwMixingLayout, dwMixingFlags, dwMixingMode, "
        "dwMixingCenterLevel, dwMixingSurroundLevel, dwMixingLFELevel"
        ") VALUES (0, ";

      strSQL += PrepareSQL("%i, ", settings.audio_bTrayIcon);
      strSQL += PrepareSQL("%i, ", settings.audio_bDRCEnabled);
      strSQL += PrepareSQL("%i, ", settings.audio_iDRCLevel);
      for (int i = 0; i < Bitstream_NB; ++i)
        strSQL += PrepareSQL("%i, ", settings.audio_bBitstream[i]);
      strSQL += PrepareSQL("%i, ", settings.audio_bDTSHDFraming);
      strSQL += PrepareSQL("%i, ", settings.audio_bAutoAVSync);
      strSQL += PrepareSQL("%i, ", settings.audio_bExpandMono);
      strSQL += PrepareSQL("%i, ", settings.audio_bExpand61);
      strSQL += PrepareSQL("%i, ", settings.audio_bOutputStandardLayout);
      strSQL += PrepareSQL("%i, ", settings.audio_b51Legacy);
      strSQL += PrepareSQL("%i, ", settings.audio_bAllowRawSPDIF);
      for (int i = 0; i < SampleFormat_NB; ++i)
        strSQL += PrepareSQL("%i, ", settings.audio_bSampleFormats[i]);
      for (int i = 0; i < Codec_AudioNB; ++i)
        strSQL += PrepareSQL("%i, ", settings.audio_bAudioFormats[i]);
      strSQL += PrepareSQL("%i, ", settings.audio_bSampleConvertDither);
      strSQL += PrepareSQL("%i, ", settings.audio_bAudioDelayEnabled);
      strSQL += PrepareSQL("%i, ", settings.audio_iAudioDelay);
      strSQL += PrepareSQL("%i, ", settings.audio_bMixingEnabled);
      strSQL += PrepareSQL("%i, ", settings.audio_dwMixingLayout);
      strSQL += PrepareSQL("%i, ", settings.audio_dwMixingFlags);
      strSQL += PrepareSQL("%i, ", settings.audio_dwMixingMode);
      strSQL += PrepareSQL("%i, ", settings.audio_dwMixingCenterLevel);
      strSQL += PrepareSQL("%i, ", settings.audio_dwMixingSurroundLevel);
      strSQL += PrepareSQL("%i ", settings.audio_dwMixingLFELevel);
      strSQL += ")";

      m_pDS->exec(strSQL.c_str());
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CDSPlayerDatabase::SetLAVSplitterSettings(CLavSettings &settings)
{
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    std::string str;
    std::string strSQL = PrepareSQL("select * from lavsplitterSettings where id = 0");
    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    {
      m_pDS->close();

      // update the item
      strSQL = "update lavsplitterSettings set ";
      strSQL += PrepareSQL("bTrayIcon=%i, ", settings.splitter_bTrayIcon);
      g_charsetConverter.wToUTF8(settings.splitter_prefAudioLangs, str, false);
      strSQL += PrepareSQL("prefAudioLangs='%s', ", str.c_str());
      g_charsetConverter.wToUTF8(settings.splitter_prefSubLangs, str, false);
      strSQL += PrepareSQL("prefSubLangs='%s', ", str.c_str());
      g_charsetConverter.wToUTF8(settings.splitter_subtitleAdvanced, str, false);
      strSQL += PrepareSQL("subtitleAdvanced='%s', ", str.c_str());
      strSQL += PrepareSQL("subtitleMode=%i, ", settings.splitter_subtitleMode);
      strSQL += PrepareSQL("bPGSForcedStream=%i, ", settings.splitter_bPGSForcedStream);
      strSQL += PrepareSQL("bPGSOnlyForced=%i, ", settings.splitter_bPGSOnlyForced);
      strSQL += PrepareSQL("iVC1Mode=%i, ", settings.splitter_iVC1Mode);
      strSQL += PrepareSQL("bSubstreams=%i, ", settings.splitter_bSubstreams);
      strSQL += PrepareSQL("bMatroskaExternalSegments=%i, ", settings.splitter_bMatroskaExternalSegments);
      strSQL += PrepareSQL("bStreamSwitchRemoveAudio=%i, ", settings.splitter_bStreamSwitchRemoveAudio);
      strSQL += PrepareSQL("bImpairedAudio=%i, ", settings.splitter_bImpairedAudio);
      strSQL += PrepareSQL("bPreferHighQualityAudio=%i, ", settings.splitter_bPreferHighQualityAudio);
      strSQL += PrepareSQL("dwQueueMaxSize=%i, ", settings.splitter_dwQueueMaxSize);
      strSQL += PrepareSQL("dwQueueMaxPacketsSize=%i, ", settings.splitter_dwQueueMaxPacketsSize);
      strSQL += PrepareSQL("dwNetworkAnalysisDuration=%i ", settings.splitter_dwNetworkAnalysisDuration);

      strSQL += "where id=0";
      m_pDS->exec(strSQL.c_str());
      return;
    }
    else
    { // add the items
      m_pDS->close();

      strSQL = "INSERT INTO lavsplitterSettings (id, bTrayIcon, prefAudioLangs, prefSubLangs, subtitleAdvanced, subtitleMode, bPGSForcedStream, bPGSOnlyForced, "
        "iVC1Mode, bSubstreams, bMatroskaExternalSegments, bStreamSwitchRemoveAudio, bImpairedAudio, bPreferHighQualityAudio, dwQueueMaxSize, dwQueueMaxPacketsSize, dwNetworkAnalysisDuration"
        ") VALUES (0, ";

      strSQL += PrepareSQL("%i, ", settings.splitter_bTrayIcon);
      g_charsetConverter.wToUTF8(settings.splitter_prefAudioLangs, str, false);
      strSQL += PrepareSQL("'%s', ", str.c_str());
      g_charsetConverter.wToUTF8(settings.splitter_prefSubLangs, str, false);
      strSQL += PrepareSQL("'%s', ", str.c_str());
      g_charsetConverter.wToUTF8(settings.splitter_subtitleAdvanced, str, false);
      strSQL += PrepareSQL("'%s', ", str.c_str());
      strSQL += PrepareSQL("%i, ", settings.splitter_subtitleMode);
      strSQL += PrepareSQL("%i, ", settings.splitter_bPGSForcedStream);
      strSQL += PrepareSQL("%i, ", settings.splitter_bPGSOnlyForced);
      strSQL += PrepareSQL("%i, ", settings.splitter_iVC1Mode);
      strSQL += PrepareSQL("%i, ", settings.splitter_bSubstreams);
      strSQL += PrepareSQL("%i, ", settings.splitter_bMatroskaExternalSegments);
      strSQL += PrepareSQL("%i, ", settings.splitter_bStreamSwitchRemoveAudio);
      strSQL += PrepareSQL("%i, ", settings.splitter_bImpairedAudio);
      strSQL += PrepareSQL("%i, ", settings.splitter_bPreferHighQualityAudio);
      strSQL += PrepareSQL("%i, ", settings.splitter_dwQueueMaxSize);
      strSQL += PrepareSQL("%i, ", settings.splitter_dwQueueMaxPacketsSize);
      strSQL += PrepareSQL("%i ", settings.splitter_dwNetworkAnalysisDuration);
      strSQL += ")";

      m_pDS->exec(strSQL.c_str());
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CDSPlayerDatabase::EraseLAVVideo()
{
  try
  {
    std::string sql = "DELETE FROM lavvideoSettings";
    m_pDS->exec(sql);
    CLog::Log(LOGINFO, "Deleting LAVVideo settings");
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}
void CDSPlayerDatabase::EraseLAVAudio()
{
  try
  {
    std::string sql = "DELETE FROM lavaudioSettings";
    m_pDS->exec(sql);
    CLog::Log(LOGINFO, "Deleting LAVAudio settings");
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}
void CDSPlayerDatabase::EraseLAVSplitter()
{
  try
  {
    std::string sql = "DELETE FROM lavsplitterSettings";
    m_pDS->exec(sql);
    CLog::Log(LOGINFO, "Deleting LAVSplitter settings");
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

int CDSPlayerDatabase::GetLastTvShowId(bool bLastWatched)
{
  try
  {
    std::string strSelect = bLastWatched ? "lastWatched" : "lastPlayed";
    std::string strSQL = PrepareSQL("SELECT %s FROM lastTvId LIMIT 1", strSelect.c_str());

    if (!m_pDS->query(strSQL.c_str()))
      return -1;

    return m_pDS->fv(strSelect.c_str()).get_asInt();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return -1;
}

void CDSPlayerDatabase::SetLastTvShowId(bool bLastWatched, int id)
{
  if (id < 0)
    return;

  try
  {
    std::string strSelect = bLastWatched ? "lastWatched" : "lastPlayed";
    std::string strSQL = PrepareSQL("SELECT * FROM lastTvId LIMIT 1");

    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    {
      m_pDS->close();
      // update the item

      strSQL = PrepareSQL("UPDATE lastTvId set %s=%i ", strSelect.c_str(), id);
      m_pDS->exec(strSQL.c_str());
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CDSPlayerDatabase::SetSubtitleExtTrackName(const std::string &strFilenameAndPath, const std::string &subTrackName)
{
  if (strFilenameAndPath.empty())
    return;

  try
  {
    std::string strSQL;

    if (subTrackName.empty())
    {
      strSQL = PrepareSQL("DELETE FROM settings WHERE file like '%s'", strFilenameAndPath.c_str());
      m_pDS->exec(strSQL.c_str());
      return;
    }

    strSQL = PrepareSQL("SELECT * FROM settings WHERE file like '%s'", strFilenameAndPath.c_str());
    m_pDS->query(strSQL.c_str());
    if (m_pDS->num_rows() > 0)
    {
      m_pDS->close();
      // update the item

       strSQL = PrepareSQL("UPDATE settings set extSubTrackName='%s'", subTrackName.c_str());

      m_pDS->exec(strSQL.c_str());
    }
    else
    {
      m_pDS->close();

      strSQL = PrepareSQL("INSERT INTO settings (file, extSubTrackName) VALUES ('%s','%s')", strFilenameAndPath.c_str(), subTrackName.c_str());
      m_pDS->exec(strSQL.c_str());
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

std::string CDSPlayerDatabase::GetSubtitleExtTrackName(const std::string &strFilenameAndPath)
{
  try
  {
    std::string strSQL = PrepareSQL("SELECT * FROM settings WHERE file like '%s'", strFilenameAndPath.c_str());

    if (!m_pDS->query(strSQL.c_str()))
      return "";

    return m_pDS->fv("extSubTrackName").get_asString();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return "";
}

#endif