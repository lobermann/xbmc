/*
 *      Copyright (C) 2016 Team Kodi
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
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#if (defined HAVE_CONFIG_H) && (!defined TARGET_WINDOWS)
  #include "config.h"
#endif

#include "VideoDatabase.h"

#include <odb/odb_gen/ODBBookmark.h>
#include <odb/odb_gen/ODBBookmark_odb.h>
#include <odb/odb_gen/ODBFile.h>
#include <odb/odb_gen/ODBFile_odb.h>
#include <odb/odb_gen/ODBGenre.h>
#include <odb/odb_gen/ODBGenre_odb.h>
#include <odb/odb_gen/ODBPath.h>
#include <odb/odb_gen/ODBPath_odb.h>
#include <odb/odb_gen/ODBPerson.h>
#include <odb/odb_gen/ODBPerson_odb.h>
#include <odb/odb_gen/ODBPersonLink.h>
#include <odb/odb_gen/ODBPersonLink_odb.h>
#include <odb/odb_gen/ODBRating.h>
#include <odb/odb_gen/ODBRating_odb.h>
#include <odb/odb_gen/ODBSet.h>
#include <odb/odb_gen/ODBSet_odb.h>
#include <odb/odb_gen/ODBStreamDetails.h>
#include <odb/odb_gen/ODBStreamDetails_odb.h>
#include <odb/odb_gen/ODBUniqueID.h>
#include <odb/odb_gen/ODBUniqueID_odb.h>
#include <odb/odb_gen/ODBArt.h>
#include <odb/odb_gen/ODBArt_odb.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "video/VideoInfoTag.h"
#include "addons/AddonManager.h"
#include "Application.h"
#include "dbwrappers/dataset.h"
#include "dialogs/GUIDialogExtendedProgressBar.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogYesNo.h"
#include "FileItem.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "filesystem/MultiPathDirectory.h"
#include "filesystem/StackDirectory.h"
#include "guiinfo/GUIInfoLabels.h"
#include "GUIInfoManager.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "GUIPassword.h"
#include "interfaces/AnnouncementManager.h"
#include "playlists/SmartPlayList.h"
#include "profiles/ProfilesManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "storage/MediaManager.h"
#include "TextureCache.h"
#include "threads/SystemClock.h"
#include "URL.h"
#include "Util.h"
#include "utils/FileUtils.h"
#include "utils/GroupUtils.h"
#include "utils/LabelFormatter.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/XMLUtils.h"
#include "video/VideoDbUrl.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "VideoInfoScanner.h"
#include "XBDateTime.h"

using namespace dbiplus;
using namespace XFILE;
using namespace VIDEO;
using namespace ADDON;
using namespace KODI::MESSAGING;

//********************************************************************************************************************************
CVideoDatabase::CVideoDatabase(void) : m_cdb(CCommonDatabase::GetInstance())
{

}

//********************************************************************************************************************************
CVideoDatabase::~CVideoDatabase(void)
{}

//********************************************************************************************************************************
bool CVideoDatabase::Open()
{
  return CDatabase::Open(g_advancedSettings.m_databaseVideo);
}

void CVideoDatabase::CreateTables()
{
  CLog::Log(LOGINFO, "create bookmark table");
  m_pDS->exec("CREATE TABLE bookmark ( idBookmark integer primary key, idFile integer, timeInSeconds double, totalTimeInSeconds double, thumbNailImage text, player text, playerState text, type integer)\n");

  CLog::Log(LOGINFO, "create settings table");
  m_pDS->exec("CREATE TABLE settings ( idFile integer, Deinterlace bool,"
              "ViewMode integer,ZoomAmount float, PixelRatio float, VerticalShift float, AudioStream integer, SubtitleStream integer,"
              "SubtitleDelay float, SubtitlesOn bool, Brightness float, Contrast float, Gamma float,"
              "VolumeAmplification float, AudioDelay float, OutputToAllSpeakers bool, ResumeTime integer,"
              "Sharpness float, NoiseReduction float, NonLinStretch bool, PostProcess bool,"
              "ScalingMethod integer, DeinterlaceMode integer, StereoMode integer, StereoInvert bool, VideoStream integer)\n");

  CLog::Log(LOGINFO, "create stacktimes table");
  m_pDS->exec("CREATE TABLE stacktimes (idFile integer, times text)\n");

  CLog::Log(LOGINFO, "create genre table");
  m_pDS->exec("CREATE TABLE genre ( genre_id integer primary key, name TEXT)\n");
  m_pDS->exec("CREATE TABLE genre_link (genre_id integer, media_id integer, media_type TEXT)");

  CLog::Log(LOGINFO, "create country table");
  m_pDS->exec("CREATE TABLE country ( country_id integer primary key, name TEXT)");
  m_pDS->exec("CREATE TABLE country_link (country_id integer, media_id integer, media_type TEXT)");

  CLog::Log(LOGINFO, "create movie table");
  std::string columns = "CREATE TABLE movie ( idMovie integer primary key, idFile integer";

  for (int i = 0; i < VIDEODB_MAX_COLUMNS; i++)
    columns += StringUtils::Format(",c%02d text", i);

  columns += ", idSet integer, userrating integer, premiered text)";
  m_pDS->exec(columns);

  CLog::Log(LOGINFO, "create actor table");
  m_pDS->exec("CREATE TABLE actor ( actor_id INTEGER PRIMARY KEY, name TEXT, art_urls TEXT )");
  m_pDS->exec("CREATE TABLE actor_link(actor_id INTEGER, media_id INTEGER, media_type TEXT, role TEXT, cast_order INTEGER)");
  m_pDS->exec("CREATE TABLE director_link(actor_id INTEGER, media_id INTEGER, media_type TEXT)");
  m_pDS->exec("CREATE TABLE writer_link(actor_id INTEGER, media_id INTEGER, media_type TEXT)");

  CLog::Log(LOGINFO, "create path table");
  m_pDS->exec("CREATE TABLE path ( idPath integer primary key, strPath text, strContent text, strScraper text, strHash text, scanRecursive integer, useFolderNames bool, strSettings text, noUpdate bool, exclude bool, dateAdded text, idParentPath integer)");

  CLog::Log(LOGINFO, "create files table");
  m_pDS->exec("CREATE TABLE files ( idFile integer primary key, idPath integer, strFilename text, playCount integer, lastPlayed text, dateAdded text)");

  CLog::Log(LOGINFO, "create tvshow table");
  columns = "CREATE TABLE tvshow ( idShow integer primary key";

  for (int i = 0; i < VIDEODB_MAX_COLUMNS; i++)
    columns += StringUtils::Format(",c%02d text", i);

  columns += ", userrating integer, duration INTEGER)";
  m_pDS->exec(columns);

  CLog::Log(LOGINFO, "create episode table");
  columns = "CREATE TABLE episode ( idEpisode integer primary key, idFile integer";
  for (int i = 0; i < VIDEODB_MAX_COLUMNS; i++)
  {
    std::string column;
    if ( i == VIDEODB_ID_EPISODE_SEASON || i == VIDEODB_ID_EPISODE_EPISODE || i == VIDEODB_ID_EPISODE_BOOKMARK)
      column = StringUtils::Format(",c%02d varchar(24)", i);
    else
      column = StringUtils::Format(",c%02d text", i);

    columns += column;
  }
  columns += ", idShow integer, userrating integer, idSeason integer)";
  m_pDS->exec(columns);

  CLog::Log(LOGINFO, "create tvshowlinkpath table");
  m_pDS->exec("CREATE TABLE tvshowlinkpath (idShow integer, idPath integer)\n");

  CLog::Log(LOGINFO, "create movielinktvshow table");
  m_pDS->exec("CREATE TABLE movielinktvshow ( idMovie integer, IdShow integer)\n");

  CLog::Log(LOGINFO, "create studio table");
  m_pDS->exec("CREATE TABLE studio ( studio_id integer primary key, name TEXT)\n");
  m_pDS->exec("CREATE TABLE studio_link (studio_id integer, media_id integer, media_type TEXT)");

  CLog::Log(LOGINFO, "create musicvideo table");
  columns = "CREATE TABLE musicvideo ( idMVideo integer primary key, idFile integer";
  for (int i = 0; i < VIDEODB_MAX_COLUMNS; i++)
    columns += StringUtils::Format(",c%02d text", i);

  columns += ", userrating integer, premiered text)";
  m_pDS->exec(columns);

  CLog::Log(LOGINFO, "create streaminfo table");
  m_pDS->exec("CREATE TABLE streamdetails (idFile integer, iStreamType integer, "
    "strVideoCodec text, fVideoAspect float, iVideoWidth integer, iVideoHeight integer, "
    "strAudioCodec text, iAudioChannels integer, strAudioLanguage text, "
    "strSubtitleLanguage text, iVideoDuration integer, strStereoMode text, strVideoLanguage text)");

  CLog::Log(LOGINFO, "create sets table");
  m_pDS->exec("CREATE TABLE sets ( idSet integer primary key, strSet text, strOverview text)");

  CLog::Log(LOGINFO, "create seasons table");
  m_pDS->exec("CREATE TABLE seasons ( idSeason integer primary key, idShow integer, season integer, name text, userrating integer)");

  CLog::Log(LOGINFO, "create art table");
  m_pDS->exec("CREATE TABLE art(art_id INTEGER PRIMARY KEY, media_id INTEGER, media_type TEXT, type TEXT, url TEXT)");

  CLog::Log(LOGINFO, "create tag table");
  m_pDS->exec("CREATE TABLE tag (tag_id integer primary key, name TEXT)");
  m_pDS->exec("CREATE TABLE tag_link (tag_id integer, media_id integer, media_type TEXT)");

  CLog::Log(LOGINFO, "create rating table");
  m_pDS->exec("CREATE TABLE rating (rating_id INTEGER PRIMARY KEY, media_id INTEGER, media_type TEXT, rating_type TEXT, rating FLOAT, votes INTEGER)");

  CLog::Log(LOGINFO, "create uniqueid table");
  m_pDS->exec("CREATE TABLE uniqueid (uniqueid_id INTEGER PRIMARY KEY, media_id INTEGER, media_type TEXT, value TEXT, type TEXT)");
}

void CVideoDatabase::CreateLinkIndex(const char *table)
{
  m_pDS->exec(PrepareSQL("CREATE UNIQUE INDEX ix_%s_1 ON %s (name(255))", table, table));
  m_pDS->exec(PrepareSQL("CREATE UNIQUE INDEX ix_%s_link_1 ON %s_link (%s_id, media_type(20), media_id)", table, table, table));
  m_pDS->exec(PrepareSQL("CREATE UNIQUE INDEX ix_%s_link_2 ON %s_link (media_id, media_type(20), %s_id)", table, table, table));
  m_pDS->exec(PrepareSQL("CREATE INDEX ix_%s_link_3 ON %s_link (media_type(20))", table, table));
}

void CVideoDatabase::CreateForeignLinkIndex(const char *table, const char *foreignkey)
{
  m_pDS->exec(PrepareSQL("CREATE UNIQUE INDEX ix_%s_link_1 ON %s_link (%s_id, media_type(20), media_id)", table, table, foreignkey));
  m_pDS->exec(PrepareSQL("CREATE UNIQUE INDEX ix_%s_link_2 ON %s_link (media_id, media_type(20), %s_id)", table, table, foreignkey));
  m_pDS->exec(PrepareSQL("CREATE INDEX ix_%s_link_3 ON %s_link (media_type(20))", table, table));
}

void CVideoDatabase::CreateAnalytics()
{
  /* indexes should be added on any columns that are used in in  */
  /* a where or a join. primary key on a column is the same as a */
  /* unique index on that column, so there is no need to add any */
  /* index if no other columns are refered                       */

  /* order of indexes are important, for an index to be considered all  */
  /* columns up to the column in question have to have been specified   */
  /* select * from foolink where foo_id = 1, can not take               */
  /* advantage of a index that has been created on ( bar_id, foo_id )   */
  /* however an index on ( foo_id, bar_id ) will be considered for use  */

  CLog::Log(LOGINFO, "%s - creating indicies", __FUNCTION__);
  m_pDS->exec("CREATE INDEX ix_bookmark ON bookmark (idFile, type)");
  m_pDS->exec("CREATE UNIQUE INDEX ix_settings ON settings ( idFile )\n");
  m_pDS->exec("CREATE UNIQUE INDEX ix_stacktimes ON stacktimes ( idFile )\n");
  m_pDS->exec("CREATE INDEX ix_path ON path ( strPath(255) )");
  m_pDS->exec("CREATE INDEX ix_path2 ON path ( idParentPath )");
  m_pDS->exec("CREATE INDEX ix_files ON files ( idPath, strFilename(255) )");

  m_pDS->exec("CREATE UNIQUE INDEX ix_movie_file_1 ON movie (idFile, idMovie)");
  m_pDS->exec("CREATE UNIQUE INDEX ix_movie_file_2 ON movie (idMovie, idFile)");

  m_pDS->exec("CREATE UNIQUE INDEX ix_tvshowlinkpath_1 ON tvshowlinkpath ( idShow, idPath )\n");
  m_pDS->exec("CREATE UNIQUE INDEX ix_tvshowlinkpath_2 ON tvshowlinkpath ( idPath, idShow )\n");
  m_pDS->exec("CREATE UNIQUE INDEX ix_movielinktvshow_1 ON movielinktvshow ( idShow, idMovie)\n");
  m_pDS->exec("CREATE UNIQUE INDEX ix_movielinktvshow_2 ON movielinktvshow ( idMovie, idShow)\n");

  m_pDS->exec("CREATE UNIQUE INDEX ix_episode_file_1 on episode (idEpisode, idFile)");
  m_pDS->exec("CREATE UNIQUE INDEX id_episode_file_2 on episode (idFile, idEpisode)");
  std::string createColIndex = StringUtils::Format("CREATE INDEX ix_episode_season_episode on episode (c%02d, c%02d)", VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_EPISODE_EPISODE);
  m_pDS->exec(createColIndex);
  createColIndex = StringUtils::Format("CREATE INDEX ix_episode_bookmark on episode (c%02d)", VIDEODB_ID_EPISODE_BOOKMARK);
  m_pDS->exec(createColIndex);
  m_pDS->exec("CREATE INDEX ix_episode_show1 on episode(idEpisode,idShow)");
  m_pDS->exec("CREATE INDEX ix_episode_show2 on episode(idShow,idEpisode)");

  m_pDS->exec("CREATE UNIQUE INDEX ix_musicvideo_file_1 on musicvideo (idMVideo, idFile)");
  m_pDS->exec("CREATE UNIQUE INDEX ix_musicvideo_file_2 on musicvideo (idFile, idMVideo)");

  m_pDS->exec("CREATE INDEX ixMovieBasePath ON movie ( c23(12) )");
  m_pDS->exec("CREATE INDEX ixMusicVideoBasePath ON musicvideo ( c14(12) )");
  m_pDS->exec("CREATE INDEX ixEpisodeBasePath ON episode ( c19(12) )");

  m_pDS->exec("CREATE INDEX ix_streamdetails ON streamdetails (idFile)");
  m_pDS->exec("CREATE INDEX ix_seasons ON seasons (idShow, season)");
  m_pDS->exec("CREATE INDEX ix_art ON art(media_id, media_type(20), type(20))");

  m_pDS->exec("CREATE INDEX ix_rating ON rating(media_id, media_type(20))");

  m_pDS->exec("CREATE INDEX ix_uniqueid1 ON uniqueid(media_id, media_type(20), type(20))");
  m_pDS->exec("CREATE INDEX ix_uniqueid2 ON uniqueid(media_type(20), value(20))");

  CreateLinkIndex("tag");
  CreateLinkIndex("actor");
  CreateForeignLinkIndex("director", "actor");
  CreateForeignLinkIndex("writer", "actor");
  CreateLinkIndex("studio");
  CreateLinkIndex("genre");
  CreateLinkIndex("country");

  CLog::Log(LOGINFO, "%s - creating triggers", __FUNCTION__);
  m_pDS->exec("CREATE TRIGGER delete_movie AFTER DELETE ON movie FOR EACH ROW BEGIN "
              "DELETE FROM genre_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM actor_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM director_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM studio_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM country_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM writer_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM movielinktvshow WHERE idMovie=old.idMovie; "
              "DELETE FROM art WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM tag_link WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM rating WHERE media_id=old.idMovie AND media_type='movie'; "
              "DELETE FROM uniqueid WHERE media_id=old.idMovie AND media_type='movie'; "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_tvshow AFTER DELETE ON tvshow FOR EACH ROW BEGIN "
              "DELETE FROM actor_link WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM director_link WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM studio_link WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM tvshowlinkpath WHERE idShow=old.idShow; "
              "DELETE FROM genre_link WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM movielinktvshow WHERE idShow=old.idShow; "
              "DELETE FROM seasons WHERE idShow=old.idShow; "
              "DELETE FROM art WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM tag_link WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM rating WHERE media_id=old.idShow AND media_type='tvshow'; "
              "DELETE FROM uniqueid WHERE media_id=old.idShow AND media_type='tvshow'; "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_musicvideo AFTER DELETE ON musicvideo FOR EACH ROW BEGIN "
              "DELETE FROM actor_link WHERE media_id=old.idMVideo AND media_type='musicvideo'; "
              "DELETE FROM director_link WHERE media_id=old.idMVideo AND media_type='musicvideo'; "
              "DELETE FROM genre_link WHERE media_id=old.idMVideo AND media_type='musicvideo'; "
              "DELETE FROM studio_link WHERE media_id=old.idMVideo AND media_type='musicvideo'; "
              "DELETE FROM art WHERE media_id=old.idMVideo AND media_type='musicvideo'; "
              "DELETE FROM tag_link WHERE media_id=old.idMVideo AND media_type='musicvideo'; "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_episode AFTER DELETE ON episode FOR EACH ROW BEGIN "
              "DELETE FROM actor_link WHERE media_id=old.idEpisode AND media_type='episode'; "
              "DELETE FROM director_link WHERE media_id=old.idEpisode AND media_type='episode'; "
              "DELETE FROM writer_link WHERE media_id=old.idEpisode AND media_type='episode'; "
              "DELETE FROM art WHERE media_id=old.idEpisode AND media_type='episode'; "
              "DELETE FROM rating WHERE media_id=old.idEpisode AND media_type='episode'; "
              "DELETE FROM uniqueid WHERE media_id=old.idEpisode AND media_type='episode'; "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_season AFTER DELETE ON seasons FOR EACH ROW BEGIN "
              "DELETE FROM art WHERE media_id=old.idSeason AND media_type='season'; "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_set AFTER DELETE ON sets FOR EACH ROW BEGIN "
              "DELETE FROM art WHERE media_id=old.idSet AND media_type='set'; "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_person AFTER DELETE ON actor FOR EACH ROW BEGIN "
              "DELETE FROM art WHERE media_id=old.actor_id AND media_type IN ('actor','artist','writer','director'); "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_tag AFTER DELETE ON tag_link FOR EACH ROW BEGIN "
              "DELETE FROM tag WHERE tag_id=old.tag_id AND tag_id NOT IN (SELECT DISTINCT tag_id FROM tag_link); "
              "END");
  m_pDS->exec("CREATE TRIGGER delete_file AFTER DELETE ON files FOR EACH ROW BEGIN "
              "DELETE FROM bookmark WHERE idFile=old.idFile; "
              "DELETE FROM settings WHERE idFile=old.idFile; "
              "DELETE FROM stacktimes WHERE idFile=old.idFile; "
              "DELETE FROM streamdetails WHERE idFile=old.idFile; "
              "END");

  CreateViews();
}

void CVideoDatabase::CreateViews()
{
  CLog::Log(LOGINFO, "create episode_view");
  std::string episodeview = PrepareSQL("CREATE VIEW episode_view AS SELECT "
                                      "  episode.*,"
                                      "  files.strFileName AS strFileName,"
                                      "  path.strPath AS strPath,"
                                      "  files.playCount AS playCount,"
                                      "  files.lastPlayed AS lastPlayed,"
                                      "  files.dateAdded AS dateAdded,"
                                      "  tvshow.c%02d AS strTitle,"
                                      "  tvshow.c%02d AS genre,"
                                      "  tvshow.c%02d AS studio,"
                                      "  tvshow.c%02d AS premiered,"
                                      "  tvshow.c%02d AS mpaa,"
                                      "  bookmark.timeInSeconds AS resumeTimeInSeconds, "
                                      "  bookmark.totalTimeInSeconds AS totalTimeInSeconds, "
                                      "  rating.rating AS rating, "
                                      "  rating.votes AS votes, "
                                      "  rating.rating_type AS rating_type, "
                                      "  uniqueid.value AS uniqueid_value, "
                                      "  uniqueid.type AS uniqueid_type "
                                      "FROM episode"
                                      "  JOIN files ON"
                                      "    files.idFile=episode.idFile"
                                      "  JOIN tvshow ON"
                                      "    tvshow.idShow=episode.idShow"
                                      "  JOIN seasons ON"
                                      "    seasons.idSeason=episode.idSeason"
                                      "  JOIN path ON"
                                      "    files.idPath=path.idPath"
                                      "  LEFT JOIN bookmark ON"
                                      "    bookmark.idFile=episode.idFile AND bookmark.type=1"
                                      "  LEFT JOIN rating ON"
                                      "    rating.rating_id=episode.c%02d"
                                      "  LEFT JOIN uniqueid ON"
                                      "    uniqueid.uniqueid_id=episode.c%02d",
                                      VIDEODB_ID_TV_TITLE, VIDEODB_ID_TV_GENRE,
                                      VIDEODB_ID_TV_STUDIOS, VIDEODB_ID_TV_PREMIERED,
                                      VIDEODB_ID_TV_MPAA, VIDEODB_ID_EPISODE_RATING_ID,
                                      VIDEODB_ID_EPISODE_IDENT_ID);
  m_pDS->exec(episodeview);

  CLog::Log(LOGINFO, "create tvshowcounts");
  std::string tvshowcounts = PrepareSQL("CREATE VIEW tvshowcounts AS SELECT "
                                       "      tvshow.idShow AS idShow,"
                                       "      MAX(files.lastPlayed) AS lastPlayed,"
                                       "      NULLIF(COUNT(episode.c12), 0) AS totalCount,"
                                       "      COUNT(files.playCount) AS watchedcount,"
                                       "      NULLIF(COUNT(DISTINCT(episode.c12)), 0) AS totalSeasons, "
                                       "      MAX(files.dateAdded) as dateAdded "
                                       "    FROM tvshow"
                                       "      LEFT JOIN episode ON"
                                       "        episode.idShow=tvshow.idShow"
                                       "      LEFT JOIN files ON"
                                       "        files.idFile=episode.idFile "
                                       "    GROUP BY tvshow.idShow");
  m_pDS->exec(tvshowcounts);

  CLog::Log(LOGINFO, "create tvshow_view");
  std::string tvshowview = PrepareSQL("CREATE VIEW tvshow_view AS SELECT "
                                     "  tvshow.*,"
                                     "  path.idParentPath AS idParentPath,"
                                     "  path.strPath AS strPath,"
                                     "  tvshowcounts.dateAdded AS dateAdded,"
                                     "  lastPlayed, totalCount, watchedcount, totalSeasons, "
                                     "  rating.rating AS rating, "
                                     "  rating.votes AS votes, "
                                     "  rating.rating_type AS rating_type, "
                                     "  uniqueid.value AS uniqueid_value, "
                                     "  uniqueid.type AS uniqueid_type "
                                     "FROM tvshow"
                                     "  LEFT JOIN tvshowlinkpath ON"
                                     "    tvshowlinkpath.idShow=tvshow.idShow"
                                     "  LEFT JOIN path ON"
                                     "    path.idPath=tvshowlinkpath.idPath"
                                     "  INNER JOIN tvshowcounts ON"
                                     "    tvshow.idShow = tvshowcounts.idShow "
                                     "  LEFT JOIN rating ON"
                                     "    rating.rating_id=tvshow.c%02d "
                                     "  LEFT JOIN uniqueid ON"
                                     "    uniqueid.uniqueid_id=tvshow.c%02d "
                                     "GROUP BY tvshow.idShow",
                                     VIDEODB_ID_TV_RATING_ID, VIDEODB_ID_TV_IDENT_ID);
  m_pDS->exec(tvshowview);
  CLog::Log(LOGINFO, "create season_view");
  std::string seasonview = PrepareSQL("CREATE VIEW season_view AS SELECT "
                                     "  seasons.*, "
                                     "  tvshow_view.strPath AS strPath,"
                                     "  tvshow_view.c%02d AS showTitle,"
                                     "  tvshow_view.c%02d AS plot,"
                                     "  tvshow_view.c%02d AS premiered,"
                                     "  tvshow_view.c%02d AS genre,"
                                     "  tvshow_view.c%02d AS studio,"
                                     "  tvshow_view.c%02d AS mpaa,"
                                     "  count(DISTINCT episode.idEpisode) AS episodes,"
                                     "  count(files.playCount) AS playCount,"
                                     "  min(episode.c%02d) AS aired "
                                     "FROM seasons"
                                     "  JOIN tvshow_view ON"
                                     "    tvshow_view.idShow = seasons.idShow"
                                     "  JOIN episode ON"
                                     "    episode.idShow = seasons.idShow AND episode.c%02d = seasons.season"
                                     "  JOIN files ON"
                                     "    files.idFile = episode.idFile "
                                     "GROUP BY seasons.idSeason",
                                     VIDEODB_ID_TV_TITLE, VIDEODB_ID_TV_PLOT, VIDEODB_ID_TV_PREMIERED,
                                     VIDEODB_ID_TV_GENRE, VIDEODB_ID_TV_STUDIOS, VIDEODB_ID_TV_MPAA,
                                     VIDEODB_ID_EPISODE_AIRED, VIDEODB_ID_EPISODE_SEASON);
  m_pDS->exec(seasonview);

  CLog::Log(LOGINFO, "create musicvideo_view");
  m_pDS->exec("CREATE VIEW musicvideo_view AS SELECT"
              "  musicvideo.*,"
              "  files.strFileName as strFileName,"
              "  path.strPath as strPath,"
              "  files.playCount as playCount,"
              "  files.lastPlayed as lastPlayed,"
              "  files.dateAdded as dateAdded, "
              "  bookmark.timeInSeconds AS resumeTimeInSeconds, "
              "  bookmark.totalTimeInSeconds AS totalTimeInSeconds "
              "FROM musicvideo"
              "  JOIN files ON"
              "    files.idFile=musicvideo.idFile"
              "  JOIN path ON"
              "    path.idPath=files.idPath"
              "  LEFT JOIN bookmark ON"
              "    bookmark.idFile=musicvideo.idFile AND bookmark.type=1");

  CLog::Log(LOGINFO, "create movie_view");
  
  std::string movieview = PrepareSQL("CREATE VIEW movie_view AS SELECT"
                                      "  movie.*,"
                                      "  sets.strSet AS strSet,"
                                      "  sets.strOverview AS strSetOverview,"
                                      "  files.strFileName AS strFileName,"
                                      "  path.strPath AS strPath,"
                                      "  files.playCount AS playCount,"
                                      "  files.lastPlayed AS lastPlayed, "
                                      "  files.dateAdded AS dateAdded, "
                                      "  bookmark.timeInSeconds AS resumeTimeInSeconds, "
                                      "  bookmark.totalTimeInSeconds AS totalTimeInSeconds, "
                                      "  rating.rating AS rating, "
                                      "  rating.votes AS votes, "
                                      "  rating.rating_type AS rating_type, "
                                      "  uniqueid.value AS uniqueid_value, "
                                      "  uniqueid.type AS uniqueid_type "
                                      "FROM movie"
                                      "  LEFT JOIN sets ON"
                                      "    sets.idSet = movie.idSet"
                                      "  JOIN files ON"
                                      "    files.idFile=movie.idFile"
                                      "  JOIN path ON"
                                      "    path.idPath=files.idPath"
                                      "  LEFT JOIN bookmark ON"
                                      "    bookmark.idFile=movie.idFile AND bookmark.type=1"
                                      "  LEFT JOIN rating ON"
                                      "    rating.rating_id=movie.c%02d"
                                      "  LEFT JOIN uniqueid ON"
                                      "    uniqueid.uniqueid_id=movie.c%02d",
                                      VIDEODB_ID_RATING_ID, VIDEODB_ID_IDENT_ID);
  m_pDS->exec(movieview);
}

//********************************************************************************************************************************
int CVideoDatabase::GetPathId(const std::string& strPath)
{
  std::string strSQL;
  try
  {
    int idPath=-1;

    std::string strPath1(strPath);
    if (URIUtils::IsStack(strPath) || StringUtils::StartsWithNoCase(strPath, "rar://") || StringUtils::StartsWithNoCase(strPath, "zip://"))
      URIUtils::GetParentPath(strPath,strPath1);

    URIUtils::AddSlashAtEnd(strPath1);
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    typedef odb::query<CODBPath> query;
    
    CODBPath path;
    if(m_cdb.getDB()->query_one<CODBPath>(query::path == strPath1, path))
      idPath =path.m_idPath;
    
    if(odb_transaction)
      odb_transaction->commit();
    
    //TODO: Return a unsigned long, function should also return it
    return idPath;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception to getpath (%s) : %s", __FUNCTION__, strPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s unable to getpath (%s)", __FUNCTION__, strSQL.c_str());
  }
  return -1;
}

bool CVideoDatabase::GetPaths(std::set<std::string> &paths)
{
  try
  {
    //if (NULL == m_pDB.get()) return false;
    //if (NULL == m_pDS.get()) return false;

    paths.clear();
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;
      
    odb::result<CODBPath> res = m_cdb.getDB()->query<CODBPath>( (query::content == "movies" || query::content == "musicvideos")
                                                               && !query::path.like("multipath://%")
                                                               && query::noUpdate == true);
    for(CODBPath& path: res)
    {
      paths.insert(path.m_path);
    }
      
    res = m_cdb.getDB()->query<CODBPath>( (query::content == "tvshows" || query::content == "musicvideos")
                                         && !query::path.like("multipath://%")
                                         && query::noUpdate == true); //TODO: Consider tvshowlinkpath
    for(CODBPath& path: res)
    {
      paths.insert(path.m_path);
    }
      
    if(odb_transaction)
      odb_transaction->commit();

    // grab all paths with movie content set
    /*if (!m_pDS->query("select strPath,noUpdate from path"
                      " where (strContent = 'movies' or strContent = 'musicvideos')"
                      " and strPath NOT like 'multipath://%%'"
                      " order by strPath"))
      return false;

    while (!m_pDS->eof())
    {
      if (!m_pDS->fv("noUpdate").get_asBool())
        paths.insert(m_pDS->fv("strPath").get_asString());
      m_pDS->next();
    }
    m_pDS->close();*/

    // then grab all tvshow paths
    /*if (!m_pDS->query("select strPath,noUpdate from path"
                      " where ( strContent = 'tvshows'"
                      "       or idPath in (select idPath from tvshowlinkpath))"
                      " and strPath NOT like 'multipath://%%'"
                      " order by strPath"))
      return false;

    while (!m_pDS->eof())
    {
      if (!m_pDS->fv("noUpdate").get_asBool())
        paths.insert(m_pDS->fv("strPath").get_asString());
      m_pDS->next();
    }
    m_pDS->close();*/

    // finally grab all other paths holding a movie which is not a stack or a rar archive
    // - this isnt perfect but it should do fine in most situations.
    // reason we need it to hold a movie is stacks from different directories (cdx folders for instance)
    // not making mistakes must take priority
    
    //TODO: Needs to be moved to odb
    /*if (!m_pDS->query("select strPath,noUpdate from path"
                       " where idPath in (select idPath from files join movie on movie.idFile=files.idFile)"
                       " and idPath NOT in (select idPath from tvshowlinkpath)"
                       " and idPath NOT in (select idPath from files where strFileName like 'video_ts.ifo')" // dvd folders get stacked to a single item in parent folder
                       " and idPath NOT in (select idPath from files where strFileName like 'index.bdmv')" // bluray folders get stacked to a single item in parent folder
                       " and strPath NOT like 'multipath://%%'"
                       " and strContent NOT in ('movies', 'tvshows', 'None')" // these have been added above
                       " order by strPath"))

      return false;
    while (!m_pDS->eof())
    {
      if (!m_pDS->fv("noUpdate").get_asBool())
        paths.insert(m_pDS->fv("strPath").get_asString());
      m_pDS->next();
    }
    m_pDS->close();*/
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetPathsLinkedToTvShow(int idShow, std::vector<std::string> &paths)
{
  std::string sql;
  try
  {
    sql = PrepareSQL("SELECT strPath FROM path JOIN tvshowlinkpath ON tvshowlinkpath.idPath=path.idPath WHERE idShow=%i", idShow);
    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      paths.emplace_back(m_pDS->fv(0).get_asString());
      m_pDS->next();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s error during query: %s",__FUNCTION__, sql.c_str());
  }
  return false;
}

bool CVideoDatabase::GetPathsForTvShow(int idShow, std::set<int>& paths)
{
  std::string strSQL;
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    // add base path
    strSQL = PrepareSQL("SELECT strPath FROM tvshow_view WHERE idShow=%i", idShow);
    if (m_pDS->query(strSQL))
      paths.insert(GetPathId(m_pDS->fv(0).get_asString()));

    // add all other known paths
    strSQL = PrepareSQL("SELECT DISTINCT idPath FROM files JOIN episode ON episode.idFile=files.idFile WHERE episode.idShow=%i",idShow);
    m_pDS->query(strSQL);
    while (!m_pDS->eof())
    {
      paths.insert(m_pDS->fv(0).get_asInt());
      m_pDS->next();
    }
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s error during query: %s",__FUNCTION__, strSQL.c_str());
  }
  return false;
}

int CVideoDatabase::RunQuery(const std::string &sql)
{
  unsigned int time = XbmcThreads::SystemClockMillis();
  int rows = -1;
  if (m_pDS->query(sql))
  {
    rows = m_pDS->num_rows();
    if (rows == 0)
      m_pDS->close();
  }
  CLog::Log(LOGDEBUG, "%s took %d ms for %d items query: %s", __FUNCTION__, XbmcThreads::SystemClockMillis() - time, rows, sql.c_str());
  return rows;
}

bool CVideoDatabase::GetSubPaths(const std::string &basepath, std::vector<std::pair<int, std::string>>& subpaths)
{
  std::string sql;
  try
  {
    //if (!m_pDB.get() || !m_pDS.get())
    //  return false;
    
    std::string path(basepath);
    URIUtils::AddSlashAtEnd(path);
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;
      
    odb::result<CODBPath> res = m_cdb.getDB()->query<CODBPath>( (query::path.like(path + "%")) +
                                                               " AND idPath NOT IN (SELECT path FROM file WHERE filename LIKE 'video_ts.ifo')"
                                                               " AND idPath NOT IN (SELECT path FROM file WHERE filename LIKE 'index.bdmv')"); //TODO: See if there is way to do this sub-queries in odb
    for(CODBPath& path: res)
    {
      subpaths.emplace_back(path.m_idPath, path.m_path);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  
    /*sql = PrepareSQL("SELECT idPath,strPath FROM path WHERE SUBSTR(strPath,1,%i)='%s'"
                     " AND idPath NOT IN (SELECT idPath FROM files WHERE strFileName LIKE 'video_ts.ifo')"
                     " AND idPath NOT IN (SELECT idPath FROM files WHERE strFileName LIKE 'index.bdmv')"
                     , StringUtils::utf8_strlen(path.c_str()), path.c_str());

    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      subpaths.emplace_back(m_pDS->fv(0).get_asInt(), m_pDS->fv(1).get_asString());
      m_pDS->next();
    }
    m_pDS->close();*/
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception during query: %s",__FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s error during query: %s",__FUNCTION__, sql.c_str());
  }
  return false;
}

int CVideoDatabase::AddPath(const std::string& strPath, const std::string &parentPath /*= "" */, const CDateTime& dateAdded /* = CDateTime() */)
{
  std::string strSQL;
  try
  {
    int idPath = GetPathId(strPath);
    if (idPath >= 0)
      return idPath; // already have the path

    //if (NULL == m_pDB.get()) return -1;
    //if (NULL == m_pDS.get()) return -1;

    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    std::string strPath1(strPath);
    if (URIUtils::IsStack(strPath) || StringUtils::StartsWithNoCase(strPath, "rar://") || StringUtils::StartsWithNoCase(strPath, "zip://"))
      URIUtils::GetParentPath(strPath,strPath1);

    URIUtils::AddSlashAtEnd(strPath1);

    int idParentPath = GetPathId(parentPath.empty() ? (std::string)URIUtils::GetParentPath(strPath1) : parentPath);
    
    //TODO: Add Path and GetPathId to return CODBPath Objects instead of IDs
    CODBPath path;
    path.m_path = strPath1;
    
    if (dateAdded.IsValid())
      path.m_dateAdded = CODBDate(dateAdded.GetAsULongLong(), dateAdded.GetAsDBDateTime());
    
    if (idParentPath >= 0)
    {
      path.m_parentPath = std::shared_ptr<CODBPath>(m_cdb.getDB()->load<CODBPath>(idParentPath));
    }
    
    m_cdb.getDB()->persist(path);
    if(odb_transaction)
      odb_transaction->commit();
    
    return path.m_idPath;

    // add the path
    /*if (idParentPath < 0)
    {
      if (dateAdded.IsValid())
        strSQL=PrepareSQL("insert into path (idPath, strPath, dateAdded) values (NULL, '%s', '%s')", strPath1.c_str(), dateAdded.GetAsDBDateTime().c_str());
      else
        strSQL=PrepareSQL("insert into path (idPath, strPath) values (NULL, '%s')", strPath1.c_str());
    }
    else
    {
      if (dateAdded.IsValid())
        strSQL = PrepareSQL("insert into path (idPath, strPath, dateAdded, idParentPath) values (NULL, '%s', '%s', %i)", strPath1.c_str(), dateAdded.GetAsDBDateTime().c_str(), idParentPath);
      else
        strSQL=PrepareSQL("insert into path (idPath, strPath, idParentPath) values (NULL, '%s', %i)", strPath1.c_str(), idParentPath);
    }
    m_pDS->exec(strSQL);
    idPath = (int)m_pDS->lastinsertid();
    return idPath;*/
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception on addpath (%s) - %s", __FUNCTION__, strSQL.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s unable to addpath (%s)", __FUNCTION__, strSQL.c_str());
  }
  return -1;
}

bool CVideoDatabase::GetPathHash(const std::string &path, std::string &hash)
{
  try
  {
    //if (NULL == m_pDB.get()) return false;
    //if (NULL == m_pDS.get()) return false;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;
    
    CODBPath odb_path;
    if(m_cdb.getDB()->query_one<CODBPath>(query::path == query::_ref(path), odb_path))
      hash = odb_path.m_hash;
    else
    {
      if(odb_transaction)
        odb_transaction->commit();
      return false;
    }
    if(odb_transaction)
      odb_transaction->commit();

    /*std::string strSQL=PrepareSQL("select strHash from path where strPath='%s'", path.c_str());
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() == 0)
      return false;
    hash = m_pDS->fv("strHash").get_asString();*/
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, path.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, path.c_str());
  }

  return false;
}

bool CVideoDatabase::GetSourcePath(const std::string &path, std::string &sourcePath)
{
  SScanSettings dummy;
  return GetSourcePath(path, sourcePath, dummy);
}

bool CVideoDatabase::GetSourcePath(const std::string &path, std::string &sourcePath, SScanSettings& settings)
{
  try
  {
    if (path.empty())
      return false;

    std::string strPath2;

    if (URIUtils::IsMultiPath(path))
      strPath2 = CMultiPathDirectory::GetFirstPath(path);
    else
      strPath2 = path;

    std::string strPath1 = URIUtils::GetDirectory(strPath2);
    int idPath = GetPathId(strPath1);
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;

    if (idPath > -1)
    {
      // check if the given path already is a source itself
      CODBPath res;
      if (m_cdb.getDB()->query_one<CODBPath>(query::idPath == idPath
                                             && query::content.is_not_null() && query::content != ""
                                             && query::scraper.is_not_null() && query::scraper != ""
                                             , res))
      {
        settings.parent_name_root = settings.parent_name = res.m_useFolderNames;
        settings.recurse = res.m_scanRecursive;
        settings.noupdate = res.m_noUpdate;
        settings.exclude = res.m_exclude;

        if(odb_transaction)
          odb_transaction->commit();
        sourcePath = path;
        return true;
      }
    }

    // look for parent paths until there is one which is a source
    std::string strParent;
    bool found = false;
    while (URIUtils::GetParentPath(strPath1, strParent))
    {
      CODBPath res;
      if (m_cdb.getDB()->query_one<CODBPath>(query::path == strParent, res))
      {
        if (!res.m_content.empty() && !res.m_scraper.empty())
        {
          settings.parent_name_root = settings.parent_name = res.m_useFolderNames;
          settings.recurse = res.m_scanRecursive;
          settings.noupdate = res.m_noUpdate;
          settings.exclude = res.m_exclude;
          found = true;
          break;
        }
      }

      strPath1 = strParent;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    //m_pDS->close();

    if (found)
    {
      sourcePath = strParent;
      return true;
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

//********************************************************************************************************************************
int CVideoDatabase::AddFile(const std::string& strFileNameAndPath)
{
  std::string strSQL = "";
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBFile> query;

    std::string strFileName, strPath;
    SplitPath(strFileNameAndPath,strPath,strFileName);

    int idPath = AddPath(strPath);
    if (idPath < 0)
      return -1;
    
    std::shared_ptr<CODBPath> path(new CODBPath);
    if (!m_cdb.getDB()->query_one<CODBPath>(odb::query<CODBPath>::idPath == idPath, *path))
      return -1;

    CODBFile file;
    if (m_cdb.getDB()->query_one<CODBFile>(query::filename == strFileName && query::path->idPath == idPath, file))
    {
      if(odb_transaction)
        odb_transaction->commit();
      return file.m_idFile;
    }
    
    file.m_path = path;
    file.m_filename = strFileName;
    m_cdb.getDB()->persist(file);
    if(odb_transaction)
      odb_transaction->commit();
    return file.m_idFile;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception on addfile (%s) - %s", __FUNCTION__, strSQL.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s unable to addfile (%s)", __FUNCTION__, strSQL.c_str());
  }
  return -1;
}

int CVideoDatabase::AddFile(const CFileItem& item)
{
  if (item.IsVideoDb() && item.HasVideoInfoTag())
  {
    if (item.GetVideoInfoTag()->m_iFileId != -1)
      return item.GetVideoInfoTag()->m_iFileId;
    else
      return AddFile(item.GetVideoInfoTag()->m_strFileNameAndPath);
  }
  return AddFile(item.GetPath());
}

void CVideoDatabase::UpdateFileDateAdded(int idFile, const std::string& strFileNameAndPath, const CDateTime& dateAdded /* = CDateTime() */)
{
  if (idFile < 0 || strFileNameAndPath.empty())
    return;

  CDateTime finalDateAdded = dateAdded;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBFile> query;

    if (!finalDateAdded.IsValid())
    {
      // 1 prefering to use the files mtime(if it's valid) and only using the file's ctime if the mtime isn't valid
      if (g_advancedSettings.m_iVideoLibraryDateAdded == 1)
        finalDateAdded = CFileUtils::GetModificationDate(strFileNameAndPath, false);
      //2 using the newer datetime of the file's mtime and ctime
      else if (g_advancedSettings.m_iVideoLibraryDateAdded == 2)
        finalDateAdded = CFileUtils::GetModificationDate(strFileNameAndPath, true);
      //0 using the current datetime if non of the above matches or one returns an invalid datetime
      if (!finalDateAdded.IsValid())
        finalDateAdded = CDateTime::GetCurrentDateTime();
    }
    
    CODBFile file;
    if (!m_cdb.getDB()->query_one<CODBFile>(query::idFile == idFile, file))
      return;
    
    file.m_dateAdded.setDateTime(finalDateAdded.GetAsULongLong(), finalDateAdded.GetAsDBDateTime());
    m_cdb.getDB()->update(file);
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s, %s) exception - %s", __FUNCTION__, CURL::GetRedacted(strFileNameAndPath).c_str(), finalDateAdded.GetAsDBDateTime().c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s, %s) failed", __FUNCTION__, CURL::GetRedacted(strFileNameAndPath).c_str(), finalDateAdded.GetAsDBDateTime().c_str());
  }
}

bool CVideoDatabase::SetPathHash(const std::string &path, const std::string &hash)
{
  try
  {
    //if (NULL == m_pDB.get()) return false;
    //if (NULL == m_pDS.get()) return false;

    int idPath = AddPath(path);
    if (idPath < 0) return false;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;
    
    CODBPath odb_path;
    if(m_cdb.getDB()->query_one<CODBPath>(query::idPath == idPath, odb_path))
    {
      odb_path.m_hash = hash;
      m_cdb.getDB()->update (odb_path);
    }
    if(odb_transaction)
      odb_transaction->commit();

    //std::string strSQL=PrepareSQL("update path set strHash='%s' where idPath=%ld", hash.c_str(), idPath);
    //m_pDS->exec(strSQL);

    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s, %s) exception - %s", __FUNCTION__, path.c_str(), hash.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s, %s) failed", __FUNCTION__, path.c_str(), hash.c_str());
  }

  return false;
}

bool CVideoDatabase::LinkMovieToTvshow(int idMovie, int idShow, bool bRemove)
{
   try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    if (bRemove) // delete link
    {
      std::string strSQL=PrepareSQL("delete from movielinktvshow where idMovie=%i and idShow=%i", idMovie, idShow);
      m_pDS->exec(strSQL);
      return true;
    }

    std::string strSQL=PrepareSQL("insert into movielinktvshow (idShow,idMovie) values (%i,%i)", idShow,idMovie);
    m_pDS->exec(strSQL);

    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i, %i) failed", __FUNCTION__, idMovie, idShow);
  }

  return false;
}

bool CVideoDatabase::IsLinkedToTvshow(int idMovie)
{
   try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    std::string strSQL=PrepareSQL("select * from movielinktvshow where idMovie=%i", idMovie);
    m_pDS->query(strSQL);
    if (m_pDS->eof())
    {
      m_pDS->close();
      return false;
    }

    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idMovie);
  }

  return false;
}

bool CVideoDatabase::GetLinksToTvShow(int idMovie, std::vector<int>& ids)
{
   try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    std::string strSQL=PrepareSQL("select * from movielinktvshow where idMovie=%i", idMovie);
    m_pDS2->query(strSQL);
    while (!m_pDS2->eof())
    {
      ids.push_back(m_pDS2->fv(1).get_asInt());
      m_pDS2->next();
    }

    m_pDS2->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idMovie);
  }

  return false;
}


//********************************************************************************************************************************
int CVideoDatabase::GetFileId(const std::string& strFilenameAndPath)
{
  try
  {
    std::string strPath, strFileName;
    SplitPath(strFilenameAndPath,strPath,strFileName);

    int idPath = GetPathId(strPath);
    if (idPath >= 0)
    {
      std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
      
      typedef odb::query<CODBFile> query;
      CODBFile res;
      
      if(m_cdb.getDB()->query_one<CODBFile>(query::filename == strFileName && query::path->idPath == idPath, res))
      {
        return res.m_idFile;
      }
      
      if(odb_transaction)
        odb_transaction->commit();
      
      /*std::string strSQL;
      strSQL=PrepareSQL("select idFile from files where strFileName='%s' and idPath=%i", strFileName.c_str(),idPath);
      m_pDS->query(strSQL);
      if (m_pDS->num_rows() > 0)
      {
        int idFile = m_pDS->fv("files.idFile").get_asInt();
        m_pDS->close();
        return idFile;
      }*/
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strFilenameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

int CVideoDatabase::GetFileId(const CFileItem &item)
{
  if (item.IsVideoDb() && item.HasVideoInfoTag())
  {
    if (item.GetVideoInfoTag()->m_iFileId != -1)
      return item.GetVideoInfoTag()->m_iFileId;
    else
      return GetFileId(item.GetVideoInfoTag()->m_strFileNameAndPath);
  }
  return GetFileId(item.GetPath());
}

//********************************************************************************************************************************
int CVideoDatabase::GetMovieId(const std::string& strFilenameAndPath)
{
  try
  {
    //if (NULL == m_pDB.get()) return -1;
    //if (NULL == m_pDS.get()) return -1;
    int idMovie = -1;

    // needed for query parameters
    int idFile = GetFileId(strFilenameAndPath);
    int idPath=-1;
    std::string strPath;
    if (idFile < 0)
    {
      std::string strFile;
      SplitPath(strFilenameAndPath,strPath,strFile);

      // have to join movieinfo table for correct results
      idPath = GetPathId(strPath);
      if (idPath < 0 && strPath != strFilenameAndPath)
        return -1;
    }

    if (idFile == -1 && strPath != strFilenameAndPath)
      return -1;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (idFile == -1)
    {
      typedef odb::query<ODBView_Movie_File_Path> query;
      ODBView_Movie_File_Path res;
      if (m_cdb.getDB()->query_one<ODBView_Movie_File_Path>(query::CODBPath::idPath == idPath, res))
        idMovie = res.movie->m_idMovie;
    }
    else
    {
      typedef odb::query<CODBMovie> query;
      CODBMovie res;
      if (m_cdb.getDB()->query_one<CODBMovie>(query::file->idFile == idFile, res))
        idMovie =res.m_idMovie;
    }
    
    if(odb_transaction)
      odb_transaction->commit();

    return idMovie;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strFilenameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

int CVideoDatabase::GetTvShowId(const std::string& strPath)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;
    int idTvShow = -1;

    // have to join movieinfo table for correct results
    int idPath = GetPathId(strPath);
    if (idPath < 0)
      return -1;

    std::string strSQL;
    std::string strPath1=strPath;
    std::string strParent;
    int iFound=0;

    strSQL=PrepareSQL("select idShow from tvshowlinkpath where tvshowlinkpath.idPath=%i",idPath);
    m_pDS->query(strSQL);
    if (!m_pDS->eof())
      iFound = 1;

    while (iFound == 0 && URIUtils::GetParentPath(strPath1, strParent))
    {
      strSQL=PrepareSQL("SELECT idShow FROM path INNER JOIN tvshowlinkpath ON tvshowlinkpath.idPath=path.idPath WHERE strPath='%s'",strParent.c_str());
      m_pDS->query(strSQL);
      if (!m_pDS->eof())
      {
        int idShow = m_pDS->fv("idShow").get_asInt();
        if (idShow != -1)
          iFound = 2;
      }
      strPath1 = strParent;
    }

    if (m_pDS->num_rows() > 0)
      idTvShow = m_pDS->fv("idShow").get_asInt();
    m_pDS->close();

    return idTvShow;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strPath.c_str());
  }
  return -1;
}

int CVideoDatabase::GetEpisodeId(const std::string& strFilenameAndPath, int idEpisode, int idSeason) // input value is episode/season number hint - for multiparters
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    // need this due to the nested GetEpisodeInfo query
    std::unique_ptr<Dataset> pDS;
    pDS.reset(m_pDB->CreateDataset());
    if (NULL == pDS.get()) return -1;

    int idFile = GetFileId(strFilenameAndPath);
    if (idFile < 0)
      return -1;

    std::string strSQL=PrepareSQL("select idEpisode from episode where idFile=%i", idFile);

    CLog::Log(LOGDEBUG, "%s (%s), query = %s", __FUNCTION__, CURL::GetRedacted(strFilenameAndPath).c_str(), strSQL.c_str());
    pDS->query(strSQL);
    if (pDS->num_rows() > 0)
    {
      if (idEpisode == -1)
        idEpisode = pDS->fv("episode.idEpisode").get_asInt();
      else // use the hint!
      {
        while (!pDS->eof())
        {
          CVideoInfoTag tag;
          int idTmpEpisode = pDS->fv("episode.idEpisode").get_asInt();
          GetEpisodeInfo(strFilenameAndPath, tag, idTmpEpisode, VideoDbDetailsNone);
          if (tag.m_iEpisode == idEpisode && (idSeason == -1 || tag.m_iSeason == idSeason)) {
            // match on the episode hint, and there's no season hint or a season hint match
            idEpisode = idTmpEpisode;
            break;
          }
          pDS->next();
        }
        if (pDS->eof())
          idEpisode = -1;
      }
    }
    else
      idEpisode = -1;

    pDS->close();

    return idEpisode;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

int CVideoDatabase::GetMusicVideoId(const std::string& strFilenameAndPath)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    int idFile = GetFileId(strFilenameAndPath);
    if (idFile < 0)
      return -1;

    std::string strSQL=PrepareSQL("select idMVideo from musicvideo where idFile=%i", idFile);

    CLog::Log(LOGDEBUG, "%s (%s), query = %s", __FUNCTION__, CURL::GetRedacted(strFilenameAndPath).c_str(), strSQL.c_str());
    m_pDS->query(strSQL);
    int idMVideo=-1;
    if (m_pDS->num_rows() > 0)
      idMVideo = m_pDS->fv("idMVideo").get_asInt();
    m_pDS->close();

    return idMVideo;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

//********************************************************************************************************************************
int CVideoDatabase::AddMovie(const std::string& strFilenameAndPath)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    int idMovie = GetMovieId(strFilenameAndPath);
    if (idMovie < 0)
    {
      int idFile = AddFile(strFilenameAndPath); //TODO: Should return the file object
      if (idFile < 0)
        return -1;
      UpdateFileDateAdded(idFile, strFilenameAndPath);
      
      std::shared_ptr<CODBFile> odb_file(new CODBFile);
      if (!m_cdb.getDB()->query_one<CODBFile>(odb::query<CODBFile>::idFile == idFile, *odb_file))
        return -1;
      
      CODBMovie movie;
      movie.m_file = odb_file;
      
      m_cdb.getDB()->persist(movie);
      
      idMovie = (int)movie.m_idMovie;
    }
    
    if(odb_transaction)
      odb_transaction->commit();

    return idMovie;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strFilenameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

bool CVideoDatabase::AddPathToTvShow(int idShow, const std::string &path, const std::string &parentPath, const CDateTime& dateAdded /* = CDateTime() */)
{
  // Check if this path is already added
  int idPath = GetPathId(path);
  if (idPath < 0)
  {
    CDateTime finalDateAdded = dateAdded;
    // Skip looking at the files ctime/mtime if defined by the user through as.xml
    if (!finalDateAdded.IsValid() && g_advancedSettings.m_iVideoLibraryDateAdded > 0)
    {
      struct __stat64 buffer;
      if (XFILE::CFile::Stat(path, &buffer) == 0)
      {
        time_t now = time(NULL);
        // Make sure we have a valid date (i.e. not in the future)
        if ((time_t)buffer.st_ctime <= now)
        {
          struct tm *time;
#ifdef HAVE_LOCALTIME_R
          struct tm result = {};
          time = localtime_r((const time_t*)&buffer.st_ctime, &result);
#else
          time = localtime((const time_t*)&buffer.st_ctime);
#endif
          if (time)
            finalDateAdded = *time;
        }
      }
    }

    if (!finalDateAdded.IsValid())
      finalDateAdded = CDateTime::GetCurrentDateTime();

    idPath = AddPath(path, parentPath, finalDateAdded);
  }

  return ExecuteQuery(PrepareSQL("REPLACE INTO tvshowlinkpath(idShow, idPath) VALUES (%i,%i)", idShow, idPath));
}

int CVideoDatabase::AddTvShow()
{
  if (ExecuteQuery("INSERT INTO tvshow(idShow) VALUES(NULL)"))
    return (int)m_pDS->lastinsertid();
  return -1;
}

//********************************************************************************************************************************
int CVideoDatabase::AddEpisode(int idShow, const std::string& strFilenameAndPath)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    int idFile = AddFile(strFilenameAndPath);
    if (idFile < 0)
      return -1;
    UpdateFileDateAdded(idFile, strFilenameAndPath);

    std::string strSQL=PrepareSQL("insert into episode (idEpisode, idFile, idShow) values (NULL, %i, %i)", idFile, idShow);
    m_pDS->exec(strSQL);
    return (int)m_pDS->lastinsertid();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

int CVideoDatabase::AddMusicVideo(const std::string& strFilenameAndPath)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    int idMVideo = GetMusicVideoId(strFilenameAndPath);
    if (idMVideo < 0)
    {
      int idFile = AddFile(strFilenameAndPath);
      if (idFile < 0)
        return -1;
      UpdateFileDateAdded(idFile, strFilenameAndPath);
      std::string strSQL=PrepareSQL("insert into musicvideo (idMVideo, idFile) values (NULL, %i)", idFile);
      m_pDS->exec(strSQL);
      idMVideo = (int)m_pDS->lastinsertid();
    }

    return idMVideo;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

//********************************************************************************************************************************
int CVideoDatabase::AddToTable(const std::string& table, const std::string& firstField, const std::string& secondField, const std::string& value)
{
  //TODO: Deprecate this function
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    std::string strSQL = PrepareSQL("select %s from %s where %s like '%s'", firstField.c_str(), table.c_str(), secondField.c_str(), value.substr(0, 255).c_str());
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      // doesnt exists, add it
      strSQL = PrepareSQL("insert into %s (%s, %s) values(NULL, '%s')", table.c_str(), firstField.c_str(), secondField.c_str(), value.substr(0, 255).c_str());
      m_pDS->exec(strSQL);
      int id = (int)m_pDS->lastinsertid();
      return id;
    }
    else
    {
      int id = m_pDS->fv(firstField.c_str()).get_asInt();
      m_pDS->close();
      return id;
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, value.c_str() );
  }

  return -1;
}

int CVideoDatabase::UpdateRatings(int mediaId, const char *mediaType, const RatingMap& values, const std::string& defaultRating)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    std::string sql = PrepareSQL("DELETE FROM rating WHERE media_id=%i AND media_type='%s'", mediaId, mediaType);
    m_pDS->exec(sql);

    return AddRatings(mediaId, mediaType, values, defaultRating);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s unable to update ratings of  (%s)", __FUNCTION__, mediaType);
  }
  return -1;
}

int CVideoDatabase::AddRatings(int mediaId, const char *mediaType, const RatingMap& values, const std::string& defaultRating)
{
  int ratingid = -1;
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    for (const auto& i : values)
    {
      int id;
      std::string strSQL = PrepareSQL("SELECT rating_id FROM rating WHERE media_id=%i AND media_type='%s' AND rating_type = '%s'", mediaId, mediaType, i.first.c_str());
      m_pDS->query(strSQL);
      if (m_pDS->num_rows() == 0)
      {
        m_pDS->close();
        // doesnt exists, add it
        strSQL = PrepareSQL("INSERT INTO rating (media_id, media_type, rating_type, rating, votes) VALUES (%i, '%s', '%s', %f, %i)", mediaId, mediaType, i.first.c_str(), i.second.rating, i.second.votes);
        m_pDS->exec(strSQL);
        id = (int)m_pDS->lastinsertid();
      }
      else
      {
        id = m_pDS->fv(0).get_asInt();
        m_pDS->close();
        strSQL = PrepareSQL("UPDATE rating SET rating = %f, votes = %i WHERE rating_id = %i", i.second.rating, i.second.votes, id);
        m_pDS->exec(strSQL);        
      }
      if (i.first == defaultRating)
        ratingid = id;
    }
    return ratingid;
    
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i - %s) failed", __FUNCTION__, mediaId, mediaType);
  }

  return ratingid;
}

int CVideoDatabase::UpdateUniqueIDs(int mediaId, const char *mediaType, const CVideoInfoTag& details)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    std::string sql = PrepareSQL("DELETE FROM uniqueid WHERE media_id=%i AND media_type='%s'", mediaId, mediaType);
    m_pDS->exec(sql);

    return AddUniqueIDs(mediaId, mediaType, details);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s unable to update unique ids of (%s)", __FUNCTION__, mediaType);
  }
  return -1;
}

int CVideoDatabase::AddUniqueIDs(int mediaId, const char *mediaType, const CVideoInfoTag& details)
{
  int uniqueid = -1;
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    for (const auto& i : details.GetUniqueIDs())
    {
      int id;
      std::string strSQL = PrepareSQL("SELECT uniqueid_id FROM uniqueid WHERE media_id=%i AND media_type='%s' AND type = '%s'", mediaId, mediaType, i.first.c_str());
      m_pDS->query(strSQL);
      if (m_pDS->num_rows() == 0)
      {
        m_pDS->close();
        // doesnt exists, add it
        strSQL = PrepareSQL("INSERT INTO uniqueid (media_id, media_type, value, type) VALUES (%i, '%s', '%s', '%s')", mediaId, mediaType, i.second.c_str(), i.first.c_str());
        m_pDS->exec(strSQL);
        id = (int)m_pDS->lastinsertid();
      }
      else
      {
        id = m_pDS->fv(0).get_asInt();
        m_pDS->close();
        strSQL = PrepareSQL("UPDATE uniqueid SET value = '%s', type = '%s' WHERE uniqueid_id = %i", i.second.c_str(), i.first.c_str(), id);
        m_pDS->exec(strSQL);
      }
      if (i.first == details.GetDefaultUniqueID())
        uniqueid = id;
    }
    return uniqueid;

  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i - %s) failed", __FUNCTION__, mediaId, mediaType);
  }

  return uniqueid;
}

int CVideoDatabase::AddSet(const std::string& strSet, const std::string& strOverview /* = "" */)
{
  //TODO: This functino should return the ODB Oject
  long returnVal = -1;
  if (strSet.empty())
    return returnVal;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    typedef odb::query<CODBSet> query;
    
    CODBSet odbSet;
    if(m_cdb.getDB()->query_one<CODBSet>(query::name == strSet, odbSet))
    {
      return odbSet.m_idSet;
    }
    else
    {
      odbSet.m_name = strSet;
      odbSet.m_overview = strOverview;
      m_cdb.getDB()->persist(odbSet);
      returnVal = odbSet.m_idSet;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strSet.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSet.c_str());
  }

  return returnVal;
}

int CVideoDatabase::AddTag(const std::string& name)
{
  if (name.empty())
    return -1;
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBTag odbTag;
    if (!m_cdb.getDB()->query_one<CODBTag>(odb::query<CODBTag>::name == name, odbTag))
    {
      odbTag.m_name = name;
      m_cdb.getDB()->persist(odbTag);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return odbTag.m_idTag;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }

  return -1;
}

int CVideoDatabase::AddActor(const std::string& name, const std::string& thumbURLs, const std::string &thumb)
{
  //TODO: Can be depracated, should be done in the respecive type set function
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;
    int idActor = -1;

    // ATTENTION: the trimming of actor names should really not be done here but after the scraping / NFO-parsing
    std::string trimmedName = name.c_str();
    StringUtils::Trim(trimmedName);

    std::string strSQL=PrepareSQL("select actor_id from actor where name like '%s'", trimmedName.substr(0, 255).c_str());
    m_pDS->query(strSQL);
    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      // doesnt exists, add it
      strSQL=PrepareSQL("insert into actor (actor_id, name, art_urls) values(NULL, '%s', '%s')", trimmedName.substr(0,255).c_str(), thumbURLs.c_str());
      m_pDS->exec(strSQL);
      idActor = (int)m_pDS->lastinsertid();
    }
    else
    {
      idActor = m_pDS->fv(0).get_asInt();
      m_pDS->close();
      // update the thumb url's
      if (!thumbURLs.empty())
      {
        strSQL=PrepareSQL("update actor set art_urls = '%s' where actor_id = %i", thumbURLs.c_str(), idActor);
        m_pDS->exec(strSQL);
      }
    }
    // add artwork
    if (!thumb.empty())
      SetArtForItem(idActor, "actor", "thumb", thumb);
    return idActor;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, name.c_str() );
  }
  return -1;
}



void CVideoDatabase::AddLinkToActor(int mediaId, const char *mediaType, int actorId, const std::string &role, int order)
{
  std::string sql=PrepareSQL("SELECT 1 FROM actor_link WHERE actor_id=%i AND media_id=%i AND media_type='%s'", actorId, mediaId, mediaType);

  if (GetSingleValue(sql).empty())
  { // doesnt exists, add it
    sql = PrepareSQL("INSERT INTO actor_link (actor_id, media_id, media_type, role, cast_order) VALUES(%i,%i,'%s','%s',%i)", actorId, mediaId, mediaType, role.c_str(), order);
    ExecuteQuery(sql);
  }
}

void CVideoDatabase::AddToLinkTable(int mediaId, const std::string& mediaType, const std::string& table, int valueId, const char *foreignKey)
{
  const char *key = foreignKey ? foreignKey : table.c_str();
  std::string sql = PrepareSQL("SELECT 1 FROM %s_link WHERE %s_id=%i AND media_id=%i AND media_type='%s'", table.c_str(), key, valueId, mediaId, mediaType.c_str());

  if (GetSingleValue(sql).empty())
  { // doesnt exists, add it
    sql = PrepareSQL("INSERT INTO %s_link (%s_id,media_id,media_type) VALUES(%i,%i,'%s')", table.c_str(), key, valueId, mediaId, mediaType.c_str());
    ExecuteQuery(sql);
  }
}

void CVideoDatabase::RemoveFromLinkTable(int mediaId, const std::string& mediaType, const std::string& table, int valueId, const char *foreignKey)
{
  const char *key = foreignKey ? foreignKey : table.c_str();
  std::string sql = PrepareSQL("DELETE FROM %s_link WHERE %s_id=%i AND media_id=%i AND media_type='%s'", table.c_str(), key, valueId, mediaId, mediaType.c_str());

  ExecuteQuery(sql);
}

void CVideoDatabase::AddLinksToItem(int mediaId, const std::string& mediaType, const std::string& field, const std::vector<std::string>& values)
{
  for (const auto &i : values)
  {
    if (!i.empty())
    {
      int idValue = AddToTable(field, field + "_id", "name", i);
      if (idValue > -1)
        AddToLinkTable(mediaId, mediaType, field, idValue);
    }
  }
}

void CVideoDatabase::UpdateLinksToItem(int mediaId, const std::string& mediaType, const std::string& field, const std::vector<std::string>& values)
{
  std::string sql = PrepareSQL("DELETE FROM %s_link WHERE media_id=%i AND media_type='%s'", field.c_str(), mediaId, mediaType.c_str());
  m_pDS->exec(sql);

  AddLinksToItem(mediaId, mediaType, field, values);
}

void CVideoDatabase::AddActorLinksToItem(int mediaId, const std::string& mediaType, const std::string& field, const std::vector<std::string>& values)
{
  for (const auto &i : values)
  {
    if (!i.empty())
    {
      int idValue = AddActor(i, "");
      if (idValue > -1)
        AddToLinkTable(mediaId, mediaType, field, idValue, "actor");
    }
  }
}

void CVideoDatabase::UpdateActorLinksToItem(int mediaId, const std::string& mediaType, const std::string& field, const std::vector<std::string>& values)
{
  std::string sql = PrepareSQL("DELETE FROM %s_link WHERE media_id=%i AND media_type='%s'", field.c_str(), mediaId, mediaType.c_str());
  m_pDS->exec(sql);

  AddActorLinksToItem(mediaId, mediaType, field, values);
}

//****Tags****
void CVideoDatabase::AddTagToItem(int media_id, int tag_id, const std::string &type)
{
  if (type.empty())
    return;
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::shared_ptr<CODBTag> odbTag(new CODBTag);
    if (!m_cdb.getDB()->query_one<CODBTag>(odb::query<CODBTag>::idTag == tag_id, *odbTag))
      return;
    
    if (type == MediaTypeMovie)
    {
      CODBMovie odbMovie;
      if (!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == media_id, odbMovie))
        return;
      
      m_cdb.getDB()->load(odbMovie, odbMovie.section_foreign);
      
      //Make sure it is not already added
      for (auto& i : odbMovie.m_tags)
      {
        if (!i.load())
          continue; //TODO: Maybe remove the tag if it can not be loaded?
        
        if (i->m_idTag == tag_id)
          return;
      }
      
      odbMovie.m_tags.push_back(odbTag);
      m_cdb.getDB()->update(odbMovie, odbMovie.section_foreign);
    }
    //TODO: Implement other needed types
    else
    {
      CLog::Log(LOGERROR, "%s unknown type %s", __FUNCTION__, type.c_str());
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CVideoDatabase::RemoveTagFromItem(int media_id, int tag_id, const std::string &type)
{
  if (type.empty())
    return;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::shared_ptr<CODBTag> odbTag(new CODBTag);
    if (!m_cdb.getDB()->query_one<CODBTag>(odb::query<CODBTag>::idTag == tag_id, *odbTag))
      return;
    
    if (type == MediaTypeMovie)
    {
      CODBMovie odbMovie;
      if (!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == media_id, odbMovie))
        return;
      
      m_cdb.getDB()->load(odbMovie, odbMovie.section_foreign);
      
      for (std::vector< odb::lazy_shared_ptr<CODBTag> >::iterator i = odbMovie.m_tags.begin(); i != odbMovie.m_tags.end(); i++)
      {
        if (!(*i).load())
          continue; //TODO: Maybe remove the tag if it can not be loaded?
        
        if ((*i)->m_idTag == tag_id)
        {
          odbMovie.m_tags.erase(i);
          break;
        }
      }
      
      m_cdb.getDB()->update(odbMovie, odbMovie.section_foreign);
    }
    //TODO: Implement other needed types
    else
    {
      CLog::Log(LOGERROR, "%s unknown type %s", __FUNCTION__, type.c_str());
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CVideoDatabase::RemoveTagsFromItem(int media_id, const std::string &type)
{
  if (type.empty())
    return;
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (type == MediaTypeMovie)
    {
      CODBMovie odbMovie;
      if (!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == media_id, odbMovie))
        return;
      
      m_cdb.getDB()->load(odbMovie, odbMovie.section_foreign);
      
      //Make sure it is not already added
      for (std::vector< odb::lazy_shared_ptr<CODBTag> >::iterator i = odbMovie.m_tags.begin(); i != odbMovie.m_tags.end();)
      {
          i = odbMovie.m_tags.erase(i);
      }
      
      m_cdb.getDB()->update(odbMovie, odbMovie.section_foreign);
    }
    //TODO: Implement other needed types
    else
    {
      CLog::Log(LOGERROR, "%s unknown type %s", __FUNCTION__, type.c_str());
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

//****Actors****
void CVideoDatabase::AddCast(int mediaId, const char *mediaType, const std::vector< SActorInfo > &cast)
{
  if (cast.empty())
    return;

  int order = std::max_element(cast.begin(), cast.end())->order;
  for (const auto &i : cast)
  {
    int idActor = AddActor(i.strName, i.thumbUrl.m_xml, i.thumb);
    AddLinkToActor(mediaId, mediaType, idActor, i.strRole, i.order >= 0 ? i.order : ++order);
  }
}

//********************************************************************************************************************************
bool CVideoDatabase::LoadVideoInfo(const std::string& strFilenameAndPath, CVideoInfoTag& details, int getDetails /* = VideoDbDetailsAll */)
{
  if (GetMovieInfo(strFilenameAndPath, details))
    return true;
  if (GetEpisodeInfo(strFilenameAndPath, details))
    return true;
  if (GetMusicVideoInfo(strFilenameAndPath, details))
    return true;
  if (GetFileInfo(strFilenameAndPath, details))
    return true;

  return false;
}

bool CVideoDatabase::HasMovieInfo(const std::string& strFilenameAndPath)
{
  try
  {
    int idMovie = GetMovieId(strFilenameAndPath);
    return (idMovie > 0); // index of zero is also invalid
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

bool CVideoDatabase::HasTvShowInfo(const std::string& strPath)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    int idTvShow = GetTvShowId(strPath);
    return (idTvShow > 0); // index of zero is also invalid
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strPath.c_str());
  }
  return false;
}

bool CVideoDatabase::HasEpisodeInfo(const std::string& strFilenameAndPath)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    int idEpisode = GetEpisodeId(strFilenameAndPath);
    return (idEpisode > 0); // index of zero is also invalid
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

bool CVideoDatabase::HasMusicVideoInfo(const std::string& strFilenameAndPath)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    int idMVideo = GetMusicVideoId(strFilenameAndPath);
    return (idMVideo > 0); // index of zero is also invalid
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

void CVideoDatabase::DeleteDetailsForTvShow(const std::string& strPath)
{
  int idTvShow = GetTvShowId(strPath);
  if (idTvShow >= 0)
    DeleteDetailsForTvShow(idTvShow);
}

void CVideoDatabase::DeleteDetailsForTvShow(int idTvShow)
{
  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    std::string strSQL;
    strSQL=PrepareSQL("DELETE from genre_link WHERE media_id=%i AND media_type='tvshow'", idTvShow);
    m_pDS->exec(strSQL);

    strSQL=PrepareSQL("DELETE FROM actor_link WHERE media_id=%i AND media_type='tvshow'", idTvShow);
    m_pDS->exec(strSQL);

    strSQL=PrepareSQL("DELETE FROM director_link WHERE media_id=%i AND media_type='tvshow'", idTvShow);
    m_pDS->exec(strSQL);

    strSQL=PrepareSQL("DELETE FROM studio_link WHERE media_id=%i AND media_type='tvshow'", idTvShow);
    m_pDS->exec(strSQL);

    strSQL = PrepareSQL("DELETE FROM rating WHERE media_id=%i AND media_type='tvshow'", idTvShow);
    m_pDS->exec(strSQL);

    strSQL = PrepareSQL("DELETE FROM uniqueid WHERE media_id=%i AND media_type='tvshow'", idTvShow);
    m_pDS->exec(strSQL);

    // remove all info other than the id
    // we do this due to the way we have the link between the file + movie tables.

    std::vector<std::string> ids;
    for (int iType = VIDEODB_ID_TV_MIN + 1; iType < VIDEODB_ID_TV_MAX; iType++)
      ids.emplace_back(StringUtils::Format("c%02d=NULL", iType));

    strSQL = "update tvshow set ";
    strSQL += StringUtils::Join(ids, ", ");
    strSQL += PrepareSQL(" where idShow=%i", idTvShow);
    m_pDS->exec(strSQL);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idTvShow);
  }
}

//********************************************************************************************************************************
void CVideoDatabase::GetMoviesByActor(const std::string& name, CFileItemList& items)
{
  Filter filter;
  odb::query<ODBView_Movie> optQuery = (odb::query<ODBView_Movie>::actor::name.like(name) || odb::query<ODBView_Movie>::director::name.like(name));
  GetMoviesByWhere("videodb://movies/titles/", filter, items, SortDescription(), VideoDbDetailsNone, optQuery);
}

void CVideoDatabase::GetTvShowsByActor(const std::string& name, CFileItemList& items)
{
  Filter filter;
  filter.join  = "LEFT JOIN actor_link ON actor_link.media_id=tvshow_view.idShow AND actor_link.media_type='tvshow' "
                 "LEFT JOIN actor a ON a.actor_id=actor_link.actor_id "
                 "LEFT JOIN director_link ON director_link.media_id=tvshow_view.idShow AND director_link.media_type='tvshow' "
                 "LEFT JOIN actor d ON d.actor_id=director_link.actor_id";
  filter.where = PrepareSQL("a.name='%s' OR d.name='%s'", name.c_str(), name.c_str());
  GetTvShowsByWhere("videodb://tvshows/titles/", filter, items);
}

void CVideoDatabase::GetEpisodesByActor(const std::string& name, CFileItemList& items)
{
  Filter filter;
  filter.join  = "LEFT JOIN actor_link ON actor_link.media_id=episode_view.idEpisode AND actor_link.media_type='episode' "
                 "LEFT JOIN actor a ON a.actor_id=actor_link.actor_id "
                 "LEFT JOIN director_link ON director_link.media_id=episode_view.idEpisode AND director_link.media_type='episode' "
                 "LEFT JOIN actor d ON d.actor_id=director_link.actor_id";
  filter.where = PrepareSQL("a.name='%s' OR d.name='%s'", name.c_str(), name.c_str());
  filter.group = "episode_view.idEpisode";
  GetEpisodesByWhere("videodb://tvshows/titles/", filter, items);
}

void CVideoDatabase::GetMusicVideosByArtist(const std::string& strArtist, CFileItemList& items)
{
  try
  {
    items.Clear();
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    std::string strSQL;
    if (strArtist.empty())  //! @todo SMARTPLAYLISTS what is this here for???
      strSQL=PrepareSQL("select distinct * from musicvideo_view join actor_link on actor_link.media_id=musicvideo_view.idMVideo AND actor_link.media_type='musicvideo' join actor on actor.actor_id=actor_link.actor_id");
    else
      strSQL=PrepareSQL("select * from musicvideo_view join actor_link on actor_link.media_id=musicvideo_view.idMVideo AND actor_link.media_type='musicvideo' join actor on actor.actor_id=actor_link.actor_id where actor.name='%s'", strArtist.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      CVideoInfoTag tag = GetDetailsForMusicVideo(m_pDS);
      CFileItemPtr pItem(new CFileItem(tag));
      pItem->SetLabel(StringUtils::Join(tag.m_artist, g_advancedSettings.m_videoItemSeparator));
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strArtist.c_str());
  }
}

//********************************************************************************************************************************
bool CVideoDatabase::GetMovieInfo(const std::string& strFilenameAndPath, CVideoInfoTag& details, int idMovie /* = -1 */, int getDetails /* = VideoDbDetailsAll */)
{
  try
  {
    if (idMovie < 0)
      idMovie = GetMovieId(strFilenameAndPath);
    if (idMovie < 0) return false;

    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    

    odb::result<ODBView_Movie> res(m_cdb.getDB()->query<ODBView_Movie>(odb::query<ODBView_Movie>::CODBMovie::idMovie == idMovie));
    for (odb::result<ODBView_Movie>::iterator i = res.begin(); i != res.end(); i++)
    {
      details = GetDetailsForMovie(i, getDetails);
      break;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return !details.IsEmpty();
  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strFilenameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

//********************************************************************************************************************************
bool CVideoDatabase::GetTvShowInfo(const std::string& strPath, CVideoInfoTag& details, int idTvShow /* = -1 */, CFileItem *item /* = NULL */, int getDetails /* = VideoDbDetailsAll */)
{
  try
  {
    if (idTvShow < 0)
      idTvShow = GetTvShowId(strPath);
    if (idTvShow < 0) return false;

    std::string sql = PrepareSQL("SELECT * FROM tvshow_view WHERE idShow=%i GROUP BY idShow", idTvShow);
    if (!m_pDS->query(sql))
      return false;
    details = GetDetailsForTvShow(m_pDS, getDetails, item);
    return !details.IsEmpty();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strPath.c_str());
  }
  return false;
}

bool CVideoDatabase::GetSeasonInfo(int idSeason, CVideoInfoTag& details)
{
  if (idSeason < 0)
    return false;

  try
  {
    if (!m_pDB.get() || !m_pDS.get())
      return false;

    std::string sql = PrepareSQL("SELECT idShow FROM seasons WHERE idSeason=%i", idSeason);
    if (!m_pDS->query(sql))
      return false;

    int idShow = -1;
    if (m_pDS->num_rows() == 1)
      idShow = m_pDS->fv(0).get_asInt();

    m_pDS->close();

    if (idShow < 0)
      return false;

    CFileItemList seasons;
    if (!GetSeasonsNav(StringUtils::Format("videodb://tvshows/titles/%i/", idShow), seasons, -1, -1, -1, -1, idShow, false) || seasons.Size() <= 0)
      return false;

    for (int index = 0; index < seasons.Size(); index++)
    {
      const CFileItemPtr season = seasons.Get(index);
      if (season->HasVideoInfoTag() && season->GetVideoInfoTag()->m_iDbId == idSeason && season->GetVideoInfoTag()->m_iIdShow == idShow)
      {
        details = *season->GetVideoInfoTag();
        return true;
      }
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idSeason);
  }
  return false;
}

bool CVideoDatabase::GetEpisodeInfo(const std::string& strFilenameAndPath, CVideoInfoTag& details, int idEpisode /* = -1 */, int getDetails /* = VideoDbDetailsAll */)
{
  try
  {
    if (idEpisode < 0)
      idEpisode = GetEpisodeId(strFilenameAndPath);
    if (idEpisode < 0) return false;

    std::string sql = PrepareSQL("select * from episode_view where idEpisode=%i",idEpisode);
    if (!m_pDS->query(sql))
      return false;
    details = GetDetailsForEpisode(m_pDS, getDetails);
    return !details.IsEmpty();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

bool CVideoDatabase::GetMusicVideoInfo(const std::string& strFilenameAndPath, CVideoInfoTag& details, int idMVideo /* = -1 */, int getDetails /* = VideoDbDetailsAll */)
{
  try
  {
    if (idMVideo < 0)
      idMVideo = GetMusicVideoId(strFilenameAndPath);
    if (idMVideo < 0) return false;

    std::string sql = PrepareSQL("select * from musicvideo_view where idMVideo=%i", idMVideo);
    if (!m_pDS->query(sql))
      return false;
    details = GetDetailsForMusicVideo(m_pDS, getDetails);
    return !details.IsEmpty();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

bool CVideoDatabase::GetSetInfo(int idSet, CVideoInfoTag& details)
{
  try
  {
    if (idSet < 0)
      return false;

    odb::query<ODBView_Movie> optionalQuery = odb::query<ODBView_Movie>::set::idSet == idSet;
    
    Filter filter;
    CFileItemList items;
    if (!GetSetsByWhere("videodb://movies/sets/", filter, items, false, optionalQuery) ||
        items.Size() != 1 ||
        !items[0]->HasVideoInfoTag())
      return false;

    details = *(items[0]->GetVideoInfoTag());
    return !details.IsEmpty();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%d) failed", __FUNCTION__, idSet);
  }
  return false;
}

bool CVideoDatabase::GetFileInfo(const std::string& strFilenameAndPath, CVideoInfoTag& details, int idFile /* = -1 */)
{
  try
  {
    if (idFile < 0)
      idFile = GetFileId(strFilenameAndPath);
    if (idFile < 0)
      return false;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    typedef odb::query<CODBFile> query;
    CODBFile odbFile;
    if(m_cdb.getDB()->query_one<CODBFile>(query::idFile == idFile, odbFile))
    {
      details.m_iFileId = odbFile.m_idFile;
      if (odbFile.m_path.load())
          details.m_strPath = odbFile.m_path->m_path;
      std::string strFileName = odbFile.m_filename;
      ConstructPath(details.m_strFileNameAndPath, details.m_strPath, strFileName);
      details.m_playCount = std::max(details.m_playCount, static_cast<int>(odbFile.m_playCount)); //TODO: Replace by signed int?
      if (!details.m_lastPlayed.IsValid())
      {
        CDateTime datetime;
        datetime.FromDBDateTime(odbFile.m_lastPlayed.m_date);
        details.m_lastPlayed = datetime;
      }
      if (!details.m_dateAdded.IsValid())
      {
        CDateTime datetime;
        datetime.FromDBDateTime(odbFile.m_dateAdded.m_date);
        details.m_dateAdded = datetime;
      }
      if (!details.m_resumePoint.IsSet())
      {
        CODBBookmark odbBookmark;
        if(m_cdb.getDB()->query_one<CODBBookmark>(odb::query<CODBBookmark>::file->idFile == odbFile.m_idFile &&
                                                  odb::query<CODBBookmark>::type == CBookmark::RESUME, odbBookmark))
        {
          details.m_resumePoint.timeInSeconds = odbBookmark.m_timeInSeconds;
          details.m_resumePoint.totalTimeInSeconds = odbBookmark.m_totalTimeInSeconds;
          details.m_resumePoint.type = CBookmark::RESUME;
        }
      }
      
      // get streamdetails
      GetStreamDetails(details);
    }
    
    if(odb_transaction)
      odb_transaction->commit();

    return !details.IsEmpty();
  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s (%i) exception - %s", __FUNCTION__, idFile, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return false;
}

std::string CVideoDatabase::GetValueString(const CVideoInfoTag &details, int min, int max, const SDbTableOffsets *offsets) const
{
  std::vector<std::string> conditions;
  for (int i = min + 1; i < max; ++i)
  {
    switch (offsets[i].type)
    {
    case VIDEODB_TYPE_STRING:
      conditions.emplace_back(PrepareSQL("c%02d='%s'", i, ((std::string*)(((char*)&details)+offsets[i].offset))->c_str()));
      break;
    case VIDEODB_TYPE_INT:
      conditions.emplace_back(PrepareSQL("c%02d='%i'", i, *(int*)(((char*)&details)+offsets[i].offset)));
      break;
    case VIDEODB_TYPE_COUNT:
      {
        int value = *(int*)(((char*)&details)+offsets[i].offset);
        if (value)
          conditions.emplace_back(PrepareSQL("c%02d=%i", i, value));
        else
          conditions.emplace_back(PrepareSQL("c%02d=NULL", i));
      }
      break;
    case VIDEODB_TYPE_BOOL:
      conditions.emplace_back(PrepareSQL("c%02d='%s'", i, *(bool*)(((char*)&details)+offsets[i].offset)?"true":"false"));
      break;
    case VIDEODB_TYPE_FLOAT:
      conditions.emplace_back(PrepareSQL("c%02d='%f'", i, *(float*)(((char*)&details)+offsets[i].offset)));
      break;
    case VIDEODB_TYPE_STRINGARRAY:
      conditions.emplace_back(PrepareSQL("c%02d='%s'", i, StringUtils::Join(*((std::vector<std::string>*)(((char*)&details)+offsets[i].offset)),
                                                                          g_advancedSettings.m_videoItemSeparator).c_str()));
      break;
    case VIDEODB_TYPE_DATE:
      conditions.emplace_back(PrepareSQL("c%02d='%s'", i, ((CDateTime*)(((char*)&details)+offsets[i].offset))->GetAsDBDate().c_str()));
      break;
    case VIDEODB_TYPE_DATETIME:
      conditions.emplace_back(PrepareSQL("c%02d='%s'", i, ((CDateTime*)(((char*)&details)+offsets[i].offset))->GetAsDBDateTime().c_str()));
      break;
    case VIDEODB_TYPE_UNUSED: // Skip the unused field to avoid populating unused data
      continue;
    }
  }
  return StringUtils::Join(conditions, ",");
}

//********************************************************************************************************************************
int CVideoDatabase::SetDetailsForItem(CVideoInfoTag& details, const std::map<std::string, std::string> &artwork)
{
  return SetDetailsForItem(details.m_iDbId, details.m_type, details, artwork);
}

int CVideoDatabase::SetDetailsForItem(int id, const MediaType& mediaType, CVideoInfoTag& details, const std::map<std::string, std::string> &artwork)
{
  if (mediaType == MediaTypeNone)
    return -1;

  if (mediaType == MediaTypeMovie)
    return SetDetailsForMovie(details.GetPath(), details, artwork, id);
  else if (mediaType == MediaTypeVideoCollection)
    return SetDetailsForMovieSet(details, artwork, id);
  else if (mediaType == MediaTypeTvShow)
  {
    std::map<int, std::map<std::string, std::string> > seasonArtwork;
    if (!UpdateDetailsForTvShow(id, details, artwork, seasonArtwork))
      return -1;

    return id;
  }
  else if (mediaType == MediaTypeSeason)
    return SetDetailsForSeason(details, artwork, details.m_iIdShow, id);
  else if (mediaType == MediaTypeEpisode)
    return SetDetailsForEpisode(details.GetPath(), details, artwork, details.m_iIdShow, id);
  else if (mediaType == MediaTypeMusicVideo)
    return SetDetailsForMusicVideo(details.GetPath(), details, artwork, id);

  return -1;
}

int CVideoDatabase::SetDetailsForMovie(const std::string& strFilenameAndPath, CVideoInfoTag& details,
    const std::map<std::string, std::string> &artwork, int idMovie /* = -1 */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    if (idMovie < 0)
      idMovie = GetMovieId(strFilenameAndPath);

    if (idMovie > -1)
      DeleteMovie(idMovie, true); // true to keep the table entry
    else
    {
      // only add a new movie if we don't already have a valid idMovie
      // (DeleteMovie is called with bKeepId == true so the movie won't
      // be removed from the movie table)
      idMovie = AddMovie(strFilenameAndPath);
      if (idMovie < 0)
      {
        RollbackTransaction();
        return idMovie;
      }
    }
    
    CODBMovie odb_movie;
    if (!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == idMovie, odb_movie))
      return idMovie;
    
    m_cdb.getDB()->load(odb_movie, odb_movie.section_foreign);
    m_cdb.getDB()->load(odb_movie, odb_movie.section_artwork);
    
    //We need the file object many times below
    odb_movie.m_file.load();

    // update dateadded if it's set
    if (details.m_dateAdded.IsValid())
    {
      if (details.m_iFileId <= 0)
        details.m_iFileId = GetFileId(strFilenameAndPath);

      UpdateFileDateAdded(details.m_iFileId, strFilenameAndPath, details.m_dateAdded);
    }

    //TODO: should be moved into its own function
    //TODO: Make only changes, not full recreate
    //TODO: This functino is sometimes called with empty elements
    if (!details.m_cast.empty())
    {
      for (auto& i: odb_movie.m_actors)
      {
        if (i.load())
          m_cdb.getDB()->erase(*i);
      }
      odb_movie.m_actors.clear();
    
      for (auto& i: details.m_cast)
      {
        std::shared_ptr<CODBPerson> person(new CODBPerson);
        if (!m_cdb.getDB()->query_one<CODBPerson>(odb::query<CODBPerson>::name == i.strName, *person))
        {
          std::string trim_name(i.strName);
          person->m_name = StringUtils::Trim(trim_name).substr(0,255);

          std::shared_ptr<CODBArt> art(new CODBArt);
          art->m_media_type = "actor";
          art->m_type = "thumb";
          art->m_url = i.thumb;
          m_cdb.getDB()->persist(art);
          person->m_art = art;

          m_cdb.getDB()->persist(person);
        }

        std::shared_ptr<CODBPersonLink> link(new CODBPersonLink);
        link->m_person = person;
        link->m_role = i.strRole;
        m_cdb.getDB()->persist(link);
        odb_movie.m_actors.push_back(link);
      }
    }
    
    SetMovieDetailsGenres(odb_movie, details.m_genre);
    SetMovieDetailsStudios(odb_movie, details.m_studio);
    SetMovieDetailsCountries(odb_movie, details.m_country);
    SetMovieDetailsTags(odb_movie, details.m_tags);
    SetMovieDetailsDirectors(odb_movie, details.m_director);
    SetMovieDetailsWritingCredits(odb_movie, details.m_writingCredits);
    SetMovieDetailsRating(odb_movie, details);
    SetMovieDetailsUniqueIDs(odb_movie, details);

    // add set...
    if (!details.m_strSet.empty())
    {
      std::shared_ptr<CODBSet> set(new CODBSet);
      if (!m_cdb.getDB()->query_one<CODBSet>(odb::query<CODBSet>::name == details.m_strSet, *set))
      {
        set->m_name = details.m_strSet;
        set->m_overview = details.m_strSetOverview;
        m_cdb.getDB()->persist(set);
      }
      
      //idSet = AddSet(details.m_strSet, details.m_strSetOverview);
      // add art if not available
      if (set->m_artwork.empty())
      {
        for(const auto& i: artwork)
        {
          std::shared_ptr<CODBArt> art(new CODBArt);
          art->m_media_type = "set";
          art->m_type = i.first;
          art->m_url = i.second;
          m_cdb.getDB()->persist(art);
          set->m_artwork.push_back(art);
        }

        m_cdb.getDB()->update(set);
      }

      odb_movie.m_set = set;
    }

    if (details.HasStreamDetails())
      SetStreamDetailsForFileId(details.m_streamDetails, GetFileId(strFilenameAndPath));

    SetMovieDetailsArtwork(odb_movie, artwork);

    if (!details.HasUniqueID() && details.HasYear())
    { // query DB for any movies matching online id and year
      ODBView_Movie_File_UID res;
      
      if (m_cdb.getDB()->query_one<ODBView_Movie_File_UID>(odb::query<ODBView_Movie_File_UID>::CODBUniqueID::value == details.GetUniqueID()
                                                           && odb::query<ODBView_Movie_File_UID>::CODBMovie::premiered.year == details.GetYear()
                                                           && odb::query<ODBView_Movie_File_UID>::CODBMovie::idMovie != idMovie
                                                           && odb::query<ODBView_Movie_File_UID>::CODBFile::playCount > 0
                                                           , res))
      {
        CODBFile matching_file;
        if (m_cdb.getDB()->query_one<CODBFile>(odb::query<CODBFile>::idFile == GetFileId(strFilenameAndPath), matching_file))
        {
          matching_file.m_playCount = res.file->m_playCount;
          matching_file.m_lastPlayed = res.file->m_lastPlayed;
          m_cdb.getDB()->update(matching_file);
        }
      }
    }
    
    SetMovieDetailsValues(odb_movie, details);
    
    m_cdb.getDB()->update(odb_movie);
    m_cdb.getDB()->update(odb_movie, odb_movie.section_foreign);
    m_cdb.getDB()->update(odb_movie, odb_movie.section_artwork);
    if(odb_transaction)
      odb_transaction->commit();

    return idMovie;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strFilenameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  return -1;
}

int CVideoDatabase::UpdateDetailsForMovie(int idMovie, CVideoInfoTag& details, const std::map<std::string, std::string> &artwork, const std::set<std::string> &updatedDetails)
{
  if (idMovie < 0)
    return idMovie;

  try
  {
    CLog::Log(LOGDEBUG, "%s: Starting updates for movie %i", __FUNCTION__, idMovie);
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBMovie odb_movie;
    if (!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == idMovie, odb_movie))
      return idMovie;
    
    m_cdb.getDB()->load(odb_movie, odb_movie.section_foreign);
    m_cdb.getDB()->load(odb_movie, odb_movie.section_artwork);

    // process the link table updates
    if (updatedDetails.find("genre") != updatedDetails.end())
      SetMovieDetailsGenres(odb_movie, details.m_genre);
    if (updatedDetails.find("studio") != updatedDetails.end())
      SetMovieDetailsStudios(odb_movie, details.m_studio);
    if (updatedDetails.find("country") != updatedDetails.end())
      SetMovieDetailsCountries(odb_movie, details.m_country);
    if (updatedDetails.find("tag") != updatedDetails.end())
      SetMovieDetailsTags(odb_movie, details.m_tags);
    if (updatedDetails.find("director") != updatedDetails.end())
      SetMovieDetailsDirectors(odb_movie, details.m_director);
    if (updatedDetails.find("writer") != updatedDetails.end())
      SetMovieDetailsWritingCredits(odb_movie, details.m_writingCredits);
    if (updatedDetails.find("art.altered") != updatedDetails.end())
      SetMovieDetailsArtwork(odb_movie, artwork);
    if (updatedDetails.find("ratings") != updatedDetails.end())
      SetMovieDetailsRating(odb_movie, details);
      //details.m_iIdRating = UpdateRatings(idMovie, MediaTypeMovie, details.m_ratings, details.GetDefaultRating());
    if (updatedDetails.find("uniqueid") != updatedDetails.end())
      SetMovieDetailsUniqueIDs(odb_movie, details);
      //details.m_iIdUniqueID = UpdateUniqueIDs(idMovie, MediaTypeMovie, details);
    if (updatedDetails.find("dateadded") != updatedDetails.end() && details.m_dateAdded.IsValid())
    {
      if (details.m_iFileId <= 0)
        details.m_iFileId = GetFileId(details.GetPath());

      UpdateFileDateAdded(details.m_iFileId, details.GetPath(), details.m_dateAdded);
    }

    // track if the set was updated
    int idSet = 0;
    if (updatedDetails.find("set") != updatedDetails.end())
    { // set
      idSet = -1;
      if (!details.m_strSet.empty())
      {
        std::shared_ptr<CODBSet> set(new CODBSet);
        if (!m_cdb.getDB()->query_one<CODBSet>(odb::query<CODBSet>::name == details.m_strSet, *set))
        {
          set->m_name = details.m_strSet;
          set->m_overview = details.m_strSetOverview;
          m_cdb.getDB()->persist(set);
        }
        
        SetSetDetailsArtwork(*set, artwork);
      }
    }

    //TODO: Add show link
    /*if (updatedDetails.find("showlink") != updatedDetails.end())
    {
      // remove existing links
      std::vector<int> tvShowIds;
      GetLinksToTvShow(idMovie, tvShowIds);
      for (const auto& idTVShow : tvShowIds)
        LinkMovieToTvshow(idMovie, idTVShow, true);

      // setup links to shows if the linked shows are in the db
      for (const auto& showLink : details.m_showLink)
      {
        CFileItemList items;
        GetTvShowsByName(showLink, items);
        if (!items.IsEmpty())
          LinkMovieToTvshow(idMovie, items[0]->GetVideoInfoTag()->m_iDbId, false);
        else
          CLog::Log(LOGWARNING, "%s: Failed to link movie %s to show %s", __FUNCTION__, details.m_strTitle.c_str(), showLink.c_str());
      }
    }*/

    SetMovieDetailsValues(odb_movie, details);
    
    m_cdb.getDB()->update(odb_movie);
    m_cdb.getDB()->update(odb_movie, odb_movie.section_foreign);
    m_cdb.getDB()->update(odb_movie, odb_movie.section_artwork);

    if(odb_transaction)
      odb_transaction->commit();

    CLog::Log(LOGDEBUG, "%s: Finished updates for movie %i", __FUNCTION__, idMovie);

    return idMovie;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%i) exception - %s", __FUNCTION__, idMovie, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idMovie);
  }
  RollbackTransaction();
  return -1;
}

void CVideoDatabase::SetMovieDetailsGenres(CODBMovie& odbMovie, std::vector<std::string>& genres)
{
  if (!genres.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBGenre> >::iterator i = odbMovie.m_genres.begin(); i != odbMovie.m_genres.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_genres.erase(i);
    }
    
    for (auto& i: genres)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_genres)
      {
        if (j->m_name == i)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBGenre> genre(new CODBGenre);
      if (!m_cdb.getDB()->query_one<CODBGenre>(odb::query<CODBGenre>::name == i, *genre))
      {
        genre->m_name = i;
        m_cdb.getDB()->persist(genre);
      }
      genre->m_synced = true;
      odbMovie.m_genres.push_back(genre);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBGenre> >::iterator i = odbMovie.m_genres.begin(); i != odbMovie.m_genres.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_genres.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsStudios(CODBMovie& odbMovie, std::vector<std::string>& studios)
{
  if (!studios.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBStudio> >::iterator i = odbMovie.m_studios.begin(); i != odbMovie.m_studios.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_studios.erase(i);
    }
    
    for (auto& i: studios)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_studios)
      {
        if (j->m_name == i)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBStudio> studio(new CODBStudio);
      if (!m_cdb.getDB()->query_one<CODBStudio>(odb::query<CODBStudio>::name == i, *studio))
      {
        studio->m_name = i;
        m_cdb.getDB()->persist(studio);
      }
      studio->m_synced = true;
      odbMovie.m_studios.push_back(studio);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBStudio> >::iterator i = odbMovie.m_studios.begin(); i != odbMovie.m_studios.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_studios.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsCountries(CODBMovie& odbMovie, std::vector<std::string>& countries)
{
  if (!countries.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBCountry> >::iterator i = odbMovie.m_countries.begin(); i != odbMovie.m_countries.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_countries.erase(i);
    }
    
    for (auto& i: countries)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_countries)
      {
        if (j->m_name == i)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBCountry> country(new CODBCountry);
      if (!m_cdb.getDB()->query_one<CODBCountry>(odb::query<CODBCountry>::name == i, *country))
      {
        country->m_name = i;
        m_cdb.getDB()->persist(country);
      }
      country->m_synced = true;
      odbMovie.m_countries.push_back(country);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBCountry> >::iterator i = odbMovie.m_countries.begin(); i != odbMovie.m_countries.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_countries.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsTags(CODBMovie& odbMovie, std::vector<std::string>& tags)
{
  if (!tags.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBTag> >::iterator i = odbMovie.m_tags.begin(); i != odbMovie.m_tags.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_tags.erase(i);
    }
    
    for (auto& i: tags)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_tags)
      {
        if (j->m_name == i)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBTag> tag(new CODBTag);
      if (!m_cdb.getDB()->query_one<CODBTag>(odb::query<CODBTag>::name == i, *tag))
      {
        tag->m_name = i;
        m_cdb.getDB()->persist(tag);
      }
      tag->m_synced = true;
      odbMovie.m_tags.push_back(tag);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBTag> >::iterator i = odbMovie.m_tags.begin(); i != odbMovie.m_tags.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_tags.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsDirectors(CODBMovie& odbMovie, std::vector<std::string>& directors)
{
  if (!directors.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBPersonLink> >::iterator i = odbMovie.m_directors.begin(); i != odbMovie.m_directors.end();)
    {
      if ((*i).load() && (*i)->m_person.load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_directors.erase(i);
    }
    
    for (auto& i: directors)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_directors)
      {
        if (j->m_person->m_name == i)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBPerson> person(new CODBPerson);
      if (!m_cdb.getDB()->query_one<CODBPerson>(odb::query<CODBPerson>::name == i, *person))
      {
        std::string trim_name(i);
        person->m_name = StringUtils::Trim(trim_name).substr(0,255);
        m_cdb.getDB()->persist(person);
      }
      
      std::shared_ptr<CODBPersonLink> link(new CODBPersonLink);
      link->m_person = person;
      m_cdb.getDB()->persist(link);
      link->m_synced = true;
      odbMovie.m_directors.push_back(link);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBPersonLink> >::iterator i = odbMovie.m_directors.begin(); i != odbMovie.m_directors.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_directors.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsWritingCredits(CODBMovie& odbMovie, std::vector<std::string>& writingCredits)
{
  if (!writingCredits.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBPersonLink> >::iterator i = odbMovie.m_writingCredits.begin(); i != odbMovie.m_writingCredits.end();)
    {
      if ((*i).load() && (*i)->m_person.load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_writingCredits.erase(i);
    }
    
    for (auto& i: writingCredits)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_writingCredits)
      {
        if (j->m_person->m_name == i)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBPerson> person(new CODBPerson);
      if (!m_cdb.getDB()->query_one<CODBPerson>(odb::query<CODBPerson>::name == i, *person))
      {
        std::string trim_name(i);
        person->m_name = StringUtils::Trim(trim_name).substr(0,255);
        m_cdb.getDB()->persist(person);
      }
      
      std::shared_ptr<CODBPersonLink> link(new CODBPersonLink);
      link->m_person = person;
      m_cdb.getDB()->persist(link);
      link->m_synced = true;
      odbMovie.m_writingCredits.push_back(link);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBPersonLink> >::iterator i = odbMovie.m_writingCredits.begin(); i != odbMovie.m_writingCredits.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_writingCredits.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsArtwork(CODBMovie& odbMovie, const std::map<std::string, std::string>& artwork)
{
  if (!artwork.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBArt> >::iterator i = odbMovie.m_artwork.begin(); i != odbMovie.m_artwork.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_artwork.erase(i);
    }
    
    for (auto& i: artwork)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_artwork)
      {
        if (j->m_type == i.first && j->m_url == i.second)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBArt> art(new CODBArt);
      art->m_media_type = "movie";
      art->m_type = i.first;
      art->m_url = i.second;
      m_cdb.getDB()->persist(art);
      art->m_synced = true;
      odbMovie.m_artwork.push_back(art);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBArt> >::iterator i = odbMovie.m_artwork.begin(); i != odbMovie.m_artwork.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_artwork.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsRating(CODBMovie& odbMovie, CVideoInfoTag& details)
{
  if (!details.m_ratings.empty())
  {
    details.m_iIdRating = -1;
    
    for (std::vector<odb::lazy_shared_ptr<CODBRating> >::iterator i = odbMovie.m_ratings.begin(); i != odbMovie.m_ratings.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_ratings.erase(i);
    }
    
    for (auto& i: details.m_ratings)
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_ratings)
      {
        if (j->m_rating == i.second.rating && j->m_votes == i.second.votes)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      std::shared_ptr<CODBRating> rating;
      if (!already_exists)
      {
        rating = std::shared_ptr<CODBRating>(new CODBRating);
        typedef odb::query<CODBRating> query;
        if (m_cdb.getDB()->query_one<CODBRating>(query::file->idFile == odbMovie.m_file->m_idFile
                                                 && query::ratingType == i.first
                                                 , *rating) )
        {
          rating->m_rating = i.second.rating;
          rating->m_votes = i.second.votes;
          m_cdb.getDB()->update(rating);
        }
        else
        {
          rating->m_file = odbMovie.m_file;
          rating->m_rating = i.second.rating;
          rating->m_votes = i.second.votes;
          m_cdb.getDB()->persist(rating);
        }
        rating->m_synced = true;
        odbMovie.m_ratings.push_back(rating);
      }
      //Set the default rating
      if(i.first == details.GetDefaultRating())
      {
        odbMovie.m_defaultRating = rating;
      }
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBRating> >::iterator i = odbMovie.m_ratings.begin(); i != odbMovie.m_ratings.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_ratings.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsUniqueIDs(CODBMovie& odbMovie, CVideoInfoTag& details)
{
  if (!details.m_ratings.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBUniqueID> >::iterator i = odbMovie.m_ids.begin(); i != odbMovie.m_ids.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbMovie.m_ids.erase(i);
    }
    
    for (auto& i: details.GetUniqueIDs())
    {
      bool already_exists = false;
      for (auto& j: odbMovie.m_ids)
      {
        if (j->m_type == i.first && j->m_value == i.second)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if(!already_exists)
      {
        std::shared_ptr<CODBUniqueID> uid(new CODBUniqueID);
        typedef odb::query<CODBUniqueID> query;
        if (m_cdb.getDB()->query_one<CODBUniqueID>(query::file->idFile == odbMovie.m_file->m_idFile
                                                   && query::type == i.first
                                                   , *uid))
        {
          uid->m_value = i.second;
          m_cdb.getDB()->update(uid);
        }
        else
        {
          uid->m_type = i.first;
          uid->m_value = i.second;
          m_cdb.getDB()->persist(uid);
        }
        uid->m_synced = true;
        odbMovie.m_ids.push_back(uid);
        
        if(uid->m_type == details.GetDefaultUniqueID())
          odbMovie.m_defaultID = uid;
      }
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBUniqueID> >::iterator i = odbMovie.m_ids.begin(); i != odbMovie.m_ids.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbMovie.m_ids.erase(i);
      }
      else
        i++;
    }
  }
}

void CVideoDatabase::SetMovieDetailsValues(CODBMovie& odbMovie, CVideoInfoTag& details)
{
  odbMovie.m_title = details.m_strTitle;
  odbMovie.m_plot = details.m_strPlot;
  odbMovie.m_plotoutline = details.m_strPlotOutline;
  odbMovie.m_tagline = details.m_strTagLine;
  odbMovie.m_credits = StringUtils::Join(details.m_writingCredits, g_advancedSettings.m_videoItemSeparator);
  odbMovie.m_thumbUrl = details.m_strPictureURL.m_xml;
  odbMovie.m_sortTitle = details.m_strSortTitle;
  odbMovie.m_runtime = details.m_duration;
  odbMovie.m_mpaa = details.m_strMPAARating;
  odbMovie.m_top250 = details.m_iTop250;
  odbMovie.m_originalTitle = details.m_strOriginalTitle;
  odbMovie.m_thumbUrl_spoof = details.m_strPictureURL.m_spoof;
  odbMovie.m_trailer = details.m_strTrailer;
  odbMovie.m_fanart = details.m_fanart.m_xml;
  
  //TODO: These relations can be refacored away in the future
  odbMovie.m_file->m_path.load();
  odbMovie.m_basePath = odbMovie.m_file->m_path;
  odbMovie.m_file->m_path->m_parentPath.load();
  odbMovie.m_parentPath = odbMovie.m_file->m_path->m_parentPath;
  
  if (details.m_iUserRating > 0 && details.m_iUserRating < 11)
    odbMovie.m_userrating = details.m_iUserRating;
  
  if (details.m_premiered.IsValid())
  {
    if (details.HasPremiered())
    {
      odbMovie.m_premiered.setDateTime(details.GetPremiered().GetAsULongLong(), details.GetPremiered().GetAsDBDateTime());
    }
    else
    {
      CDateTime yeartime(details.GetYear(), 1, 1, 0, 0, 0);
      odbMovie.m_premiered.setDateTime(yeartime.GetAsULongLong(), yeartime.GetAsDBDateTime());
    }
  }
}

void CVideoDatabase::SetSetDetailsArtwork(CODBSet& odbSet, const std::map<std::string, std::string>& artwork)
{
  if (!artwork.empty())
  {
    for (std::vector<odb::lazy_shared_ptr<CODBArt> >::iterator i = odbSet.m_artwork.begin(); i != odbSet.m_artwork.end();)
    {
      if ((*i).load())
      {
        (*i)->m_synced = false;
        i++;
      }
      else
        //Object could not be loaded, removing it
        i = odbSet.m_artwork.erase(i);
    }
    
    for (auto& i: artwork)
    {
      bool already_exists = false;
      for (auto& j: odbSet.m_artwork)
      {
        if (j->m_type == i.first && j->m_url == i.second)
        {
          j->m_synced = true;
          already_exists = true;
          break;
        }
      }
      
      if (already_exists)
        continue;
      
      std::shared_ptr<CODBArt> art(new CODBArt);
      art->m_media_type = "set";
      art->m_type = i.first;
      art->m_url = i.second;
      m_cdb.getDB()->persist(art);
      art->m_synced = true;
      odbSet.m_artwork.push_back(art);
    }
    
    //Cleanup
    for (std::vector<odb::lazy_shared_ptr<CODBArt> >::iterator i = odbSet.m_artwork.begin(); i != odbSet.m_artwork.end();)
    {
      if (!(*i)->m_synced)
      {
        i = odbSet.m_artwork.erase(i);
      }
      else
        i++;
    }
  }
}

int CVideoDatabase::SetDetailsForMovieSet(const CVideoInfoTag& details, const std::map<std::string, std::string> &artwork, int idSet /* = -1 */)
{
  if (details.m_strTitle.empty())
    return -1;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (idSet < 0)
    {
      idSet = AddSet(details.m_strTitle, details.m_strPlot);
      if (idSet < 0)
      {
        if(odb_transaction)
          odb_transaction->rollback();
        return -1;
      }
    }
    
    CODBSet odbSet;
    if (!m_cdb.getDB()->query_one<CODBSet>(odb::query<CODBSet>::idSet == idSet, odbSet))
    {
      return -1;
    }
    
    SetSetDetailsArtwork(odbSet, artwork);
    
    odbSet.m_name = details.m_strTitle;
    odbSet.m_overview = details.m_strPlot;
    
    m_cdb.getDB()->update(odbSet);

    if(odb_transaction)
      odb_transaction->commit();

    return idSet;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%i) exception - %s", __FUNCTION__, idSet, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idSet);
  }
  RollbackTransaction();
  return -1;
}

int CVideoDatabase::GetMatchingTvShow(const CVideoInfoTag &details)
{
  // first try matching on uniqueid, then on title + year
  int id = -1;
  if (!details.HasUniqueID())
    id = GetDbId(PrepareSQL("SELECT idShow FROM tvshow JOIN uniqueid ON uniqueid.media_id=tvshow.idShow AND uniqueid.media_type='tvshow' WHERE uniqueid.value='%s'", details.GetUniqueID().c_str()));
  if (id < 0)
    id = GetDbId(PrepareSQL("SELECT idShow FROM tvshow WHERE c%02d='%s' AND c%02d='%s'", VIDEODB_ID_TV_TITLE, details.m_strTitle.c_str(), VIDEODB_ID_TV_PREMIERED, details.GetPremiered().GetAsDBDate().c_str()));
  return id;
}

int CVideoDatabase::SetDetailsForTvShow(const std::vector<std::pair<std::string, std::string> > &paths,
    CVideoInfoTag& details, const std::map<std::string, std::string> &artwork,
    const std::map<int, std::map<std::string, std::string> > &seasonArt, int idTvShow /*= -1 */)
{

  /*
   The steps are as follows.
   1. Check if the tvshow is found on any of the given paths.  If found, we have the show id.
   2. Search for a matching show.  If found, we have the show id.
   3. If we don't have the id, add a new show.
   4. Add the paths to the show.
   5. Add details for the show.
   */

  if (idTvShow < 0)
  {
    for (const auto &i : paths)
    {
      idTvShow = GetTvShowId(i.first);
      if (idTvShow > -1)
        break;
    }
  }
  if (idTvShow < 0)
    idTvShow = GetMatchingTvShow(details);
  if (idTvShow < 0)
  {
    idTvShow = AddTvShow();
    if (idTvShow < 0)
      return -1;
  }

  // add any paths to the tvshow
  for (const auto &i : paths)
    AddPathToTvShow(idTvShow, i.first, i.second, details.m_dateAdded);

  UpdateDetailsForTvShow(idTvShow, details, artwork, seasonArt);

  return idTvShow;
}

bool CVideoDatabase::UpdateDetailsForTvShow(int idTvShow, CVideoInfoTag &details,
    const std::map<std::string, std::string> &artwork, const std::map<int, std::map<std::string, std::string>> &seasonArt)
{
  BeginTransaction();

  DeleteDetailsForTvShow(idTvShow);

  AddCast(idTvShow, "tvshow", details.m_cast);
  AddLinksToItem(idTvShow, MediaTypeTvShow, "genre", details.m_genre);
  AddLinksToItem(idTvShow, MediaTypeTvShow, "studio", details.m_studio);
  AddLinksToItem(idTvShow, MediaTypeTvShow, "tag", details.m_tags);
  AddActorLinksToItem(idTvShow, MediaTypeTvShow, "director", details.m_director);

  // add ratings
  details.m_iIdRating = AddRatings(idTvShow, MediaTypeTvShow, details.m_ratings, details.GetDefaultRating());

  // add unique ids
  details.m_iIdUniqueID = UpdateUniqueIDs(idTvShow, MediaTypeTvShow, details);

  // add "all seasons" - the rest are added in SetDetailsForEpisode
  AddSeason(idTvShow, -1);

  // add any named seasons
  for (const auto& namedSeason : details.m_namedSeasons)
  {
    // make sure the named season exists
    int seasonId = AddSeason(idTvShow, namedSeason.first);

    // get any existing details for the named season
    CVideoInfoTag season;
    if (!GetSeasonInfo(seasonId, season) || season.m_strSortTitle == namedSeason.second)
      continue;

    season.SetSortTitle(namedSeason.second);
    SetDetailsForSeason(season, std::map<std::string, std::string>(), idTvShow, seasonId);
  }

  SetArtForItem(idTvShow, MediaTypeTvShow, artwork);
  for (const auto &i : seasonArt)
  {
    int idSeason = AddSeason(idTvShow, i.first);
    if (idSeason > -1)
      SetArtForItem(idSeason, MediaTypeSeason, i.second);
  }

  // and insert the new row
  std::string sql = "UPDATE tvshow SET " + GetValueString(details, VIDEODB_ID_TV_MIN, VIDEODB_ID_TV_MAX, DbTvShowOffsets);
  if (details.m_iUserRating > 0 && details.m_iUserRating < 11)
    sql += PrepareSQL(", userrating = %i", details.m_iUserRating);
  else
    sql += ", userrating = NULL";  
  if (details.m_duration > 0)
    sql += PrepareSQL(", duration = %i", details.m_duration);
  else
    sql += ", duration = NULL";
  sql += PrepareSQL(" WHERE idShow=%i", idTvShow);
  if (ExecuteQuery(sql))
  {
    CommitTransaction();
    return true;
  }
  RollbackTransaction();
  return false;
}

int CVideoDatabase::SetDetailsForSeason(const CVideoInfoTag& details, const std::map<std::string,
    std::string> &artwork, int idShow, int idSeason /* = -1 */)
{
  if (idShow < 0 || details.m_iSeason < -1)
    return -1;

   try
  {
    BeginTransaction();
    if (idSeason < 0)
    {
      idSeason = AddSeason(idShow, details.m_iSeason);
      if (idSeason < 0)
      {
        RollbackTransaction();
        return -1;
      }
    }

    SetArtForItem(idSeason, MediaTypeSeason, artwork);

    // and insert the new row
    std::string sql = PrepareSQL("UPDATE seasons SET season=%i", details.m_iSeason);
    if (!details.m_strSortTitle.empty())
      sql += PrepareSQL(", name='%s'", details.m_strSortTitle.c_str());
    if (details.m_iUserRating > 0 && details.m_iUserRating < 11)
      sql += PrepareSQL(", userrating = %i", details.m_iUserRating);
    else
      sql += ", userrating = NULL";
    sql += PrepareSQL(" WHERE idSeason=%i", idSeason);
    m_pDS->exec(sql.c_str());
    CommitTransaction();

    return idSeason;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idSeason);
  }
  RollbackTransaction();
  return -1;
}

int CVideoDatabase::SetDetailsForEpisode(const std::string& strFilenameAndPath, CVideoInfoTag& details,
    const std::map<std::string, std::string> &artwork, int idShow, int idEpisode)
{
  try
  {
    BeginTransaction();
    if (idEpisode < 0)
      idEpisode = GetEpisodeId(strFilenameAndPath);

    if (idEpisode > 0)
      DeleteEpisode(idEpisode, true); // true to keep the table entry
    else
    {
      // only add a new episode if we don't already have a valid idEpisode
      // (DeleteEpisode is called with bKeepId == true so the episode won't
      // be removed from the episode table)
      idEpisode = AddEpisode(idShow,strFilenameAndPath);
      if (idEpisode < 0)
      {
        RollbackTransaction();
        return -1;
      }
    }

    // update dateadded if it's set
    if (details.m_dateAdded.IsValid())
    {
      if (details.m_iFileId <= 0)
        details.m_iFileId = GetFileId(strFilenameAndPath);

      UpdateFileDateAdded(details.m_iFileId, strFilenameAndPath, details.m_dateAdded);
    }

    AddCast(idEpisode, "episode", details.m_cast);
    AddActorLinksToItem(idEpisode, MediaTypeEpisode, "director", details.m_director);
    AddActorLinksToItem(idEpisode, MediaTypeEpisode, "writer", details.m_writingCredits);

    // add ratings
    details.m_iIdRating = AddRatings(idEpisode, MediaTypeEpisode, details.m_ratings, details.GetDefaultRating());

    // add unique ids
    details.m_iIdUniqueID = UpdateUniqueIDs(idEpisode, MediaTypeEpisode, details);

    if (details.HasStreamDetails())
    {
      if (details.m_iFileId != -1)
        SetStreamDetailsForFileId(details.m_streamDetails, details.m_iFileId);
      else
        SetStreamDetailsForFile(details.m_streamDetails, strFilenameAndPath);
    }

    // ensure we have this season already added
    int idSeason = AddSeason(idShow, details.m_iSeason);

    SetArtForItem(idEpisode, MediaTypeEpisode, artwork);

    if (details.m_iEpisode != -1 && details.m_iSeason != -1)
    { // query DB for any episodes matching idShow, Season and Episode
      std::string strSQL = PrepareSQL("SELECT files.playCount, files.lastPlayed "
                                      "FROM episode INNER JOIN files ON files.idFile=episode.idFile "
                                      "WHERE episode.c%02d=%i AND episode.c%02d=%i AND episode.idShow=%i "
                                      "AND episode.idEpisode!=%i AND files.playCount > 0", 
                                      VIDEODB_ID_EPISODE_SEASON, details.m_iSeason, VIDEODB_ID_EPISODE_EPISODE,
                                      details.m_iEpisode, idShow, idEpisode);
      m_pDS->query(strSQL);

      if (!m_pDS->eof())
      {
        int playCount = m_pDS->fv("files.playCount").get_asInt();

        CDateTime lastPlayed;
        lastPlayed.SetFromDBDateTime(m_pDS->fv("files.lastPlayed").get_asString());

        int idFile = GetFileId(strFilenameAndPath);

        // update with playCount and lastPlayed
        strSQL = PrepareSQL("update files set playCount=%i,lastPlayed='%s' where idFile=%i", playCount, lastPlayed.GetAsDBDateTime().c_str(), idFile);
        m_pDS->exec(strSQL);
      }

      m_pDS->close();
    }
    // and insert the new row
    std::string sql = "UPDATE episode SET " + GetValueString(details, VIDEODB_ID_EPISODE_MIN, VIDEODB_ID_EPISODE_MAX, DbEpisodeOffsets);
    if (details.m_iUserRating > 0 && details.m_iUserRating < 11)
      sql += PrepareSQL(", userrating = %i", details.m_iUserRating);
    else
      sql += ", userrating = NULL";
    sql += PrepareSQL(", idSeason = %i", idSeason);
    sql += PrepareSQL(" where idEpisode=%i", idEpisode);
    m_pDS->exec(sql);
    CommitTransaction();

    return idEpisode;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  RollbackTransaction();
  return -1;
}

int CVideoDatabase::GetSeasonId(int showID, int season)
{
  std::string sql = PrepareSQL("idShow=%i AND season=%i", showID, season);
  std::string id = GetSingleValue("seasons", "idSeason", sql);
  if (id.empty())
    return -1;
  return strtol(id.c_str(), NULL, 10);
}

int CVideoDatabase::AddSeason(int showID, int season)
{
  int seasonId = GetSeasonId(showID, season);
  if (seasonId < 0)
  {
    if (ExecuteQuery(PrepareSQL("INSERT INTO seasons (idShow,season,name) VALUES(%i,%i,'')", showID, season)))
      seasonId = (int)m_pDS->lastinsertid();
  }
  return seasonId;
}

int CVideoDatabase::SetDetailsForMusicVideo(const std::string& strFilenameAndPath, const CVideoInfoTag& details,
    const std::map<std::string, std::string> &artwork, int idMVideo /* = -1 */)
{
  try
  {
    BeginTransaction();

    if (idMVideo < 0)
      idMVideo = GetMusicVideoId(strFilenameAndPath);

    if (idMVideo > -1)
      DeleteMusicVideo(idMVideo, true); // Keep id
    else
    {
      // only add a new musicvideo if we don't already have a valid idMVideo
      // (DeleteMusicVideo is called with bKeepId == true so the musicvideo won't
      // be removed from the musicvideo table)
      idMVideo = AddMusicVideo(strFilenameAndPath);
      if (idMVideo < 0)
      {
        RollbackTransaction();
        return -1;
      }
    }

    // update dateadded if it's set
    if (details.m_dateAdded.IsValid())
    {
      int idFile = details.m_iFileId;
      if (idFile <= 0)
        idFile = GetFileId(strFilenameAndPath);

      UpdateFileDateAdded(idFile, strFilenameAndPath, details.m_dateAdded);
    }

    AddActorLinksToItem(idMVideo, MediaTypeMusicVideo, "actor", details.m_artist);
    AddActorLinksToItem(idMVideo, MediaTypeMusicVideo, "director", details.m_director);
    AddLinksToItem(idMVideo, MediaTypeMusicVideo, "genre", details.m_genre);
    AddLinksToItem(idMVideo, MediaTypeMusicVideo, "studio", details.m_studio);
    AddLinksToItem(idMVideo, MediaTypeMusicVideo, "tag", details.m_tags);

    if (details.HasStreamDetails())
      SetStreamDetailsForFileId(details.m_streamDetails, GetFileId(strFilenameAndPath));

    SetArtForItem(idMVideo, MediaTypeMusicVideo, artwork);

    // update our movie table (we know it was added already above)
    // and insert the new row
    std::string sql = "UPDATE musicvideo SET " + GetValueString(details, VIDEODB_ID_MUSICVIDEO_MIN, VIDEODB_ID_MUSICVIDEO_MAX, DbMusicVideoOffsets);
    if (details.m_iUserRating > 0 && details.m_iUserRating < 11)
      sql += PrepareSQL(", userrating = %i", details.m_iUserRating);
    else
      sql += ", userrating = NULL";
    if (details.HasPremiered())
      sql += PrepareSQL(", premiered = '%s'", details.GetPremiered().GetAsDBDate().c_str());
    else
      sql += PrepareSQL(", premiered = '%i'", details.GetYear());
    sql += PrepareSQL(" where idMVideo=%i", idMVideo);
    m_pDS->exec(sql);
    CommitTransaction();

    return idMVideo;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
  RollbackTransaction();
  return -1;
}

void CVideoDatabase::SetStreamDetailsForFile(const CStreamDetails& details, const std::string &strFileNameAndPath)
{
  // AddFile checks to make sure the file isn't already in the DB first
  int idFile = AddFile(strFileNameAndPath);
  if (idFile < 0)
    return;
  SetStreamDetailsForFileId(details, idFile);
}

void CVideoDatabase::SetStreamDetailsForFileId(const CStreamDetails& details, int idFile)
{
  if (idFile < 0)
    return;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::shared_ptr<CODBFile> odb_file(new CODBFile);
    if (!m_cdb.getDB()->query_one<CODBFile>(odb::query<CODBFile>::idFile == idFile, *odb_file))
      return;
    
    m_cdb.getDB()->erase_query<CODBStreamDetails>(odb::query<CODBStreamDetails>::file == odb_file->m_idFile);

    for (int i=1; i<=details.GetVideoStreamCount(); i++)
    {
      CODBStreamDetails sd;
      sd.m_file = odb_file;
      sd.m_streamType = (int)CStreamDetail::VIDEO;
      sd.m_videoCodec = details.GetVideoCodec(i);
      sd.m_videoAspect = details.GetVideoAspect(i);
      sd.m_videoWidth = details.GetVideoWidth(i);
      sd.m_videoHeight = details.GetVideoHeight(i);
      sd.m_videoDuration = details.GetVideoDuration(i);
      sd.m_stereoMode = details.GetStereoMode(i);
      sd.m_videoLanguage = details.GetVideoLanguage(i);
      m_cdb.getDB()->persist(sd);
    }
    for (int i=1; i<=details.GetAudioStreamCount(); i++)
    {
      CODBStreamDetails sd;
      sd.m_file = odb_file;
      sd.m_streamType = (int)CStreamDetail::AUDIO;
      sd.m_audioCodec = details.GetAudioCodec(i);
      sd.m_audioChannels = details.GetAudioChannels(i);
      sd.m_audioLanguage = details.GetAudioLanguage(i);
      m_cdb.getDB()->persist(sd);
    }
    for (int i=1; i<=details.GetSubtitleStreamCount(); i++)
    {
      CODBStreamDetails sd;
      sd.m_file = odb_file;
      sd.m_streamType = (int)CStreamDetail::SUBTITLE;
      sd.m_subtitleLanguage = details.GetSubtitleLanguage(i);
      m_cdb.getDB()->persist(sd);
    }

    // update the runtime information, if empty
    if (details.GetVideoDuration())
    {
      CODBMovie movie;
      if (m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::file->idFile == odb_file->m_idFile, movie))
      {
        movie.m_runtime = details.GetVideoDuration();
        m_cdb.getDB()->update(movie);
      }
      
      /*std::vector<std::pair<std::string, int> > tables;
      tables.emplace_back("episode", VIDEODB_ID_EPISODE_RUNTIME);
      tables.emplace_back("musicvideo", VIDEODB_ID_MUSICVIDEO_RUNTIME);
      for (const auto &i : tables)
      {
        std::string sql = PrepareSQL("update %s set c%02d=%d where idFile=%d and c%02d=''",
                                    i.first.c_str(), i.second, details.GetVideoDuration(), idFile, i.second);
        m_pDS->exec(sql);
      }*/
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%i) exception - %s", __FUNCTION__, idFile, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idFile);
  }
}

//********************************************************************************************************************************
void CVideoDatabase::GetFilePathById(int idMovie, std::string &filePath, VIDEODB_CONTENT_TYPE iType)
{
  try
  {
    if (idMovie < 0) return ;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::string db_filename;
    std::string db_path;
    
    if (iType == VIDEODB_CONTENT_MOVIES)
    {
      CODBMovie movie;
      if (m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == idMovie, movie))
      {
        m_cdb.getDB()->load(movie, movie.section_foreign);
        if (!movie.m_file.load() || movie.m_file->m_path.load())
          return;
        
        db_filename = movie.m_file->m_filename;
        db_path = movie.m_file->m_path->m_path;
      }
    }
    else if (iType == VIDEODB_CONTENT_EPISODES)
    {
      //TODO: Needs Implementing
    }
    else if (iType == VIDEODB_CONTENT_TVSHOWS)
    {
      //TODO: Needs Implementing
    }
    else if (iType ==VIDEODB_CONTENT_MUSICVIDEOS)
    {
      //TODO: Needs Implementing
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    if (iType != VIDEODB_CONTENT_TVSHOWS)
    {
      ConstructPath(filePath, db_path, db_filename);
    }
    else
      filePath = db_path;
    
    /*if (iType == VIDEODB_CONTENT_EPISODES)
      strSQL=PrepareSQL("SELECT path.strPath, files.strFileName FROM path INNER JOIN files ON path.idPath=files.idPath INNER JOIN episode ON files.idFile=episode.idFile WHERE episode.idEpisode=%i ORDER BY strFilename", idMovie );
    if (iType == VIDEODB_CONTENT_TVSHOWS)
      strSQL=PrepareSQL("SELECT path.strPath FROM path INNER JOIN tvshowlinkpath ON path.idPath=tvshowlinkpath.idPath WHERE tvshowlinkpath.idShow=%i", idMovie );
    if (iType ==VIDEODB_CONTENT_MUSICVIDEOS)
      strSQL=PrepareSQL("SELECT path.strPath, files.strFileName FROM path INNER JOIN files ON path.idPath=files.idPath INNER JOIN musicvideo ON files.idFile=musicvideo.idFile WHERE musicvideo.idMVideo=%i ORDER BY strFilename", idMovie );*/
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

//********************************************************************************************************************************
void CVideoDatabase::GetBookMarksForFile(const std::string& strFilenameAndPath, VECBOOKMARKS& bookmarks, CBookmark::EType type /*= CBookmark::STANDARD*/, bool bAppend, long partNumber)
{
  try
  {
    if (URIUtils::IsStack(strFilenameAndPath) && CFileItem(CStackDirectory::GetFirstStackedFile(strFilenameAndPath),false).IsDiscImage())
    {
      CStackDirectory dir;
      CFileItemList fileList;
      const CURL pathToUrl(strFilenameAndPath);
      dir.GetDirectory(pathToUrl, fileList);
      if (!bAppend)
        bookmarks.clear();
      for (int i = fileList.Size() - 1; i >= 0; i--) // put the bookmarks of the highest part first in the list
        GetBookMarksForFile(fileList[i]->GetPath(), bookmarks, type, true, (i+1));
    }
    else
    {
      int idFile = GetFileId(strFilenameAndPath);
      if (idFile < 0) return ;
      if (!bAppend)
        bookmarks.erase(bookmarks.begin(), bookmarks.end());
     
      std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
      
      typedef odb::query<CODBBookmark> query;
      
      odb::result<CODBBookmark> res(m_cdb.getDB()->query<CODBBookmark>(query::file->idFile == idFile && query::type == (int)type));
      for (auto i: res)
      {
        CBookmark bookmark;
        bookmark.timeInSeconds = (int)i.m_timeInSeconds;
        bookmark.partNumber = partNumber;
        bookmark.totalTimeInSeconds = (int)i.m_totalTimeInSeconds;
        bookmark.thumbNailImage = i.m_thumbNailImage;
        bookmark.playerState = i.m_playerState;
        bookmark.player = i.m_player;
        bookmark.type = type;
        if (type == CBookmark::EPISODE)
        {
          //TODO: Needs to be implemented
          /*std::string strSQL2=PrepareSQL("select c%02d, c%02d from episode where c%02d=%i order by c%02d, c%02d", VIDEODB_ID_EPISODE_EPISODE, VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_EPISODE_BOOKMARK, m_pDS->fv("idBookmark").get_asInt(), VIDEODB_ID_EPISODE_SORTSEASON, VIDEODB_ID_EPISODE_SORTEPISODE);
          m_pDS2->query(strSQL2);
          bookmark.episodeNumber = m_pDS2->fv(0).get_asInt();
          bookmark.seasonNumber = m_pDS2->fv(1).get_asInt();
          m_pDS2->close();*/
        }
        bookmarks.push_back(bookmark);
      }
      
      if(odb_transaction)
        odb_transaction->commit();
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

bool CVideoDatabase::GetResumeBookMark(const std::string& strFilenameAndPath, CBookmark &bookmark)
{
  VECBOOKMARKS bookmarks;
  GetBookMarksForFile(strFilenameAndPath, bookmarks, CBookmark::RESUME);
  if (!bookmarks.empty())
  {
    bookmark = bookmarks[0];
    return true;
  }
  return false;
}

void CVideoDatabase::DeleteResumeBookMark(const std::string &strFilenameAndPath)
{
  if (!m_pDB.get() || !m_pDS.get())
    return;

  int fileID = GetFileId(strFilenameAndPath);
  if (fileID < -1)
    return;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBBookmark> query;
    
    m_cdb.getDB()->erase_query<CODBBookmark>(query::file->idFile == fileID && query::type == (int)CBookmark::RESUME);
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

void CVideoDatabase::GetEpisodesByFile(const std::string& strFilenameAndPath, std::vector<CVideoInfoTag>& episodes)
{
  try
  {
    std::string strSQL = PrepareSQL("select * from episode_view where idFile=%i order by c%02d, c%02d asc", GetFileId(strFilenameAndPath), VIDEODB_ID_EPISODE_SORTSEASON, VIDEODB_ID_EPISODE_SORTEPISODE);
    m_pDS->query(strSQL);
    while (!m_pDS->eof())
    {
      episodes.emplace_back(GetDetailsForEpisode(m_pDS));
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

//********************************************************************************************************************************
void CVideoDatabase::AddBookMarkToFile(const std::string& strFilenameAndPath, const CBookmark &bookmark, CBookmark::EType type /*= CBookmark::STANDARD*/)
{
  try
  {
    int idFile = AddFile(strFilenameAndPath);
    if (idFile < 0)
      return;
      
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::shared_ptr<CODBFile> odb_file(new CODBFile);
    if(!m_cdb.getDB()->query_one<CODBFile>(odb::query<CODBFile>::idFile == idFile, *odb_file))
      return;
    
    typedef odb::query<CODBBookmark> query;
    query bookmark_query;
    int idBookmark=-1;
    if (type == CBookmark::RESUME) // get the same resume mark bookmark each time type
    {
      bookmark_query = query::file->idFile == idFile;
    }
    else if (type == CBookmark::STANDARD) // get the same bookmark again, and update. not sure here as a dvd can have same time in multiple places, state will differ thou
    {
      /* get a bookmark within the same time as previous */
      double mintime = bookmark.timeInSeconds - 0.5f;
      double maxtime = bookmark.timeInSeconds + 0.5f;
      bookmark_query = query::file->idFile == idFile && (query::timeInSeconds >= mintime && query::timeInSeconds <= maxtime) && query::playerState.like(bookmark.playerState);
    }

    CODBBookmark odb_Bookmark;
    if (type != CBookmark::EPISODE && m_cdb.getDB()->query_one<CODBBookmark>(bookmark_query, odb_Bookmark))
    {
      odb_Bookmark.m_timeInSeconds = bookmark.timeInSeconds;
      odb_Bookmark.m_totalTimeInSeconds = bookmark.totalTimeInSeconds;
      odb_Bookmark.m_thumbNailImage = bookmark.thumbNailImage;
      odb_Bookmark.m_player = bookmark.player;
      odb_Bookmark.m_playerState = bookmark.playerState;
      m_cdb.getDB()->update(odb_Bookmark);
    }
    else
    {
      odb_Bookmark.m_file = odb_file;
      odb_Bookmark.m_timeInSeconds = bookmark.timeInSeconds;
      odb_Bookmark.m_totalTimeInSeconds = bookmark.totalTimeInSeconds;
      odb_Bookmark.m_thumbNailImage = bookmark.thumbNailImage;
      odb_Bookmark.m_player = bookmark.player;
      odb_Bookmark.m_playerState = bookmark.playerState;
      odb_Bookmark.m_type = (int)type;
      
      m_cdb.getDB()->persist(odb_Bookmark);
    }

    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

void CVideoDatabase::ClearBookMarkOfFile(const std::string& strFilenameAndPath, CBookmark& bookmark, CBookmark::EType type /*= CBookmark::STANDARD*/)
{
  try
  {
    int idFile = GetFileId(strFilenameAndPath);
    if (idFile < 0) return ;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    typedef odb::query<CODBBookmark> query;

    /* a litle bit uggly, we clear first bookmark that is within one second of given */
    /* should be no problem since we never add bookmarks that are closer than that   */
    double mintime = bookmark.timeInSeconds - 0.5f;
    double maxtime = bookmark.timeInSeconds + 0.5f;
    
    CODBBookmark odb_Bookmark;
    if(m_cdb.getDB()->query_one<CODBBookmark>(query::file->idFile == idFile
                                                                     && query::type == (int)type
                                                                     && query::playerState.like(bookmark.playerState)
                                                                     && query::player.like(bookmark.player)
                                                                     && (query::timeInSeconds >= mintime && query::timeInSeconds <= maxtime)
                                              , odb_Bookmark))
    {
      m_cdb.getDB()->erase(odb_Bookmark);
      
      //TODO: Needs to be implemented
      /*if (type == CBookmark::EPISODE)
      {
        strSQL=PrepareSQL("update episode set c%02d=-1 where idFile=%i and c%02d=%i", VIDEODB_ID_EPISODE_BOOKMARK, idFile, VIDEODB_ID_EPISODE_BOOKMARK, idBookmark);
        m_pDS->exec(strSQL);
      }*/
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strFilenameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

//********************************************************************************************************************************
void CVideoDatabase::ClearBookMarksOfFile(const std::string& strFilenameAndPath, CBookmark::EType type /*= CBookmark::STANDARD*/)
{
  int idFile = GetFileId(strFilenameAndPath);
  if (idFile >= 0)
    return ClearBookMarksOfFile(idFile, type);
}

void CVideoDatabase::ClearBookMarksOfFile(int idFile, CBookmark::EType type /*= CBookmark::STANDARD*/)
{
  if (idFile < 0)
    return;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    typedef odb::query<CODBBookmark> query;
    
    m_cdb.getDB()->erase_query<CODBBookmark>(query::file->idFile == idFile && query::type == (int)type);

    //TODO: Needs to be implemented
    /*if (type == CBookmark::EPISODE)
    {
      strSQL=PrepareSQL("update episode set c%02d=-1 where idFile=%i", VIDEODB_ID_EPISODE_BOOKMARK, idFile);
      m_pDS->exec(strSQL);
    }*/
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%d) exception - %s", __FUNCTION__, idFile, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%d) failed", __FUNCTION__, idFile);
  }
}


bool CVideoDatabase::GetBookMarkForEpisode(const CVideoInfoTag& tag, CBookmark& bookmark)
{
  try
  {
    std::string strSQL = PrepareSQL("select bookmark.* from bookmark join episode on episode.c%02d=bookmark.idBookmark where episode.idEpisode=%i", VIDEODB_ID_EPISODE_BOOKMARK, tag.m_iDbId);
    m_pDS2->query( strSQL );
    if (!m_pDS2->eof())
    {
      bookmark.timeInSeconds = m_pDS2->fv("timeInSeconds").get_asDouble();
      bookmark.totalTimeInSeconds = m_pDS2->fv("totalTimeInSeconds").get_asDouble();
      bookmark.thumbNailImage = m_pDS2->fv("thumbNailImage").get_asString();
      bookmark.playerState = m_pDS2->fv("playerState").get_asString();
      bookmark.player = m_pDS2->fv("player").get_asString();
      bookmark.type = (CBookmark::EType)m_pDS2->fv("type").get_asInt();
    }
    else
    {
      m_pDS2->close();
      return false;
    }
    m_pDS2->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
    return false;
  }
  return true;
}

void CVideoDatabase::AddBookMarkForEpisode(const CVideoInfoTag& tag, const CBookmark& bookmark)
{
  try
  {
    int idFile = GetFileId(tag.m_strFileNameAndPath);
    // delete the current episode for the selected episode number
    std::string strSQL = PrepareSQL("delete from bookmark where idBookmark in (select c%02d from episode where c%02d=%i and c%02d=%i and idFile=%i)", VIDEODB_ID_EPISODE_BOOKMARK, VIDEODB_ID_EPISODE_SEASON, tag.m_iSeason, VIDEODB_ID_EPISODE_EPISODE, tag.m_iEpisode, idFile);
    m_pDS->exec(strSQL);

    AddBookMarkToFile(tag.m_strFileNameAndPath, bookmark, CBookmark::EPISODE);
    int idBookmark = (int)m_pDS->lastinsertid();
    strSQL = PrepareSQL("update episode set c%02d=%i where c%02d=%i and c%02d=%i and idFile=%i", VIDEODB_ID_EPISODE_BOOKMARK, idBookmark, VIDEODB_ID_EPISODE_SEASON, tag.m_iSeason, VIDEODB_ID_EPISODE_EPISODE, tag.m_iEpisode, idFile);
    m_pDS->exec(strSQL);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, tag.m_iDbId);
  }
}

void CVideoDatabase::DeleteBookMarkForEpisode(const CVideoInfoTag& tag)
{
  try
  {
    std::string strSQL = PrepareSQL("delete from bookmark where idBookmark in (select c%02d from episode where idEpisode=%i)", VIDEODB_ID_EPISODE_BOOKMARK, tag.m_iDbId);
    m_pDS->exec(strSQL);
    strSQL = PrepareSQL("update episode set c%02d=-1 where idEpisode=%i", VIDEODB_ID_EPISODE_BOOKMARK, tag.m_iDbId);
    m_pDS->exec(strSQL);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, tag.m_iDbId);
  }
}

//********************************************************************************************************************************
void CVideoDatabase::DeleteMovie(const std::string& strFilenameAndPath, bool bKeepId /* = false */)
{
  int idMovie = GetMovieId(strFilenameAndPath);
  if (idMovie > -1)
    DeleteMovie(idMovie, bKeepId);
}

void CVideoDatabase::DeleteMovie(int idMovie, bool bKeepId /* = false */)
{
  if (idMovie < 0)
    return;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    // keep the movie table entry, linking to tv shows, and bookmarks
    // so we can update the data in place
    // the ancilliary tables are still purged
    if (!bKeepId)
    {
      CODBMovie res;
      if(!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == idMovie, res))
        return;
      
      m_cdb.getDB()->load(res, res.section_foreign);
      
      if(!res.m_file.load() || !res.m_file->m_path.load())
        return;
      
      std::string path = res.m_file->m_path->m_path;
      if (!path.empty())
        InvalidatePathHash(path);
      
      m_cdb.getDB()->erase(res);
    }

    //! @todo move this below CommitTransaction() once UPnP doesn't rely on this anymore
    if (!bKeepId)
      AnnounceRemove(MediaTypeMovie, idMovie);

    if(odb_transaction)
      odb_transaction->commit();

  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CVideoDatabase::DeleteTvShow(const std::string& strPath)
{
  int idTvShow = GetTvShowId(strPath);
  if (idTvShow >= 0)
    DeleteTvShow(idTvShow);
}

void CVideoDatabase::DeleteTvShow(int idTvShow, bool bKeepId /* = false */)
{
  if (idTvShow < 0)
    return;

  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    BeginTransaction();

    std::set<int> paths;
    GetPathsForTvShow(idTvShow, paths);

    std::string strSQL=PrepareSQL("SELECT episode.idEpisode FROM episode WHERE episode.idShow=%i",idTvShow);
    m_pDS2->query(strSQL);
    while (!m_pDS2->eof())
    {
      DeleteEpisode(m_pDS2->fv(0).get_asInt(), bKeepId);
      m_pDS2->next();
    }

    DeleteDetailsForTvShow(idTvShow);

    strSQL=PrepareSQL("delete from seasons where idShow=%i", idTvShow);
    m_pDS->exec(strSQL);

    // keep tvshow table and movielink table so we can update data in place
    if (!bKeepId)
    {
      strSQL=PrepareSQL("delete from tvshow where idShow=%i", idTvShow);
      m_pDS->exec(strSQL);

      for (const auto &i : paths)
      {
        std::string path = GetSingleValue(PrepareSQL("SELECT strPath FROM path WHERE idPath=%i", i));
        if (!path.empty())
          InvalidatePathHash(path);
      }
    }

    //! @todo move this below CommitTransaction() once UPnP doesn't rely on this anymore
    if (!bKeepId)
      AnnounceRemove(MediaTypeTvShow, idTvShow);

    CommitTransaction();

  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%d) failed", __FUNCTION__, idTvShow);
    RollbackTransaction();
  }
}

void CVideoDatabase::DeleteSeason(int idSeason, bool bKeepId /* = false */)
{
  if (idSeason < 0)
    return;

  try
  {
    if (m_pDB.get() == NULL ||
        m_pDS.get() == NULL ||
        m_pDS2.get() == NULL)
      return;

    BeginTransaction();

    std::string strSQL = PrepareSQL("SELECT episode.idEpisode FROM episode "
                                    "JOIN seasons ON seasons.idSeason = %d AND episode.idShow = seasons.idShow AND episode.c%02d = seasons.season ",
                                   idSeason, VIDEODB_ID_EPISODE_SEASON);
    m_pDS2->query(strSQL);
    while (!m_pDS2->eof())
    {
      DeleteEpisode(m_pDS2->fv(0).get_asInt(), bKeepId);
      m_pDS2->next();
    }

    ExecuteQuery(PrepareSQL("DELETE FROM seasons WHERE idSeason = %i", idSeason));

    CommitTransaction();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%d) failed", __FUNCTION__, idSeason);
    RollbackTransaction();
  }
}

void CVideoDatabase::DeleteEpisode(const std::string& strFilenameAndPath, bool bKeepId /* = false */)
{
  int idEpisode = GetEpisodeId(strFilenameAndPath);
  if (idEpisode > -1)
    DeleteEpisode(idEpisode, bKeepId);
}

void CVideoDatabase::DeleteEpisode(int idEpisode, bool bKeepId /* = false */)
{
  if (idEpisode < 0)
    return;

  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    //! @todo move this below CommitTransaction() once UPnP doesn't rely on this anymore
    if (!bKeepId)
      AnnounceRemove(MediaTypeEpisode, idEpisode);

    // keep episode table entry and bookmarks so we can update the data in place
    // the ancilliary tables are still purged
    if (!bKeepId)
    {
      int idFile = GetDbId(PrepareSQL("SELECT idFile FROM episode WHERE idEpisode=%i", idEpisode));
      std::string path = GetSingleValue(PrepareSQL("SELECT strPath FROM path JOIN files ON files.idPath=path.idPath WHERE files.idFile=%i", idFile));
      if (!path.empty())
        InvalidatePathHash(path);

      std::string strSQL = PrepareSQL("delete from episode where idEpisode=%i", idEpisode);
      m_pDS->exec(strSQL);
    }

  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%d) failed", __FUNCTION__, idEpisode);
  }
}

void CVideoDatabase::DeleteMusicVideo(const std::string& strFilenameAndPath, bool bKeepId /* = false */)
{
  int idMVideo = GetMusicVideoId(strFilenameAndPath);
  if (idMVideo > -1)
    DeleteMusicVideo(idMVideo, bKeepId);
}

void CVideoDatabase::DeleteMusicVideo(int idMVideo, bool bKeepId /* = false */)
{
  if (idMVideo < 0)
    return;

  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    BeginTransaction();

    // keep the music video table entry and bookmarks so we can update data in place
    // the ancilliary tables are still purged
    if (!bKeepId)
    {
      int idFile = GetDbId(PrepareSQL("SELECT idFile FROM musicvideo WHERE idMVideo=%i", idMVideo));
      std::string path = GetSingleValue(PrepareSQL("SELECT strPath FROM path JOIN files ON files.idPath=path.idPath WHERE files.idFile=%i", idFile));
      if (!path.empty())
        InvalidatePathHash(path);

      std::string strSQL = PrepareSQL("delete from musicvideo where idMVideo=%i", idMVideo);
      m_pDS->exec(strSQL);
    }

    //! @todo move this below CommitTransaction() once UPnP doesn't rely on this anymore
    if (!bKeepId)
      AnnounceRemove(MediaTypeMusicVideo, idMVideo);

    CommitTransaction();

  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
    RollbackTransaction();
  }
}

int CVideoDatabase::GetDbId(const std::string &query)
{
  std::string result = GetSingleValue(query);
  if (!result.empty())
  {
    int idDb = strtol(result.c_str(), NULL, 10);
    if (idDb > 0)
      return idDb;
  }
  return -1;
}

void CVideoDatabase::DeleteStreamDetails(int idFile)
{
    m_pDS->exec(PrepareSQL("delete from streamdetails where idFile=%i", idFile));
}

void CVideoDatabase::DeleteSet(int idSet)
{
  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    std::string strSQL;
    strSQL=PrepareSQL("delete from sets where idSet = %i", idSet);
    m_pDS->exec(strSQL);
    strSQL=PrepareSQL("update movie set idSet = null where idSet = %i", idSet);
    m_pDS->exec(strSQL);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idSet);
  }
}

void CVideoDatabase::ClearMovieSet(int idMovie)
{
  SetMovieSet(idMovie, -1);
}

void CVideoDatabase::SetMovieSet(int idMovie, int idSet)
{
  if (idSet >= 0)
    ExecuteQuery(PrepareSQL("update movie set idSet = %i where idMovie = %i", idSet, idMovie));
  else
    ExecuteQuery(PrepareSQL("update movie set idSet = null where idMovie = %i", idMovie));
}

void CVideoDatabase::DeleteTag(int idTag, VIDEODB_CONTENT_TYPE mediaType)
{
  try
  {
    if (m_pDB.get() == NULL || m_pDS.get() == NULL)
      return;

    std::string type;
    if (mediaType == VIDEODB_CONTENT_MOVIES)
      type = MediaTypeMovie;
    else if (mediaType == VIDEODB_CONTENT_TVSHOWS)
      type = MediaTypeTvShow;
    else if (mediaType == VIDEODB_CONTENT_MUSICVIDEOS)
      type = MediaTypeMusicVideo;
    else
      return;

    std::string strSQL = PrepareSQL("DELETE FROM tag_link WHERE tag_id = %i AND media_type = '%s'", idTag, type.c_str());
    m_pDS->exec(strSQL);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idTag);
  }
}

void CVideoDatabase::GetDetailsFromDB(std::unique_ptr<Dataset> &pDS, int min, int max, const SDbTableOffsets *offsets, CVideoInfoTag &details, int idxOffset)
{
  GetDetailsFromDB(pDS->get_sql_record(), min, max, offsets, details, idxOffset);
}

void CVideoDatabase::GetDetailsFromDB(const dbiplus::sql_record* const record, int min, int max, const SDbTableOffsets *offsets, CVideoInfoTag &details, int idxOffset)
{
  for (int i = min + 1; i < max; i++)
  {
    switch (offsets[i].type)
    {
    case VIDEODB_TYPE_STRING:
      *(std::string*)(((char*)&details)+offsets[i].offset) = record->at(i+idxOffset).get_asString();
      break;
    case VIDEODB_TYPE_INT:
    case VIDEODB_TYPE_COUNT:
      *(int*)(((char*)&details)+offsets[i].offset) = record->at(i+idxOffset).get_asInt();
      break;
    case VIDEODB_TYPE_BOOL:
      *(bool*)(((char*)&details)+offsets[i].offset) = record->at(i+idxOffset).get_asBool();
      break;
    case VIDEODB_TYPE_FLOAT:
      *(float*)(((char*)&details)+offsets[i].offset) = record->at(i+idxOffset).get_asFloat();
      break;
    case VIDEODB_TYPE_STRINGARRAY:
    {
      std::string value = record->at(i+idxOffset).get_asString();
      if (!value.empty())
        *(std::vector<std::string>*)(((char*)&details)+offsets[i].offset) = StringUtils::Split(value, g_advancedSettings.m_videoItemSeparator);
      break;
    }
    case VIDEODB_TYPE_DATE:
      ((CDateTime*)(((char*)&details)+offsets[i].offset))->SetFromDBDate(record->at(i+idxOffset).get_asString());
      break;
    case VIDEODB_TYPE_DATETIME:
      ((CDateTime*)(((char*)&details)+offsets[i].offset))->SetFromDBDateTime(record->at(i+idxOffset).get_asString());
      break;
    case VIDEODB_TYPE_UNUSED: // Skip the unused field to avoid populating unused data
      continue;
    }
  }
}

DWORD movieTime = 0;
DWORD castTime = 0;

bool CVideoDatabase::GetStreamDetails(CFileItem& item)
{
  // Note that this function (possibly) creates VideoInfoTags for items that don't have one yet!
  int fileId = -1;

  if (item.HasVideoInfoTag())
    fileId = item.GetVideoInfoTag()->m_iFileId;

  if (fileId < 0)
    fileId = GetFileId(item);

  if (fileId < 0)
    return false;

  // Have a file id, get stream details if available (creates tag either way)
  item.GetVideoInfoTag()->m_iFileId = fileId;
  return GetStreamDetails(*item.GetVideoInfoTag());
}

bool CVideoDatabase::GetStreamDetails(CVideoInfoTag& tag)
{
  if (tag.m_iFileId < 0)
    return false;

  bool retVal = false;

  CStreamDetails& details = tag.m_streamDetails;
  details.Reset();
  
  std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
  
  odb::result<CODBStreamDetails> res(m_cdb.getDB()->query<CODBStreamDetails>(odb::query<CODBStreamDetails>::idStreamDetail == tag.m_iFileId));
  for (auto i: res)
  {
    switch (i.m_streamType)
    {
    case CStreamDetail::VIDEO:
      {
        CStreamDetailVideo *p = new CStreamDetailVideo();
        p->m_strCodec = i.m_videoCodec;
        p->m_fAspect = i.m_videoAspect;
        p->m_iWidth = i.m_videoWidth;
        p->m_iHeight = i.m_videoHeight;
        p->m_iDuration = i.m_videoDuration;
        p->m_strStereoMode = i.m_stereoMode;
        p->m_strLanguage = i.m_videoLanguage;
        details.AddStream(p);
        retVal = true;
        break;
      }
    case CStreamDetail::AUDIO:
      {
        CStreamDetailAudio *p = new CStreamDetailAudio();
        p->m_strCodec = i.m_audioCodec;
        p->m_iChannels = i.m_audioChannels;
        p->m_strLanguage = i.m_audioLanguage;
        details.AddStream(p);
        retVal = true;
        break;
      }
    case CStreamDetail::SUBTITLE:
      {
        CStreamDetailSubtitle *p = new CStreamDetailSubtitle();
        p->m_strLanguage = i.m_subtitleLanguage;
        details.AddStream(p);
        retVal = true;
        break;
      }
    }
  }

  details.DetermineBestStreams();

  if (details.GetVideoDuration() > 0)
    tag.m_duration = details.GetVideoDuration();
  
  if(odb_transaction)
    odb_transaction->commit();

  return retVal;
}
 
bool CVideoDatabase::GetResumePoint(CVideoInfoTag& tag)
{
  if (tag.m_iFileId < 0)
    return false;

  bool match = false;

  try
  {
    if (URIUtils::IsStack(tag.m_strFileNameAndPath) && CFileItem(CStackDirectory::GetFirstStackedFile(tag.m_strFileNameAndPath),false).IsDiscImage())
    {
      CStackDirectory dir;
      CFileItemList fileList;
      const CURL pathToUrl(tag.m_strFileNameAndPath);
      dir.GetDirectory(pathToUrl, fileList);
      tag.m_resumePoint.Reset();
      for (int i = fileList.Size() - 1; i >= 0; i--)
      {
        CBookmark bookmark;
        if (GetResumeBookMark(fileList[i]->GetPath(), bookmark))
        {
          tag.m_resumePoint = bookmark;
          tag.m_resumePoint.partNumber = (i+1); /* store part number in here */
          match = true;
          break;
        }
      }
    }
    else
    {
      std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
      typedef odb::query<CODBBookmark> query;
      
      CODBBookmark odb_bookmark;
      //TODO: Previously the code could support multiple results, but there should not be multiple resume points
      if(m_cdb.getDB()->query_one<CODBBookmark>(query::file->idFile == tag.m_iFileId && query::type == (int)CBookmark::RESUME, odb_bookmark))
      {
        tag.m_resumePoint.timeInSeconds = odb_bookmark.m_timeInSeconds;
        tag.m_resumePoint.totalTimeInSeconds = odb_bookmark.m_totalTimeInSeconds;
        tag.m_resumePoint.partNumber = 0; // regular files or non-iso stacks don't need partNumber
        tag.m_resumePoint.type = CBookmark::RESUME;
        match = true;
      }
      
      if(odb_transaction)
        odb_transaction->commit();
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, tag.m_strFileNameAndPath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, tag.m_strFileNameAndPath.c_str());
  }

  return match;
}

CVideoInfoTag CVideoDatabase::GetDetailsForMovie(const odb::result<ODBView_Movie>::iterator record, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoInfoTag details;

  DWORD time = XbmcThreads::SystemClockMillis();

  details.m_iDbId = record->movie->m_idMovie;
  details.m_type = MediaTypeMovie;
  
  
  details.SetTitle(record->movie->m_title);
  details.SetPlot(record->movie->m_plot);
  details.SetPlotOutline(record->movie->m_plotoutline);
  details.SetTagLine(record->movie->m_tagline);
  
  details.m_strPictureURL.m_spoof = record->movie->m_thumbUrl_spoof;
  details.m_strPictureURL.m_xml = record->movie->m_thumbUrl;
  
  details.SetSortTitle(record->movie->m_sortTitle);
  details.m_duration = record->movie->m_runtime;
  details.SetMPAARating(record->movie->m_mpaa);
  details.m_iTop250 = record->movie->m_top250;
  details.SetOriginalTitle(record->movie->m_originalTitle);
  details.SetTrailer(record->movie->m_trailer);
  details.m_fanart.m_xml = record->movie->m_fanart;

  details.SetUserrating(record->movie->m_userrating);
  CDateTime premiered;
  premiered.SetFromULongLong(record->movie->m_premiered.m_ulong_date);
  details.SetPremiered(premiered);
  details.m_iUserRating = record->movie->m_userrating;
  
  m_cdb.getDB()->load(*(record->movie), record->movie->section_foreign);
  
  if(record->movie->m_basePath.load())
    details.SetBasePath(record->movie->m_basePath->m_path);
  if(record->movie->m_parentPath.load())
    details.m_parentPathID = record->movie->m_parentPath->m_idPath;
  
  if (record->movie->m_set.load())
  {
    details.m_iSetId = record->movie->m_set->m_idSet;
    details.m_strSet = record->movie->m_set->m_name;
    details.m_strSetOverview = record->movie->m_set->m_overview;
  }
  
  if (record->movie->m_file.load())
  {
    details.m_iFileId = record->movie->m_file->m_idFile;
    if(record->movie->m_file->m_path.load())
    {
      details.m_strPath = record->movie->m_file->m_path->m_path;
    }
    std::string strFileName = record->movie->m_file->m_filename;
    ConstructPath(details.m_strFileNameAndPath,details.m_strPath,strFileName);
    
    details.m_playCount = record->movie->m_file->m_playCount;
    CDateTime lastplayed;
    lastplayed.SetFromULongLong(record->movie->m_file->m_lastPlayed.m_ulong_date);
    details.m_lastPlayed = lastplayed;
    CDateTime dateadded;
    dateadded.SetFromULongLong(record->movie->m_file->m_dateAdded.m_ulong_date);
    details.m_dateAdded = dateadded;
  }
  
  m_cdb.getDB()->load(*(record->movie), record->movie->section_foreign);
  if(record->movie->m_defaultRating.load())
  {
    details.SetRating(record->movie->m_defaultRating->m_rating,
                      record->movie->m_defaultRating->m_votes,
                      record->movie->m_defaultRating->m_ratingType, true);
  }
  
  if(record->movie->m_defaultID.load())
  {
    details.SetUniqueID(record->movie->m_defaultID->m_value, record->movie->m_defaultID->m_type ,true);
  }
  
  for (auto genre: record->movie->m_genres)
  {
    if (genre.load())
    {
      details.m_genre.emplace_back(genre->m_name);
    }
  }
  
  for (auto country: record->movie->m_countries)
  {
    if (country.load())
    {
      details.m_country.emplace_back(country->m_name);
    }
  }
  
  movieTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();
  
  //Load the bookmark
  CODBBookmark odbBookmark;
  if (m_cdb.getDB()->query_one<CODBBookmark>(odb::query<CODBBookmark>::file->idFile == details.m_iFileId
                                             && odb::query<CODBBookmark>::type == (int)CBookmark::RESUME, odbBookmark))
  {
    details.m_resumePoint.player = odbBookmark.m_player;
    
    details.m_resumePoint.timeInSeconds = (int)odbBookmark.m_timeInSeconds;
    details.m_resumePoint.totalTimeInSeconds = (int)odbBookmark.m_totalTimeInSeconds;
    details.m_resumePoint.thumbNailImage = odbBookmark.m_thumbNailImage;
    details.m_resumePoint.playerState = odbBookmark.m_playerState;
    details.m_resumePoint.player = odbBookmark.m_player;
    details.m_resumePoint.type = CBookmark::RESUME;
  }

  if (getDetails)
  {
    if (getDetails & VideoDbDetailsCast)
    {
      int order = 0;
      for (auto i: record->movie->m_actors)
      {
        if (!i.load())
          continue;
        
        if (!i->m_person.load())
          continue;
        
        SActorInfo info;
        info.strName = i->m_person->m_name;
        info.strRole = i->m_role;
        info.order = order++;
        if (i->m_person->m_art.load())
        {
          info.thumb = i->m_person->m_art->m_url;
          info.thumbUrl.ParseEpisodeGuide(info.thumb);
        }
        
        details.m_cast.emplace_back(std::move(info));
        castTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();
      }
      
      for (auto i: record->movie->m_directors)
      {
        if (!i.load())
          continue;
        
        if (!i->m_person.load())
          continue;
        
        details.m_director.emplace_back(i->m_person->m_name);
      }
      
      for (auto i: record->movie->m_writingCredits)
      {
        if (!i.load())
          continue;
        if (!i->m_person.load())
          continue;
        
        details.m_writingCredits.emplace_back(StringUtils::Trim(i->m_person->m_name));
      }
    }

    if (getDetails & VideoDbDetailsTag)
    {
      for (auto i: record->movie->m_tags)
      {
        if (!i.load())
          continue;
        
        details.m_tags.emplace_back(i->m_name);
      }
    }

    if (getDetails & VideoDbDetailsRating)
    {
      for (auto i: record->movie->m_ratings)
      {
        if (!i.load())
          continue;
        
        details.m_ratings[i->m_ratingType] = CRating(i->m_rating, i->m_votes);
      }
      GetRatings(details.m_iDbId, MediaTypeMovie, details.m_ratings);
    }

    if (getDetails & VideoDbDetailsUniqueID)
    {
      for (auto i: record->movie->m_ids)
      {
        if (!i.load())
          continue;
        
        details.SetUniqueID(i->m_type, i->m_value);
      }
    }
    
    details.m_strPictureURL.Parse();

    /*if (getDetails & VideoDbDetailsShowLink)
    {
      // create tvshowlink string
      std::vector<int> links;
      GetLinksToTvShow(idMovie, links);
      for (unsigned int i = 0; i < links.size(); ++i)
      {
        std::string strSQL = PrepareSQL("select c%02d from tvshow where idShow=%i",
          VIDEODB_ID_TV_TITLE, links[i]);
        m_pDS2->query(strSQL);
        if (!m_pDS2->eof())
          details.m_showLink.emplace_back(m_pDS2->fv(0).get_asString());
      }
      m_pDS2->close();
    }*/

    if (getDetails & VideoDbDetailsStream)
      GetStreamDetails(details);

    details.m_parsedDetails = getDetails;
  }
  return details;
}

CVideoInfoTag CVideoDatabase::GetDetailsForTvShow(std::unique_ptr<Dataset> &pDS, int getDetails /* = VideoDbDetailsNone */, CFileItem* item /* = NULL */)
{
  return GetDetailsForTvShow(pDS->get_sql_record(), getDetails, item);
}

CVideoInfoTag CVideoDatabase::GetDetailsForTvShow(const dbiplus::sql_record* const record, int getDetails /* = VideoDbDetailsNone */, CFileItem* item /* = NULL */)
{
  CVideoInfoTag details;

  if (record == NULL)
    return details;

  DWORD time = XbmcThreads::SystemClockMillis();
  int idTvShow = record->at(0).get_asInt();

  GetDetailsFromDB(record, VIDEODB_ID_TV_MIN, VIDEODB_ID_TV_MAX, DbTvShowOffsets, details, 1);
  details.m_bHasPremiered = details.m_premiered.IsValid();
  details.m_iDbId = idTvShow;
  details.m_type = MediaTypeTvShow;
  details.m_strPath = record->at(VIDEODB_DETAILS_TVSHOW_PATH).get_asString();
  details.m_basePath = details.m_strPath;
  details.m_parentPathID = record->at(VIDEODB_DETAILS_TVSHOW_PARENTPATHID).get_asInt();
  details.m_dateAdded.SetFromDBDateTime(record->at(VIDEODB_DETAILS_TVSHOW_DATEADDED).get_asString());
  details.m_lastPlayed.SetFromDBDateTime(record->at(VIDEODB_DETAILS_TVSHOW_LASTPLAYED).get_asString());
  details.m_iSeason = record->at(VIDEODB_DETAILS_TVSHOW_NUM_SEASONS).get_asInt();
  details.m_iEpisode = record->at(VIDEODB_DETAILS_TVSHOW_NUM_EPISODES).get_asInt();
  details.m_playCount = record->at(VIDEODB_DETAILS_TVSHOW_NUM_WATCHED).get_asInt();
  details.m_strShowTitle = details.m_strTitle;
  details.m_iUserRating = record->at(VIDEODB_DETAILS_TVSHOW_USER_RATING).get_asInt();
  details.SetRating(record->at(VIDEODB_DETAILS_TVSHOW_RATING).get_asFloat(), 
                    record->at(VIDEODB_DETAILS_TVSHOW_VOTES).get_asInt(),
                    record->at(VIDEODB_DETAILS_TVSHOW_RATING_TYPE).get_asString(), true);
  details.SetUniqueID(record->at(VIDEODB_DETAILS_TVSHOW_UNIQUEID_VALUE).get_asString(), record->at(VIDEODB_DETAILS_TVSHOW_UNIQUEID_TYPE).get_asString(), true);
  details.m_duration = record->at(VIDEODB_DETAILS_TVSHOW_DURATION).get_asInt();

  movieTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();

  if (getDetails)
  {
    if (getDetails & VideoDbDetailsCast)
    {
      GetCast(details.m_iDbId, "tvshow", details.m_cast);
      castTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();
    }

    if (getDetails & VideoDbDetailsTag)
      GetTags(details.m_iDbId, MediaTypeTvShow, details.m_tags);

    if (getDetails & VideoDbDetailsRating)
      GetRatings(details.m_iDbId, MediaTypeTvShow, details.m_ratings);

    if (getDetails & VideoDbDetailsUniqueID)
      GetUniqueIDs(details.m_iDbId, MediaTypeTvShow, details);

    details.m_strPictureURL.Parse();

    details.m_parsedDetails = getDetails;
  }

  if (item != NULL)
  {
    item->m_dateTime = details.GetPremiered();
    item->SetProperty("totalseasons", details.m_iSeason);
    item->SetProperty("totalepisodes", details.m_iEpisode);
    item->SetProperty("numepisodes", details.m_iEpisode); // will be changed later to reflect watchmode setting
    item->SetProperty("watchedepisodes", details.m_playCount);
    item->SetProperty("unwatchedepisodes", details.m_iEpisode - details.m_playCount);
  }
  details.m_playCount = (details.m_iEpisode <= details.m_playCount) ? 1 : 0;

  return details;
}

CVideoInfoTag CVideoDatabase::GetDetailsForEpisode(std::unique_ptr<Dataset> &pDS, int getDetails /* = VideoDbDetailsNone */)
{
  return GetDetailsForEpisode(pDS->get_sql_record(), getDetails);
}

CVideoInfoTag CVideoDatabase::GetDetailsForEpisode(const dbiplus::sql_record* const record, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoInfoTag details;

  if (record == NULL)
    return details;

  DWORD time = XbmcThreads::SystemClockMillis();
  int idEpisode = record->at(0).get_asInt();

  GetDetailsFromDB(record, VIDEODB_ID_EPISODE_MIN, VIDEODB_ID_EPISODE_MAX, DbEpisodeOffsets, details);
  details.m_iDbId = idEpisode;
  details.m_type = MediaTypeEpisode;
  details.m_iFileId = record->at(VIDEODB_DETAILS_FILEID).get_asInt();
  details.m_strPath = record->at(VIDEODB_DETAILS_EPISODE_PATH).get_asString();
  std::string strFileName = record->at(VIDEODB_DETAILS_EPISODE_FILE).get_asString();
  ConstructPath(details.m_strFileNameAndPath,details.m_strPath,strFileName);
  details.m_playCount = record->at(VIDEODB_DETAILS_EPISODE_PLAYCOUNT).get_asInt();
  details.m_lastPlayed.SetFromDBDateTime(record->at(VIDEODB_DETAILS_EPISODE_LASTPLAYED).get_asString());
  details.m_dateAdded.SetFromDBDateTime(record->at(VIDEODB_DETAILS_EPISODE_DATEADDED).get_asString());
  details.m_strMPAARating = record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_MPAA).get_asString();
  details.m_strShowTitle = record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_NAME).get_asString();
  details.m_genre = StringUtils::Split(record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_GENRE).get_asString(), g_advancedSettings.m_videoItemSeparator);
  details.m_studio = StringUtils::Split(record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_STUDIO).get_asString(), g_advancedSettings.m_videoItemSeparator);
  details.SetPremieredFromDBDate(record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_AIRED).get_asString());
  details.m_iIdShow = record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_ID).get_asInt();
  details.m_iIdSeason = record->at(VIDEODB_DETAILS_EPISODE_SEASON_ID).get_asInt();

  details.m_resumePoint.timeInSeconds = record->at(VIDEODB_DETAILS_EPISODE_RESUME_TIME).get_asInt();
  details.m_resumePoint.totalTimeInSeconds = record->at(VIDEODB_DETAILS_EPISODE_TOTAL_TIME).get_asInt();
  details.m_resumePoint.type = CBookmark::RESUME;
  details.m_iUserRating = record->at(VIDEODB_DETAILS_EPISODE_USER_RATING).get_asInt();
  details.SetRating(record->at(VIDEODB_DETAILS_EPISODE_RATING).get_asFloat(), 
                    record->at(VIDEODB_DETAILS_EPISODE_VOTES).get_asInt(), 
                    record->at(VIDEODB_DETAILS_EPISODE_RATING_TYPE).get_asString(), true);
  details.SetUniqueID(record->at(VIDEODB_DETAILS_EPISODE_UNIQUEID_VALUE).get_asString(), record->at(VIDEODB_DETAILS_EPISODE_UNIQUEID_TYPE).get_asString(), true);
  movieTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();

  if (getDetails)
  {
    if (getDetails & VideoDbDetailsCast)
    {
      GetCast(details.m_iDbId, MediaTypeEpisode, details.m_cast);
      GetCast(details.m_iIdShow, MediaTypeTvShow, details.m_cast);
      castTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();
    }

    if (getDetails & VideoDbDetailsRating)
      GetRatings(details.m_iDbId, MediaTypeEpisode, details.m_ratings);

    if (getDetails & VideoDbDetailsUniqueID)
      GetUniqueIDs(details.m_iDbId, MediaTypeEpisode, details);

    details.m_strPictureURL.Parse();
    
    if (getDetails &  VideoDbDetailsBookmark)
      GetBookMarkForEpisode(details, details.m_EpBookmark);
    
    if (getDetails & VideoDbDetailsStream)
      GetStreamDetails(details);

    details.m_parsedDetails = getDetails;
  }
  return details;
}

CVideoInfoTag CVideoDatabase::GetDetailsForMusicVideo(std::unique_ptr<Dataset> &pDS, int getDetails /* = VideoDbDetailsNone */)
{
  return GetDetailsForMusicVideo(pDS->get_sql_record(), getDetails);
}

CVideoInfoTag CVideoDatabase::GetDetailsForMusicVideo(const dbiplus::sql_record* const record, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoInfoTag details;

  unsigned int time = XbmcThreads::SystemClockMillis();
  int idMVideo = record->at(0).get_asInt();

  GetDetailsFromDB(record, VIDEODB_ID_MUSICVIDEO_MIN, VIDEODB_ID_MUSICVIDEO_MAX, DbMusicVideoOffsets, details);
  details.m_iDbId = idMVideo;
  details.m_type = MediaTypeMusicVideo;
  
  details.m_iFileId = record->at(VIDEODB_DETAILS_FILEID).get_asInt();
  details.m_strPath = record->at(VIDEODB_DETAILS_MUSICVIDEO_PATH).get_asString();
  std::string strFileName = record->at(VIDEODB_DETAILS_MUSICVIDEO_FILE).get_asString();
  ConstructPath(details.m_strFileNameAndPath,details.m_strPath,strFileName);
  details.m_playCount = record->at(VIDEODB_DETAILS_MUSICVIDEO_PLAYCOUNT).get_asInt();
  details.m_lastPlayed.SetFromDBDateTime(record->at(VIDEODB_DETAILS_MUSICVIDEO_LASTPLAYED).get_asString());
  details.m_dateAdded.SetFromDBDateTime(record->at(VIDEODB_DETAILS_MUSICVIDEO_DATEADDED).get_asString());
  details.m_resumePoint.timeInSeconds = record->at(VIDEODB_DETAILS_MUSICVIDEO_RESUME_TIME).get_asInt();
  details.m_resumePoint.totalTimeInSeconds = record->at(VIDEODB_DETAILS_MUSICVIDEO_TOTAL_TIME).get_asInt();
  details.m_resumePoint.type = CBookmark::RESUME;
  details.m_iUserRating = record->at(VIDEODB_DETAILS_MUSICVIDEO_USER_RATING).get_asInt();
  std::string premieredString = record->at(VIDEODB_DETAILS_MUSICVIDEO_PREMIERED).get_asString();
  if (premieredString.size() == 4)
    details.SetYear(record->at(VIDEODB_DETAILS_MUSICVIDEO_PREMIERED).get_asInt());
  else
    details.SetPremieredFromDBDate(premieredString);
  
  movieTime += XbmcThreads::SystemClockMillis() - time; time = XbmcThreads::SystemClockMillis();

  if (getDetails)
  {
    if (getDetails & VideoDbDetailsTag)
      GetTags(details.m_iDbId, MediaTypeMusicVideo, details.m_tags);

    details.m_strPictureURL.Parse();

    if (getDetails & VideoDbDetailsStream)
      GetStreamDetails(details);

    details.m_parsedDetails = getDetails;
  }
  return details;
}

void CVideoDatabase::GetCast(int media_id, const std::string &media_type, std::vector<SActorInfo> &cast)
{
  try
  {
    if (!m_pDB.get()) return;
    if (!m_pDS2.get()) return;

    std::string sql = PrepareSQL("SELECT actor.name,"
                                 "  actor_link.role,"
                                 "  actor_link.cast_order,"
                                 "  actor.art_urls,"
                                 "  art.url "
                                 "FROM actor_link"
                                 "  JOIN actor ON"
                                 "    actor_link.actor_id=actor.actor_id"
                                 "  LEFT JOIN art ON"
                                 "    art.media_id=actor.actor_id AND art.media_type='actor' AND art.type='thumb' "
                                 "WHERE actor_link.media_id=%i AND actor_link.media_type='%s'"
                                 "ORDER BY actor_link.cast_order", media_id, media_type.c_str());
    m_pDS2->query(sql);
    while (!m_pDS2->eof())
    {
      SActorInfo info;
      info.strName = m_pDS2->fv(0).get_asString();
      bool found = false;
      for (const auto &i : cast)
      {
        if (i.strName == info.strName)
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        info.strRole = m_pDS2->fv(1).get_asString();
        info.order = m_pDS2->fv(2).get_asInt();
        info.thumbUrl.ParseString(m_pDS2->fv(3).get_asString());
        info.thumb = m_pDS2->fv(4).get_asString();
        cast.emplace_back(std::move(info));
      }
      m_pDS2->next();
    }
    m_pDS2->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%i,%s) failed", __FUNCTION__, media_id, media_type.c_str());
  }
}

void CVideoDatabase::GetTags(int media_id, const std::string &media_type, std::vector<std::string> &tags)
{
  try
  {
    if (!m_pDB.get()) return;
    if (!m_pDS2.get()) return;

    std::string sql = PrepareSQL("SELECT tag.name FROM tag INNER JOIN tag_link ON tag_link.tag_id = tag.tag_id WHERE tag_link.media_id = %i AND tag_link.media_type = '%s' ORDER BY tag.tag_id", media_id, media_type.c_str());
    m_pDS2->query(sql);
    while (!m_pDS2->eof())
    {
      tags.emplace_back(m_pDS2->fv(0).get_asString());
      m_pDS2->next();
    }
    m_pDS2->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%i,%s) failed", __FUNCTION__, media_id, media_type.c_str());
  }
}

void CVideoDatabase::GetRatings(int media_id, const std::string &media_type, RatingMap &ratings)
{
  try
  {
    if (!m_pDB.get()) return;
    if (!m_pDS2.get()) return;

    std::string sql = PrepareSQL("SELECT rating.rating_type, rating.rating, rating.votes FROM rating WHERE rating.media_id = %i AND rating.media_type = '%s'", media_id, media_type.c_str());
    m_pDS2->query(sql);
    while (!m_pDS2->eof())
    {
      ratings[m_pDS2->fv(0).get_asString()] = CRating(m_pDS2->fv(1).get_asFloat(), m_pDS2->fv(2).get_asInt());
      m_pDS2->next();
    }
    m_pDS2->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%i,%s) failed", __FUNCTION__, media_id, media_type.c_str());
  }
}

void CVideoDatabase::GetUniqueIDs(int media_id, const std::string &media_type, CVideoInfoTag& details)
{
  try
  {
    if (!m_pDB.get()) return;
    if (!m_pDS2.get()) return;

    std::string sql = PrepareSQL("SELECT type, value FROM uniqueid WHERE media_id = %i AND media_type = '%s'", media_id, media_type.c_str());
    m_pDS2->query(sql);
    while (!m_pDS2->eof())
    {
      details.SetUniqueID(m_pDS2->fv(1).get_asString(), m_pDS2->fv(0).get_asString());
      m_pDS2->next();
    }
    m_pDS2->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%i,%s) failed", __FUNCTION__, media_id, media_type.c_str());
  }
}

bool CVideoDatabase::GetVideoSettings(const CFileItem &item, CVideoSettings &settings)
{
  return GetVideoSettings(GetFileId(item), settings);
}

/// \brief GetVideoSettings() obtains any saved video settings for the current file.
/// \retval Returns true if the settings exist, false otherwise.
bool CVideoDatabase::GetVideoSettings(const std::string &filePath, CVideoSettings &settings)
{
  return GetVideoSettings(GetFileId(filePath), settings);
}

bool CVideoDatabase::GetVideoSettings(int idFile, CVideoSettings &settings)
{
  try
  {
    if (idFile < 0) return false;
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    std::string strSQL=PrepareSQL("select * from settings where settings.idFile = '%i'", idFile);
    m_pDS->query( strSQL );

    if (m_pDS->num_rows() > 0)
    { // get the video settings info
      settings.m_AudioDelay = m_pDS->fv("AudioDelay").get_asFloat();
      settings.m_AudioStream = m_pDS->fv("AudioStream").get_asInt();
      settings.m_Brightness = m_pDS->fv("Brightness").get_asFloat();
      settings.m_Contrast = m_pDS->fv("Contrast").get_asFloat();
      settings.m_CustomPixelRatio = m_pDS->fv("PixelRatio").get_asFloat();
      settings.m_CustomNonLinStretch = m_pDS->fv("NonLinStretch").get_asBool();
      settings.m_NoiseReduction = m_pDS->fv("NoiseReduction").get_asFloat();
      settings.m_PostProcess = m_pDS->fv("PostProcess").get_asBool();
      settings.m_Sharpness = m_pDS->fv("Sharpness").get_asFloat();
      settings.m_CustomZoomAmount = m_pDS->fv("ZoomAmount").get_asFloat();
      settings.m_CustomVerticalShift = m_pDS->fv("VerticalShift").get_asFloat();
      settings.m_Gamma = m_pDS->fv("Gamma").get_asFloat();
      settings.m_SubtitleDelay = m_pDS->fv("SubtitleDelay").get_asFloat();
      settings.m_SubtitleOn = m_pDS->fv("SubtitlesOn").get_asBool();
      settings.m_SubtitleStream = m_pDS->fv("SubtitleStream").get_asInt();
      settings.m_ViewMode = m_pDS->fv("ViewMode").get_asInt();
      settings.m_ResumeTime = m_pDS->fv("ResumeTime").get_asInt();
      settings.m_InterlaceMethod = (EINTERLACEMETHOD)m_pDS->fv("Deinterlace").get_asInt();
      settings.m_VolumeAmplification = m_pDS->fv("VolumeAmplification").get_asFloat();
      settings.m_OutputToAllSpeakers = m_pDS->fv("OutputToAllSpeakers").get_asBool();
      settings.m_ScalingMethod = (ESCALINGMETHOD)m_pDS->fv("ScalingMethod").get_asInt();
      settings.m_StereoMode = m_pDS->fv("StereoMode").get_asInt();
      settings.m_StereoInvert = m_pDS->fv("StereoInvert").get_asBool();
      settings.m_SubtitleCached = false;
      settings.m_VideoStream = m_pDS->fv("VideoStream").get_asInt();
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
void CVideoDatabase::SetVideoSettings(const std::string& strFilenameAndPath, const CVideoSettings &setting)
{
  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;
    int idFile = AddFile(strFilenameAndPath);
    if (idFile < 0)
      return;
    std::string strSQL = PrepareSQL("select * from settings where idFile=%i", idFile);
    m_pDS->query( strSQL );
    if (m_pDS->num_rows() > 0)
    {
      m_pDS->close();
      // update the item
      strSQL=PrepareSQL("update settings set Deinterlace=%i,ViewMode=%i,ZoomAmount=%f,PixelRatio=%f,VerticalShift=%f,"
                       "AudioStream=%i,SubtitleStream=%i,SubtitleDelay=%f,SubtitlesOn=%i,Brightness=%f,Contrast=%f,Gamma=%f,"
                       "VolumeAmplification=%f,AudioDelay=%f,OutputToAllSpeakers=%i,Sharpness=%f,NoiseReduction=%f,NonLinStretch=%i,PostProcess=%i,ScalingMethod=%i,",
                       setting.m_InterlaceMethod, setting.m_ViewMode, setting.m_CustomZoomAmount, setting.m_CustomPixelRatio, setting.m_CustomVerticalShift,
                       setting.m_AudioStream, setting.m_SubtitleStream, setting.m_SubtitleDelay, setting.m_SubtitleOn,
                       setting.m_Brightness, setting.m_Contrast, setting.m_Gamma, setting.m_VolumeAmplification, setting.m_AudioDelay,
                       setting.m_OutputToAllSpeakers,setting.m_Sharpness,setting.m_NoiseReduction,setting.m_CustomNonLinStretch,setting.m_PostProcess,setting.m_ScalingMethod);
      std::string strSQL2;
      strSQL2=PrepareSQL("ResumeTime=%i,StereoMode=%i,StereoInvert=%i, VideoStream=%i where idFile=%i\n", setting.m_ResumeTime, setting.m_StereoMode,
                        setting.m_StereoInvert, setting.m_VideoStream, idFile);
      strSQL += strSQL2;
      m_pDS->exec(strSQL);
      return ;
    }
    else
    { // add the items
      m_pDS->close();
      strSQL= "INSERT INTO settings (idFile,Deinterlace,ViewMode,ZoomAmount,PixelRatio, VerticalShift, "
                "AudioStream,SubtitleStream,SubtitleDelay,SubtitlesOn,Brightness,"
                "Contrast,Gamma,VolumeAmplification,AudioDelay,OutputToAllSpeakers,"
                "ResumeTime,"
                "Sharpness,NoiseReduction,NonLinStretch,PostProcess,ScalingMethod,StereoMode,StereoInvert,VideoStream) "
              "VALUES ";
      strSQL += PrepareSQL("(%i,%i,%i,%f,%f,%f,%i,%i,%f,%i,%f,%f,%f,%f,%f,%i,%i,%f,%f,%i,%i,%i,%i,%i,%i)",
                           idFile, setting.m_InterlaceMethod, setting.m_ViewMode, setting.m_CustomZoomAmount, setting.m_CustomPixelRatio, setting.m_CustomVerticalShift,
                           setting.m_AudioStream, setting.m_SubtitleStream, setting.m_SubtitleDelay, setting.m_SubtitleOn, setting.m_Brightness,
                           setting.m_Contrast, setting.m_Gamma, setting.m_VolumeAmplification, setting.m_AudioDelay, setting.m_OutputToAllSpeakers,
                           setting.m_ResumeTime,
                           setting.m_Sharpness, setting.m_NoiseReduction, setting.m_CustomNonLinStretch, setting.m_PostProcess, setting.m_ScalingMethod,
                           setting.m_StereoMode, setting.m_StereoInvert, setting.m_VideoStream);
      m_pDS->exec(strSQL);
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strFilenameAndPath.c_str());
  }
}

void CVideoDatabase::SetArtForItem(int mediaId, const MediaType &mediaType, const std::map<std::string, std::string> &art)
{
  //TODO: This function can be removed, as art will be set for each type in its own function
  for (const auto &i : art)
    SetArtForItem(mediaId, mediaType, i.first, i.second);
}

void CVideoDatabase::SetArtForItem(int mediaId, const MediaType &mediaType, const std::string &artType, const std::string &url)
{
  try
  {
    //This function is used when selecting a new poster
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    // don't set <foo>.<bar> art types - these are derivative types from parent items
    if (artType.find('.') != std::string::npos)
      return;
    
    if (mediaType == MediaTypeMovie)
    {
      ODBView_Movie_Art movie_art;
      if (m_cdb.getDB()->query_one<ODBView_Movie_Art>(odb::query<ODBView_Movie_Art>::CODBMovie::idMovie == mediaId &&
                                                       odb::query<ODBView_Movie_Art>::CODBArt::type == artType
                                                       , movie_art))
      {
        movie_art.art->m_url = url;
        m_cdb.getDB()->update(movie_art.art);
      }
      else
      {
        CODBMovie odb_movie;
        if (m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == mediaId, odb_movie))
        {
          std::shared_ptr<CODBArt> art(new CODBArt);
          art->m_url = url;
          art->m_media_type = mediaType;
          art->m_type = artType;
          m_cdb.getDB()->persist(art);
          
          m_cdb.getDB()->load(odb_movie, odb_movie.section_artwork);
          odb_movie.m_artwork.push_back(art);
          m_cdb.getDB()->update(odb_movie);
          m_cdb.getDB()->update(odb_movie, odb_movie.section_artwork);
        }
      }
    }
    else
    {
      CLog::Log(LOGERROR, "%s(%d, '%s', '%s', '%s') unknown media type", __FUNCTION__, mediaId, mediaType.c_str(), artType.c_str(), url.c_str());
    }
    
    //TODO: Implement other types
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s(%d, '%s', '%s', '%s') failed - %s", __FUNCTION__, mediaId, mediaType.c_str(), artType.c_str(), url.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%d, '%s', '%s', '%s') failed", __FUNCTION__, mediaId, mediaType.c_str(), artType.c_str(), url.c_str());
  }
}

bool CVideoDatabase::GetArtForItem(int mediaId, const MediaType &mediaType, std::map<std::string, std::string> &art)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (mediaType == MediaTypeMovie)
    {
      typedef odb::query<CODBMovie> query;
      CODBMovie odbMovie;
      if (m_cdb.getDB()->query_one<CODBMovie>(query::idMovie == mediaId, odbMovie))
      {
        m_cdb.getDB()->load(odbMovie, odbMovie.section_artwork);
        for (auto& i: odbMovie.m_artwork)
        {
          if (i.load())
          {
            art.insert(make_pair(i->m_type, i->m_url));
          }
        }
      }
    }
    else if (mediaType == MediaTypeActor || mediaType == MediaTypeDirector)
    {
      typedef odb::query<CODBPerson> query;
      CODBPerson odbPerson;
      if (m_cdb.getDB()->query_one<CODBPerson>(query::idPerson == mediaId, odbPerson))
      {
        if (odbPerson.m_art.load())
        {
          art.insert(make_pair(odbPerson.m_art->m_type, odbPerson.m_art->m_url));
        }
      }
    }
    else if (mediaType == "genre")
    {
      //TODO: This is called but no where defined. Need to be checked
    }
    //TODO: Add for TV Shows and MusicMovies
    else
    {
      CLog::Log(LOGERROR, "%s(%d) failed, unknown mediaType %s", __FUNCTION__, mediaId, mediaType.c_str());
    }
    
    if(odb_transaction)
      odb_transaction->commit();

    return !art.empty();
  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%d) failed", __FUNCTION__, mediaId);
  }
  return false;
}

std::string CVideoDatabase::GetArtForItem(int mediaId, const MediaType &mediaType, const std::string &artType)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (mediaType == MediaTypeMovie)
    {
      typedef odb::query<ODBView_Movie_Art> query;
      odb::result<ODBView_Movie_Art> res(m_cdb.getDB()->query<ODBView_Movie_Art>(query::CODBMovie::idMovie == mediaId &&
                                                                                 query::CODBArt::type == artType));
      odb::result<ODBView_Movie_Art>::iterator i = res.begin();
      if (i != res.end())
        return i->art->m_url;
    }
    //TODO: Add for TV Shows and MusicMovies
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%d) failed", __FUNCTION__, mediaId);
  }
  
  return "";
}

bool CVideoDatabase::RemoveArtForItem(int mediaId, const MediaType &mediaType, const std::string &artType)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (mediaType == MediaTypeMovie)
    {
      CODBMovie odbMovie;
      if (m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == mediaId, odbMovie))
      {
        m_cdb.getDB()->load(odbMovie, odbMovie.section_artwork);
        for (std::vector<odb::lazy_shared_ptr<CODBArt> >::iterator i = odbMovie.m_artwork.begin();
             i != odbMovie.m_artwork.end();)
        {
          if ((*i).load())
          {
            if ((*i)->m_type == artType)
            {
              m_cdb.getDB()->erase<CODBArt>(*(*i));
              i = odbMovie.m_artwork.erase(i);
            }
            else
              i++;
          }
          else
          {
            i = odbMovie.m_artwork.erase(i);
          }
        }
      }
    }
    //TODO: Add for TV Shows and MusicMovies
    
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%d) failed", __FUNCTION__, mediaId);
  }
    
  return false;
}

bool CVideoDatabase::RemoveArtForItem(int mediaId, const MediaType &mediaType, const std::set<std::string> &artTypes)
{
  bool result = true;
  for (const auto &i : artTypes)
    result &= RemoveArtForItem(mediaId, mediaType, i);

  return result;
}

bool CVideoDatabase::GetTvShowSeasons(int showId, std::map<int, int> &seasons)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS2.get()) return false; // using dataset 2 as we're likely called in loops on dataset 1

    // get all seasons for this show
    std::string sql = PrepareSQL("select idSeason,season from seasons where idShow=%i", showId);
    m_pDS2->query(sql);

    seasons.clear();
    while (!m_pDS2->eof())
    {
      seasons.insert(std::make_pair(m_pDS2->fv(1).get_asInt(), m_pDS2->fv(0).get_asInt()));
      m_pDS2->next();
    }
    m_pDS2->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%d) failed", __FUNCTION__, showId);
  }
  return false;
}

bool CVideoDatabase::GetTvShowSeasonArt(int showId, std::map<int, std::map<std::string, std::string> > &seasonArt)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS2.get()) return false; // using dataset 2 as we're likely called in loops on dataset 1

    std::map<int, int> seasons;
    GetTvShowSeasons(showId, seasons);

    for (const auto &i : seasons)
    {
      std::map<std::string, std::string> art;
      GetArtForItem(i.second, MediaTypeSeason, art);
      seasonArt.insert(std::make_pair(i.first,art));
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%d) failed", __FUNCTION__, showId);
  }
  return false;
}

bool CVideoDatabase::GetArtTypes(const MediaType &mediaType, std::vector<std::string> &artTypes)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    std::string media_type = "movie";
    if (mediaType == MediaTypeTvShow)
    {
      
    }
    //TODO: Add for TV Shows and MusicMovies
    
    odb::result<ODBView_Art_Type> res(m_cdb.getDB()->query<ODBView_Art_Type>(odb::query<ODBView_Art_Type>::media_type == mediaType));

    for (auto& i : res)
    {
      artTypes.emplace_back(i.m_type);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%s) failed", __FUNCTION__, mediaType.c_str());
  }
  return false;
}

/// \brief GetStackTimes() obtains any saved video times for the stacked file
/// \retval Returns true if the stack times exist, false otherwise.
bool CVideoDatabase::GetStackTimes(const std::string &filePath, std::vector<int> &times)
{
  try
  {
    // obtain the FileID (if it exists)
    int idFile = GetFileId(filePath);
    if (idFile < 0) return false;
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;
    // ok, now obtain the settings for this file
    std::string strSQL=PrepareSQL("select times from stacktimes where idFile=%i\n", idFile);
    m_pDS->query( strSQL );
    if (m_pDS->num_rows() > 0)
    { // get the video settings info
      int timeTotal = 0;
      std::vector<std::string> timeString = StringUtils::Split(m_pDS->fv("times").get_asString(), ",");
      times.clear();
      for (const auto &i : timeString)
      {
        times.push_back(atoi(i.c_str()));
        timeTotal += atoi(i.c_str());
      }
      m_pDS->close();
      return (timeTotal > 0);
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

/// \brief Sets the stack times for a particular video file
void CVideoDatabase::SetStackTimes(const std::string& filePath, const std::vector<int> &times)
{
  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;
    int idFile = AddFile(filePath);
    if (idFile < 0)
      return;

    // delete any existing items
    m_pDS->exec( PrepareSQL("delete from stacktimes where idFile=%i", idFile) );

    // add the items
    std::string timeString = StringUtils::Format("%i", times[0]);
    for (unsigned int i = 1; i < times.size(); i++)
      timeString += StringUtils::Format(",%i", times[i]);

    m_pDS->exec( PrepareSQL("insert into stacktimes (idFile,times) values (%i,'%s')\n", idFile, timeString.c_str()) );
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, filePath.c_str());
  }
}

void CVideoDatabase::RemoveContentForPath(const std::string& strPath, CGUIDialogProgress *progress /* = NULL */)
{
  if(URIUtils::IsMultiPath(strPath))
  {
    std::vector<std::string> paths;
    CMultiPathDirectory::GetPaths(strPath, paths);

    for(unsigned i=0;i<paths.size();i++)
      RemoveContentForPath(paths[i], progress);
  }

  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;

    if (progress)
    {
      progress->SetHeading(CVariant{700});
      progress->SetLine(0, CVariant{""});
      progress->SetLine(1, CVariant{313});
      progress->SetLine(2, CVariant{330});
      progress->SetPercentage(0);
      progress->Open();
      progress->ShowProgressBar(true);
    }
    std::vector<std::pair<int, std::string> > paths;
    GetSubPaths(strPath, paths);
    int iCurr = 0;
    for (const auto &i : paths)
    {
      bool bMvidsChecked=false;
      if (progress)
      {
        progress->SetPercentage((int)((float)(iCurr++)/paths.size()*100.f));
        progress->Progress();
      }

      if (HasTvShowInfo(i.second))
        DeleteTvShow(i.second);
      else
      {
        std::string strSQL = PrepareSQL("select files.strFilename from files join movie on movie.idFile=files.idFile where files.idPath=%i", i.first);
        m_pDS2->query(strSQL);
        if (m_pDS2->eof())
        {
          strSQL = PrepareSQL("select files.strFilename from files join musicvideo on musicvideo.idFile=files.idFile where files.idPath=%i", i.first);
          m_pDS2->query(strSQL);
          bMvidsChecked = true;
        }
        while (!m_pDS2->eof())
        {
          std::string strMoviePath;
          std::string strFileName = m_pDS2->fv("files.strFilename").get_asString();
          ConstructPath(strMoviePath, i.second, strFileName);
          if (HasMovieInfo(strMoviePath))
            DeleteMovie(strMoviePath);
          if (HasMusicVideoInfo(strMoviePath))
            DeleteMusicVideo(strMoviePath);
          m_pDS2->next();
          if (m_pDS2->eof() && !bMvidsChecked)
          {
            strSQL =PrepareSQL("select files.strFilename from files join musicvideo on musicvideo.idFile=files.idFile where files.idPath=%i", i.first);
            m_pDS2->query(strSQL);
            bMvidsChecked = true;
          }
        }
        m_pDS2->close();
        m_pDS2->exec(PrepareSQL("update path set strContent='', strScraper='', strHash='',strSettings='',useFolderNames=0,scanRecursive=0 where idPath=%i", i.first));
      }
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strPath.c_str());
  }
  if (progress)
    progress->Close();
}

void CVideoDatabase::SetScraperForPath(const std::string& filePath, const ScraperPtr& scraper, const VIDEO::SScanSettings& settings)
{
  // if we have a multipath, set scraper for all contained paths
  if(URIUtils::IsMultiPath(filePath))
  {
    std::vector<std::string> paths;
    CMultiPathDirectory::GetPaths(filePath, paths);

    for(unsigned i=0;i<paths.size();i++)
      SetScraperForPath(paths[i],scraper,settings);

    return;
  }

  try
  {
    int idPath = AddPath(filePath);
    if (idPath < 0)
      return;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;
    
    CODBPath path;
    if (!m_cdb.getDB()->query_one<CODBPath>(query::idPath == idPath, path))
    {
      if(odb_transaction)
        odb_transaction->rollback();
      return;
    }

    // Update
    std::string strSQL;
    if (settings.exclude)
    { //NB See note in ::GetScraperForPath about strContent=='none'
      path.m_content = "";
      path.m_scraper = "";
      path.m_scanRecursive = 0;
      path.m_useFolderNames = false;
      path.m_settings = "";
      path.m_noUpdate = false;
      path.m_exclude = true;
    }
    else if(!scraper)
    { // catch clearing content, but not excluding
      path.m_content = "";
      path.m_scraper = "";
      path.m_scanRecursive = 0;
      path.m_useFolderNames = false;
      path.m_settings = "";
      path.m_noUpdate = false;
      path.m_exclude = false;
    }
    else
    {
      std::string content = TranslateContent(scraper->Content());
      
      path.m_content = content;
      path.m_scraper = scraper->ID();
      path.m_scanRecursive = settings.recurse;
      path.m_useFolderNames = settings.parent_name;
      path.m_settings = scraper->GetPathSettings();
      path.m_noUpdate = settings.noupdate;
      path.m_exclude = false;
    }
    m_cdb.getDB()->update(path);
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, filePath.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, filePath.c_str());
  }
}

bool CVideoDatabase::ScraperInUse(const std::string &scraperID) const
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    std::string sql = PrepareSQL("select count(1) from path where strScraper='%s'", scraperID.c_str());
    if (!m_pDS->query(sql) || m_pDS->num_rows() == 0)
      return false;
    bool found = m_pDS->fv(0).get_asInt() > 0;
    m_pDS->close();
    return found;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s(%s) failed", __FUNCTION__, scraperID.c_str());
  }
  return false;
}

class CArtItem
{
public:
  CArtItem() { art_id = 0; media_id = 0; };
  int art_id;
  std::string art_type;
  std::string art_url;
  int media_id;
  std::string media_type;
};

// used for database update to v83
class CShowItem
{
public:
  bool operator==(const CShowItem &r) const
  {
    return (!ident.empty() && ident == r.ident) || (title == r.title && year == r.year);
  };
  int    id;
  int    path;
  std::string title;
  std::string year;
  std::string ident;
};

// used for database update to v84
class CShowLink
{
public:
  int show;
  int pathId;
  std::string path;
};

void CVideoDatabase::UpdateTables(int iVersion)
{
  if (iVersion < 76)
  {
    m_pDS->exec("ALTER TABLE settings ADD StereoMode integer");
    m_pDS->exec("ALTER TABLE settings ADD StereoInvert bool");
  }
  if (iVersion < 77)
    m_pDS->exec("ALTER TABLE streamdetails ADD strStereoMode text");

  if (iVersion < 81)
  { // add idParentPath to path table
    m_pDS->exec("ALTER TABLE path ADD idParentPath integer");
    std::map<std::string, int> paths;
    m_pDS->query("select idPath,strPath from path");
    while (!m_pDS->eof())
    {
      paths.insert(make_pair(m_pDS->fv(1).get_asString(), m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }
    m_pDS->close();
    // run through these paths figuring out the parent path, and add to the table if found
    for (const auto &i : paths)
    {
      std::string parent = URIUtils::GetParentPath(i.first);
      auto j = paths.find(parent);
      if (j != paths.end())
        m_pDS->exec(PrepareSQL("UPDATE path SET idParentPath=%i WHERE idPath=%i", j->second, i.second));
    }
  }
  if (iVersion < 82)
  {
    // drop parent path id and basePath from tvshow table
    m_pDS->exec("UPDATE tvshow SET c16=NULL,c17=NULL");
  }
  if (iVersion < 83)
  {
    // drop duplicates in tvshow table, and update tvshowlinkpath accordingly
    std::string sql = PrepareSQL("SELECT tvshow.idShow,idPath,c%02d,c%02d,c%02d FROM tvshow JOIN tvshowlinkpath ON tvshow.idShow = tvshowlinkpath.idShow", VIDEODB_ID_TV_TITLE, VIDEODB_ID_TV_PREMIERED, VIDEODB_ID_TV_IDENT_ID);
    m_pDS->query(sql);
    std::vector<CShowItem> shows;
    while (!m_pDS->eof())
    {
      CShowItem show;
      show.id    = m_pDS->fv(0).get_asInt();
      show.path  = m_pDS->fv(1).get_asInt();
      show.title = m_pDS->fv(2).get_asString();
      show.year  = m_pDS->fv(3).get_asString();
      show.ident = m_pDS->fv(4).get_asString();
      shows.emplace_back(std::move(show));
      m_pDS->next();
    }
    m_pDS->close();
    if (!shows.empty())
    {
      for (auto i = shows.begin() + 1; i != shows.end(); ++i)
      {
        // has this show been found before?
        auto j = find(shows.begin(), i, *i);
        if (j != i)
        { // this is a duplicate
          // update the tvshowlinkpath table
          m_pDS->exec(PrepareSQL("UPDATE tvshowlinkpath SET idShow = %d WHERE idShow = %d AND idPath = %d", j->id, i->id, i->path));
          // update episodes, seasons, movie links
          m_pDS->exec(PrepareSQL("UPDATE episode SET idShow = %d WHERE idShow = %d", j->id, i->id));
          m_pDS->exec(PrepareSQL("UPDATE seasons SET idShow = %d WHERE idShow = %d", j->id, i->id));
          m_pDS->exec(PrepareSQL("UPDATE movielinktvshow SET idShow = %d WHERE idShow = %d", j->id, i->id));
          // delete tvshow
          m_pDS->exec(PrepareSQL("DELETE FROM genrelinktvshow WHERE idShow=%i", i->id));
          m_pDS->exec(PrepareSQL("DELETE FROM actorlinktvshow WHERE idShow=%i", i->id));
          m_pDS->exec(PrepareSQL("DELETE FROM directorlinktvshow WHERE idShow=%i", i->id));
          m_pDS->exec(PrepareSQL("DELETE FROM studiolinktvshow WHERE idShow=%i", i->id));
          m_pDS->exec(PrepareSQL("DELETE FROM tvshow WHERE idShow = %d", i->id));
        }
      }
      // cleanup duplicate seasons
      m_pDS->exec("DELETE FROM seasons WHERE idSeason NOT IN (SELECT idSeason FROM (SELECT min(idSeason) as idSeason FROM seasons GROUP BY idShow,season) AS sub)");
    }
  }
  if (iVersion < 84)
  { // replace any multipaths in tvshowlinkpath table
    m_pDS->query("SELECT idShow, tvshowlinkpath.idPath, strPath FROM tvshowlinkpath JOIN path ON tvshowlinkpath.idPath=path.idPath WHERE path.strPath LIKE 'multipath://%'");
    std::vector<CShowLink> shows;
    while (!m_pDS->eof())
    {
      CShowLink link;
      link.show   = m_pDS->fv(0).get_asInt();
      link.pathId = m_pDS->fv(1).get_asInt();
      link.path   = m_pDS->fv(2).get_asString();
      shows.emplace_back(std::move(link));
      m_pDS->next();
    }
    m_pDS->close();
    // update these
    for (auto i = shows.begin(); i != shows.end(); ++i)
    {
      std::vector<std::string> paths;
      CMultiPathDirectory::GetPaths(i->path, paths);
      for (auto j = paths.begin(); j != paths.end(); ++j)
      {
        int idPath = AddPath(*j, URIUtils::GetParentPath(*j));
        /* we can't rely on REPLACE INTO here as analytics (indices) aren't online yet */
        if (GetSingleValue(PrepareSQL("SELECT 1 FROM tvshowlinkpath WHERE idShow=%i AND idPath=%i", i->show, idPath)).empty())
          m_pDS->exec(PrepareSQL("INSERT INTO tvshowlinkpath(idShow, idPath) VALUES(%i,%i)", i->show, idPath));
      }
      m_pDS->exec(PrepareSQL("DELETE FROM tvshowlinkpath WHERE idShow=%i AND idPath=%i", i->show, i->pathId));
    }
  }
  if (iVersion < 85)
  {
    // drop multipaths from the path table - they're not needed for anything at all
    m_pDS->exec("DELETE FROM path WHERE strPath LIKE 'multipath://%'");
  }
  if (iVersion < 87)
  { // due to the tvshow merging above, there could be orphaned season or show art
    m_pDS->exec("DELETE from art WHERE media_type='tvshow' AND NOT EXISTS (SELECT 1 FROM tvshow WHERE tvshow.idShow = art.media_id)");
    m_pDS->exec("DELETE from art WHERE media_type='season' AND NOT EXISTS (SELECT 1 FROM seasons WHERE seasons.idSeason = art.media_id)");
  }
  if (iVersion < 91)
  {
    // create actor link table
    m_pDS->exec("CREATE TABLE actor_link(actor_id INT, media_id INT, media_type TEXT, role TEXT, cast_order INT)");
    m_pDS->exec("INSERT INTO actor_link(actor_id, media_id, media_type, role, cast_order) SELECT DISTINCT idActor, idMovie, 'movie', strRole, iOrder from actorlinkmovie");
    m_pDS->exec("INSERT INTO actor_link(actor_id, media_id, media_type, role, cast_order) SELECT DISTINCT idActor, idShow, 'tvshow', strRole, iOrder from actorlinktvshow");
    m_pDS->exec("INSERT INTO actor_link(actor_id, media_id, media_type, role, cast_order) SELECT DISTINCT idActor, idEpisode, 'episode', strRole, iOrder from actorlinkepisode");
    m_pDS->exec("DROP TABLE IF EXISTS actorlinkmovie");
    m_pDS->exec("DROP TABLE IF EXISTS actorlinktvshow");
    m_pDS->exec("DROP TABLE IF EXISTS actorlinkepisode");
    m_pDS->exec("CREATE TABLE actor(actor_id INTEGER PRIMARY KEY, name TEXT, art_urls TEXT)");
    m_pDS->exec("INSERT INTO actor(actor_id, name, art_urls) SELECT idActor,strActor,strThumb FROM actors");
    m_pDS->exec("DROP TABLE IF EXISTS actors");

    // directors
    m_pDS->exec("CREATE TABLE director_link(actor_id INTEGER, media_id INTEGER, media_type TEXT)");
    m_pDS->exec("INSERT INTO director_link(actor_id, media_id, media_type) SELECT DISTINCT idDirector, idMovie, 'movie' FROM directorlinkmovie");
    m_pDS->exec("INSERT INTO director_link(actor_id, media_id, media_type) SELECT DISTINCT idDirector, idShow, 'tvshow' FROM directorlinktvshow");
    m_pDS->exec("INSERT INTO director_link(actor_id, media_id, media_type) SELECT DISTINCT idDirector, idEpisode, 'episode' FROM directorlinkepisode");
    m_pDS->exec("INSERT INTO director_link(actor_id, media_id, media_type) SELECT DISTINCT idDirector, idMVideo, 'musicvideo' FROM directorlinkmusicvideo");
    m_pDS->exec("DROP TABLE IF EXISTS directorlinkmovie");
    m_pDS->exec("DROP TABLE IF EXISTS directorlinktvshow");
    m_pDS->exec("DROP TABLE IF EXISTS directorlinkepisode");
    m_pDS->exec("DROP TABLE IF EXISTS directorlinkmusicvideo");

    // writers
    m_pDS->exec("CREATE TABLE writer_link(actor_id INTEGER, media_id INTEGER, media_type TEXT)");
    m_pDS->exec("INSERT INTO writer_link(actor_id, media_id, media_type) SELECT DISTINCT idWriter, idMovie, 'movie' FROM writerlinkmovie");
    m_pDS->exec("INSERT INTO writer_link(actor_id, media_id, media_type) SELECT DISTINCT idWriter, idEpisode, 'episode' FROM writerlinkepisode");
    m_pDS->exec("DROP TABLE IF EXISTS writerlinkmovie");
    m_pDS->exec("DROP TABLE IF EXISTS writerlinkepisode");

    // music artist
    m_pDS->exec("INSERT INTO actor_link(actor_id, media_id, media_type) SELECT DISTINCT idArtist, idMVideo, 'musicvideo' FROM artistlinkmusicvideo");
    m_pDS->exec("DROP TABLE IF EXISTS artistlinkmusicvideo");

    // studios
    m_pDS->exec("CREATE TABLE studio_link(studio_id INTEGER, media_id INTEGER, media_type TEXT)");
    m_pDS->exec("INSERT INTO studio_link(studio_id, media_id, media_type) SELECT DISTINCT idStudio, idMovie, 'movie' FROM studiolinkmovie");
    m_pDS->exec("INSERT INTO studio_link(studio_id, media_id, media_type) SELECT DISTINCT idStudio, idShow, 'tvshow' FROM studiolinktvshow");
    m_pDS->exec("INSERT INTO studio_link(studio_id, media_id, media_type) SELECT DISTINCT idStudio, idMVideo, 'musicvideo' FROM studiolinkmusicvideo");
    m_pDS->exec("DROP TABLE IF EXISTS studiolinkmovie");
    m_pDS->exec("DROP TABLE IF EXISTS studiolinktvshow");
    m_pDS->exec("DROP TABLE IF EXISTS studiolinkmusicvideo");
    m_pDS->exec("CREATE TABLE studionew(studio_id INTEGER PRIMARY KEY, name TEXT)");
    m_pDS->exec("INSERT INTO studionew(studio_id, name) SELECT idStudio,strStudio FROM studio");
    m_pDS->exec("DROP TABLE IF EXISTS studio");
    m_pDS->exec("ALTER TABLE studionew RENAME TO studio");

    // genres
    m_pDS->exec("CREATE TABLE genre_link(genre_id INTEGER, media_id INTEGER, media_type TEXT)");
    m_pDS->exec("INSERT INTO genre_link(genre_id, media_id, media_type) SELECT DISTINCT idGenre, idMovie, 'movie' FROM genrelinkmovie");
    m_pDS->exec("INSERT INTO genre_link(genre_id, media_id, media_type) SELECT DISTINCT idGenre, idShow, 'tvshow' FROM genrelinktvshow");
    m_pDS->exec("INSERT INTO genre_link(genre_id, media_id, media_type) SELECT DISTINCT idGenre, idMVideo, 'musicvideo' FROM genrelinkmusicvideo");
    m_pDS->exec("DROP TABLE IF EXISTS genrelinkmovie");
    m_pDS->exec("DROP TABLE IF EXISTS genrelinktvshow");
    m_pDS->exec("DROP TABLE IF EXISTS genrelinkmusicvideo");
    m_pDS->exec("CREATE TABLE genrenew(genre_id INTEGER PRIMARY KEY, name TEXT)");
    m_pDS->exec("INSERT INTO genrenew(genre_id, name) SELECT idGenre,strGenre FROM genre");
    m_pDS->exec("DROP TABLE IF EXISTS genre");
    m_pDS->exec("ALTER TABLE genrenew RENAME TO genre");

    // country
    m_pDS->exec("CREATE TABLE country_link(country_id INTEGER, media_id INTEGER, media_type TEXT)");
    m_pDS->exec("INSERT INTO country_link(country_id, media_id, media_type) SELECT DISTINCT idCountry, idMovie, 'movie' FROM countrylinkmovie");
    m_pDS->exec("DROP TABLE IF EXISTS countrylinkmovie");
    m_pDS->exec("CREATE TABLE countrynew(country_id INTEGER PRIMARY KEY, name TEXT)");
    m_pDS->exec("INSERT INTO countrynew(country_id, name) SELECT idCountry,strCountry FROM country");
    m_pDS->exec("DROP TABLE IF EXISTS country");
    m_pDS->exec("ALTER TABLE countrynew RENAME TO country");

    // tags
    m_pDS->exec("CREATE TABLE tag_link(tag_id INTEGER, media_id INTEGER, media_type TEXT)");
    m_pDS->exec("INSERT INTO tag_link(tag_id, media_id, media_type) SELECT DISTINCT idTag, idMedia, media_type FROM taglinks");
    m_pDS->exec("DROP TABLE IF EXISTS taglinks");
    m_pDS->exec("CREATE TABLE tagnew(tag_id INTEGER PRIMARY KEY, name TEXT)");
    m_pDS->exec("INSERT INTO tagnew(tag_id, name) SELECT idTag,strTag FROM tag");
    m_pDS->exec("DROP TABLE IF EXISTS tag");
    m_pDS->exec("ALTER TABLE tagnew RENAME TO tag");
  }

  if (iVersion < 93)
  {
    // cleanup main tables
    std::string valuesSql;
    for(int i = 0; i < VIDEODB_MAX_COLUMNS; i++)
    {
      valuesSql += StringUtils::Format("c%02d = TRIM(c%02d)", i, i);
      if (i < VIDEODB_MAX_COLUMNS - 1)
        valuesSql += ",";
    }
    m_pDS->exec("UPDATE episode SET " + valuesSql);
    m_pDS->exec("UPDATE movie SET " + valuesSql);
    m_pDS->exec("UPDATE musicvideo SET " + valuesSql);
    m_pDS->exec("UPDATE tvshow SET " + valuesSql);

    // cleanup additional tables
    std::map<std::string, std::vector<std::string>> additionalTablesMap = {
      {"actor", {"actor_link", "director_link", "writer_link"}},
      {"studio", {"studio_link"}},
      {"genre", {"genre_link"}},
      {"country", {"country_link"}},
      {"tag", {"tag_link"}}
    };
    for (const auto& addtionalTableEntry : additionalTablesMap)
    {
      std::string table = addtionalTableEntry.first;
      std::string tablePk = addtionalTableEntry.first + "_id";
      std::map<int, std::string> duplicatesMinMap;
      std::map<int, std::string> duplicatesMap;

      // cleanup name
      m_pDS->exec(PrepareSQL("UPDATE %s SET name = TRIM(name)",
                             table.c_str()));

      // shrink name to length 255
      m_pDS->exec(PrepareSQL("UPDATE %s SET name = SUBSTR(name, 1, 255) WHERE LENGTH(name) > 255",
                             table.c_str()));

      // fetch main entries
      m_pDS->query(PrepareSQL("SELECT MIN(%s), name FROM %s GROUP BY name HAVING COUNT(1) > 1",
                              tablePk.c_str(), table.c_str()));

      while (!m_pDS->eof())
      {
        duplicatesMinMap.insert(std::make_pair(m_pDS->fv(0).get_asInt(), m_pDS->fv(1).get_asString()));
        m_pDS->next();
      }
      m_pDS->close();

      // fetch duplicate entries
      for (const auto& entry : duplicatesMinMap)
      {
        m_pDS->query(PrepareSQL("SELECT %s FROM %s WHERE name = '%s' AND %s <> %i",
                                tablePk.c_str(), table.c_str(),
                                entry.second.c_str(), tablePk.c_str(), entry.first));

        std::stringstream ids;
        while (!m_pDS->eof())
        {
          int id = m_pDS->fv(0).get_asInt();
          m_pDS->next();

          ids << id;
          if (!m_pDS->eof())
            ids << ",";
        }
        m_pDS->close();

        duplicatesMap.insert(std::make_pair(entry.first, ids.str()));
      }

      // cleanup duplicates in link tables
      for (const auto& subTable : addtionalTableEntry.second)
      {
        // create indexes to speed up things
        m_pDS->exec(PrepareSQL("CREATE INDEX ix_%s ON %s (%s)",
                               subTable.c_str(), subTable.c_str(), tablePk.c_str()));

        // migrate every duplicate entry to the main entry
        for (const auto& entry : duplicatesMap)
        {
          m_pDS->exec(PrepareSQL("UPDATE %s SET %s = %i WHERE %s IN (%s) ",
                                 subTable.c_str(), tablePk.c_str(), entry.first,
                                 tablePk.c_str(), entry.second.c_str()));
        }

        // clear all duplicates in the link tables
        if (subTable == "actor_link")
        {
          // as a distinct won't work because of role and cast_order and a group by kills a
          // low powered mysql, we de-dupe it with REPLACE INTO while using the real unique index
          m_pDS->exec("CREATE TABLE temp_actor_link(actor_id INT, media_id INT, media_type TEXT, role TEXT, cast_order INT)");
          m_pDS->exec("CREATE UNIQUE INDEX ix_temp_actor_link ON temp_actor_link (actor_id, media_type(20), media_id)");
          m_pDS->exec("REPLACE INTO temp_actor_link SELECT * FROM actor_link");
          m_pDS->exec("DROP INDEX ix_temp_actor_link ON temp_actor_link");
        }
        else
        {
          m_pDS->exec(PrepareSQL("CREATE TABLE temp_%s AS SELECT DISTINCT * FROM %s",
                                 subTable.c_str(), subTable.c_str()));
        }

        m_pDS->exec(PrepareSQL("DROP TABLE IF EXISTS %s",
                               subTable.c_str()));

        m_pDS->exec(PrepareSQL("ALTER TABLE temp_%s RENAME TO %s",
                               subTable.c_str(), subTable.c_str()));
      }

      // delete duplicates in main table
      for (const auto& entry : duplicatesMap)
      {
        m_pDS->exec(PrepareSQL("DELETE FROM %s WHERE %s IN (%s)",
                               table.c_str(), tablePk.c_str(), entry.second.c_str()));
      }
    }
  }

  if (iVersion < 96)
  {
    m_pDS->exec("ALTER TABLE movie ADD userrating integer");
    m_pDS->exec("ALTER TABLE episode ADD userrating integer");
    m_pDS->exec("ALTER TABLE tvshow ADD userrating integer");
    m_pDS->exec("ALTER TABLE musicvideo ADD userrating integer");
  }

  if (iVersion < 97)
    m_pDS->exec("ALTER TABLE sets ADD strOverview TEXT");

  if (iVersion < 98)
    m_pDS->exec("ALTER TABLE seasons ADD name text");

  if (iVersion < 99)
  {
    // Add idSeason to episode table, so we don't have to join via idShow and season in the future
    m_pDS->exec("ALTER TABLE episode ADD idSeason integer");

    m_pDS->query("SELECT idSeason, idShow, season FROM seasons");
    while (!m_pDS->eof())
    {
      m_pDS2->exec(PrepareSQL("UPDATE episode "
        "SET idSeason = %d "
        "WHERE "
        "episode.idShow = %d AND "
        "episode.c%02d = %d",
        m_pDS->fv(0).get_asInt(), m_pDS->fv(1).get_asInt(), 
        VIDEODB_ID_EPISODE_SEASON, m_pDS->fv(2).get_asInt()));

      m_pDS->next();
    }
  }
  if (iVersion < 101)
    m_pDS->exec("ALTER TABLE seasons ADD userrating INTEGER");

  if (iVersion < 102)
  {
    m_pDS->exec("CREATE TABLE rating (rating_id INTEGER PRIMARY KEY, media_id INTEGER, media_type TEXT, rating_type TEXT, rating FLOAT, votes INTEGER)");

    std::string sql = PrepareSQL("SELECT DISTINCT idMovie, c%02d, c%02d FROM movie", VIDEODB_ID_RATING_ID, VIDEODB_ID_VOTES);
    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      m_pDS2->exec(PrepareSQL("INSERT INTO rating(media_id, media_type, rating_type, rating, votes) VALUES (%i, 'movie', 'default', %f, %i)", m_pDS->fv(0).get_asInt(), (float)strtod(m_pDS->fv(1).get_asString().c_str(), NULL), StringUtils::ReturnDigits(m_pDS->fv(2).get_asString())));
      int idRating = (int)m_pDS2->lastinsertid();
      m_pDS2->exec(PrepareSQL("UPDATE movie SET c%02d=%i WHERE idMovie=%i", VIDEODB_ID_RATING_ID, idRating, m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }
    m_pDS->close();

    sql = PrepareSQL("SELECT DISTINCT idShow, c%02d, c%02d FROM tvshow", VIDEODB_ID_TV_RATING_ID, VIDEODB_ID_TV_VOTES);
    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      m_pDS2->exec(PrepareSQL("INSERT INTO rating(media_id, media_type, rating_type, rating, votes) VALUES (%i, 'tvshow', 'default', %f, %i)", m_pDS->fv(0).get_asInt(), (float)strtod(m_pDS->fv(1).get_asString().c_str(), NULL), StringUtils::ReturnDigits(m_pDS->fv(2).get_asString())));
      int idRating = (int)m_pDS2->lastinsertid();
      m_pDS2->exec(PrepareSQL("UPDATE tvshow SET c%02d=%i WHERE idShow=%i", VIDEODB_ID_TV_RATING_ID, idRating, m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }
    m_pDS->close();

    sql = PrepareSQL("SELECT DISTINCT idEpisode, c%02d, c%02d FROM episode", VIDEODB_ID_EPISODE_RATING_ID, VIDEODB_ID_EPISODE_VOTES);
    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      m_pDS2->exec(PrepareSQL("INSERT INTO rating(media_id, media_type, rating_type, rating, votes) VALUES (%i, 'episode', 'default', %f, %i)", m_pDS->fv(0).get_asInt(), (float)strtod(m_pDS->fv(1).get_asString().c_str(), NULL), StringUtils::ReturnDigits(m_pDS->fv(2).get_asString())));
      int idRating = (int)m_pDS2->lastinsertid();
      m_pDS2->exec(PrepareSQL("UPDATE episode SET c%02d=%i WHERE idEpisode=%i", VIDEODB_ID_EPISODE_RATING_ID, idRating, m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }
    m_pDS->close();
  }

  if (iVersion < 103)
  {
    m_pDS->exec("ALTER TABLE settings ADD VideoStream integer");
    m_pDS->exec("ALTER TABLE streamdetails ADD strVideoLanguage text");
  }

  if (iVersion < 104)
  {
    m_pDS->exec("ALTER TABLE tvshow ADD duration INTEGER");

    std::string sql = PrepareSQL( "SELECT episode.idShow, MAX(episode.c%02d) "
                                  "FROM episode "

                                  "LEFT JOIN streamdetails "
                                  "ON streamdetails.idFile = episode.idFile "
                                  "AND streamdetails.iStreamType = 0 " // only grab video streams

                                  "WHERE episode.c%02d <> streamdetails.iVideoDuration "
                                  "OR streamdetails.iVideoDuration IS NULL "
                                  "GROUP BY episode.idShow", VIDEODB_ID_EPISODE_RUNTIME, VIDEODB_ID_EPISODE_RUNTIME);

    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      m_pDS2->exec(PrepareSQL("UPDATE tvshow SET duration=%i WHERE idShow=%i", m_pDS->fv(1).get_asInt(), m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }
    m_pDS->close();
  }
  
  if (iVersion < 105)
  {
    m_pDS->exec("ALTER TABLE movie ADD premiered TEXT");
    m_pDS->exec(PrepareSQL("UPDATE movie SET premiered=c%02d", VIDEODB_ID_YEAR));
    m_pDS->exec("ALTER TABLE musicvideo ADD premiered TEXT");
    m_pDS->exec(PrepareSQL("UPDATE musicvideo SET premiered=c%02d", VIDEODB_ID_MUSICVIDEO_YEAR));
  }

  if (iVersion < 107)
  {
    // need this due to the nested GetScraperPath query
    std::unique_ptr<Dataset> pDS;
    pDS.reset(m_pDB->CreateDataset());
    if (NULL == pDS.get()) return;

    pDS->exec("CREATE TABLE uniqueid (uniqueid_id INTEGER PRIMARY KEY, media_id INTEGER, media_type TEXT, value TEXT, type TEXT)");

    for (int i = 0; i < 3; ++i)
    {
      std::string mediatype, columnID;
      int columnUniqueID;
      switch (i)
      {
      case (0):
        mediatype = "movie";
        columnID = "idMovie";
        columnUniqueID = VIDEODB_ID_IDENT_ID;
        break;
      case (1):
        mediatype = "tvshow";
        columnID = "idShow";
        columnUniqueID = VIDEODB_ID_TV_IDENT_ID;
        break;
      case (2):
        mediatype = "episode";
        columnID = "idEpisode";
        columnUniqueID = VIDEODB_ID_EPISODE_IDENT_ID;
        break;
      default:
        continue;
      }
      pDS->query(PrepareSQL("SELECT %s, c%02d FROM %s", columnID.c_str(), columnUniqueID, mediatype.c_str()));
      while (!pDS->eof())
      {
        std::string uniqueid = pDS->fv(1).get_asString();
        if (!uniqueid.empty())
        {
          int mediaid = pDS->fv(0).get_asInt();
          if (StringUtils::StartsWith(uniqueid, "tt"))
            m_pDS2->exec(PrepareSQL("INSERT INTO uniqueid(media_id, media_type, type, value) VALUES (%i, '%s', 'imdb', '%s')", mediaid, mediatype.c_str(), uniqueid.c_str()));
          else
            m_pDS2->exec(PrepareSQL("INSERT INTO uniqueid(media_id, media_type, type, value) VALUES (%i, '%s', 'unknown', '%s')", mediaid, mediatype.c_str(), uniqueid.c_str()));
          m_pDS2->exec(PrepareSQL("UPDATE %s SET c%02d='%i' WHERE %s=%i", mediatype.c_str(), columnUniqueID, (int)m_pDS2->lastinsertid(), columnID.c_str(), mediaid));
        }
        pDS->next();
      }
      pDS->close();
    }
  }
}

int CVideoDatabase::GetSchemaVersion() const
{
  return 107;
}

bool CVideoDatabase::LookupByFolders(const std::string &path, bool shows)
{
  SScanSettings settings;
  bool foundDirectly = false;
  ScraperPtr scraper = GetScraperForPath(path, settings, foundDirectly);
  if (scraper && scraper->Content() == CONTENT_TVSHOWS && !shows)
    return false; // episodes
  return settings.parent_name_root; // shows, movies, musicvids
}

bool CVideoDatabase::GetPlayCounts(const std::string &strPath, CFileItemList &items)
{
  if(URIUtils::IsMultiPath(strPath))
  {
    std::vector<std::string> paths;
    CMultiPathDirectory::GetPaths(strPath, paths);

    bool ret = false;
    for(unsigned i=0;i<paths.size();i++)
      ret |= GetPlayCounts(paths[i], items);

    return ret;
  }
  int pathID;
  if (URIUtils::IsPlugin(strPath))
  {
    CURL url(strPath);
    pathID = GetPathId(url.GetWithoutFilename());
  }
  else
    pathID = GetPathId(strPath);
  if (pathID < 0)
    return false; // path (and thus files) aren't in the database

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    items.SetFastLookup(true); // note: it's possibly quicker the other way around (map on db returned items)?

    odb::result<CODBFile> res(m_cdb.getDB()->query<CODBFile>(odb::query<CODBFile>::path->idPath == pathID));
    for (odb::result<CODBFile>::iterator i = res.begin(); i != res.end(); i++)
    {
      std::string path;
      ConstructPath(path, strPath, i->m_filename);
      CFileItemPtr item = items.Get(path);
      if (item)
      {
        item->GetVideoInfoTag()->m_playCount = i->m_playCount;
        if (!item->GetVideoInfoTag()->m_resumePoint.IsSet())
        {
          CODBBookmark bookmark;
          
          if (!m_cdb.getDB()->query_one<CODBBookmark>(odb::query<CODBBookmark>::file->idFile == i->m_idFile &&
                                                      odb::query<CODBBookmark>::type == CBookmark::RESUME,
                                                      bookmark))
            continue;
          
          item->GetVideoInfoTag()->m_resumePoint.timeInSeconds = bookmark.m_timeInSeconds;
          item->GetVideoInfoTag()->m_resumePoint.totalTimeInSeconds = bookmark.m_totalTimeInSeconds;
          item->GetVideoInfoTag()->m_resumePoint.type = CBookmark::RESUME;
        }
      }
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

int CVideoDatabase::GetPlayCount(int iFileId)
{
  if (iFileId < 0)
    return 0;  // not in db, so not watched

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBFile odb_file;
    if (!m_cdb.getDB()->query_one<CODBFile>(odb::query<CODBFile>::idFile == iFileId, odb_file))
      return 0;
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return odb_file.m_playCount;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return -1;
}

int CVideoDatabase::GetPlayCount(const std::string& strFilenameAndPath)
{
  return GetPlayCount(GetFileId(strFilenameAndPath));
}

int CVideoDatabase::GetPlayCount(const CFileItem &item)
{
  return GetPlayCount(GetFileId(item));
}

void CVideoDatabase::UpdateFanart(const CFileItem &item, VIDEODB_CONTENT_TYPE type)
{
  if (!item.HasVideoInfoTag() || item.GetVideoInfoTag()->m_iDbId < 0) return;

  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    if (type == VIDEODB_CONTENT_MOVIES)
    {
      CODBMovie odb_movie;
      if (!m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == item.GetVideoInfoTag()->m_iDbId, odb_movie))
      {
        return;
      }
      
      odb_movie.m_fanart = item.GetVideoInfoTag()->m_fanart.m_xml;
      m_cdb.getDB()->update(odb_movie);
      
      AnnounceUpdate(MediaTypeMovie, item.GetVideoInfoTag()->m_iDbId);
    }
    else if (type == VIDEODB_CONTENT_TVSHOWS)
    {
      //TODO: Implement
      AnnounceUpdate(MediaTypeTvShow, item.GetVideoInfoTag()->m_iDbId);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s - exception on updating fanart for %s - %s", __FUNCTION__, item.GetPath().c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s - error updating fanart for %s", __FUNCTION__, item.GetPath().c_str());
  }
}

void CVideoDatabase::SetPlayCount(const CFileItem &item, int count, const CDateTime &date)
{
  int id;
  if (item.HasProperty("original_listitem_url") &&
      URIUtils::IsPlugin(item.GetProperty("original_listitem_url").asString()))
  {
    CFileItem item2(item);
    item2.SetPath(item.GetProperty("original_listitem_url").asString());
    id = AddFile(item2);
  }
  else
    id = AddFile(item);
  if (id < 0)
    return;

  // and mark as watched
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBFile odb_file;
    if (!m_cdb.getDB()->query_one<CODBFile>(odb::query<CODBFile>::idFile == id, odb_file))
      return;
    
    if (count)
      odb_file.m_playCount = count;
    
    if (date.IsValid())
    {
      odb_file.m_lastPlayed.setDateTime(date.GetAsULongLong(), date.GetAsDBDateTime());
    }
    else
    {
      CDateTime current = CDateTime::GetCurrentDateTime();
      odb_file.m_lastPlayed.setDateTime(current.GetAsULongLong(), current.GetAsDBDateTime());
    }
    
    m_cdb.getDB()->update(odb_file);
    
    if(odb_transaction)
      odb_transaction->commit();

    // We only need to announce changes to video items in the library
    if (item.HasVideoInfoTag() && item.GetVideoInfoTag()->m_iDbId > 0)
    {
      CVariant data;
      if (g_application.IsVideoScanning())
        data["transaction"] = true;
      // Only provide the "playcount" value if it has actually changed
      if (item.GetVideoInfoTag()->m_playCount != count)
        data["playcount"] = count;
      ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnUpdate", CFileItemPtr(new CFileItem(item)), data);
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

void CVideoDatabase::IncrementPlayCount(const CFileItem &item)
{
  SetPlayCount(item, GetPlayCount(item) + 1);
}

void CVideoDatabase::UpdateLastPlayed(const CFileItem &item)
{
  SetPlayCount(item, GetPlayCount(item), CDateTime::GetCurrentDateTime());
}

void CVideoDatabase::UpdateMovieTitle(int idMovie, const std::string& strNewMovieTitle, VIDEODB_CONTENT_TYPE iType)
{
  try
  {
    if (NULL == m_pDB.get()) return ;
    if (NULL == m_pDS.get()) return ;
    std::string content;
    if (iType == VIDEODB_CONTENT_MOVIES)
    {
      CLog::Log(LOGINFO, "Changing Movie:id:%i New Title:%s", idMovie, strNewMovieTitle.c_str());
      content = MediaTypeMovie;
    }
    else if (iType == VIDEODB_CONTENT_EPISODES)
    {
      CLog::Log(LOGINFO, "Changing Episode:id:%i New Title:%s", idMovie, strNewMovieTitle.c_str());
      content = MediaTypeEpisode;
    }
    else if (iType == VIDEODB_CONTENT_TVSHOWS)
    {
      CLog::Log(LOGINFO, "Changing TvShow:id:%i New Title:%s", idMovie, strNewMovieTitle.c_str());
      content = MediaTypeTvShow;
    }
    else if (iType == VIDEODB_CONTENT_MUSICVIDEOS)
    {
      CLog::Log(LOGINFO, "Changing MusicVideo:id:%i New Title:%s", idMovie, strNewMovieTitle.c_str());
      content = MediaTypeMusicVideo;
    }
    else if (iType == VIDEODB_CONTENT_MOVIE_SETS)
    {
      CLog::Log(LOGINFO, "Changing Movie set:id:%i New Title:%s", idMovie, strNewMovieTitle.c_str());
      
      std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
      
      CODBSet set;
      if (m_cdb.getDB()->query_one<CODBSet>(odb::query<CODBSet>::idSet == idMovie, set))
      {
        set.m_name = strNewMovieTitle;
        m_cdb.getDB()->update(set);
      }
      
      if(odb_transaction)
        odb_transaction->commit();
    }

    if (!content.empty())
    {
      SetSingleValue(iType, idMovie, FieldTitle, strNewMovieTitle);
      AnnounceUpdate(content, idMovie);
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (int idMovie, const std::string& strNewMovieTitle) failed on MovieID:%i and Title:%s", __FUNCTION__, idMovie, strNewMovieTitle.c_str());
  }
}

bool CVideoDatabase::UpdateVideoSortTitle(int idDb, const std::string& strNewSortTitle, VIDEODB_CONTENT_TYPE iType /* = VIDEODB_CONTENT_MOVIES */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::string content = MediaTypeMovie;

    if(iType == VIDEODB_CONTENT_MOVIES)
    {
      CODBMovie movie;
      if (m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == idDb, movie))
      {
        movie.m_sortTitle = strNewSortTitle;
        m_cdb.getDB()->update(movie);
      }
    }
    else if (iType == VIDEODB_CONTENT_TVSHOWS)
    {
      content = MediaTypeTvShow;
      
      //TODO: Implement TV Shows
    }
    
    if(odb_transaction)
      odb_transaction->commit();

    AnnounceUpdate(content, idDb);
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (int idDb, const std::string& strNewSortTitle, VIDEODB_CONTENT_TYPE iType) failed on ID: %i and Sort Title: %s", __FUNCTION__, idDb, strNewSortTitle.c_str());
  }

  return false;
}

/// \brief EraseVideoSettings() Erases the videoSettings table and reconstructs it
void CVideoDatabase::EraseVideoSettings(const std::string &path /* = ""*/)
{
  try
  {
    std::string sql = "DELETE FROM settings";

    if (!path.empty())
    {
      Filter pathFilter;
      pathFilter.AppendWhere(PrepareSQL("idFile IN (SELECT idFile FROM files INNER JOIN path ON path.idPath = files.idPath AND path.strPath LIKE \"%s%%\")", path.c_str()));
      sql += std::string(" WHERE ") + pathFilter.where.c_str();

      CLog::Log(LOGINFO, "Deleting settings information for all files under %s", path.c_str());
    }
    else
      CLog::Log(LOGINFO, "Deleting settings information for all files");
    
    m_pDS->exec(sql);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
}

bool CVideoDatabase::GetGenresNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, std::pair<std::string,int> > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<ODBView_Movie_Genre> res(m_cdb.getDB()->query<ODBView_Movie_Genre>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<ODBView_Movie_Genre>::iterator i = res.begin(); i != res.end(); i++)
      {
        int id = i->m_idGenre;
        std::string str = i->m_name;
        int playCount = i->m_playCount; //TODO: Figure out where / why this is needed and if this value is correct
        
        // was this already found?
        auto it = mapItems.find(id);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string, int>(str,playCount)));
        }
      }
    }
    
    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", static_cast<int>(mapItems.size()));
      items.Add(pItem);
      
      m_pDS->close();
      return true;
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.first));
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "genre";
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder = true;
      if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_playCount = i.second.second;
      pItem->SetLabelPreformated(true);
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
}

bool CVideoDatabase::GetCountriesNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, std::pair<std::string,int> > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<ODBView_Movie_Country> res(m_cdb.getDB()->query<ODBView_Movie_Country>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<ODBView_Movie_Country>::iterator i = res.begin(); i != res.end(); i++)
      {
        int id = i->m_idCountry;
        std::string str = i->m_name;
        int playCount = i->m_playCount; //TODO: Figure out where / why this is needed and if this value is correct
        
        // was this already found?
        auto it = mapItems.find(id);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string, int>(str,playCount)));
        }
      }
    }
    
    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", static_cast<int>(mapItems.size()));
      items.Add(pItem);
      
      m_pDS->close();
      return true;
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.first));
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "country";
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder = true;
      if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_playCount = i.second.second;
      pItem->SetLabelPreformated(true);
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
}

bool CVideoDatabase::GetStudiosNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, std::pair<std::string,int> > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<ODBView_Movie_Studio> res(m_cdb.getDB()->query<ODBView_Movie_Studio>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<ODBView_Movie_Studio>::iterator i = res.begin(); i != res.end(); i++)
      {
        int id = i->m_idStudio;
        std::string str = i->m_name;
        int playCount = i->m_playCount; //TODO: Figure out where / why this is needed and if this value is correct
        
        // was this already found?
        auto it = mapItems.find(id);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string, int>(str,playCount)));
        }
      }
    }
    
    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", static_cast<int>(mapItems.size()));
      items.Add(pItem);
      
      m_pDS->close();
      return true;
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.first));
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "studio";
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder = true;
      if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_playCount = i.second.second;
      pItem->SetLabelPreformated(true);
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
}

bool CVideoDatabase::GetNavCommon(const std::string& strBaseDir, CFileItemList& items, const char *type, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    std::string strSQL;
    Filter extFilter = filter;
    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
    {
      std::string view, view_id, media_type, extraField, extraJoin;
      if (idContent == VIDEODB_CONTENT_MOVIES)
      {
        view       = MediaTypeMovie;
        view_id    = "idMovie";
        media_type = MediaTypeMovie;
        extraField = "files.playCount";
      }
      else if (idContent == VIDEODB_CONTENT_TVSHOWS) //this will not get tvshows with 0 episodes
      {
        view       = MediaTypeEpisode;
        view_id    = "idShow";
        media_type = MediaTypeTvShow;
        // in order to make use of FieldPlaycount in smart playlists we need an extra join
        if (StringUtils::EqualsNoCase(type, "tag"))
          extraJoin  = PrepareSQL("JOIN tvshow_view ON tvshow_view.idShow = tag_link.media_id AND tag_link.media_type='tvshow'");
      }
      else if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
      {
        view       = MediaTypeMusicVideo;
        view_id    = "idMVideo";
        media_type = MediaTypeMusicVideo;
        extraField = "files.playCount";
      }
      else
        return false;

      strSQL = "SELECT %s " + PrepareSQL("FROM %s ", type);
      extFilter.fields = PrepareSQL("%s.%s_id, %s.name, path.strPath", type, type, type);
      extFilter.AppendField(extraField);
      extFilter.AppendJoin(PrepareSQL("JOIN %s_link ON %s.%s_id = %s_link.%s_id", type, type, type, type, type));
      extFilter.AppendJoin(PrepareSQL("JOIN %s_view ON %s_link.media_id = %s_view.%s AND %s_link.media_type='%s'", view.c_str(), type, view.c_str(), view_id.c_str(), type, media_type.c_str()));
      extFilter.AppendJoin(PrepareSQL("JOIN files ON files.idFile = %s_view.idFile", view.c_str()));
      extFilter.AppendJoin("JOIN path ON path.idPath = files.idPath");
      extFilter.AppendJoin(extraJoin);
    }
    else
    {
      std::string view, view_id, media_type, extraField, extraJoin;
      if (idContent == VIDEODB_CONTENT_MOVIES)
      {
        view       = MediaTypeMovie;
        view_id    = "idMovie";
        media_type = MediaTypeMovie;
        extraField = "count(1), count(files.playCount)";
        extraJoin  = PrepareSQL("JOIN files ON files.idFile = %s_view.idFile", view.c_str());
      }
      else if (idContent == VIDEODB_CONTENT_TVSHOWS)
      {
        view       = MediaTypeTvShow;
        view_id    = "idShow";
        media_type = MediaTypeTvShow;
      }
      else if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
      {
        view       = MediaTypeMusicVideo;
        view_id    = "idMVideo";
        media_type = MediaTypeMusicVideo;
        extraField = "count(1), count(files.playCount)";
        extraJoin  = PrepareSQL("JOIN files ON files.idFile = %s_view.idFile", view.c_str());
      }
      else
        return false;

      strSQL = "SELECT %s " + PrepareSQL("FROM %s ", type);
      extFilter.fields = PrepareSQL("%s.%s_id, %s.name", type, type, type);
      extFilter.AppendField(extraField);
      extFilter.AppendJoin(PrepareSQL("JOIN %s_link ON %s.%s_id = %s_link.%s_id", type, type, type, type, type));
      extFilter.AppendJoin(PrepareSQL("JOIN %s_view ON %s_link.media_id = %s_view.%s AND %s_link.media_type='%s'",
                                      view.c_str(), type, view.c_str(), view_id.c_str(), type, media_type.c_str()));
      extFilter.AppendJoin(extraJoin);
      extFilter.AppendGroup(PrepareSQL("%s.%s_id", type, type));
    }

    if (countOnly)
    {
      extFilter.fields = PrepareSQL("COUNT(DISTINCT %s.%s_id)", type, type);
      extFilter.group.clear();
      extFilter.order.clear();
    }
    strSQL = StringUtils::Format(strSQL.c_str(), !extFilter.fields.empty() ? extFilter.fields.c_str() : "*");

    CVideoDbUrl videoUrl;
    if (!BuildSQL(strBaseDir, strSQL, extFilter, strSQL, videoUrl))
      return false;

    int iRowsFound = RunQuery(strSQL);
    if (iRowsFound <= 0)
      return iRowsFound == 0;

    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", iRowsFound == 1 ? m_pDS->fv(0).get_asInt() : iRowsFound);
      items.Add(pItem);

      m_pDS->close();
      return true;
    }

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
    {
      std::map<int, std::pair<std::string,int> > mapItems;
      while (!m_pDS->eof())
      {
        int id = m_pDS->fv(0).get_asInt();
        std::string str = m_pDS->fv(1).get_asString();

        // was this already found?
        auto it = mapItems.find(id);
        if (it == mapItems.end())
        {
          // check path
          if (g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv(2).get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
              mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string, int>(str,m_pDS->fv(3).get_asInt()))); //fv(3) is file.playCount
            else if (idContent == VIDEODB_CONTENT_TVSHOWS)
              mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string,int>(str,0)));
          }
        }
        m_pDS->next();
      }
      m_pDS->close();

      for (const auto &i : mapItems)
      {
        CFileItemPtr pItem(new CFileItem(i.second.first));
        pItem->GetVideoInfoTag()->m_iDbId = i.first;
        pItem->GetVideoInfoTag()->m_type = type;

        CVideoDbUrl itemUrl = videoUrl;
        std::string path = StringUtils::Format("%i/", i.first);
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());

        pItem->m_bIsFolder = true;
        if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
          pItem->GetVideoInfoTag()->m_playCount = i.second.second;
        if (!items.Contains(pItem->GetPath()))
        {
          pItem->SetLabelPreformated(true);
          items.Add(pItem);
        }
      }
    }
    else
    {
      while (!m_pDS->eof())
      {
        CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
        pItem->GetVideoInfoTag()->m_iDbId = m_pDS->fv(0).get_asInt();
        pItem->GetVideoInfoTag()->m_type = type;

        CVideoDbUrl itemUrl = videoUrl;
        std::string path = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());

        pItem->m_bIsFolder = true;
        pItem->SetLabelPreformated(true);
        if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        { // fv(3) is the number of videos watched, fv(2) is the total number.  We set the playcount
          // only if the number of videos watched is equal to the total number (i.e. every video watched)
          pItem->GetVideoInfoTag()->m_playCount = (m_pDS->fv(3).get_asInt() == m_pDS->fv(2).get_asInt()) ? 1 : 0;
        }
        items.Add(pItem);
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetTagsNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, std::pair<std::string,int> > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<ODBView_Movie_Tag> res(m_cdb.getDB()->query<ODBView_Movie_Tag>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<ODBView_Movie_Tag>::iterator i = res.begin(); i != res.end(); i++)
      {
        int id = i->m_idTag;
        std::string str = i->m_name;
        int playCount = i->m_playCount; //TODO: Figure out where / why this is needed and if this value is correct
        
        // was this already found?
        auto it = mapItems.find(id);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string, int>(str,playCount)));
        }
      }
    }
    
    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", static_cast<int>(mapItems.size()));
      items.Add(pItem);
      
      m_pDS->close();
      return true;
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.first));
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "country";
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder = true;
      if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_playCount = i.second.second;
      pItem->SetLabelPreformated(true);
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
  
  return GetNavCommon(strBaseDir, items, "tag", idContent, filter, countOnly);
}

bool CVideoDatabase::GetSetsNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool ignoreSingleMovieSets /* = false */)
{
  if (idContent != VIDEODB_CONTENT_MOVIES)
    return false;

  return GetSetsByWhere(strBaseDir, filter, items, ignoreSingleMovieSets);
}

bool CVideoDatabase::GetSetsByWhere(const std::string& strBaseDir, const Filter &filter, CFileItemList& items, bool ignoreSingleMovieSets /* = false */, odb::query<ODBView_Movie> optionalQueries /* = empty */)
{
  try
  {
    CVideoDbUrl videoUrl;
    if (!videoUrl.FromString(strBaseDir))
      return false;

    if (!GetMoviesByWhere(strBaseDir, filter, items, SortDescription(), VideoDbDetailsNone, optionalQueries))
      return false;

    CFileItemList sets;
    GroupAttribute groupingAttributes = ignoreSingleMovieSets ? GroupAttributeIgnoreSingleItems : GroupAttributeNone;
    if (!GroupUtils::Group(GroupBySet, strBaseDir, items, sets, groupingAttributes))
      return false;

    items.ClearItems();
    items.Append(sets);

    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetMusicVideoAlbumsNav(const std::string& strBaseDir, CFileItemList& items, int idArtist /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    CVideoDbUrl videoUrl;
    if (!videoUrl.FromString(strBaseDir))
      return false;

    std::string strSQL = "select %s from musicvideo_view ";
    Filter extFilter = filter;
    extFilter.fields = PrepareSQL("musicvideo_view.c%02d, musicvideo_view.idMVideo, actor.name", VIDEODB_ID_MUSICVIDEO_ALBUM);
    extFilter.AppendJoin(PrepareSQL("JOIN actor_link ON actor_link.media_id=musicvideo_view.idMVideo AND actor_link.media_type='musicvideo'"));
    extFilter.AppendJoin(PrepareSQL("JOIN actor ON actor.actor_id = actor_link.actor_id"));
    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
    {
      extFilter.fields += ", path.strPath";
      extFilter.AppendJoin("join files on files.idFile = musicvideo_view.idFile join path on path.idPath = files.idPath");
    }

    if (idArtist > -1)
      videoUrl.AddOption("artistid", idArtist);

    extFilter.AppendGroup(PrepareSQL("musicvideo_view.c%02d", VIDEODB_ID_MUSICVIDEO_ALBUM));

    if (countOnly)
    {
      extFilter.fields = "COUNT(1)";
      extFilter.group.clear();
      extFilter.order.clear();
    }
    strSQL = StringUtils::Format(strSQL.c_str(), !extFilter.fields.empty() ? extFilter.fields.c_str() : "*");

    if (!BuildSQL(videoUrl.ToString(), strSQL, extFilter, strSQL, videoUrl))
      return false;

    int iRowsFound = RunQuery(strSQL);
    if (iRowsFound <= 0)
      return iRowsFound == 0;

    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", iRowsFound == 1 ? m_pDS->fv(0).get_asInt() : iRowsFound);
      items.Add(pItem);

      m_pDS->close();
      return true;
    }

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
    {
      std::map<int, std::pair<std::string,std::string> > mapAlbums;
      while (!m_pDS->eof())
      {
        int lidMVideo = m_pDS->fv(1).get_asInt();
        std::string strAlbum = m_pDS->fv(0).get_asString();
        auto it = mapAlbums.find(lidMVideo);
        // was this genre already found?
        if (it == mapAlbums.end())
        {
          // check path
          if (g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
            mapAlbums.insert(make_pair(lidMVideo, make_pair(strAlbum,m_pDS->fv(2).get_asString())));
        }
        m_pDS->next();
      }
      m_pDS->close();

      for (const auto &i : mapAlbums)
      {
        if (!i.second.first.empty())
        {
          CFileItemPtr pItem(new CFileItem(i.second.first));

          CVideoDbUrl itemUrl = videoUrl;
          std::string path = StringUtils::Format("%i/", i.first);
          itemUrl.AppendPath(path);
          pItem->SetPath(itemUrl.ToString());

          pItem->m_bIsFolder=true;
          pItem->SetLabelPreformated(true);
          if (!items.Contains(pItem->GetPath()))
          {
            pItem->GetVideoInfoTag()->m_artist.push_back(i.second.second);
            items.Add(pItem);
          }
        }
      }
    }
    else
    {
      while (!m_pDS->eof())
      {
        if (!m_pDS->fv(0).get_asString().empty())
        {
          CFileItemPtr pItem(new CFileItem(m_pDS->fv(0).get_asString()));

          CVideoDbUrl itemUrl = videoUrl;
          std::string path = StringUtils::Format("%i/", m_pDS->fv(1).get_asInt());
          itemUrl.AppendPath(path);
          pItem->SetPath(itemUrl.ToString());

          pItem->m_bIsFolder=true;
          pItem->SetLabelPreformated(true);
          if (!items.Contains(pItem->GetPath()))
          {
            pItem->GetVideoInfoTag()->m_artist.emplace_back(m_pDS->fv(2).get_asString());
            items.Add(pItem);
          }
        }
        m_pDS->next();
      }
      m_pDS->close();
    }

//    CLog::Log(LOGDEBUG, __FUNCTION__" Time: %d ms", XbmcThreads::SystemClockMillis() - time);
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetWritersNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  return GetPeopleNav(strBaseDir, items, "writer", idContent, filter, countOnly);
}

bool CVideoDatabase::GetDirectorsNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, std::pair<std::string,int> > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<ODBView_Movie_Director> res(m_cdb.getDB()->query<ODBView_Movie_Director>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<ODBView_Movie_Director>::iterator i = res.begin(); i != res.end(); i++)
      {
        int id = i->person->m_idPerson;
        std::string str = i->person->m_name;
        int playCount = i->m_playCount; //TODO: Figure out where / why this is needed and if this value is correct
        
        // was this already found?
        auto it = mapItems.find(id);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, std::pair<std::string,int> >(id, std::pair<std::string, int>(str,playCount)));
        }
      }
    }
    
    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", static_cast<int>(mapItems.size()));
      items.Add(pItem);
      
      m_pDS->close();
      return true;
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.first));
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "director";
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder = true;
      if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_playCount = i.second.second;
      pItem->SetLabelPreformated(true);
      items.Add(pItem);
    }
    
    // set thumbs - ideally this should be in the normal thumb setting routines
    /*for (int i = 0; i < items.Size() && !countOnly; i++)
    {
      CFileItemPtr pItem = items[i];
      if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->SetIconImage("DefaultArtist.png");
      else
        pItem->SetIconImage("DefaultActor.png");
    }*/
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
}

bool CVideoDatabase::GetActorsNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, CActor > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<ODBView_Movie_Actor> res(m_cdb.getDB()->query<ODBView_Movie_Actor>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<ODBView_Movie_Actor>::iterator i = res.begin(); i != res.end(); i++)
      {
        int idActor = i->person->m_idPerson;
        CActor actor;
        actor.name = i->person->m_name;
        if (i->person->m_art.load())
          actor.thumb = i->person->m_art->m_url;
        else
          actor.thumb = "";
        
        if (idContent != VIDEODB_CONTENT_TVSHOWS)
        {
          actor.playcount = i->m_playCount;
          actor.appearances = 1;
        }
        else actor.appearances = 1; //TODO: Need to be added
        
        // was this already found?
        auto it = mapItems.find(idActor);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, CActor>(idActor, actor));
        }
      }
    }
    
    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", static_cast<int>(mapItems.size()));
      items.Add(pItem);
      
      m_pDS->close();
      return true;
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.name));
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder=true;
      pItem->GetVideoInfoTag()->m_playCount = i.second.playcount;
      pItem->GetVideoInfoTag()->m_strPictureURL.ParseString("<thumb>"+i.second.thumb+"</thumb>");
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "actor";
      pItem->GetVideoInfoTag()->m_relevance = i.second.appearances;
      if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_artist.emplace_back(pItem->GetLabel());
      items.Add(pItem);
    }
    
    // set thumbs - ideally this should be in the normal thumb setting routines
    for (int i = 0; i < items.Size() && !countOnly; i++)
    {
      CFileItemPtr pItem = items[i];
      if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->SetIconImage("DefaultArtist.png");
      else
        pItem->SetIconImage("DefaultActor.png");
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
}

bool CVideoDatabase::GetPeopleNav(const std::string& strBaseDir, CFileItemList& items, const char *type, int idContent /* = -1 */, const Filter &filter /* = Filter() */, bool countOnly /* = false */)
{
  if (NULL == m_pDB.get()) return false;
  if (NULL == m_pDS.get()) return false;

  try
  {
    //! @todo This routine (and probably others at this same level) use playcount as a reference to filter on at a later
    //!       point.  This means that we *MUST* filter these levels as you'll get double ups.  Ideally we'd allow playcount
    //!       to filter through as we normally do for tvshows to save this happening.
    //!       Also, we apply this same filtering logic to the locked or unlocked paths to prevent these from showing.
    //!       Whether or not this should happen is a tricky one - it complicates all the high level categories (everything
    //!       above titles).

    // General routine that the other actor/director/writer routines call

    // get primary genres for movies
    std::string strSQL;
    Filter extFilter = filter;
    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
    {
      std::string view, view_id, media_type, extraField, group;
      if (idContent == VIDEODB_CONTENT_MOVIES)
      {
        view       = MediaTypeMovie;
        view_id    = "idMovie";
        media_type = MediaTypeMovie;
        extraField = "files.playCount";
      }
      else if (idContent == VIDEODB_CONTENT_TVSHOWS)
      {
        view       = MediaTypeEpisode;
        view_id    = "idShow";
        media_type = MediaTypeTvShow;
        extraField = "count(DISTINCT idShow)";
        group = "actor.actor_id";
      }
      else if (idContent == VIDEODB_CONTENT_EPISODES)
      {
        view       = MediaTypeEpisode;
        view_id    = "idEpisode";
        media_type = MediaTypeEpisode;
        extraField = "files.playCount";
      }
      else if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
      {
        view       = MediaTypeMusicVideo;
        view_id    = "idMVideo";
        media_type = MediaTypeMusicVideo;
        extraField = "files.playCount";
      }
      else
        return false;

      strSQL = "SELECT %s FROM actor ";
      extFilter.fields = "actor.actor_id, actor.name, actor.art_urls, path.strPath";
      extFilter.AppendField(extraField);
      extFilter.AppendJoin(PrepareSQL("JOIN %s_link ON actor.actor_id = %s_link.actor_id", type, type));
      extFilter.AppendJoin(PrepareSQL("JOIN %s_view ON %s_link.media_id = %s_view.%s AND %s_link.media_type='%s'", view.c_str(), type, view.c_str(), view_id.c_str(), type, media_type.c_str()));
      extFilter.AppendJoin(PrepareSQL("JOIN files ON files.idFile = %s_view.idFile", view.c_str()));
      extFilter.AppendJoin("JOIN path ON path.idPath = files.idPath");
      extFilter.AppendGroup(group);
    }
    else
    {
      std::string view, view_id, media_type, extraField, extraJoin;
      if (idContent == VIDEODB_CONTENT_MOVIES)
      {
        view       = MediaTypeMovie;
        view_id    = "idMovie";
        media_type = MediaTypeMovie;
        extraField = "count(1), count(files.playCount)";
        extraJoin  = PrepareSQL(" JOIN files ON files.idFile=%s_view.idFile", view.c_str());
      }
      else if (idContent == VIDEODB_CONTENT_TVSHOWS)
      {
        view       = MediaTypeTvShow;
        view_id    = "idShow";
        media_type = MediaTypeTvShow;
        extraField = "count(idShow)";
      }
      else if (idContent == VIDEODB_CONTENT_EPISODES)
      {
        view       = MediaTypeEpisode;
        view_id    = "idEpisode";
        media_type = MediaTypeEpisode;
        extraField = "count(1), count(files.playCount)";
        extraJoin  = PrepareSQL("JOIN files ON files.idFile = %s_view.idFile", view.c_str());
      }
      else if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
      {
        view       = MediaTypeMusicVideo;
        view_id    = "idMVideo";
        media_type = MediaTypeMusicVideo;
        extraField = "count(1), count(files.playCount)";
        extraJoin  = PrepareSQL("JOIN files ON files.idFile = %s_view.idFile", view.c_str());
      }
      else
        return false;

      strSQL ="SELECT %s FROM actor ";
      extFilter.fields = "actor.actor_id, actor.name, actor.art_urls";
      extFilter.AppendField(extraField);
      extFilter.AppendJoin(PrepareSQL("JOIN %s_link on actor.actor_id = %s_link.actor_id", type, type));
      extFilter.AppendJoin(PrepareSQL("JOIN %s_view on %s_link.media_id = %s_view.%s AND %s_link.media_type='%s'", view.c_str(), type, view.c_str(), view_id.c_str(), type, media_type.c_str()));
      extFilter.AppendJoin(extraJoin);
      extFilter.AppendGroup("actor.actor_id");
    }

    if (countOnly)
    {
      extFilter.fields = "COUNT(1)";
      extFilter.group.clear();
      extFilter.order.clear();
    }
    strSQL = StringUtils::Format(strSQL.c_str(), !extFilter.fields.empty() ? extFilter.fields.c_str() : "*");

    CVideoDbUrl videoUrl;
    if (!BuildSQL(strBaseDir, strSQL, extFilter, strSQL, videoUrl))
      return false;

    // run query
    unsigned int time = XbmcThreads::SystemClockMillis();
    if (!m_pDS->query(strSQL)) return false;
    CLog::Log(LOGDEBUG, "%s -  query took %i ms",
              __FUNCTION__, XbmcThreads::SystemClockMillis() - time); time = XbmcThreads::SystemClockMillis();
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound == 0)
    {
      m_pDS->close();
      return true;
    }

    if (countOnly)
    {
      CFileItemPtr pItem(new CFileItem());
      pItem->SetProperty("total", iRowsFound == 1 ? m_pDS->fv(0).get_asInt() : iRowsFound);
      items.Add(pItem);

      m_pDS->close();
      return true;
    }

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
    {
      std::map<int, CActor> mapActors;

      while (!m_pDS->eof())
      {
        int idActor = m_pDS->fv(0).get_asInt();
        CActor actor;
        actor.name = m_pDS->fv(1).get_asString();
        actor.thumb = m_pDS->fv(2).get_asString();
        if (idContent != VIDEODB_CONTENT_TVSHOWS)
        {
          actor.playcount = m_pDS->fv(3).get_asInt();
          actor.appearances = 1;
        }
        else actor.appearances = m_pDS->fv(4).get_asInt();
        auto it = mapActors.find(idActor);
        // is this actor already known?
        if (it == mapActors.end())
        {
          // check path
          if (g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
            mapActors.insert(std::pair<int, CActor>(idActor, actor));
        }
        else if (idContent != VIDEODB_CONTENT_TVSHOWS)
            it->second.appearances++;
        m_pDS->next();
      }
      m_pDS->close();

      for (const auto &i : mapActors)
      {
        CFileItemPtr pItem(new CFileItem(i.second.name));

        CVideoDbUrl itemUrl = videoUrl;
        std::string path = StringUtils::Format("%i/", i.first);
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());

        pItem->m_bIsFolder=true;
        pItem->GetVideoInfoTag()->m_playCount = i.second.playcount;
        pItem->GetVideoInfoTag()->m_strPictureURL.ParseString(i.second.thumb);
        pItem->GetVideoInfoTag()->m_iDbId = i.first;
        pItem->GetVideoInfoTag()->m_type = type;
        pItem->GetVideoInfoTag()->m_relevance = i.second.appearances;
        items.Add(pItem);
      }
    }
    else
    {
      while (!m_pDS->eof())
      {
        try
        {
          CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));

          CVideoDbUrl itemUrl = videoUrl;
          std::string path = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
          itemUrl.AppendPath(path);
          pItem->SetPath(itemUrl.ToString());

          pItem->m_bIsFolder=true;
          pItem->GetVideoInfoTag()->m_strPictureURL.ParseString(m_pDS->fv(2).get_asString());
          pItem->GetVideoInfoTag()->m_iDbId = m_pDS->fv(0).get_asInt();
          pItem->GetVideoInfoTag()->m_type = type;
          if (idContent != VIDEODB_CONTENT_TVSHOWS)
          {
            // fv(4) is the number of videos watched, fv(3) is the total number.  We set the playcount
            // only if the number of videos watched is equal to the total number (i.e. every video watched)
            pItem->GetVideoInfoTag()->m_playCount = (m_pDS->fv(4).get_asInt() == m_pDS->fv(3).get_asInt()) ? 1 : 0;
          }
          pItem->GetVideoInfoTag()->m_relevance = m_pDS->fv(3).get_asInt();
          if (idContent == VIDEODB_CONTENT_MUSICVIDEOS)
            pItem->GetVideoInfoTag()->m_artist.emplace_back(pItem->GetLabel());
          items.Add(pItem);
          m_pDS->next();
        }
        catch (...)
        {
          m_pDS->close();
          CLog::Log(LOGERROR, "%s: out of memory - retrieved %i items", __FUNCTION__, items.Size());
          return items.Size() > 0;
        }
      }
      m_pDS->close();
    }
    CLog::Log(LOGDEBUG, "%s item retrieval took %i ms",
              __FUNCTION__, XbmcThreads::SystemClockMillis() - time); time = XbmcThreads::SystemClockMillis();

    return true;
  }
  catch (...)
  {
    m_pDS->close();
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetYearsNav(const std::string& strBaseDir, CFileItemList& items, int idContent /* = -1 */, const Filter &filter /* = Filter() */)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    std::map<int, std::pair<std::string,int> > mapItems;
    
    //TODO: Implement the filter for odb
    if (idContent == VIDEODB_CONTENT_MOVIES)
    {
      odb::result<CODBMovie> res(m_cdb.getDB()->query<CODBMovie>()); //TODO: Just returns all now, filter needs to be added
      for (odb::result<CODBMovie>::iterator i = res.begin(); i != res.end(); i++)
      {
        m_cdb.getDB()->load(*i, i->section_foreign);
        if (!i->m_file.load() || !i->m_file->m_path.load())
          continue;
        
        int lYear = i->m_premiered.m_year;
        
        int playCount = i->m_file->m_playCount; //TODO: Figure out where / why this is needed and if this value is correct
        
        // was this already found?
        auto it = mapItems.find(lYear);
        if (it == mapItems.end())
        {
          // check path
          if ( (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser) &&
              !g_passwordManager.IsDatabasePathUnlocked(std::string(i->m_file->m_path->m_path),*CMediaSourceSettings::GetInstance().GetSources("video")))
          {
            continue;
          }
          mapItems.insert(std::pair<int, std::pair<std::string,int> >(lYear, std::pair<std::string, int>(std::to_string(lYear), playCount)));
        }
        else
        {
          it->second.second += playCount;
        }
      }
    }
    
    CVideoDbUrl videoUrl;
    videoUrl.Reset();
    videoUrl.FromString(strBaseDir);
    
    for (const auto &i : mapItems)
    {
      CFileItemPtr pItem(new CFileItem(i.second.first));
      pItem->GetVideoInfoTag()->m_iDbId = i.first;
      pItem->GetVideoInfoTag()->m_type = "genre";
      
      CVideoDbUrl itemUrl = videoUrl;
      std::string path = StringUtils::Format("%i/", i.first);
      itemUrl.AppendPath(path);
      pItem->SetPath(itemUrl.ToString());
      
      pItem->m_bIsFolder = true;
      if (idContent == VIDEODB_CONTENT_MOVIES || idContent == VIDEODB_CONTENT_MUSICVIDEOS)
        pItem->GetVideoInfoTag()->m_playCount = i.second.second;
      
      //TODO:
      // fv(4) is the number of videos watched, fv(3) is the total number.  We set the playcount
      // only if the number of videos watched is equal to the total number (i.e. every video watched)
      
      pItem->SetLabelPreformated(true);
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return false;
}

bool CVideoDatabase::GetSeasonsNav(const std::string& strBaseDir, CFileItemList& items, int idActor, int idDirector, int idGenre, int idYear, int idShow, bool getLinkedMovies /* = true */)
{
  // parse the base path to get additional filters
  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strBaseDir))
    return false;

  if (idShow != -1)
    videoUrl.AddOption("tvshowid", idShow);
  if (idActor != -1)
    videoUrl.AddOption("actorid", idActor);
  else if (idDirector != -1)
    videoUrl.AddOption("directorid", idDirector);
  else if (idGenre != -1)
    videoUrl.AddOption("genreid", idGenre);
  else if (idYear != -1)
    videoUrl.AddOption("year", idYear);

  if (!GetSeasonsByWhere(videoUrl.ToString(), Filter(), items, false))
    return false;

  // now add any linked movies
  if (getLinkedMovies && idShow != -1)
  {
    Filter movieFilter;
    movieFilter.join = PrepareSQL("join movielinktvshow on movielinktvshow.idMovie=movie_view.idMovie");
    movieFilter.where = PrepareSQL("movielinktvshow.idShow = %i", idShow);
    CFileItemList movieItems;
    GetMoviesByWhere("videodb://movies/titles/", movieFilter, movieItems);

    if (movieItems.Size() > 0)
      items.Append(movieItems);
  }

  return true;
}

bool CVideoDatabase::GetSeasonsByWhere(const std::string& strBaseDir, const Filter &filter, CFileItemList& items, bool appendFullShowPath /* = true */, const SortDescription &sortDescription /* = SortDescription() */)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    int total = -1;

    std::string strSQL = "SELECT %s FROM season_view ";
    CVideoDbUrl videoUrl;
    std::string strSQLExtra;
    Filter extFilter = filter;
    SortDescription sorting = sortDescription;
    if (!BuildSQL(strBaseDir, strSQLExtra, extFilter, strSQLExtra, videoUrl, sorting))
      return false;

    // Apply the limiting directly here if there's no special sorting but limiting
    if (extFilter.limit.empty() &&
      sorting.sortBy == SortByNone &&
      (sorting.limitStart > 0 || sorting.limitEnd > 0))
    {
      total = (int)strtol(GetSingleValue(PrepareSQL(strSQL, "COUNT(1)") + strSQLExtra, m_pDS).c_str(), NULL, 10);
      strSQLExtra += DatabaseUtils::BuildLimitClause(sorting.limitEnd, sorting.limitStart);
    }

    strSQL = PrepareSQL(strSQL, !extFilter.fields.empty() ? extFilter.fields.c_str() : "*") + strSQLExtra;

    int iRowsFound = RunQuery(strSQL);
    if (iRowsFound <= 0)
      return iRowsFound == 0;

    // store the total value of items as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);

    std::set<std::pair<int, int>> mapSeasons;
    while (!m_pDS->eof())
    {
      int id = m_pDS->fv(VIDEODB_ID_SEASON_ID).get_asInt();
      int showId = m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_ID).get_asInt();
      int iSeason = m_pDS->fv(VIDEODB_ID_SEASON_NUMBER).get_asInt();
      std::string name = m_pDS->fv(VIDEODB_ID_SEASON_NAME).get_asString();
      std::string path = m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_PATH).get_asString();

      if (mapSeasons.find(std::make_pair(showId, iSeason)) == mapSeasons.end() &&
         (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() == LOCK_MODE_EVERYONE || g_passwordManager.bMasterUser ||
          g_passwordManager.IsDatabasePathUnlocked(path, *CMediaSourceSettings::GetInstance().GetSources("video"))))
      {
        mapSeasons.insert(std::make_pair(showId, iSeason));

        std::string strLabel = name;
        if (strLabel.empty())
        {
          if (iSeason == 0)
            strLabel = g_localizeStrings.Get(20381);
          else
            strLabel = StringUtils::Format(g_localizeStrings.Get(20358).c_str(), iSeason);
        }
        CFileItemPtr pItem(new CFileItem(strLabel));

        CVideoDbUrl itemUrl = videoUrl;
        std::string strDir;
        if (appendFullShowPath)
          strDir += StringUtils::Format("%d/", showId);
        strDir += StringUtils::Format("%d/", iSeason);
        itemUrl.AppendPath(strDir);
        pItem->SetPath(itemUrl.ToString());

        pItem->m_bIsFolder = true;
        pItem->GetVideoInfoTag()->m_strTitle = strLabel;
        if (!name.empty())
          pItem->GetVideoInfoTag()->m_strSortTitle = name;
        pItem->GetVideoInfoTag()->m_iSeason = iSeason;
        pItem->GetVideoInfoTag()->m_iDbId = id;
        pItem->GetVideoInfoTag()->m_iIdSeason = id;
        pItem->GetVideoInfoTag()->m_type = MediaTypeSeason;
        pItem->GetVideoInfoTag()->m_strPath = path;
        pItem->GetVideoInfoTag()->m_strShowTitle = m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_TITLE).get_asString();
        pItem->GetVideoInfoTag()->m_strPlot = m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_PLOT).get_asString();
        pItem->GetVideoInfoTag()->SetPremieredFromDBDate(m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_PREMIERED).get_asString());
        pItem->GetVideoInfoTag()->m_firstAired.SetFromDBDate(m_pDS->fv(VIDEODB_ID_SEASON_PREMIERED).get_asString());
        pItem->GetVideoInfoTag()->m_iUserRating = m_pDS->fv(VIDEODB_ID_SEASON_USER_RATING).get_asInt();
        // season premiered date based on first episode airdate associated to the season		
        // tvshow premiered date is used as a fallback		
        if (pItem->GetVideoInfoTag()->m_firstAired.IsValid())
          pItem->GetVideoInfoTag()->SetPremiered(pItem->GetVideoInfoTag()->m_firstAired);
        else if (pItem->GetVideoInfoTag()->HasPremiered())
          pItem->GetVideoInfoTag()->SetPremiered(pItem->GetVideoInfoTag()->GetPremiered());
        else if (pItem->GetVideoInfoTag()->HasYear())
          pItem->GetVideoInfoTag()->SetYear(pItem->GetVideoInfoTag()->GetYear());
        pItem->GetVideoInfoTag()->m_genre = StringUtils::Split(m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_GENRE).get_asString(), g_advancedSettings.m_videoItemSeparator);
        pItem->GetVideoInfoTag()->m_studio = StringUtils::Split(m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_STUDIO).get_asString(), g_advancedSettings.m_videoItemSeparator);
        pItem->GetVideoInfoTag()->m_strMPAARating = m_pDS->fv(VIDEODB_ID_SEASON_TVSHOW_MPAA).get_asString();
        pItem->GetVideoInfoTag()->m_iIdShow = showId;

        int totalEpisodes = m_pDS->fv(VIDEODB_ID_SEASON_EPISODES_TOTAL).get_asInt();
        int watchedEpisodes = m_pDS->fv(VIDEODB_ID_SEASON_EPISODES_WATCHED).get_asInt();
        pItem->GetVideoInfoTag()->m_iEpisode = totalEpisodes;
        pItem->SetProperty("totalepisodes", totalEpisodes);
        pItem->SetProperty("numepisodes", totalEpisodes); // will be changed later to reflect watchmode setting
        pItem->SetProperty("watchedepisodes", watchedEpisodes);
        pItem->SetProperty("unwatchedepisodes", totalEpisodes - watchedEpisodes);
        if (iSeason == 0)
          pItem->SetProperty("isspecial", true);
        pItem->GetVideoInfoTag()->m_playCount = (totalEpisodes == watchedEpisodes) ? 1 : 0;
        pItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, (pItem->GetVideoInfoTag()->m_playCount > 0) && (pItem->GetVideoInfoTag()->m_iEpisode > 0));

        items.Add(pItem);
      }

      m_pDS->next();
    }
    m_pDS->close();

    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetSortedVideos(const MediaType &mediaType, const std::string& strBaseDir, const SortDescription &sortDescription, CFileItemList& items, const Filter &filter /* = Filter() */)
{
  if (NULL == m_pDB.get() || NULL == m_pDS.get())
    return false;

  if (mediaType != MediaTypeMovie && mediaType != MediaTypeTvShow && mediaType != MediaTypeEpisode && mediaType != MediaTypeMusicVideo)
    return false;

  SortDescription sorting = sortDescription;
  if (sortDescription.sortBy == SortByFile ||
      sortDescription.sortBy == SortByTitle ||
      sortDescription.sortBy == SortBySortTitle ||
      sortDescription.sortBy == SortByLabel ||
      sortDescription.sortBy == SortByDateAdded ||
      sortDescription.sortBy == SortByRating ||
      sortDescription.sortBy == SortByUserRating ||
      sortDescription.sortBy == SortByYear ||
      sortDescription.sortBy == SortByLastPlayed ||
      sortDescription.sortBy == SortByPlaycount)
    sorting.sortAttributes = (SortAttribute)(sortDescription.sortAttributes | SortAttributeIgnoreFolders);

  bool success = false;
  if (mediaType == MediaTypeMovie)
    success = GetMoviesByWhere(strBaseDir, filter, items, sorting);
  else if (mediaType == MediaTypeTvShow)
    success = GetTvShowsByWhere(strBaseDir, filter, items, sorting);
  else if (mediaType == MediaTypeEpisode)
    success = GetEpisodesByWhere(strBaseDir, filter, items, true, sorting);
  else if (mediaType == MediaTypeMusicVideo)
    success = GetMusicVideosByWhere(strBaseDir, filter, items, true, sorting);
  else
    return false;

  items.SetContent(CMediaTypes::ToPlural(mediaType));
  return success;
}

bool CVideoDatabase::GetItems(const std::string &strBaseDir, CFileItemList &items, const Filter &filter /* = Filter() */, const SortDescription &sortDescription /* = SortDescription() */)
{
  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strBaseDir))
    return false;

  return GetItems(strBaseDir, videoUrl.GetType(), videoUrl.GetItemType(), items, filter, sortDescription);
}

bool CVideoDatabase::GetItems(const std::string &strBaseDir, const std::string &mediaType, const std::string &itemType, CFileItemList &items, const Filter &filter /* = Filter() */, const SortDescription &sortDescription /* = SortDescription() */)
{
  VIDEODB_CONTENT_TYPE contentType;
  if (StringUtils::EqualsNoCase(mediaType, "movies"))
    contentType = VIDEODB_CONTENT_MOVIES;
  else if (StringUtils::EqualsNoCase(mediaType, "tvshows"))
  {
    if (StringUtils::EqualsNoCase(itemType, "episodes"))
      contentType = VIDEODB_CONTENT_EPISODES;
    else
      contentType = VIDEODB_CONTENT_TVSHOWS;
  }
  else if (StringUtils::EqualsNoCase(mediaType, "musicvideos"))
    contentType = VIDEODB_CONTENT_MUSICVIDEOS;
  else
    return false;

  return GetItems(strBaseDir, contentType, itemType, items, filter, sortDescription);
}

bool CVideoDatabase::GetItems(const std::string &strBaseDir, VIDEODB_CONTENT_TYPE mediaType, const std::string &itemType, CFileItemList &items, const Filter &filter /* = Filter() */, const SortDescription &sortDescription /* = SortDescription() */)
{
  if (StringUtils::EqualsNoCase(itemType, "movies") && (mediaType == VIDEODB_CONTENT_MOVIES || mediaType == VIDEODB_CONTENT_MOVIE_SETS))
    return GetMoviesByWhere(strBaseDir, filter, items, sortDescription);
  else if (StringUtils::EqualsNoCase(itemType, "tvshows") && mediaType == VIDEODB_CONTENT_TVSHOWS)
    return GetTvShowsByWhere(strBaseDir, filter, items, sortDescription);
  else if (StringUtils::EqualsNoCase(itemType, "musicvideos") && mediaType == VIDEODB_CONTENT_MUSICVIDEOS)
    return GetMusicVideosByWhere(strBaseDir, filter, items, true, sortDescription);
  else if (StringUtils::EqualsNoCase(itemType, "episodes") && mediaType == VIDEODB_CONTENT_EPISODES)
    return GetEpisodesByWhere(strBaseDir, filter, items, true, sortDescription);
  else if (StringUtils::EqualsNoCase(itemType, "seasons") && mediaType == VIDEODB_CONTENT_TVSHOWS)
    return GetSeasonsNav(strBaseDir, items);
  else if (StringUtils::EqualsNoCase(itemType, "genres"))
    return GetGenresNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "years"))
    return GetYearsNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "actors"))
    return GetActorsNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "directors"))
    return GetDirectorsNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "writers"))
    return GetWritersNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "studios"))
    return GetStudiosNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "sets"))
    return GetSetsNav(strBaseDir, items, mediaType, filter, !CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOLIBRARY_GROUPSINGLEITEMSETS));
  else if (StringUtils::EqualsNoCase(itemType, "countries"))
    return GetCountriesNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "tags"))
    return GetTagsNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "artists") && mediaType == VIDEODB_CONTENT_MUSICVIDEOS)
    return GetActorsNav(strBaseDir, items, mediaType, filter);
  else if (StringUtils::EqualsNoCase(itemType, "albums") && mediaType == VIDEODB_CONTENT_MUSICVIDEOS)
    return GetMusicVideoAlbumsNav(strBaseDir, items, -1, filter);

  return false;
}

std::string CVideoDatabase::GetItemById(const std::string &itemType, int id)
{
  if (StringUtils::EqualsNoCase(itemType, "genres"))
    return GetGenreById(id);
  else if (StringUtils::EqualsNoCase(itemType, "years"))
    return StringUtils::Format("%d", id);
  else if (StringUtils::EqualsNoCase(itemType, "actors") || 
           StringUtils::EqualsNoCase(itemType, "directors") ||
           StringUtils::EqualsNoCase(itemType, "artists"))
    return GetPersonById(id);
  else if (StringUtils::EqualsNoCase(itemType, "studios"))
    return GetStudioById(id);
  else if (StringUtils::EqualsNoCase(itemType, "sets"))
    return GetSetById(id);
  else if (StringUtils::EqualsNoCase(itemType, "countries"))
    return GetCountryById(id);
  else if (StringUtils::EqualsNoCase(itemType, "tags"))
    return GetTagById(id);
  else if (StringUtils::EqualsNoCase(itemType, "albums"))
    return GetMusicVideoAlbumById(id);

  return "";
}

bool CVideoDatabase::GetMoviesNav(const std::string& strBaseDir, CFileItemList& items,
                                  int idGenre /* = -1 */, int idYear /* = -1 */, int idActor /* = -1 */, int idDirector /* = -1 */,
                                  int idStudio /* = -1 */, int idCountry /* = -1 */, int idSet /* = -1 */, int idTag /* = -1 */,
                                  const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strBaseDir))
    return false;

  if (idGenre > 0)
    videoUrl.AddOption("genreid", idGenre);
  else if (idCountry > 0)
    videoUrl.AddOption("countryid", idCountry);
  else if (idStudio > 0)
    videoUrl.AddOption("studioid", idStudio);
  else if (idDirector > 0)
    videoUrl.AddOption("directorid", idDirector);
  else if (idYear > 0)
    videoUrl.AddOption("year", idYear);
  else if (idActor > 0)
    videoUrl.AddOption("actorid", idActor);
  else if (idSet > 0)
    videoUrl.AddOption("setid", idSet);
  else if (idTag > 0)
    videoUrl.AddOption("tagid", idTag);

  Filter filter;
  return GetMoviesByWhere(videoUrl.ToString(), filter, items, sortDescription, getDetails);
}

bool CVideoDatabase::GetMoviesByWhere(const std::string& strBaseDir, const Filter &filter, CFileItemList& items, const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */, odb::query<ODBView_Movie> optionalQueries /* = empty */)
{
  try
  {
    movieTime = 0;
    castTime = 0;

    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    // parse the base path to get additional filters
    CVideoDbUrl videoUrl;
    Filter extFilter = filter;
    SortDescription sorting = sortDescription;
    if (!videoUrl.FromString(strBaseDir) || !videoUrl.IsValid())
      return false;
    
    std::string type = videoUrl.GetType();
    std::string itemType = ((const CVideoDbUrl &)videoUrl).GetItemType();
    const CUrlOptions::UrlOptions& options = videoUrl.GetOptions();
    
    typedef odb::query<ODBView_Movie> query;
    query movie_query = optionalQueries;
    
    for (auto option: options)
    {
      if (option.first == "genreid")
        movie_query += query(query::genre::idGenre == option.second.asInteger());
      else if (option.first == "genre")
        movie_query += query(query::genre::name.like(option.second.asString()));
      else if (option.first == "countryid")
        movie_query += query(query::country::idCountry == option.second.asInteger());
      else if (option.first == "country")
        movie_query += query(query::country::name.like(option.second.asString()));
      else if (option.first == "studioid")
        movie_query += query(query::studio::idStudio == option.second.asInteger());
      else if (option.first == "studio")
        movie_query += query(query::studio::name.like(option.second.asString()));
      else if (option.first == "directorid")
        movie_query += query(query::director::idPerson == option.second.asInteger());
      else if (option.first == "director")
        movie_query += query(query::director::name.like(option.second.asString()));
      else if (option.first == "actorid")
        movie_query += query(query::actor::idPerson == option.second.asInteger());
      else if (option.first == "actor")
        movie_query += query(query::actor::name.like(option.second.asString()));
      else if (option.first == "setid")
        movie_query += query(query::set::idSet == option.second.asInteger());
      else if (option.first == "set")
        movie_query += query(query::set::name.like(option.second.asString()));
      else if (option.first == "year")
        movie_query += query(query::CODBMovie::premiered.year == option.second.asInteger());
      else if (option.first == "tagid")
        movie_query += query(query::tag::idTag == option.second.asInteger());
      else if (option.first == "tag")
        movie_query += query(query::tag::name.like(option.second.asString()));
      else if (option.first == "filter" || option.first == "xsp")
      {
        CSmartPlaylist xspFilter;
        if (!xspFilter.LoadFromJson(option.second.asString()))
          return false;
        
        // check if the filter playlist matches the item type
        if (xspFilter.GetType() == itemType)
        {
          std::set<std::string> playlists;
          movie_query += xspFilter.GetMovieWhereClause(playlists);
        }
        // remove the filter if it doesn't match the item type
        else
          videoUrl.RemoveOption(option.first);
      }
      CLog::Log(LOGDEBUG, "%s added filter for %s - %s", __FUNCTION__, option.first.c_str(), option.second.asString().c_str());
    }
    
    int total = 0;

    movie_query = movie_query + SortUtils::SortODBMovieQuery(sortDescription);
    
    odb::result<ODBView_Movie> res(m_cdb.getDB()->query<ODBView_Movie>(movie_query));
    for (odb::result<ODBView_Movie>::iterator i = res.begin(); i != res.end(); i++)
    {
      CVideoInfoTag movie = GetDetailsForMovie(i, getDetails);
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() == LOCK_MODE_EVERYONE ||
          g_passwordManager.bMasterUser ||
          g_passwordManager.IsDatabasePathUnlocked(movie.m_strPath, *CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        CFileItemPtr pItem(new CFileItem(movie));
        
        CVideoDbUrl itemUrl = videoUrl;
        std::string path = StringUtils::Format("%i", movie.m_iDbId);
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());
        
        pItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED,movie.m_playCount > 0);
        items.Add(pItem);
      }
      
      ++total;
    }
    
    //TODO: Random sorting needs to be implemented

    items.SetProperty("total", total);

    // cleanup
    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetTvShowsNav(const std::string& strBaseDir, CFileItemList& items,
                                  int idGenre /* = -1 */, int idYear /* = -1 */, int idActor /* = -1 */, int idDirector /* = -1 */, int idStudio /* = -1 */, int idTag /* = -1 */,
                                  const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strBaseDir))
    return false;

  if (idGenre != -1)
    videoUrl.AddOption("genreid", idGenre);
  else if (idStudio != -1)
    videoUrl.AddOption("studioid", idStudio);
  else if (idDirector != -1)
    videoUrl.AddOption("directorid", idDirector);
  else if (idYear != -1)
    videoUrl.AddOption("year", idYear);
  else if (idActor != -1)
    videoUrl.AddOption("actorid", idActor);
  else if (idTag != -1)
    videoUrl.AddOption("tagid", idTag);

  Filter filter;
  if (!CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOLIBRARY_SHOWEMPTYTVSHOWS))
    filter.AppendWhere("totalCount IS NOT NULL AND totalCount > 0");
  return GetTvShowsByWhere(videoUrl.ToString(), filter, items, sortDescription, getDetails);
}

bool CVideoDatabase::GetTvShowsByWhere(const std::string& strBaseDir, const Filter &filter, CFileItemList& items, const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  try
  {
    movieTime = 0;

    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    int total = -1;
    
    std::string strSQL = "SELECT %s FROM tvshow_view ";
    CVideoDbUrl videoUrl;
    std::string strSQLExtra;
    Filter extFilter = filter;
    SortDescription sorting = sortDescription;
    if (!BuildSQL(strBaseDir, strSQLExtra, extFilter, strSQLExtra, videoUrl, sorting))
      return false;

    // Apply the limiting directly here if there's no special sorting but limiting
    if (extFilter.limit.empty() &&
        sorting.sortBy == SortByNone &&
        (sorting.limitStart > 0 || sorting.limitEnd > 0))
    {
      total = (int)strtol(GetSingleValue(PrepareSQL(strSQL, "COUNT(1)") + strSQLExtra, m_pDS).c_str(), NULL, 10);
      strSQLExtra += DatabaseUtils::BuildLimitClause(sorting.limitEnd, sorting.limitStart);
    }

    strSQL = PrepareSQL(strSQL, !extFilter.fields.empty() ? extFilter.fields.c_str() : "*") + strSQLExtra;

    int iRowsFound = RunQuery(strSQL);
    if (iRowsFound <= 0)
      return iRowsFound == 0;

    // store the total value of items as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);
    
    DatabaseResults results;
    results.reserve(iRowsFound);
    if (!SortUtils::SortFromDataset(sorting, MediaTypeTvShow, m_pDS, results))
      return false;

    // get data from returned rows
    items.Reserve(results.size());
    const query_data &data = m_pDS->get_result_set().records;
    for (const auto &i : results)
    {
      unsigned int targetRow = (unsigned int)i.at(FieldRow).asInteger();
      const dbiplus::sql_record* const record = data.at(targetRow);
      
      CFileItemPtr pItem(new CFileItem());
      CVideoInfoTag movie = GetDetailsForTvShow(record, getDetails, pItem.get());
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() == LOCK_MODE_EVERYONE ||
           g_passwordManager.bMasterUser                                     ||
           g_passwordManager.IsDatabasePathUnlocked(movie.m_strPath, *CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        pItem->SetFromVideoInfoTag(movie);

        CVideoDbUrl itemUrl = videoUrl;
        std::string path = StringUtils::Format("%i/", record->at(0).get_asInt());
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());

        pItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, (pItem->GetVideoInfoTag()->m_playCount > 0) && (pItem->GetVideoInfoTag()->m_iEpisode > 0));
        items.Add(pItem);
      }
    }

    // cleanup
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetEpisodesNav(const std::string& strBaseDir, CFileItemList& items, int idGenre, int idYear, int idActor, int idDirector, int idShow, int idSeason, const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strBaseDir))
    return false;

  std::string strIn;
  if (idShow != -1)
  {
    videoUrl.AddOption("tvshowid", idShow);
    if (idSeason >= 0)
      videoUrl.AddOption("season", idSeason);

    if (idGenre != -1)
      videoUrl.AddOption("genreid", idGenre);
    else if (idYear !=-1)
      videoUrl.AddOption("year", idYear);
    else if (idActor != -1)
      videoUrl.AddOption("actorid", idActor);
  }
  else if (idYear != -1)
    videoUrl.AddOption("year", idYear);
  
  if (idDirector != -1)
    videoUrl.AddOption("directorid", idDirector);

  Filter filter;
  bool ret = GetEpisodesByWhere(videoUrl.ToString(), filter, items, false, sortDescription, getDetails);

  if (idSeason == -1 && idShow != -1)
  { // add any linked movies
    Filter movieFilter;
    movieFilter.join  = PrepareSQL("join movielinktvshow on movielinktvshow.idMovie=movie_view.idMovie");
    movieFilter.where = PrepareSQL("movielinktvshow.idShow = %i", idShow);
    CFileItemList movieItems;
    GetMoviesByWhere("videodb://movies/titles/", movieFilter, movieItems);

    if (movieItems.Size() > 0)
      items.Append(movieItems);
  }

  return ret;
}

bool CVideoDatabase::GetEpisodesByWhere(const std::string& strBaseDir, const Filter &filter, CFileItemList& items, bool appendFullShowPath /* = true */, const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  try
  {
    movieTime = 0;
    castTime = 0;

    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    int total = -1;
    
    std::string strSQL = "select %s from episode_view ";
    CVideoDbUrl videoUrl;
    std::string strSQLExtra;
    Filter extFilter = filter;
    SortDescription sorting = sortDescription;
    if (!BuildSQL(strBaseDir, strSQLExtra, extFilter, strSQLExtra, videoUrl, sorting))
      return false;

    // Apply the limiting directly here if there's no special sorting but limiting
    if (extFilter.limit.empty() &&
      sorting.sortBy == SortByNone &&
      (sorting.limitStart > 0 || sorting.limitEnd > 0))
    {
      total = (int)strtol(GetSingleValue(PrepareSQL(strSQL, "COUNT(1)") + strSQLExtra, m_pDS).c_str(), NULL, 10);
      strSQLExtra += DatabaseUtils::BuildLimitClause(sorting.limitEnd, sorting.limitStart);
    }

    strSQL = PrepareSQL(strSQL, !extFilter.fields.empty() ? extFilter.fields.c_str() : "*") + strSQLExtra;

    int iRowsFound = RunQuery(strSQL);
    if (iRowsFound <= 0)
      return iRowsFound == 0;

    // store the total value of items as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);
    
    DatabaseResults results;
    results.reserve(iRowsFound);
    if (!SortUtils::SortFromDataset(sorting, MediaTypeEpisode, m_pDS, results))
      return false;
    
    // get data from returned rows
    items.Reserve(results.size());
    CLabelFormatter formatter("%H. %T", "");

    const query_data &data = m_pDS->get_result_set().records;
    for (const auto &i : results)
    {
      unsigned int targetRow = (unsigned int)i.at(FieldRow).asInteger();
      const dbiplus::sql_record* const record = data.at(targetRow);

      CVideoInfoTag movie = GetDetailsForEpisode(record, getDetails);
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() == LOCK_MODE_EVERYONE ||
          g_passwordManager.bMasterUser                                     ||
          g_passwordManager.IsDatabasePathUnlocked(movie.m_strPath, *CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        CFileItemPtr pItem(new CFileItem(movie));
        formatter.FormatLabel(pItem.get());
      
        int idEpisode = record->at(0).get_asInt();

        CVideoDbUrl itemUrl = videoUrl;
        std::string path;
        if (appendFullShowPath && videoUrl.GetItemType() != "episodes")
          path = StringUtils::Format("%i/%i/%i", record->at(VIDEODB_DETAILS_EPISODE_TVSHOW_ID).get_asInt(), movie.m_iSeason, idEpisode);
        else
          path = StringUtils::Format("%i", idEpisode);
        itemUrl.AppendPath(path);
        pItem->SetPath(itemUrl.ToString());

        pItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, movie.m_playCount > 0);
        pItem->m_dateTime = movie.m_firstAired;
        items.Add(pItem);
      }
    }

    // cleanup
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

bool CVideoDatabase::GetMusicVideosNav(const std::string& strBaseDir, CFileItemList& items, int idGenre, int idYear, int idArtist, int idDirector, int idStudio, int idAlbum, int idTag /* = -1 */, const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  CVideoDbUrl videoUrl;
  if (!videoUrl.FromString(strBaseDir))
    return false;

  if (idGenre != -1)
    videoUrl.AddOption("genreid", idGenre);
  else if (idStudio != -1)
    videoUrl.AddOption("studioid", idStudio);
  else if (idDirector != -1)
    videoUrl.AddOption("directorid", idDirector);
  else if (idYear !=-1)
    videoUrl.AddOption("year", idYear);
  else if (idArtist != -1)
    videoUrl.AddOption("artistid", idArtist);
  else if (idTag != -1)
    videoUrl.AddOption("tagid", idTag);
  if (idAlbum != -1)
    videoUrl.AddOption("albumid", idAlbum);

  Filter filter;
  return GetMusicVideosByWhere(videoUrl.ToString(), filter, items, true, sortDescription, getDetails);
}

bool CVideoDatabase::GetRecentlyAddedMoviesNav(const std::string& strBaseDir, CFileItemList& items, unsigned int limit /* = 0 */, int getDetails /* = VideoDbDetailsNone */)
{
  Filter filter;
  filter.order = "dateAdded desc, idMovie desc";
  filter.limit = PrepareSQL("%u", limit ? limit : g_advancedSettings.m_iVideoLibraryRecentlyAddedItems);
  return GetMoviesByWhere(strBaseDir, filter, items, SortDescription(), getDetails);
}

bool CVideoDatabase::GetRecentlyAddedEpisodesNav(const std::string& strBaseDir, CFileItemList& items, unsigned int limit /* = 0 */, int getDetails /* = VideoDbDetailsNone */)
{
  Filter filter;
  filter.order = "dateAdded desc, idEpisode desc";
  filter.limit = PrepareSQL("%u", limit ? limit : g_advancedSettings.m_iVideoLibraryRecentlyAddedItems);
  return GetEpisodesByWhere(strBaseDir, filter, items, false, SortDescription(), getDetails);
}

bool CVideoDatabase::GetRecentlyAddedMusicVideosNav(const std::string& strBaseDir, CFileItemList& items, unsigned int limit /* = 0 */, int getDetails /* = VideoDbDetailsNone */)
{
  Filter filter;
  filter.order = "dateAdded desc, idMVideo desc";
  filter.limit = PrepareSQL("%u", limit ? limit : g_advancedSettings.m_iVideoLibraryRecentlyAddedItems);
  return GetMusicVideosByWhere(strBaseDir, filter, items, true, SortDescription(), getDetails);
}

bool CVideoDatabase::GetInProgressTvShowsNav(const std::string& strBaseDir, CFileItemList& items, unsigned int limit /* = 0 */, int getDetails /* = VideoDbDetailsNone */)
{
  Filter filter;
  filter.order = PrepareSQL("c%02d", VIDEODB_ID_TV_TITLE);
  filter.where = "watchedCount != 0 AND totalCount != watchedCount";
  return GetTvShowsByWhere(strBaseDir, filter, items, SortDescription(), getDetails);
}

std::string CVideoDatabase::GetGenreById(int id)
{
  std::string returnVal;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBGenre odbObj;
    if (m_cdb.getDB()->query_one<CODBGenre>(odb::query<CODBGenre>::idGenre == id, odbObj))
    {
      returnVal = odbObj.m_name;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return returnVal;
}

std::string CVideoDatabase::GetCountryById(int id)
{
  std::string returnVal;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBCountry odbObj;
    if (m_cdb.getDB()->query_one<CODBCountry>(odb::query<CODBCountry>::idCountry == id, odbObj))
    {
      returnVal = odbObj.m_name;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return returnVal;
}

std::string CVideoDatabase::GetSetById(int id)
{
  std::string returnVal;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBSet odbObj;
    if (m_cdb.getDB()->query_one<CODBSet>(odb::query<CODBSet>::idSet == id, odbObj))
    {
      returnVal = odbObj.m_name;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return returnVal;
}

std::string CVideoDatabase::GetTagById(int id)
{
  std::string returnVal;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBTag odbObj;
    if (m_cdb.getDB()->query_one<CODBTag>(odb::query<CODBTag>::idTag == id, odbObj))
    {
      returnVal = odbObj.m_name;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return returnVal;
}

std::string CVideoDatabase::GetPersonById(int id)
{
  std::string returnVal;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBPerson odbObj;
    if (m_cdb.getDB()->query_one<CODBPerson>(odb::query<CODBPerson>::idPerson == id, odbObj))
    {
      returnVal = odbObj.m_name;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return returnVal;
}

std::string CVideoDatabase::GetStudioById(int id)
{
  std::string returnVal;
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    CODBStudio odbObj;
    if (m_cdb.getDB()->query_one<CODBStudio>(odb::query<CODBStudio>::idStudio == id, odbObj))
    {
      returnVal = odbObj.m_name;
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  
  return returnVal;
}

std::string CVideoDatabase::GetTvShowTitleById(int id)
{
  return GetSingleValue("tvshow", PrepareSQL("c%02d", VIDEODB_ID_TV_TITLE), PrepareSQL("idShow=%i", id));
}

std::string CVideoDatabase::GetMusicVideoAlbumById(int id)
{
  return GetSingleValue("musicvideo", PrepareSQL("c%02d", VIDEODB_ID_MUSICVIDEO_ALBUM), PrepareSQL("idMVideo=%i", id));
}

bool CVideoDatabase::HasSets() const
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    m_pDS->query("SELECT movie_view.idSet,COUNT(1) AS c FROM movie_view "
                 "JOIN sets ON sets.idSet = movie_view.idSet "
                 "GROUP BY movie_view.idSet HAVING c>1");

    bool bResult = (m_pDS->num_rows() > 0);
    m_pDS->close();
    return bResult;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

int CVideoDatabase::GetTvShowForEpisode(int idEpisode)
{
  try
  {
    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS2.get()) return false;

    // make sure we use m_pDS2, as this is called in loops using m_pDS
    std::string strSQL=PrepareSQL("select idShow from episode where idEpisode=%i", idEpisode);
    m_pDS2->query( strSQL );

    int result=-1;
    if (!m_pDS2->eof())
      result=m_pDS2->fv(0).get_asInt();
    m_pDS2->close();

    return result;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i) failed", __FUNCTION__, idEpisode);
  }
  return false;
}

int CVideoDatabase::GetSeasonForEpisode(int idEpisode)
{
  char column[5];
  sprintf(column, "c%0d", VIDEODB_ID_EPISODE_SEASON);
  std::string id = GetSingleValue("episode", column, PrepareSQL("idEpisode=%i", idEpisode));
  if (id.empty())
    return -1;
  return atoi(id.c_str());
}

bool CVideoDatabase::HasContent()
{
  return (HasContent(VIDEODB_CONTENT_MOVIES) ||
          HasContent(VIDEODB_CONTENT_TVSHOWS) ||
          HasContent(VIDEODB_CONTENT_MUSICVIDEOS));
}

bool CVideoDatabase::HasContent(VIDEODB_CONTENT_TYPE type)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    if (type == VIDEODB_CONTENT_MOVIES)
    {
      ODBView_Movie_Count count(m_cdb.getDB()->query_value<ODBView_Movie_Count>());
      return count.count > 0;
    }
    //TODO: Needs to be done
    /*else if (type == VIDEODB_CONTENT_TVSHOWS)
      sql = "select count(1) from tvshow";
    else if (type == VIDEODB_CONTENT_MUSICVIDEOS)
      sql = "select count(1) from musicvideo";*/

  }
  catch (std::exception &e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

ScraperPtr CVideoDatabase::GetScraperForPath( const std::string& strPath )
{
  SScanSettings settings;
  return GetScraperForPath(strPath, settings);
}

ScraperPtr CVideoDatabase::GetScraperForPath(const std::string& strPath, SScanSettings& settings)
{
  bool dummy;
  return GetScraperForPath(strPath, settings, dummy);
}

ScraperPtr CVideoDatabase::GetScraperForPath(const std::string& strPath, SScanSettings& settings, bool& foundDirectly)
{
  foundDirectly = false;
  try
  {
    if (strPath.empty()) return ScraperPtr();

    ScraperPtr scraper;
    std::string strPath2;

    if (URIUtils::IsMultiPath(strPath))
      strPath2 = CMultiPathDirectory::GetFirstPath(strPath);
    else
      strPath2 = strPath;

    std::string strPath1 = URIUtils::GetDirectory(strPath2);
    int idPath = GetPathId(strPath1);

    /*if (idPath > -1)
    {
      std::string strSQL=PrepareSQL("select path.strContent,path.strScraper,path.scanRecursive,path.useFolderNames,path.strSettings,path.noUpdate,path.exclude from path where path.idPath=%i",idPath);
      m_pDS->query( strSQL );
    }*/

    int iFound = 1;
    CONTENT_TYPE content = CONTENT_NONE;
    
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    typedef odb::query<CODBPath> query;
    
    CODBPath path;
    if ( idPath > -1 && m_cdb.getDB()->query_one<CODBPath>(query::idPath == idPath ,path) )
    { // path is stored in db

      if (path.m_exclude)
      {
        settings.exclude = true;
        //m_pDS->close();
        return ScraperPtr();
      }
      settings.exclude = false;

      // try and ascertain scraper for this path
      std::string strcontent = path.m_content;
      StringUtils::ToLower(strcontent);
      content = TranslateContent(strcontent);

      //FIXME paths stored should not have empty strContent
      //assert(content != CONTENT_NONE);
      std::string scraperID = path.m_scraper;

      AddonPtr addon;
      if (!scraperID.empty() && CAddonMgr::GetInstance().GetAddon(scraperID, addon))
      {
        scraper = std::dynamic_pointer_cast<CScraper>(addon);
        if (!scraper)
          return ScraperPtr();

        // store this path's content & settings
        scraper->SetPathSettings(content, path.m_settings);
        settings.parent_name = path.m_useFolderNames;
        settings.recurse = path.m_scanRecursive;
        settings.noupdate = path.m_noUpdate;
      }
    }

    if (content == CONTENT_NONE)
    { // this path is not saved in db
      // we must drill up until a scraper is configured
      std::string strParent;
      while (URIUtils::GetParentPath(strPath1, strParent))
      {
        iFound++;

        /*std::string strSQL=PrepareSQL("select path.strContent,path.strScraper,path.scanRecursive,path.useFolderNames,path.strSettings,path.noUpdate, path.exclude from path where strPath='%s'",strParent.c_str());
        m_pDS->query(strSQL);*/

        CONTENT_TYPE content = CONTENT_NONE;
        if (m_cdb.getDB()->query_one<CODBPath>(query::path == strParent ,path))
        {
          std::string strcontent = path.m_content;
          StringUtils::ToLower(strcontent);
          if (path.m_exclude)
          {
            settings.exclude = true;
            scraper.reset();
            //m_pDS->close();
            break;
          }

          content = TranslateContent(strcontent);

          AddonPtr addon;
          if (content != CONTENT_NONE &&
              CAddonMgr::GetInstance().GetAddon(path.m_scraper, addon))
          {
            scraper = std::dynamic_pointer_cast<CScraper>(addon);
            scraper->SetPathSettings(content, path.m_settings);
            settings.parent_name = path.m_useFolderNames;
            settings.recurse = path.m_scanRecursive;
            settings.noupdate = path.m_noUpdate;
            settings.exclude = false;
            break;
          }
        }
        strPath1 = strParent;
      }
    }
    
    if(odb_transaction)
      odb_transaction->commit();
    //m_pDS->close();

    if (!scraper || scraper->Content() == CONTENT_NONE)
      return ScraperPtr();

    if (scraper->Content() == CONTENT_TVSHOWS)
    {
      settings.recurse = 0;
      if(settings.parent_name) // single show
      {
        settings.parent_name_root = settings.parent_name = (iFound == 1);
      }
      else // show root
      {
        settings.parent_name_root = settings.parent_name = (iFound == 2);
      }
    }
    else if (scraper->Content() == CONTENT_MOVIES)
    {
      settings.recurse = settings.recurse - (iFound-1);
      settings.parent_name_root = settings.parent_name && (!settings.recurse || iFound > 1);
    }
    else if (scraper->Content() == CONTENT_MUSICVIDEOS)
    {
      settings.recurse = settings.recurse - (iFound-1);
      settings.parent_name_root = settings.parent_name && (!settings.recurse || iFound > 1);
    }
    else
    {
      iFound = 0;
      return ScraperPtr();
    }
    foundDirectly = (iFound == 1);
    return scraper;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s exception - %s", __FUNCTION__, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return ScraperPtr();
}

std::string CVideoDatabase::GetContentForPath(const std::string& strPath)
{
  SScanSettings settings;
  bool foundDirectly = false;
  ScraperPtr scraper = GetScraperForPath(strPath, settings, foundDirectly);
  if (scraper)
  {
    if (scraper->Content() == CONTENT_TVSHOWS)
    {
      // check for episodes or seasons.  Assumptions are:
      // 1. if episodes are in the path then we're in episodes.
      // 2. if no episodes are found, and content was set directly on this path, then we're in shows.
      // 3. if no episodes are found, and content was not set directly on this path, we're in seasons (assumes tvshows/seasons/episodes)
      std::string sql = "SELECT COUNT(*) FROM episode_view ";

      if (foundDirectly)
        sql += PrepareSQL("WHERE strPath = '%s'", strPath.c_str());
      else
        sql += PrepareSQL("WHERE strPath LIKE '%s%%'", strPath.c_str());

      m_pDS->query( sql );
      if (m_pDS->num_rows() && m_pDS->fv(0).get_asInt() > 0)
        return "episodes";
      return foundDirectly ? "tvshows" : "seasons";
    }
    return TranslateContent(scraper->Content());
  }
  return "";
}

void CVideoDatabase::GetMovieGenresByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strModSearch = "%"+strSearch+"%";
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    odb::result<ODBView_Movie_Genre> res(m_cdb.getDB()->query<ODBView_Movie_Genre>(odb::query<ODBView_Movie_Genre>::genre::name.like(strModSearch)));
    for (odb::result<ODBView_Movie_Genre>::iterator i = res.begin(); i != res.end(); i++)
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      {
        if (!g_passwordManager.IsDatabasePathUnlocked(i->m_path,
                                                      *CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          continue;
        }
      }
      
      CFileItemPtr pItem(new CFileItem(i->m_name));
      std::string strDir = StringUtils::Format("%i/", static_cast<int>(i->m_idGenre));
      pItem->SetPath("videodb://movies/genres/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strModSearch.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strModSearch.c_str());
  }
}

void CVideoDatabase::GetTvShowGenresByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL=PrepareSQL("SELECT genre.genre_id, genre.name, path.strPath FROM genre INNER JOIN genre_link ON genre_link.genre_id=genre.genre_id INNER JOIN tvshow ON genre_link.media_id=tvshow.idShow INNER JOIN tvshowlinkpath ON tvshowlinkpath.idShow=tvshow.idShow INNER JOIN path ON path.idPath=tvshowlinkpath.idPath WHERE genre_link.media_type='tvshow' AND genre.name LIKE '%%%s%%'", strSearch.c_str());
    else
      strSQL=PrepareSQL("SELECT DISTINCT genre.genre_id, genre.name FROM genre INNER JOIN genre_link ON genre_link.genre_id=genre.genre_id WHERE genre_link.media_type='tvshow' AND name LIKE '%%%s%%'", strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      pItem->SetPath("videodb://tvshows/genres/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMovieActorsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL=PrepareSQL("SELECT actor.actor_id, actor.name, path.strPath FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN movie ON actor_link.media_id=movie.idMovie INNER JOIN files ON files.idFile=movie.idFile INNER JOIN path ON path.idPath=files.idPath WHERE actor_link.media_type='movie' AND actor.name LIKE '%%%s%%'", strSearch.c_str());
    else
      strSQL=PrepareSQL("SELECT DISTINCT actor.actor_id, actor.name FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN movie ON actor_link.media_id=movie.idMovie WHERE actor_link.media_type='movie' AND actor.name LIKE '%%%s%%'", strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      pItem->SetPath("videodb://movies/actors/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetTvShowsActorsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL=PrepareSQL("SELECT actor.actor_id, actor.name, path.strPath FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN tvshow ON actor_link.media_id=tvshow.idShow INNER JOIN tvshowlinkpath ON tvshowlinkpath.idPath=tvshow.idShow INNER JOIN path ON path.idPath=tvshowlinkpath.idPath WHERE actor_link.media_type='tvshow' AND actor.name LIKE '%%%s%%'", strSearch.c_str());
    else
      strSQL=PrepareSQL("SELECT DISTINCT actor.actor_id, actor.name FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN tvshow ON actor_link.media_id=tvshow.idShow WHERE actor_link.media_type='tvshow' AND actor.name LIKE '%%%s%%'",strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      pItem->SetPath("videodb://tvshows/actors/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMusicVideoArtistsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    std::string strLike;
    if (!strSearch.empty())
      strLike = "and actor.name like '%%%s%%'";
    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL=PrepareSQL("SELECT actor.actor_id, actor.name, path.strPath FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN musicvideo ON actor_link.media_id=musicvideo.idMVideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE actor_link.media_type='musicvideo' "+strLike, strSearch.c_str());
    else
      strSQL=PrepareSQL("SELECT DISTINCT actor.actor_id, actor.name from actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id WHERE actor_link.media_type='musicvideo' "+strLike,strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      pItem->SetPath("videodb://musicvideos/artists/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMusicVideoGenresByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL=PrepareSQL("SELECT genre.genre_id, genre.name, path.strPath FROM genre INNER JOIN genre_link ON genre_link.genre_id=genre.genre_id INNER JOIN musicvideo ON genre_link.media_id=musicvideo.idMVideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE genre_link.media_type='musicvideo' AND genre.name LIKE '%%%s%%'", strSearch.c_str());
    else
      strSQL=PrepareSQL("SELECT DISTINCT genre.genre_id, genre.name FROM genre INNER JOIN genre_link ON genre_link.genre_id=genre.genre_id WHERE genre_link.media_type='musicvideo' AND genre.name LIKE '%%%s%%'", strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      pItem->SetPath("videodb://musicvideos/genres/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMusicVideoAlbumsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    strSQL = StringUtils::Format("SELECT DISTINCT"
                                 "  musicvideo.c%02d,"
                                 "  musicvideo.idMVideo,"
                                 "  path.strPath"
                                 " FROM"
                                 "  musicvideo"
                                 " JOIN files ON"
                                 "  files.idFile=musicvideo.idFile"
                                 " JOIN path ON"
                                 "  path.idPath=files.idPath", VIDEODB_ID_MUSICVIDEO_ALBUM);
    if (!strSearch.empty())
      strSQL += PrepareSQL(" WHERE musicvideo.c%02d like '%%%s%%'",VIDEODB_ID_MUSICVIDEO_ALBUM, strSearch.c_str());

    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (m_pDS->fv(0).get_asString().empty())
      {
        m_pDS->next();
        continue;
      }

      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv(2).get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(0).get_asString()));
      std::string strDir = StringUtils::Format("%i", m_pDS->fv(1).get_asInt());
      pItem->SetPath("videodb://musicvideos/titles/"+ strDir);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMusicVideosByAlbum(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT musicvideo.idMVideo, musicvideo.c%02d,musicvideo.c%02d, path.strPath FROM musicvideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE musicvideo.c%02d LIKE '%%%s%%'", VIDEODB_ID_MUSICVIDEO_ALBUM, VIDEODB_ID_MUSICVIDEO_TITLE, VIDEODB_ID_MUSICVIDEO_ALBUM, strSearch.c_str());
    else
      strSQL = PrepareSQL("select musicvideo.idMVideo,musicvideo.c%02d,musicvideo.c%02d from musicvideo where musicvideo.c%02d like '%%%s%%'",VIDEODB_ID_MUSICVIDEO_ALBUM,VIDEODB_ID_MUSICVIDEO_TITLE,VIDEODB_ID_MUSICVIDEO_ALBUM,strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()+" - "+m_pDS->fv(2).get_asString()));
      std::string strDir = StringUtils::Format("3/2/%i",m_pDS->fv("musicvideo.idMVideo").get_asInt());

      pItem->SetPath("videodb://"+ strDir);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

bool CVideoDatabase::GetMusicVideosByWhere(const std::string &baseDir, const Filter &filter, CFileItemList &items, bool checkLocks /*= true*/, const SortDescription &sortDescription /* = SortDescription() */, int getDetails /* = VideoDbDetailsNone */)
{
  try
  {
    movieTime = 0;
    castTime = 0;

    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    int total = -1;
    
    std::string strSQL = "select %s from musicvideo_view ";
    CVideoDbUrl videoUrl;
    std::string strSQLExtra;
    Filter extFilter = filter;
    SortDescription sorting = sortDescription;
    if (!BuildSQL(baseDir, strSQLExtra, extFilter, strSQLExtra, videoUrl, sorting))
      return false;

    // Apply the limiting directly here if there's no special sorting but limiting
    if (extFilter.limit.empty() &&
      sorting.sortBy == SortByNone &&
      (sorting.limitStart > 0 || sorting.limitEnd > 0))
    {
      total = (int)strtol(GetSingleValue(PrepareSQL(strSQL, "COUNT(1)") + strSQLExtra, m_pDS).c_str(), NULL, 10);
      strSQLExtra += DatabaseUtils::BuildLimitClause(sorting.limitEnd, sorting.limitStart);
    }

    strSQL = PrepareSQL(strSQL, !extFilter.fields.empty() ? extFilter.fields.c_str() : "*") + strSQLExtra;

    int iRowsFound = RunQuery(strSQL);
    if (iRowsFound <= 0)
      return iRowsFound == 0;

    // store the total value of items as a property
    if (total < iRowsFound)
      total = iRowsFound;
    items.SetProperty("total", total);
    
    DatabaseResults results;
    results.reserve(iRowsFound);
    if (!SortUtils::SortFromDataset(sorting, MediaTypeMusicVideo, m_pDS, results))
      return false;
    
    // get data from returned rows
    items.Reserve(results.size());
    // get songs from returned subtable
    const query_data &data = m_pDS->get_result_set().records;
    for (const auto &i : results)
    {
      unsigned int targetRow = (unsigned int)i.at(FieldRow).asInteger();
      const dbiplus::sql_record* const record = data.at(targetRow);
      
      CVideoInfoTag musicvideo = GetDetailsForMusicVideo(record, getDetails);
      if (!checkLocks || CProfilesManager::GetInstance().GetMasterProfile().getLockMode() == LOCK_MODE_EVERYONE || g_passwordManager.bMasterUser ||
          g_passwordManager.IsDatabasePathUnlocked(musicvideo.m_strPath, *CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        CFileItemPtr item(new CFileItem(musicvideo));

        CVideoDbUrl itemUrl = videoUrl;
        std::string path = StringUtils::Format("%i", record->at(0).get_asInt());
        itemUrl.AppendPath(path);
        item->SetPath(itemUrl.ToString());

        item->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, musicvideo.m_playCount > 0);
        items.Add(item);
      }
    }

    // cleanup
    m_pDS->close();
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return false;
}

unsigned int CVideoDatabase::GetMusicVideoIDs(const std::string& strWhere, std::vector<std::pair<int,int> > &songIDs)
{
  try
  {
    if (NULL == m_pDB.get()) return 0;
    if (NULL == m_pDS.get()) return 0;

    std::string strSQL = "select distinct idMVideo from musicvideo_view";
    if (!strWhere.empty())
      strSQL += " where " + strWhere;

    if (!m_pDS->query(strSQL)) return 0;
    songIDs.clear();
    if (m_pDS->num_rows() == 0)
    {
      m_pDS->close();
      return 0;
    }
    songIDs.reserve(m_pDS->num_rows());
    while (!m_pDS->eof())
    {
      songIDs.push_back(std::make_pair<int,int>(2,m_pDS->fv(0).get_asInt()));
      m_pDS->next();
    }    // cleanup
    m_pDS->close();
    return songIDs.size();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strWhere.c_str());
  }
  return 0;
}

bool CVideoDatabase::GetRandomMusicVideo(CFileItem* item, int& idSong, const std::string& strWhere)
{
  try
  {
    idSong = -1;

    if (NULL == m_pDB.get()) return false;
    if (NULL == m_pDS.get()) return false;

    std::string strSQL = "select * from musicvideo_view";
    if (!strWhere.empty())
      strSQL += " where " + strWhere;
    strSQL += PrepareSQL(" order by RANDOM() limit 1");
    CLog::Log(LOGDEBUG, "%s query = %s", __FUNCTION__, strSQL.c_str());
    // run query
    if (!m_pDS->query(strSQL))
      return false;
    int iRowsFound = m_pDS->num_rows();
    if (iRowsFound != 1)
    {
      m_pDS->close();
      return false;
    }
    *item->GetVideoInfoTag() = GetDetailsForMusicVideo(m_pDS);
    std::string path = StringUtils::Format("videodb://musicvideos/titles/%i",item->GetVideoInfoTag()->m_iDbId);
    item->SetPath(path);
    idSong = m_pDS->fv("idMVideo").get_asInt();
    item->SetLabel(item->GetVideoInfoTag()->m_strTitle);
    m_pDS->close();
    return true;
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strWhere.c_str());
  }
  return false;
}

int CVideoDatabase::GetMatchingMusicVideo(const std::string& strArtist, const std::string& strAlbum, const std::string& strTitle)
{
  try
  {
    if (NULL == m_pDB.get()) return -1;
    if (NULL == m_pDS.get()) return -1;

    std::string strSQL;
    if (strAlbum.empty() && strTitle.empty())
    { // we want to return matching artists only
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        strSQL=PrepareSQL("SELECT DISTINCT actor.actor_id, path.strPath FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN musicvideo ON actor_link.media_id=musicvideo.idMVideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE actor_link.media_type='musicvideo' AND actor.name like '%s'", strArtist.c_str());
      else
        strSQL=PrepareSQL("SELECT DISTINCT actor.actor_id FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id WHERE actor_link.media_type='musicvideo' AND actor.name LIKE '%s'", strArtist.c_str());
    }
    else
    { // we want to return the matching musicvideo
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        strSQL = PrepareSQL("SELECT musicvideo.idMVideo FROM actor INNER JOIN actor_link ON actor_link.actor_id=actor.actor_id INNER JOIN musicvideo ON actor_link.media_id=musicvideo.idMVideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE actor_link.media_type='musicvideo' AND musicvideo.c%02d LIKE '%s' AND musicvideo.c%02d LIKE '%s' AND actor.name LIKE '%s'", VIDEODB_ID_MUSICVIDEO_ALBUM, strAlbum.c_str(), VIDEODB_ID_MUSICVIDEO_TITLE, strTitle.c_str(), strArtist.c_str());
      else
        strSQL = PrepareSQL("select musicvideo.idMVideo from musicvideo join actor_link on actor_link.media_id=musicvideo.idMVideo AND actor_link.media_type='musicvideo' join actor on actor.actor_id=actor_link.actor_id where musicvideo.c%02d like '%s' and musicvideo.c%02d like '%s' and actor.name like '%s'",VIDEODB_ID_MUSICVIDEO_ALBUM,strAlbum.c_str(),VIDEODB_ID_MUSICVIDEO_TITLE,strTitle.c_str(),strArtist.c_str());
    }
    m_pDS->query( strSQL );

    if (m_pDS->eof())
      return -1;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        m_pDS->close();
        return -1;
      }

    int lResult = m_pDS->fv(0).get_asInt();
    m_pDS->close();
    return lResult;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  return -1;
}

void CVideoDatabase::GetMoviesByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strModSearch = "%"+strSearch+"%";
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    odb::result<CODBMovie> res(m_cdb.getDB()->query<CODBMovie>(odb::query<CODBMovie>::title.like(strModSearch)));
    for (odb::result<CODBMovie>::iterator i = res.begin(); i != res.end(); i++)
    {
      m_cdb.getDB()->load(*i, i->section_foreign);
      if(!i->m_file.load() || !i->m_file->m_path.load())
        continue;
      
      if (!g_passwordManager.IsDatabasePathUnlocked(i->m_file->m_path->m_path,
                                                    *CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        continue;
      }
      
      int movieId = i->m_idMovie;
      int setId = 0;
      if(i->m_set.load())
        setId = i->m_set->m_idSet;
      
      CFileItemPtr pItem(new CFileItem(i->m_title));
      std::string path;
      if (setId <= 0 || !CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOLIBRARY_GROUPMOVIESETS))
        path = StringUtils::Format("videodb://movies/titles/%i", movieId);
      else
        path = StringUtils::Format("videodb://movies/sets/%i/%i", setId, movieId);
      pItem->SetPath(path);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strModSearch.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strModSearch.c_str());
  }
}

void CVideoDatabase::GetTvShowsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT tvshow.idShow, tvshow.c%02d, path.strPath FROM tvshow INNER JOIN tvshowlinkpath ON tvshowlinkpath.idShow=tvshow.idShow INNER JOIN path ON path.idPath=tvshowlinkpath.idPath WHERE tvshow.c%02d LIKE '%%%s%%'", VIDEODB_ID_TV_TITLE, VIDEODB_ID_TV_TITLE, strSearch.c_str());
    else
      strSQL = PrepareSQL("select tvshow.idShow,tvshow.c%02d from tvshow where tvshow.c%02d like '%%%s%%'",VIDEODB_ID_TV_TITLE,VIDEODB_ID_TV_TITLE,strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("tvshows/titles/%i/", m_pDS->fv("tvshow.idShow").get_asInt());

      pItem->SetPath("videodb://"+ strDir);
      pItem->m_bIsFolder=true;
      pItem->GetVideoInfoTag()->m_iDbId = m_pDS->fv("tvshow.idShow").get_asInt();
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetEpisodesByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT episode.idEpisode, episode.c%02d, episode.c%02d, episode.idShow, tvshow.c%02d, path.strPath FROM episode INNER JOIN tvshow ON tvshow.idShow=episode.idShow INNER JOIN files ON files.idFile=episode.idFile INNER JOIN path ON path.idPath=files.idPath WHERE episode.c%02d LIKE '%%%s%%'", VIDEODB_ID_EPISODE_TITLE, VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_TV_TITLE, VIDEODB_ID_EPISODE_TITLE, strSearch.c_str());
    else
      strSQL = PrepareSQL("SELECT episode.idEpisode, episode.c%02d, episode.c%02d, episode.idShow, tvshow.c%02d FROM episode INNER JOIN tvshow ON tvshow.idShow=episode.idShow WHERE episode.c%02d like '%%%s%%'", VIDEODB_ID_EPISODE_TITLE, VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_TV_TITLE, VIDEODB_ID_EPISODE_TITLE, strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()+" ("+m_pDS->fv(4).get_asString()+")"));
      std::string path = StringUtils::Format("videodb://tvshows/titles/%i/%i/%i",m_pDS->fv("episode.idShow").get_asInt(),m_pDS->fv(2).get_asInt(),m_pDS->fv(0).get_asInt());
      pItem->SetPath(path);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMusicVideosByName(const std::string& strSearch, CFileItemList& items)
{
// Alternative searching - not quite as fast though due to
// retrieving all information
//  Filter filter(PrepareSQL("c%02d like '%s%%' or c%02d like '%% %s%%'", VIDEODB_ID_MUSICVIDEO_TITLE, strSearch.c_str(), VIDEODB_ID_MUSICVIDEO_TITLE, strSearch.c_str()));
//  GetMusicVideosByWhere("videodb://musicvideos/titles/", filter, items);
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT musicvideo.idMVideo, musicvideo.c%02d, path.strPath FROM musicvideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE musicvideo.c%02d LIKE '%%%s%%'", VIDEODB_ID_MUSICVIDEO_TITLE, VIDEODB_ID_MUSICVIDEO_TITLE, strSearch.c_str());
    else
      strSQL = PrepareSQL("select musicvideo.idMVideo,musicvideo.c%02d from musicvideo where musicvideo.c%02d like '%%%s%%'",VIDEODB_ID_MUSICVIDEO_TITLE,VIDEODB_ID_MUSICVIDEO_TITLE,strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));
      std::string strDir = StringUtils::Format("3/2/%i",m_pDS->fv("musicvideo.idMVideo").get_asInt());

      pItem->SetPath("videodb://"+ strDir);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetEpisodesByPlot(const std::string& strSearch, CFileItemList& items)
{
// Alternative searching - not quite as fast though due to
// retrieving all information
//  Filter filter;
//  filter.where = PrepareSQL("c%02d like '%s%%' or c%02d like '%% %s%%'", VIDEODB_ID_EPISODE_PLOT, strSearch.c_str(), VIDEODB_ID_EPISODE_PLOT, strSearch.c_str());
//  filter.where += PrepareSQL("or c%02d like '%s%%' or c%02d like '%% %s%%'", VIDEODB_ID_EPISODE_TITLE, strSearch.c_str(), VIDEODB_ID_EPISODE_TITLE, strSearch.c_str());
//  GetEpisodesByWhere("videodb://tvshows/titles/", filter, items);
//  return;
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT episode.idEpisode, episode.c%02d, episode.c%02d, episode.idShow, tvshow.c%02d, path.strPath FROM episode INNER JOIN tvshow ON tvshow.idShow=episode.idShow INNER JOIN files ON files.idFile=episode.idFile INNER JOIN path ON path.idPath=files.idPath WHERE episode.c%02d LIKE '%%%s%%'", VIDEODB_ID_EPISODE_TITLE, VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_TV_TITLE, VIDEODB_ID_EPISODE_PLOT, strSearch.c_str());
    else
      strSQL = PrepareSQL("SELECT episode.idEpisode, episode.c%02d, episode.c%02d, episode.idShow, tvshow.c%02d FROM episode INNER JOIN tvshow ON tvshow.idShow=episode.idShow WHERE episode.c%02d LIKE '%%%s%%'", VIDEODB_ID_EPISODE_TITLE, VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_TV_TITLE, VIDEODB_ID_EPISODE_PLOT, strSearch.c_str());
    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()+" ("+m_pDS->fv(4).get_asString()+")"));
      std::string path = StringUtils::Format("videodb://tvshows/titles/%i/%i/%i",m_pDS->fv("episode.idShow").get_asInt(),m_pDS->fv(2).get_asInt(),m_pDS->fv(0).get_asInt());
      pItem->SetPath(path);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMoviesByPlot(const std::string& strSearch, CFileItemList& items)
{
  //TODO: GetMoviesByXXXX functions could use a dedicated View to reduce loading of foreign elements
  std::string strModSearch = "%"+strSearch+"%";
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    odb::result<CODBMovie> res(m_cdb.getDB()->query<CODBMovie>(odb::query<CODBMovie>::plot.like(strModSearch) ||
                                                               odb::query<CODBMovie>::plotoutline.like(strModSearch) ||
                                                               odb::query<CODBMovie>::tagline.like(strModSearch)));
    for (odb::result<CODBMovie>::iterator i = res.begin(); i != res.end(); i++)
    {
      m_cdb.getDB()->load(*i, i->section_foreign);
      if(!i->m_file.load() || !i->m_file->m_path.load())
        continue;
      
      if (!g_passwordManager.IsDatabasePathUnlocked(i->m_file->m_path->m_path,
                                                    *CMediaSourceSettings::GetInstance().GetSources("video")))
      {
        continue;
      }
      
      int setId = 0;
      if(i->m_set.load())
        setId = i->m_set->m_idSet;
      
      CFileItemPtr pItem(new CFileItem(i->m_title));
      std::string path = StringUtils::Format("videodb://movies/titles/%i", static_cast<int>(i->m_idMovie));
      pItem->SetPath(path);
      pItem->m_bIsFolder=false;
      items.Add(pItem);
      
    }
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strModSearch.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strModSearch.c_str());
  }
}

void CVideoDatabase::GetMovieDirectorsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strModSearch = "%"+strSearch+"%";
  
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());
    
    odb::result<ODBView_Movie_Director> res(m_cdb.getDB()->query<ODBView_Movie_Director>(odb::query<ODBView_Movie_Director>::person::name.like(strModSearch)));
    for (odb::result<ODBView_Movie_Director>::iterator i = res.begin(); i != res.end(); i++)
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      {
        if (!g_passwordManager.IsDatabasePathUnlocked(i->m_path,
                                                      *CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          continue;
        }
      }
      
      std::string strDir = StringUtils::Format("%i/", static_cast<int>(i->person->m_idPerson));
      CFileItemPtr pItem(new CFileItem(i->person->m_name));
      
      pItem->SetPath("videodb://movies/directors/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
    }
    
    if(odb_transaction)
      odb_transaction->commit();
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%s) exception - %s", __FUNCTION__, strModSearch.c_str(), e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strModSearch.c_str());
  }
}

void CVideoDatabase::GetTvShowsDirectorsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT DISTINCT director_link.actor_id, actor.name, path.strPath FROM actor INNER JOIN director_link ON director_link.actor_id=actor.actor_id INNER JOIN tvshow ON director_link.media_id=tvshow.idShow INNER JOIN tvshowlinkpath ON tvshowlinkpath.idShow=tvshow.idShow INNER JOIN path ON path.idPath=tvshowlinkpath.idPath WHERE director_link.media_type='tvshow' AND actor.name LIKE '%%%s%%'", strSearch.c_str());
    else
      strSQL = PrepareSQL("SELECT DISTINCT director_link.actor_id, actor.name FROM actor INNER JOIN director_link ON director_link.actor_id=actor.actor_id INNER JOIN tvshow ON director_link.media_id=tvshow.idShow WHERE director_link.media_type='tvshow' AND actor.name LIKE '%%%s%%'", strSearch.c_str());

    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));

      pItem->SetPath("videodb://tvshows/directors/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::GetMusicVideoDirectorsByName(const std::string& strSearch, CFileItemList& items)
{
  std::string strSQL;

  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
      strSQL = PrepareSQL("SELECT DISTINCT director_link.actor_id, actor.name, path.strPath FROM actor INNER JOIN director_link ON director_link.actor_id=actor.actor_id INNER JOIN musicvideo ON director_link.media_id=musicvideo.idMVideo INNER JOIN files ON files.idFile=musicvideo.idFile INNER JOIN path ON path.idPath=files.idPath WHERE director_link.media_type='musicvideo' AND actor.name LIKE '%%%s%%'", strSearch.c_str());
    else
      strSQL = PrepareSQL("SELECT DISTINCT director_link.actor_id, actor.name FROM actor INNER JOIN director_link ON director_link.actor_id=actor.actor_id INNER JOIN musicvideo ON director_link.media_id=musicvideo.idMVideo WHERE director_link.media_type='musicvideo' AND actor.name LIKE '%%%s%%'", strSearch.c_str());

    m_pDS->query( strSQL );

    while (!m_pDS->eof())
    {
      if (CProfilesManager::GetInstance().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE && !g_passwordManager.bMasterUser)
        if (!g_passwordManager.IsDatabasePathUnlocked(std::string(m_pDS->fv("path.strPath").get_asString()),*CMediaSourceSettings::GetInstance().GetSources("video")))
        {
          m_pDS->next();
          continue;
        }

      std::string strDir = StringUtils::Format("%i/", m_pDS->fv(0).get_asInt());
      CFileItemPtr pItem(new CFileItem(m_pDS->fv(1).get_asString()));

      pItem->SetPath("videodb://musicvideos/albums/"+ strDir);
      pItem->m_bIsFolder=true;
      items.Add(pItem);
      m_pDS->next();
    }
    m_pDS->close();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
}

void CVideoDatabase::CleanDatabase(CGUIDialogProgressBarHandle* handle, const std::set<int>& paths, bool showProgress)
{
  CGUIDialogProgress *progress=NULL;
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    unsigned int time = XbmcThreads::SystemClockMillis();
    CLog::Log(LOGNOTICE, "%s: Starting videodatabase cleanup ..", __FUNCTION__);
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnCleanStarted");

    BeginTransaction();

    // find all the files
    std::string sql = "SELECT files.idFile, files.strFileName, path.strPath FROM files INNER JOIN path ON path.idPath=files.idPath";
    if (!paths.empty())
    {
      std::string strPaths;
      for (const auto &i : paths)
        strPaths += StringUtils::Format(",%i", i);
      sql += PrepareSQL(" AND path.idPath IN (%s)", strPaths.substr(1).c_str());
    }

    m_pDS->query(sql);
    if (m_pDS->num_rows() == 0) return;

    if (handle)
    {
      handle->SetTitle(g_localizeStrings.Get(700));
      handle->SetText("");
    }
    else if (showProgress)
    {
      progress = (CGUIDialogProgress *)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
      if (progress)
      {
        progress->SetHeading(CVariant{700});
        progress->SetLine(0, CVariant{""});
        progress->SetLine(1, CVariant{313});
        progress->SetLine(2, CVariant{330});
        progress->SetPercentage(0);
        progress->Open();
        progress->ShowProgressBar(true);
      }
    }

    std::string filesToTestForDelete;
    VECSOURCES videoSources(*CMediaSourceSettings::GetInstance().GetSources("video"));
    g_mediaManager.GetRemovableDrives(videoSources);

    int total = m_pDS->num_rows();
    int current = 0;

    while (!m_pDS->eof())
    {
      std::string path = m_pDS->fv("path.strPath").get_asString();
      std::string fileName = m_pDS->fv("files.strFileName").get_asString();
      std::string fullPath;
      ConstructPath(fullPath, path, fileName);

      // get the first stacked file
      if (URIUtils::IsStack(fullPath))
        fullPath = CStackDirectory::GetFirstStackedFile(fullPath);

      // get the actual archive path
      if (URIUtils::IsInArchive(fullPath))
        fullPath = CURL(fullPath).GetHostName();

      // remove optical, non-existing files, files with no matching source
      bool bIsSource;
      if (URIUtils::IsOnDVD(fullPath) || !CFile::Exists(fullPath, false) ||
          CUtil::GetMatchingSource(fullPath, videoSources, bIsSource) < 0)
        filesToTestForDelete += m_pDS->fv("files.idFile").get_asString() + ",";

      if (handle == NULL && progress != NULL)
      {
        int percentage = current * 100 / total;
        if (percentage > progress->GetPercentage())
        {
          progress->SetPercentage(percentage);
          progress->Progress();
        }
        if (progress->IsCanceled())
        {
          progress->Close();
          m_pDS->close();
          ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnCleanFinished");
          return;
        }
      }
      else if (handle != NULL)
        handle->SetPercentage(current * 100 / (float)total);

      m_pDS->next();
      current++;
    }
    m_pDS->close();

    std::string filesToDelete;

    // Add any files that don't have a valid idPath entry to the filesToDelete list.
    m_pDS->query("SELECT files.idFile FROM files WHERE NOT EXISTS (SELECT 1 FROM path WHERE path.idPath = files.idPath)");
    while (!m_pDS->eof())
    {
      std::string file = m_pDS->fv("files.idFile").get_asString() + ",";
      filesToTestForDelete += file;
      filesToDelete += file;

      m_pDS->next();
    }
    m_pDS->close();

    std::map<int, bool> pathsDeleteDecisions;
    std::vector<int> movieIDs;
    std::vector<int> tvshowIDs;
    std::vector<int> episodeIDs;
    std::vector<int> musicVideoIDs;

    if (!filesToTestForDelete.empty())
    {
      StringUtils::TrimRight(filesToTestForDelete, ",");

      movieIDs = CleanMediaType(MediaTypeMovie, filesToTestForDelete, pathsDeleteDecisions, filesToDelete, !showProgress);
      episodeIDs = CleanMediaType(MediaTypeEpisode, filesToTestForDelete, pathsDeleteDecisions, filesToDelete, !showProgress);
      musicVideoIDs = CleanMediaType(MediaTypeMusicVideo, filesToTestForDelete, pathsDeleteDecisions, filesToDelete, !showProgress);
    }

    if (progress != NULL)
    {
      progress->SetPercentage(100);
      progress->Progress();
    }

    if (!filesToDelete.empty())
    {
      filesToDelete = "(" + StringUtils::TrimRight(filesToDelete, ",") + ")";

      CLog::Log(LOGDEBUG, "%s: Cleaning files table", __FUNCTION__);
      sql = "DELETE FROM files WHERE idFile IN " + filesToDelete;
      m_pDS->exec(sql);
    }

    if (!movieIDs.empty())
    {
      std::string moviesToDelete;
      for (const auto &i : movieIDs)
        moviesToDelete += StringUtils::Format("%i,", i);
      moviesToDelete = "(" + StringUtils::TrimRight(moviesToDelete, ",") + ")";

      CLog::Log(LOGDEBUG, "%s: Cleaning movie table", __FUNCTION__);
      sql = "DELETE FROM movie WHERE idMovie IN " + moviesToDelete;
      m_pDS->exec(sql);
    }

    if (!episodeIDs.empty())
    {
      std::string episodesToDelete;
      for (const auto &i : episodeIDs)
        episodesToDelete += StringUtils::Format("%i,", i);
      episodesToDelete = "(" + StringUtils::TrimRight(episodesToDelete, ",") + ")";

      CLog::Log(LOGDEBUG, "%s: Cleaning episode table", __FUNCTION__);
      sql = "DELETE FROM episode WHERE idEpisode IN " + episodesToDelete;
      m_pDS->exec(sql);
    }

    CLog::Log(LOGDEBUG, "%s: Cleaning paths that don't exist and have content set...", __FUNCTION__);
    sql = "SELECT path.idPath, path.strPath, path.idParentPath FROM path "
            "WHERE NOT ((strContent IS NULL OR strContent = '') "
                   "AND (strSettings IS NULL OR strSettings = '') "
                   "AND (strHash IS NULL OR strHash = '') "
                   "AND (exclude IS NULL OR exclude != 1))";
    m_pDS->query(sql);
    std::string strIds;
    while (!m_pDS->eof())
    {
      auto pathsDeleteDecision = pathsDeleteDecisions.find(m_pDS->fv(0).get_asInt());
      // Check if we have a decision for the parent path
      auto pathsDeleteDecisionByParent = pathsDeleteDecisions.find(m_pDS->fv(2).get_asInt());
      if (((pathsDeleteDecision != pathsDeleteDecisions.end() && pathsDeleteDecision->second) ||
           (pathsDeleteDecision == pathsDeleteDecisions.end() && !CDirectory::Exists(m_pDS->fv(1).get_asString(), false))) &&
          ((pathsDeleteDecisionByParent != pathsDeleteDecisions.end() && pathsDeleteDecisionByParent->second) ||
           (pathsDeleteDecisionByParent == pathsDeleteDecisions.end())))
        strIds += m_pDS->fv(0).get_asString() + ",";

      m_pDS->next();
    }
    m_pDS->close();

    if (!strIds.empty())
    {
      sql = PrepareSQL("DELETE FROM path WHERE idPath IN (%s)", StringUtils::TrimRight(strIds, ",").c_str());
      m_pDS->exec(sql);
      sql = "DELETE FROM tvshowlinkpath WHERE NOT EXISTS (SELECT 1 FROM path WHERE path.idPath = tvshowlinkpath.idPath)";
      m_pDS->exec(sql);
    }

    CLog::Log(LOGDEBUG, "%s: Cleaning tvshow table", __FUNCTION__);

    std::string tvshowsToDelete;
    sql = "SELECT idShow FROM tvshow WHERE NOT EXISTS (SELECT 1 FROM tvshowlinkpath WHERE tvshowlinkpath.idShow = tvshow.idShow)";
    m_pDS->query(sql);
    while (!m_pDS->eof())
    {
      tvshowIDs.push_back(m_pDS->fv(0).get_asInt());
      tvshowsToDelete += m_pDS->fv(0).get_asString() + ",";
      m_pDS->next();
    }
    m_pDS->close();
    if (!tvshowsToDelete.empty())
    {
      sql = "DELETE FROM tvshow WHERE idShow IN (" + StringUtils::TrimRight(tvshowsToDelete, ",") + ")";
      m_pDS->exec(sql);
    }

    if (!musicVideoIDs.empty())
    {
      std::string musicVideosToDelete;
      for (const auto &i : musicVideoIDs)
        musicVideosToDelete += StringUtils::Format("%i,", i);
      musicVideosToDelete = "(" + StringUtils::TrimRight(musicVideosToDelete, ",") + ")";

      CLog::Log(LOGDEBUG, "%s: Cleaning musicvideo table", __FUNCTION__);
      sql = "DELETE FROM musicvideo WHERE idMVideo IN " + musicVideosToDelete;
      m_pDS->exec(sql);
    }

    CLog::Log(LOGDEBUG, "%s: Cleaning path table", __FUNCTION__);
    sql = StringUtils::Format("DELETE FROM path "
                                "WHERE (strContent IS NULL OR strContent = '') "
                                  "AND (strSettings IS NULL OR strSettings = '') "
                                  "AND (strHash IS NULL OR strHash = '') "
                                  "AND (exclude IS NULL OR exclude != 1) "
                                  "AND (idParentPath IS NULL OR NOT EXISTS (SELECT 1 FROM (SELECT idPath FROM path) as parentPath WHERE parentPath.idPath = path.idParentPath)) " // MySQL only fix (#5007)
                                  "AND NOT EXISTS (SELECT 1 FROM files WHERE files.idPath = path.idPath) "
                                  "AND NOT EXISTS (SELECT 1 FROM tvshowlinkpath WHERE tvshowlinkpath.idPath = path.idPath) "
                                  "AND NOT EXISTS (SELECT 1 FROM movie WHERE movie.c%02d = path.idPath) "
                                  "AND NOT EXISTS (SELECT 1 FROM episode WHERE episode.c%02d = path.idPath) "
                                  "AND NOT EXISTS (SELECT 1 FROM musicvideo WHERE musicvideo.c%02d = path.idPath)"
                , VIDEODB_ID_PARENTPATHID, VIDEODB_ID_EPISODE_PARENTPATHID, VIDEODB_ID_MUSICVIDEO_PARENTPATHID );
    m_pDS->exec(sql);

    CLog::Log(LOGDEBUG, "%s: Cleaning genre table", __FUNCTION__);
    sql = "DELETE FROM genre "
            "WHERE NOT EXISTS (SELECT 1 FROM genre_link WHERE genre_link.genre_id = genre.genre_id)";
    m_pDS->exec(sql);

    CLog::Log(LOGDEBUG, "%s: Cleaning country table", __FUNCTION__);
    sql = "DELETE FROM country WHERE NOT EXISTS (SELECT 1 FROM country_link WHERE country_link.country_id = country.country_id)";
    m_pDS->exec(sql);

    CLog::Log(LOGDEBUG, "%s: Cleaning actor table of actors, directors and writers", __FUNCTION__);
    sql = "DELETE FROM actor "
            "WHERE NOT EXISTS (SELECT 1 FROM actor_link WHERE actor_link.actor_id = actor.actor_id) "
              "AND NOT EXISTS (SELECT 1 FROM director_link WHERE director_link.actor_id = actor.actor_id) "
              "AND NOT EXISTS (SELECT 1 FROM writer_link WHERE writer_link.actor_id = actor.actor_id)";
    m_pDS->exec(sql);

    CLog::Log(LOGDEBUG, "%s: Cleaning studio table", __FUNCTION__);
    sql = "DELETE FROM studio "
            "WHERE NOT EXISTS (SELECT 1 FROM studio_link WHERE studio_link.studio_id = studio.studio_id)";
    m_pDS->exec(sql);

    CLog::Log(LOGDEBUG, "%s: Cleaning set table", __FUNCTION__);
    sql = "DELETE FROM sets WHERE NOT EXISTS (SELECT 1 FROM movie WHERE movie.idSet = sets.idSet)";
    m_pDS->exec(sql);

    CommitTransaction();

    if (handle)
      handle->SetTitle(g_localizeStrings.Get(331));

    Compress(false);

    CUtil::DeleteVideoDatabaseDirectoryCache();

    time = XbmcThreads::SystemClockMillis() - time;
    CLog::Log(LOGNOTICE, "%s: Cleaning videodatabase done. Operation took %s", __FUNCTION__, StringUtils::SecondsToTimeString(time / 1000).c_str());

    for (const auto &i : movieIDs)
      AnnounceRemove(MediaTypeMovie, i, true);

    for (const auto &i : episodeIDs)
      AnnounceRemove(MediaTypeEpisode, i, true);

    for (const auto &i : tvshowIDs)
      AnnounceRemove(MediaTypeTvShow, i, true);

    for (const auto &i : musicVideoIDs)
      AnnounceRemove(MediaTypeMusicVideo, i, true);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
    RollbackTransaction();
  }
  if (progress)
    progress->Close();

  ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnCleanFinished");
}

std::vector<int> CVideoDatabase::CleanMediaType(const std::string &mediaType, const std::string &cleanableFileIDs,
                                                std::map<int, bool> &pathsDeleteDecisions, std::string &deletedFileIDs, bool silent)
{
  std::vector<int> cleanedIDs;
  if (mediaType.empty() || cleanableFileIDs.empty())
    return cleanedIDs;

  std::string table = mediaType;
  std::string idField;
  std::string parentPathIdField;
  bool isEpisode = false;
  if (mediaType == MediaTypeMovie)
  {
    idField = "idMovie";
    parentPathIdField = StringUtils::Format("%s.c%02d", table.c_str(), VIDEODB_ID_PARENTPATHID);
  }
  else if (mediaType == MediaTypeEpisode)
  {
    idField = "idEpisode";
    parentPathIdField = "showPath.idParentPath";
    isEpisode = true;
  }
  else if (mediaType == MediaTypeMusicVideo)
  {
    idField = "idMVideo";
    parentPathIdField = StringUtils::Format("%s.c%02d", table.c_str(), VIDEODB_ID_MUSICVIDEO_PARENTPATHID);
  }
  else
    return cleanedIDs;

  // now grab them media items
  std::string sql = PrepareSQL("SELECT %s.%s, %s.idFile, path.idPath, parentPath.strPath FROM %s "
                                 "JOIN files ON files.idFile = %s.idFile "
                                 "JOIN path ON path.idPath = files.idPath ",
                               table.c_str(), idField.c_str(), table.c_str(), table.c_str(),
                               table.c_str());

  if (isEpisode)
    sql += "JOIN tvshowlinkpath ON tvshowlinkpath.idShow = episode.idShow JOIN path AS showPath ON showPath.idPath=tvshowlinkpath.idPath ";

  sql += PrepareSQL("LEFT JOIN path as parentPath ON parentPath.idPath = %s "
                    "WHERE %s.idFile IN (%s)",
                    parentPathIdField.c_str(),
                    table.c_str(), cleanableFileIDs.c_str());

  VECSOURCES videoSources(*CMediaSourceSettings::GetInstance().GetSources("video"));
  g_mediaManager.GetRemovableDrives(videoSources);

  // map of parent path ID to boolean pair (if not exists and user choice)
  std::map<int, std::pair<bool, bool> > sourcePathsDeleteDecisions;
  m_pDS2->query(sql);
  while (!m_pDS2->eof())
  {
    bool del = true;
    if (m_pDS2->fv(3).get_isNull() == false)
    {
      std::string parentPath = m_pDS2->fv(3).get_asString();

      // try to find the source path the parent path belongs to
      SScanSettings scanSettings;
      std::string sourcePath;
      GetSourcePath(parentPath, sourcePath, scanSettings);

      bool bIsSourceName;
      bool sourceNotFound = (CUtil::GetMatchingSource(parentPath, videoSources, bIsSourceName) < 0);

      if (sourceNotFound && sourcePath.empty())
        sourcePath = parentPath;

      int sourcePathID = GetPathId(sourcePath);
      auto sourcePathsDeleteDecision = sourcePathsDeleteDecisions.find(sourcePathID);
      if (sourcePathsDeleteDecision == sourcePathsDeleteDecisions.end())
      {
        bool sourcePathNotExists = (sourceNotFound || !CDirectory::Exists(sourcePath, false));
        // if the parent path exists, the file will be deleted without asking
        // if the parent path doesn't exist or does not belong to a valid media source,
        // ask the user whether to remove all items it contained
        if (sourcePathNotExists)
        {
          // in silent mode assume that the files are just temporarily missing
          if (silent)
            del = false;
          else
          {
            CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
            if (pDialog != NULL)
            {
              CURL sourceUrl(sourcePath);
              pDialog->SetHeading(CVariant{15012});
              pDialog->SetText(CVariant{StringUtils::Format(g_localizeStrings.Get(15013).c_str(), sourceUrl.GetWithoutUserDetails().c_str())});
              pDialog->SetChoice(0, CVariant{15015});
              pDialog->SetChoice(1, CVariant{15014});
              pDialog->Open();

              del = !pDialog->IsConfirmed();
            }
          }
        }

        sourcePathsDeleteDecisions.insert(std::make_pair(sourcePathID, std::make_pair(sourcePathNotExists, del)));
        pathsDeleteDecisions.insert(std::make_pair(sourcePathID, sourcePathNotExists && del));
      }
      // the only reason not to delete the file is if the parent path doesn't
      // exist and the user decided to delete all the items it contained
      else if (sourcePathsDeleteDecision->second.first &&
               !sourcePathsDeleteDecision->second.second)
        del = false;

      if (scanSettings.parent_name)
        pathsDeleteDecisions.insert(std::make_pair(m_pDS2->fv(2).get_asInt(), del));
    }

    if (del)
    {
      deletedFileIDs += m_pDS2->fv(1).get_asString() + ",";
      cleanedIDs.push_back(m_pDS2->fv(0).get_asInt());
    }

    m_pDS2->next();
  }
  m_pDS2->close();

  return cleanedIDs;
}

void CVideoDatabase::DumpToDummyFiles(const std::string &path)
{
  // get all tvshows
  CFileItemList items;
  GetTvShowsByWhere("videodb://tvshows/titles/", "", items);
  std::string showPath = URIUtils::AddFileToFolder(path, "shows");
  CDirectory::Create(showPath);
  for (int i = 0; i < items.Size(); i++)
  {
    // create a folder in this directory
    std::string showName = CUtil::MakeLegalFileName(items[i]->GetVideoInfoTag()->m_strShowTitle);
    std::string TVFolder = URIUtils::AddFileToFolder(showPath, showName);
    if (CDirectory::Create(TVFolder))
    { // right - grab the episodes and dump them as well
      CFileItemList episodes;
      Filter filter(PrepareSQL("idShow=%i", items[i]->GetVideoInfoTag()->m_iDbId));
      GetEpisodesByWhere("videodb://tvshows/titles/", filter, episodes);
      for (int i = 0; i < episodes.Size(); i++)
      {
        CVideoInfoTag *tag = episodes[i]->GetVideoInfoTag();
        std::string episode = StringUtils::Format("%s.s%02de%02d.avi", showName.c_str(), tag->m_iSeason, tag->m_iEpisode);
        // and make a file
        std::string episodePath = URIUtils::AddFileToFolder(TVFolder, episode);
        CFile file;
        if (file.OpenForWrite(episodePath))
          file.Close();
      }
    }
  }
  // get all movies
  items.Clear();
  GetMoviesByWhere("videodb://movies/titles/", "", items);
  std::string moviePath = URIUtils::AddFileToFolder(path, "movies");
  CDirectory::Create(moviePath);
  for (int i = 0; i < items.Size(); i++)
  {
    CVideoInfoTag *tag = items[i]->GetVideoInfoTag();
    std::string movie = StringUtils::Format("%s.avi", tag->m_strTitle.c_str());
    CFile file;
    if (file.OpenForWrite(URIUtils::AddFileToFolder(moviePath, movie)))
      file.Close();
  }
}

void CVideoDatabase::ExportToXML(const std::string &path, bool singleFile /* = true */, bool images /* = false */, bool actorThumbs /* false */, bool overwrite /*=false*/)
{
  int iFailCount = 0;
  CGUIDialogProgress *progress=NULL;
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;
    if (NULL == m_pDS2.get()) return;

    // create a 3rd dataset as well as GetEpisodeDetails() etc. uses m_pDS2, and we need to do 3 nested queries on tv shows
    std::unique_ptr<Dataset> pDS;
    pDS.reset(m_pDB->CreateDataset());
    if (NULL == pDS.get()) return;

    std::unique_ptr<Dataset> pDS2;
    pDS2.reset(m_pDB->CreateDataset());
    if (NULL == pDS2.get()) return;

    // if we're exporting to a single folder, we export thumbs as well
    std::string exportRoot = URIUtils::AddFileToFolder(path, "xbmc_videodb_" + CDateTime::GetCurrentDateTime().GetAsDBDate());
    std::string xmlFile = URIUtils::AddFileToFolder(exportRoot, "videodb.xml");
    std::string actorsDir = URIUtils::AddFileToFolder(exportRoot, "actors");
    std::string moviesDir = URIUtils::AddFileToFolder(exportRoot, "movies");
    std::string musicvideosDir = URIUtils::AddFileToFolder(exportRoot, "musicvideos");
    std::string tvshowsDir = URIUtils::AddFileToFolder(exportRoot, "tvshows");
    if (singleFile)
    {
      images = true;
      overwrite = false;
      actorThumbs = true;
      CDirectory::Remove(exportRoot);
      CDirectory::Create(exportRoot);
      CDirectory::Create(actorsDir);
      CDirectory::Create(moviesDir);
      CDirectory::Create(musicvideosDir);
      CDirectory::Create(tvshowsDir);
    }

    progress = (CGUIDialogProgress *)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
    // find all movies
    std::string sql = "select * from movie_view";

    m_pDS->query(sql);

    if (progress)
    {
      progress->SetHeading(CVariant{647});
      progress->SetLine(0, CVariant{650});
      progress->SetLine(1, CVariant{""});
      progress->SetLine(2, CVariant{""});
      progress->SetPercentage(0);
      progress->Open();
      progress->ShowProgressBar(true);
    }

    int total = m_pDS->num_rows();
    int current = 0;

    // create our xml document
    CXBMCTinyXML xmlDoc;
    TiXmlDeclaration decl("1.0", "UTF-8", "yes");
    xmlDoc.InsertEndChild(decl);
    TiXmlNode *pMain = NULL;
    if (!singleFile)
      pMain = &xmlDoc;
    else
    {
      TiXmlElement xmlMainElement("videodb");
      pMain = xmlDoc.InsertEndChild(xmlMainElement);
      XMLUtils::SetInt(pMain,"version", GetExportVersion());
    }

    while (!m_pDS->eof())
    {
      // CVideoInfoTag movie = GetDetailsForMovie(m_pDS, VideoDbDetailsAll);
      CVideoInfoTag movie;
      movie.Reset();
      // strip paths to make them relative
      if (StringUtils::StartsWith(movie.m_strTrailer, movie.m_strPath))
        movie.m_strTrailer = movie.m_strTrailer.substr(movie.m_strPath.size());
      std::map<std::string, std::string> artwork;
      if (GetArtForItem(movie.m_iDbId, movie.m_type, artwork) && singleFile)
      {
        TiXmlElement additionalNode("art");
        for (const auto &i : artwork)
          XMLUtils::SetString(&additionalNode, i.first.c_str(), i.second);
        movie.Save(pMain, "movie", true, &additionalNode);
      }
      else
        movie.Save(pMain, "movie", singleFile);

      // reset old skip state
      bool bSkip = false;

      if (progress)
      {
        progress->SetLine(1, CVariant{movie.m_strTitle});
        progress->SetPercentage(current * 100 / total);
        progress->Progress();
        if (progress->IsCanceled())
        {
          progress->Close();
          m_pDS->close();
          return;
        }
      }

      CFileItem item(movie.m_strFileNameAndPath,false);
      if (!singleFile && CUtil::SupportsWriteFileOperations(movie.m_strFileNameAndPath))
      {
        if (!item.Exists(false))
        {
          CLog::Log(LOGDEBUG, "%s - Not exporting item %s as it does not exist", __FUNCTION__, movie.m_strFileNameAndPath.c_str());
          bSkip = true;
        }
        else
        {
          std::string nfoFile(URIUtils::ReplaceExtension(item.GetTBNFile(), ".nfo"));

          if (item.IsOpticalMediaFile())
          {
            nfoFile = URIUtils::AddFileToFolder(
                                    URIUtils::GetParentPath(nfoFile),
                                    URIUtils::GetFileName(nfoFile));
          }

          if (overwrite || !CFile::Exists(nfoFile, false))
          {
            if(!xmlDoc.SaveFile(nfoFile))
            {
              CLog::Log(LOGERROR, "%s: Movie nfo export failed! ('%s')", __FUNCTION__, nfoFile.c_str());
              CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, g_localizeStrings.Get(20302), nfoFile);
              iFailCount++;
            }
          }
        }
      }
      if (!singleFile)
      {
        xmlDoc.Clear();
        TiXmlDeclaration decl("1.0", "UTF-8", "yes");
        xmlDoc.InsertEndChild(decl);
      }

      if (images && !bSkip)
      {
        if (singleFile)
        {
          std::string strFileName(movie.m_strTitle);
          if (movie.HasYear())
            strFileName += StringUtils::Format("_%i", movie.GetYear());
          item.SetPath(GetSafeFile(moviesDir, strFileName) + ".avi");
        }
        for (const auto &i : artwork)
        {
          std::string savedThumb = item.GetLocalArt(i.first, false);
          CTextureCache::GetInstance().Export(i.second, savedThumb, overwrite);
        }
        if (actorThumbs)
          ExportActorThumbs(actorsDir, movie, !singleFile, overwrite);
      }
      m_pDS->next();
      current++;
    }
    m_pDS->close();

    // find all musicvideos
    sql = "select * from musicvideo_view";

    m_pDS->query(sql);

    total = m_pDS->num_rows();
    current = 0;

    while (!m_pDS->eof())
    {
      CVideoInfoTag movie = GetDetailsForMusicVideo(m_pDS, VideoDbDetailsAll);
      std::map<std::string, std::string> artwork;
      if (GetArtForItem(movie.m_iDbId, movie.m_type, artwork) && singleFile)
      {
        TiXmlElement additionalNode("art");
        for (const auto &i : artwork)
          XMLUtils::SetString(&additionalNode, i.first.c_str(), i.second);
        movie.Save(pMain, "musicvideo", true, &additionalNode);
      }
      else
        movie.Save(pMain, "musicvideo", singleFile);

      // reset old skip state
      bool bSkip = false;

      if (progress)
      {
        progress->SetLine(1, CVariant{movie.m_strTitle});
        progress->SetPercentage(current * 100 / total);
        progress->Progress();
        if (progress->IsCanceled())
        {
          progress->Close();
          m_pDS->close();
          return;
        }
      }

      CFileItem item(movie.m_strFileNameAndPath,false);
      if (!singleFile && CUtil::SupportsWriteFileOperations(movie.m_strFileNameAndPath))
      {
        if (!item.Exists(false))
        {
          CLog::Log(LOGDEBUG, "%s - Not exporting item %s as it does not exist", __FUNCTION__, movie.m_strFileNameAndPath.c_str());
          bSkip = true;
        }
        else
        {
          std::string nfoFile(URIUtils::ReplaceExtension(item.GetTBNFile(), ".nfo"));

          if (overwrite || !CFile::Exists(nfoFile, false))
          {
            if(!xmlDoc.SaveFile(nfoFile))
            {
              CLog::Log(LOGERROR, "%s: Musicvideo nfo export failed! ('%s')", __FUNCTION__, nfoFile.c_str());
              CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, g_localizeStrings.Get(20302), nfoFile);
              iFailCount++;
            }
          }
        }
      }
      if (!singleFile)
      {
        xmlDoc.Clear();
        TiXmlDeclaration decl("1.0", "UTF-8", "yes");
        xmlDoc.InsertEndChild(decl);
      }
      if (images && !bSkip)
      {
        if (singleFile)
        {
          std::string strFileName(StringUtils::Join(movie.m_artist, g_advancedSettings.m_videoItemSeparator) + "." + movie.m_strTitle);
          if (movie.HasYear())
            strFileName += StringUtils::Format("_%i", movie.GetYear());
          item.SetPath(GetSafeFile(moviesDir, strFileName) + ".avi");
        }
        for (const auto &i : artwork)
        {
          std::string savedThumb = item.GetLocalArt(i.first, false);
          CTextureCache::GetInstance().Export(i.second, savedThumb, overwrite);
        }
      }
      m_pDS->next();
      current++;
    }
    m_pDS->close();

    // repeat for all tvshows
    sql = "SELECT * FROM tvshow_view";
    m_pDS->query(sql);

    total = m_pDS->num_rows();
    current = 0;

    while (!m_pDS->eof())
    {
      CVideoInfoTag tvshow = GetDetailsForTvShow(m_pDS, VideoDbDetailsAll);

      std::map<int, std::map<std::string, std::string> > seasonArt;
      GetTvShowSeasonArt(tvshow.m_iDbId, seasonArt);

      std::map<std::string, std::string> artwork;
      if (GetArtForItem(tvshow.m_iDbId, tvshow.m_type, artwork) && singleFile)
      {
        TiXmlElement additionalNode("art");
        for (const auto &i : artwork)
          XMLUtils::SetString(&additionalNode, i.first.c_str(), i.second);
        for (const auto &i : seasonArt)
        {
          TiXmlElement seasonNode("season");
          seasonNode.SetAttribute("num", i.first);
          for (const auto &j : i.second)
            XMLUtils::SetString(&seasonNode, j.first.c_str(), j.second);
          additionalNode.InsertEndChild(seasonNode);
        }
        tvshow.Save(pMain, "tvshow", true, &additionalNode);
      }
      else
        tvshow.Save(pMain, "tvshow", singleFile);

      // reset old skip state
      bool bSkip = false;

      if (progress)
      {
        progress->SetLine(1, CVariant{tvshow.m_strTitle});
        progress->SetPercentage(current * 100 / total);
        progress->Progress();
        if (progress->IsCanceled())
        {
          progress->Close();
          m_pDS->close();
          return;
        }
      }

      CFileItem item(tvshow.m_strPath, true);
      if (!singleFile && CUtil::SupportsWriteFileOperations(tvshow.m_strPath))
      {
        if (!item.Exists(false))
        {
          CLog::Log(LOGDEBUG, "%s - Not exporting item %s as it does not exist", __FUNCTION__, tvshow.m_strPath.c_str());
          bSkip = true;
        }
        else
        {
          std::string nfoFile = URIUtils::AddFileToFolder(tvshow.m_strPath, "tvshow.nfo");

          if (overwrite || !CFile::Exists(nfoFile, false))
          {
            if(!xmlDoc.SaveFile(nfoFile))
            {
              CLog::Log(LOGERROR, "%s: TVShow nfo export failed! ('%s')", __FUNCTION__, nfoFile.c_str());
              CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, g_localizeStrings.Get(20302), nfoFile);
              iFailCount++;
            }
          }
        }
      }
      if (!singleFile)
      {
        xmlDoc.Clear();
        TiXmlDeclaration decl("1.0", "UTF-8", "yes");
        xmlDoc.InsertEndChild(decl);
      }
      if (images && !bSkip)
      {
        if (singleFile)
          item.SetPath(GetSafeFile(tvshowsDir, tvshow.m_strTitle));

        for (const auto &i : artwork)
        {
          std::string savedThumb = item.GetLocalArt(i.first, true);
          CTextureCache::GetInstance().Export(i.second, savedThumb, overwrite);
        }

        if (actorThumbs)
          ExportActorThumbs(actorsDir, tvshow, !singleFile, overwrite);

        // export season thumbs
        for (const auto &i : seasonArt)
        {
          std::string seasonThumb;
          if (i.first == -1)
            seasonThumb = "season-all";
          else if (i.first == 0)
            seasonThumb = "season-specials";
          else
            seasonThumb = StringUtils::Format("season%02i", i.first);
          for (const auto &j : i.second)
          {
            std::string savedThumb(item.GetLocalArt(seasonThumb + "-" + j.first, true));
            if (!i.second.empty())
              CTextureCache::GetInstance().Export(j.second, savedThumb, overwrite);
          }
        }
      }

      // now save the episodes from this show
      sql = PrepareSQL("select * from episode_view where idShow=%i order by strFileName, idEpisode",tvshow.m_iDbId);
      pDS->query(sql);
      std::string showDir(item.GetPath());

      while (!pDS->eof())
      {
        CVideoInfoTag episode = GetDetailsForEpisode(pDS, VideoDbDetailsAll);
        std::map<std::string, std::string> artwork;
        if (GetArtForItem(episode.m_iDbId, MediaTypeEpisode, artwork) && singleFile)
        {
          TiXmlElement additionalNode("art");
          for (const auto &i : artwork)
            XMLUtils::SetString(&additionalNode, i.first.c_str(), i.second);
          episode.Save(pMain->LastChild(), "episodedetails", true, &additionalNode);
        }
        else if (singleFile)
          episode.Save(pMain->LastChild(), "episodedetails", singleFile);
        else
          episode.Save(pMain, "episodedetails", singleFile);
        pDS->next();
        // multi-episode files need dumping to the same XML
        while (!singleFile && !pDS->eof() &&
               episode.m_iFileId == pDS->fv("idFile").get_asInt())
        {
          episode = GetDetailsForEpisode(pDS, VideoDbDetailsAll);
          episode.Save(pMain, "episodedetails", singleFile);
          pDS->next();
        }

        // reset old skip state
        bool bSkip = false;

        CFileItem item(episode.m_strFileNameAndPath, false);
        if (!singleFile && CUtil::SupportsWriteFileOperations(episode.m_strFileNameAndPath))
        {
          if (!item.Exists(false))
          {
            CLog::Log(LOGDEBUG, "%s - Not exporting item %s as it does not exist", __FUNCTION__, episode.m_strFileNameAndPath.c_str());
            bSkip = true;
          }
          else
          {
            std::string nfoFile(URIUtils::ReplaceExtension(item.GetTBNFile(), ".nfo"));

            if (overwrite || !CFile::Exists(nfoFile, false))
            {
              if(!xmlDoc.SaveFile(nfoFile))
              {
                CLog::Log(LOGERROR, "%s: Episode nfo export failed! ('%s')", __FUNCTION__, nfoFile.c_str());
                CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, g_localizeStrings.Get(20302), nfoFile);
                iFailCount++;
              }
            }
          }
        }
        if (!singleFile)
        {
          xmlDoc.Clear();
          TiXmlDeclaration decl("1.0", "UTF-8", "yes");
          xmlDoc.InsertEndChild(decl);
        }

        if (images && !bSkip)
        {
          if (singleFile)
          {
            std::string epName = StringUtils::Format("s%02ie%02i.avi", episode.m_iSeason, episode.m_iEpisode);
            item.SetPath(URIUtils::AddFileToFolder(showDir, epName));
          }
          for (const auto &i : artwork)
          {
            std::string savedThumb = item.GetLocalArt(i.first, false);
            CTextureCache::GetInstance().Export(i.second, savedThumb, overwrite);
          }
          if (actorThumbs)
            ExportActorThumbs(actorsDir, episode, !singleFile, overwrite);
        }
      }
      pDS->close();
      m_pDS->next();
      current++;
    }
    m_pDS->close();

    if (!singleFile && progress)
    {
      progress->SetPercentage(100);
      progress->Progress();
    }

    if (singleFile)
    {
      // now dump path info
      std::set<std::string> paths;
      GetPaths(paths);
      TiXmlElement xmlPathElement("paths");
      TiXmlNode *pPaths = pMain->InsertEndChild(xmlPathElement);
      for (const auto &i : paths)
      {
        bool foundDirectly = false;
        SScanSettings settings;
        ScraperPtr info = GetScraperForPath(i, settings, foundDirectly);
        if (info && foundDirectly)
        {
          TiXmlElement xmlPathElement2("path");
          TiXmlNode *pPath = pPaths->InsertEndChild(xmlPathElement2);
          XMLUtils::SetString(pPath,"url", i);
          XMLUtils::SetInt(pPath,"scanrecursive", settings.recurse);
          XMLUtils::SetBoolean(pPath,"usefoldernames", settings.parent_name);
          XMLUtils::SetString(pPath,"content", TranslateContent(info->Content()));
          XMLUtils::SetString(pPath,"scraperpath", info->ID());
        }
      }
      xmlDoc.SaveFile(xmlFile);
    }
    CVariant data;
    if (singleFile)
    {
      data["root"] = exportRoot;
      data["file"] = xmlFile;
      if (iFailCount > 0)
        data["failcount"] = iFailCount;
    }
    ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnExport", data);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
    iFailCount++;
  }

  if (progress)
    progress->Close();

  if (iFailCount > 0)
    CGUIDialogOK::ShowAndGetInput(CVariant{647}, CVariant{StringUtils::Format(g_localizeStrings.Get(15011).c_str(), iFailCount)});
}

void CVideoDatabase::ExportActorThumbs(const std::string &strDir, const CVideoInfoTag &tag, bool singleFiles, bool overwrite /*=false*/)
{
  std::string strPath(strDir);
  if (singleFiles)
  {
    strPath = URIUtils::AddFileToFolder(tag.m_strPath, ".actors");
    if (!CDirectory::Exists(strPath))
    {
      CDirectory::Create(strPath);
      CFile::SetHidden(strPath, true);
    }
  }

  for (const auto &i : tag.m_cast)
  {
    CFileItem item;
    item.SetLabel(i.strName);
    if (!i.thumb.empty())
    {
      std::string thumbFile(GetSafeFile(strPath, i.strName));
      CTextureCache::GetInstance().Export(i.thumb, thumbFile, overwrite);
    }
  }
}

void CVideoDatabase::ImportFromXML(const std::string &path)
{
  CGUIDialogProgress *progress=NULL;
  try
  {
    if (NULL == m_pDB.get()) return;
    if (NULL == m_pDS.get()) return;

    CXBMCTinyXML xmlDoc;
    if (!xmlDoc.LoadFile(URIUtils::AddFileToFolder(path, "videodb.xml")))
      return;

    TiXmlElement *root = xmlDoc.RootElement();
    if (!root) return;

    progress = (CGUIDialogProgress *)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
    if (progress)
    {
      progress->SetHeading(CVariant{648});
      progress->SetLine(0, CVariant{649});
      progress->SetLine(1, CVariant{330});
      progress->SetLine(2, CVariant{""});
      progress->SetPercentage(0);
      progress->Open();
      progress->ShowProgressBar(true);
    }

    int iVersion = 0;
    XMLUtils::GetInt(root, "version", iVersion);

    CLog::Log(LOGDEBUG, "%s: Starting import (export version = %i)", __FUNCTION__, iVersion);

    TiXmlElement *movie = root->FirstChildElement();
    int current = 0;
    int total = 0;
    // first count the number of items...
    while (movie)
    {
      if (strnicmp(movie->Value(), MediaTypeMovie, 5)==0 ||
          strnicmp(movie->Value(), MediaTypeTvShow, 6)==0 ||
          strnicmp(movie->Value(), MediaTypeMusicVideo,10)==0 )
        total++;
      movie = movie->NextSiblingElement();
    }

    std::string actorsDir(URIUtils::AddFileToFolder(path, "actors"));
    std::string moviesDir(URIUtils::AddFileToFolder(path, "movies"));
    std::string musicvideosDir(URIUtils::AddFileToFolder(path, "musicvideos"));
    std::string tvshowsDir(URIUtils::AddFileToFolder(path, "tvshows"));
    CVideoInfoScanner scanner;
    // add paths first (so we have scraper settings available)
    TiXmlElement *path = root->FirstChildElement("paths");
    path = path->FirstChildElement();
    while (path)
    {
      std::string strPath;
      if (XMLUtils::GetString(path,"url",strPath) && !strPath.empty())
        AddPath(strPath);

      std::string content;
      if (XMLUtils::GetString(path,"content", content) && !content.empty())
      { // check the scraper exists, if so store the path
        AddonPtr addon;
        std::string id;
        XMLUtils::GetString(path,"scraperpath",id);
        if (CAddonMgr::GetInstance().GetAddon(id, addon))
        {
          SScanSettings settings;
          ScraperPtr scraper = std::dynamic_pointer_cast<CScraper>(addon);
          // FIXME: scraper settings are not exported?
          scraper->SetPathSettings(TranslateContent(content), "");
          XMLUtils::GetInt(path,"scanrecursive",settings.recurse);
          XMLUtils::GetBoolean(path,"usefoldernames",settings.parent_name);
          SetScraperForPath(strPath,scraper,settings);
        }
      }
      path = path->NextSiblingElement();
    }
    movie = root->FirstChildElement();
    while (movie)
    {
      CVideoInfoTag info;
      if (strnicmp(movie->Value(), MediaTypeMovie, 5) == 0)
      {
        info.Load(movie);
        CFileItem item(info);
        bool useFolders = info.m_basePath.empty() ? LookupByFolders(item.GetPath()) : false;
        std::string filename = info.m_strTitle;
        if (info.HasYear())
          filename += StringUtils::Format("_%i", info.GetYear());
        CFileItem artItem(item);
        artItem.SetPath(GetSafeFile(moviesDir, filename) + ".avi");
        scanner.GetArtwork(&artItem, CONTENT_MOVIES, useFolders, true, actorsDir);
        item.SetArt(artItem.GetArt());
        scanner.AddVideo(&item, CONTENT_MOVIES, useFolders, true, NULL, true);
        current++;
      }
      else if (strnicmp(movie->Value(), MediaTypeMusicVideo, 10) == 0)
      {
        info.Load(movie);
        CFileItem item(info);
        bool useFolders = info.m_basePath.empty() ? LookupByFolders(item.GetPath()) : false;
        std::string filename = StringUtils::Join(info.m_artist, g_advancedSettings.m_videoItemSeparator) + "." + info.m_strTitle;
        if (info.HasYear())
          filename += StringUtils::Format("_%i", info.GetYear());
        CFileItem artItem(item);
        artItem.SetPath(GetSafeFile(musicvideosDir, filename) + ".avi");
        scanner.GetArtwork(&artItem, CONTENT_MUSICVIDEOS, useFolders, true, actorsDir);
        item.SetArt(artItem.GetArt());
        scanner.AddVideo(&item, CONTENT_MUSICVIDEOS, useFolders, true, NULL, true);
        current++;
      }
      else if (strnicmp(movie->Value(), MediaTypeTvShow, 6) == 0)
      {
        // load the TV show in.  NOTE: This deletes all episodes under the TV Show, which may not be
        // what we desire.  It may make better sense to only delete (or even better, update) the show information
        info.Load(movie);
        URIUtils::AddSlashAtEnd(info.m_strPath);
        DeleteTvShow(info.m_strPath);
        CFileItem showItem(info);
        bool useFolders = info.m_basePath.empty() ? LookupByFolders(showItem.GetPath(), true) : false;
        CFileItem artItem(showItem);
        std::string artPath(GetSafeFile(tvshowsDir, info.m_strTitle));
        artItem.SetPath(artPath);
        scanner.GetArtwork(&artItem, CONTENT_TVSHOWS, useFolders, true, actorsDir);
        showItem.SetArt(artItem.GetArt());
        int showID = scanner.AddVideo(&showItem, CONTENT_TVSHOWS, useFolders, true, NULL, true);
        // season artwork
        std::map<int, std::map<std::string, std::string> > seasonArt;
        artItem.GetVideoInfoTag()->m_strPath = artPath;
        scanner.GetSeasonThumbs(*artItem.GetVideoInfoTag(), seasonArt, CVideoThumbLoader::GetArtTypes(MediaTypeSeason), true);
        for (const auto &i : seasonArt)
        {
          int seasonID = AddSeason(showID, i.first);
          SetArtForItem(seasonID, MediaTypeSeason, i.second);
        }
        current++;
        // now load the episodes
        TiXmlElement *episode = movie->FirstChildElement("episodedetails");
        while (episode)
        {
          // no need to delete the episode info, due to the above deletion
          CVideoInfoTag info;
          info.Load(episode);
          CFileItem item(info);
          std::string filename = StringUtils::Format("s%02ie%02i.avi", info.m_iSeason, info.m_iEpisode);
          CFileItem artItem(item);
          artItem.SetPath(GetSafeFile(artPath, filename));
          scanner.GetArtwork(&artItem, CONTENT_TVSHOWS, useFolders, true, actorsDir);
          item.SetArt(artItem.GetArt());
          scanner.AddVideo(&item,CONTENT_TVSHOWS, false, false, showItem.GetVideoInfoTag(), true);
          episode = episode->NextSiblingElement("episodedetails");
        }
      }
      movie = movie->NextSiblingElement();
      if (progress && total)
      {
        progress->SetPercentage(current * 100 / total);
        progress->SetLine(2, CVariant{info.m_strTitle});
        progress->Progress();
        if (progress->IsCanceled())
        {
          progress->Close();
          return;
        }
      }
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s failed", __FUNCTION__);
  }
  if (progress)
    progress->Close();
}

bool CVideoDatabase::ImportArtFromXML(const TiXmlNode *node, std::map<std::string, std::string> &artwork)
{
  if (!node) return false;
  const TiXmlNode *art = node->FirstChild();
  while (art && art->FirstChild())
  {
    artwork.insert(make_pair(art->ValueStr(), art->FirstChild()->ValueStr()));
    art = art->NextSibling();
  }
  return !artwork.empty();
}

void CVideoDatabase::ConstructPath(std::string& strDest, const std::string& strPath, const std::string& strFileName)
{
  if (URIUtils::IsStack(strFileName) || 
      URIUtils::IsInArchive(strFileName) || URIUtils::IsPlugin(strPath))
    strDest = strFileName;
  else
    strDest = URIUtils::AddFileToFolder(strPath, strFileName);
}

void CVideoDatabase::SplitPath(const std::string& strFileNameAndPath, std::string& strPath, std::string& strFileName)
{
  if (URIUtils::IsStack(strFileNameAndPath) || StringUtils::StartsWithNoCase(strFileNameAndPath, "rar://") || StringUtils::StartsWithNoCase(strFileNameAndPath, "zip://"))
  {
    URIUtils::GetParentPath(strFileNameAndPath,strPath);
    strFileName = strFileNameAndPath;
  }
  else if (URIUtils::IsPlugin(strFileNameAndPath))
  {
    CURL url(strFileNameAndPath);
    strPath = url.GetWithoutFilename();
    strFileName = strFileNameAndPath;
  }
  else
    URIUtils::Split(strFileNameAndPath,strPath, strFileName);
}

void CVideoDatabase::InvalidatePathHash(const std::string& strPath)
{
  SScanSettings settings;
  bool foundDirectly;
  ScraperPtr info = GetScraperForPath(strPath,settings,foundDirectly);
  SetPathHash(strPath,"");
  if (!info)
    return;
  if (info->Content() == CONTENT_TVSHOWS || (info->Content() == CONTENT_MOVIES && !foundDirectly)) // if we scan by folder name we need to invalidate parent as well
  {
    if (info->Content() == CONTENT_TVSHOWS || settings.parent_name_root)
    {
      std::string strParent;
      URIUtils::GetParentPath(strPath,strParent);
      SetPathHash(strParent,"");
    }
  }
}

bool CVideoDatabase::CommitTransaction()
{
  if (CDatabase::CommitTransaction())
  { // number of items in the db has likely changed, so recalculate
    g_infoManager.SetLibraryBool(LIBRARY_HAS_MOVIES, HasContent(VIDEODB_CONTENT_MOVIES));
    g_infoManager.SetLibraryBool(LIBRARY_HAS_TVSHOWS, HasContent(VIDEODB_CONTENT_TVSHOWS));
    g_infoManager.SetLibraryBool(LIBRARY_HAS_MUSICVIDEOS, HasContent(VIDEODB_CONTENT_MUSICVIDEOS));
    return true;
  }
  return false;
}

bool CVideoDatabase::SetSingleValue(VIDEODB_CONTENT_TYPE type, int dbId, int dbField, const std::string &strValue)
{
  std::string strSQL;
  try
  {
    if (NULL == m_pDB.get() || NULL == m_pDS.get())
      return false;

    std::string strTable, strField;
    if (type == VIDEODB_CONTENT_MOVIES)
    {
      strTable = "movie";
      strField = "idMovie";
    }
    else if (type == VIDEODB_CONTENT_TVSHOWS)
    {
      strTable = "tvshow";
      strField = "idShow";
    }
    else if (type == VIDEODB_CONTENT_EPISODES)
    {
      strTable = "episode";
      strField = "idEpisode";
    }
    else if (type == VIDEODB_CONTENT_MUSICVIDEOS)
    {
      strTable = "musicvideo";
      strField = "idMVideo";
    }

    if (strTable.empty())
      return false;

    return SetSingleValue(strTable, StringUtils::Format("c%02u", dbField), strValue, strField, dbId);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, strSQL.c_str());
  }
  return false;
}

bool CVideoDatabase::SetSingleValue(VIDEODB_CONTENT_TYPE type, int dbId, Field dbField, const std::string &strValue)
{
  MediaType mediaType = DatabaseUtils::MediaTypeFromVideoContentType(type);
  if (mediaType == MediaTypeNone)
    return false;

  int dbFieldIndex = DatabaseUtils::GetField(dbField, mediaType);
  if (dbFieldIndex < 0)
    return false;

  return SetSingleValue(type, dbId, dbFieldIndex, strValue);
}

bool CVideoDatabase::SetSingleValue(const std::string &table, const std::string &fieldName, const std::string &strValue,
                                    const std::string &conditionName /* = "" */, int conditionValue /* = -1 */)
{
  if (table.empty() || fieldName.empty())
    return false;

  std::string sql;
  try
  {
    if (NULL == m_pDB.get() || NULL == m_pDS.get())
      return false;

    sql = PrepareSQL("UPDATE %s SET %s='%s'", table.c_str(), fieldName.c_str(), strValue.c_str());
    if (!conditionName.empty())
      sql += PrepareSQL(" WHERE %s=%u", conditionName.c_str(), conditionValue);
    if (m_pDS->exec(sql) == 0)
      return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%s) failed", __FUNCTION__, sql.c_str());
  }
  return false;
}

std::string CVideoDatabase::GetSafeFile(const std::string &dir, const std::string &name) const
{
  std::string safeThumb(name);
  StringUtils::Replace(safeThumb, ' ', '_');
  return URIUtils::AddFileToFolder(dir, CUtil::MakeLegalFileName(safeThumb));
}

void CVideoDatabase::AnnounceRemove(std::string content, int id, bool scanning /* = false */)
{
  CVariant data;
  data["type"] = content;
  data["id"] = id;
  if (scanning)
    data["transaction"] = true;
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnRemove", data);
}

void CVideoDatabase::AnnounceUpdate(std::string content, int id)
{
  CVariant data;
  data["type"] = content;
  data["id"] = id;
  ANNOUNCEMENT::CAnnouncementManager::GetInstance().Announce(ANNOUNCEMENT::VideoLibrary, "xbmc", "OnUpdate", data);
}

bool CVideoDatabase::GetItemsForPath(const std::string &content, const std::string &strPath, CFileItemList &items)
{
  std::string path(strPath);
  
  if(URIUtils::IsMultiPath(path))
  {
    std::vector<std::string> paths;
    CMultiPathDirectory::GetPaths(path, paths);

    for(unsigned i=0;i<paths.size();i++)
      GetItemsForPath(content, paths[i], items);

    return items.Size() > 0;
  }
  
  int pathID = GetPathId(path);
  if (pathID < 0)
    return false;

  if (content == "movies")
  {
    Filter filter(PrepareSQL("c%02d=%d", VIDEODB_ID_PARENTPATHID, pathID));
    GetMoviesByWhere("videodb://movies/titles/", filter, items);
  }
  else if (content == "episodes")
  {
    Filter filter(PrepareSQL("c%02d=%d", VIDEODB_ID_EPISODE_PARENTPATHID, pathID));
    GetEpisodesByWhere("videodb://tvshows/titles/", filter, items);
  }
  else if (content == "tvshows")
  {
    Filter filter(PrepareSQL("idParentPath=%d", pathID));
    GetTvShowsByWhere("videodb://tvshows/titles/", filter, items);
  }
  else if (content == "musicvideos")
  {
    Filter filter(PrepareSQL("c%02d=%d", VIDEODB_ID_MUSICVIDEO_PARENTPATHID, pathID));
    GetMusicVideosByWhere("videodb://musicvideos/titles/", filter, items);
  }
  for (int i = 0; i < items.Size(); i++)
    items[i]->SetPath(items[i]->GetVideoInfoTag()->m_basePath);
  return items.Size() > 0;
}

void CVideoDatabase::AppendIdLinkFilter(const char* field, const char *table, const MediaType& mediaType, const char *view, const char *viewKey, const CUrlOptions::UrlOptions& options, Filter &filter)
{
  auto option = options.find((std::string)field + "id");
  if (option == options.end())
    return;

  filter.AppendJoin(PrepareSQL("JOIN %s_link ON %s_link.media_id=%s_view.%s AND %s_link.media_type='%s'", field, field, view, viewKey, field, mediaType.c_str()));
  filter.AppendWhere(PrepareSQL("%s_link.%s_id = %i", field, table, (int)option->second.asInteger()));
}

void CVideoDatabase::AppendLinkFilter(const char* field, const char *table, const MediaType& mediaType, const char *view, const char *viewKey, const CUrlOptions::UrlOptions& options, Filter &filter)
{
  auto option = options.find(field);
  if (option == options.end())
    return;

  filter.AppendJoin(PrepareSQL("JOIN %s_link ON %s_link.media_id=%s_view.%s AND %s_link.media_type='%s'", field, field, view, viewKey, field, mediaType.c_str()));
  filter.AppendJoin(PrepareSQL("JOIN %s ON %s.%s_id=%s_link.%s_id", table, table, field, table, field));
  filter.AppendWhere(PrepareSQL("%s.name like '%s'", table, option->second.asString().c_str()));
}

bool CVideoDatabase::GetFilter(CDbUrl &videoUrl, Filter &filter, SortDescription &sorting)
{
  if (!videoUrl.IsValid())
    return false;

  std::string type = videoUrl.GetType();
  std::string itemType = ((const CVideoDbUrl &)videoUrl).GetItemType();
  const CUrlOptions::UrlOptions& options = videoUrl.GetOptions();

  if (type == "movies")
  {
    AppendIdLinkFilter("genre", "genre", "movie", "movie", "idMovie", options, filter);
    AppendLinkFilter("genre", "genre", "movie", "movie", "idMovie", options, filter);

    AppendIdLinkFilter("country", "country", "movie", "movie", "idMovie", options, filter);
    AppendLinkFilter("country", "country", "movie", "movie", "idMovie", options, filter);

    AppendIdLinkFilter("studio", "studio", "movie", "movie", "idMovie", options, filter);
    AppendLinkFilter("studio", "studio", "movie", "movie", "idMovie", options, filter);

    AppendIdLinkFilter("director", "actor", "movie", "movie", "idMovie", options, filter);
    AppendLinkFilter("director", "actor", "movie", "movie", "idMovie", options, filter);

    auto option = options.find("year");
    if (option != options.end())
      filter.AppendWhere(PrepareSQL("movie_view.premiered like '%i%%'", (int)option->second.asInteger()));

    AppendIdLinkFilter("actor", "actor", "movie", "movie", "idMovie", options, filter);
    AppendLinkFilter("actor", "actor", "movie", "movie", "idMovie", options, filter);

    option = options.find("setid");
    if (option != options.end())
      filter.AppendWhere(PrepareSQL("movie_view.idSet = %i", (int)option->second.asInteger()));

    option = options.find("set");
    if (option != options.end())
      filter.AppendWhere(PrepareSQL("movie_view.strSet LIKE '%s'", option->second.asString().c_str()));

    AppendIdLinkFilter("tag", "tag", "movie", "movie", "idMovie", options, filter);
    AppendLinkFilter("tag", "tag", "movie", "movie", "idMovie", options, filter);
  }
  else if (type == "tvshows")
  {
    if (itemType == "tvshows")
    {
      AppendIdLinkFilter("genre", "genre", "tvshow", "tvshow", "idShow", options, filter);
      AppendLinkFilter("genre", "genre", "tvshow", "tvshow", "idShow", options, filter);

      AppendIdLinkFilter("studio", "studio", "tvshow", "tvshow", "idShow", options, filter);
      AppendLinkFilter("studio", "studio", "tvshow", "tvshow", "idShow", options, filter);

      AppendIdLinkFilter("director", "actor", "tvshow", "tvshow", "idShow", options, filter);

      auto option = options.find("year");
      if (option != options.end())
        filter.AppendWhere(PrepareSQL("tvshow_view.c%02d like '%%%i%%'", VIDEODB_ID_TV_PREMIERED, (int)option->second.asInteger()));

      AppendIdLinkFilter("actor", "actor", "tvshow", "tvshow", "idShow", options, filter);
      AppendLinkFilter("actor", "actor", "tvshow", "tvshow", "idShow", options, filter);

      AppendIdLinkFilter("tag", "tag", "tvshow", "tvshow", "idShow", options, filter);
      AppendLinkFilter("tag", "tag", "tvshow", "tvshow", "idShow", options, filter);
    }
    else if (itemType == "seasons")
    {
      auto option = options.find("tvshowid");
      if (option != options.end())
        filter.AppendWhere(PrepareSQL("season_view.idShow = %i", (int)option->second.asInteger()));

      AppendIdLinkFilter("genre", "genre", "tvshow", "season", "idShow", options, filter);

      AppendIdLinkFilter("director", "actor", "tvshow", "season", "idShow", options, filter);

      option = options.find("year");
      if (option != options.end())
        filter.AppendWhere(PrepareSQL("season_view.premiered like '%%%i%%'", (int)option->second.asInteger()));

      AppendIdLinkFilter("actor", "actor", "tvshow", "season", "idShow", options, filter);
    }
    else if (itemType == "episodes")
    {
      int idShow = -1;
      auto option = options.find("tvshowid");
      if (option != options.end())
        idShow = (int)option->second.asInteger();

      int season = -1;
      option = options.find("season");
      if (option != options.end())
        season = (int)option->second.asInteger();

      if (idShow > -1)
      {
        bool condition = false;

        AppendIdLinkFilter("genre", "genre", "tvshow", "episode", "idShow", options, filter);
        AppendLinkFilter("genre", "genre", "tvshow", "episode", "idShow", options, filter);

        AppendIdLinkFilter("director", "actor", "tvshow", "episode", "idShow", options, filter);
        AppendLinkFilter("director", "actor", "tvshow", "episode", "idShow", options, filter);
      
        option = options.find("year");
        if (option != options.end())
        {
          condition = true;
          filter.AppendWhere(PrepareSQL("episode_view.idShow = %i and episode_view.premiered like '%%%i%%'", idShow, (int)option->second.asInteger()));
        }

        AppendIdLinkFilter("actor", "actor", "tvshow", "episode", "idShow", options, filter);
        AppendLinkFilter("actor", "actor", "tvshow", "episode", "idShow", options, filter);

        if (!condition)
          filter.AppendWhere(PrepareSQL("episode_view.idShow = %i", idShow));

        if (season > -1)
        {
          if (season == 0) // season = 0 indicates a special - we grab all specials here (see below)
            filter.AppendWhere(PrepareSQL("episode_view.c%02d = %i", VIDEODB_ID_EPISODE_SEASON, season));
          else
            filter.AppendWhere(PrepareSQL("(episode_view.c%02d = %i or (episode_view.c%02d = 0 and (episode_view.c%02d = 0 or episode_view.c%02d = %i)))",
              VIDEODB_ID_EPISODE_SEASON, season, VIDEODB_ID_EPISODE_SEASON, VIDEODB_ID_EPISODE_SORTSEASON, VIDEODB_ID_EPISODE_SORTSEASON, season));
        }
      }
      else
      {
        option = options.find("year");
        if (option != options.end())
          filter.AppendWhere(PrepareSQL("episode_view.premiered like '%%%i%%'", (int)option->second.asInteger()));

        AppendIdLinkFilter("director", "actor", "episode", "episode", "idEpisode", options, filter);
        AppendLinkFilter("director", "actor", "episode", "episode", "idEpisode", options, filter);
      }
    }
  }
  else if (type == "musicvideos")
  {
    AppendIdLinkFilter("genre", "genre", "musicvideo", "musicvideo", "idMVideo", options, filter);
    AppendLinkFilter("genre", "genre", "musicvideo", "musicvideo", "idMVideo", options, filter);

    AppendIdLinkFilter("studio", "studio", "musicvideo", "musicvideo", "idMVideo", options, filter);
    AppendLinkFilter("studio", "studio", "musicvideo", "musicvideo", "idMVideo", options, filter);

    AppendIdLinkFilter("director", "actor", "musicvideo", "musicvideo", "idMVideo", options, filter);
    AppendLinkFilter("director", "actor", "musicvideo", "musicvideo", "idMVideo", options, filter);

    auto option = options.find("year");
    if (option != options.end())
      filter.AppendWhere(PrepareSQL("musicvideo_view.premiered like '%i%%'", (int)option->second.asInteger()));

    option = options.find("artistid");
    if (option != options.end())
    {
      if (itemType != "albums")
        filter.AppendJoin(PrepareSQL("JOIN actor_link ON actor_link.media_id=musicvideo_view.idMVideo AND actor_link.media_type='musicvideo'"));
      filter.AppendWhere(PrepareSQL("actor_link.actor_id = %i", (int)option->second.asInteger()));
    }

    option = options.find("artist");
    if (option != options.end())
    {
      if (itemType != "albums")
      {
        filter.AppendJoin(PrepareSQL("JOIN actor_link ON actor_link.media_id=musicvideo_view.idMVideo AND actor_link.media_type='musicvideo'"));
        filter.AppendJoin(PrepareSQL("JOIN actor ON actor.actor_id=actor_link.actor_id"));
      }
      filter.AppendWhere(PrepareSQL("actor.name LIKE '%s'", option->second.asString().c_str()));
    }

    option = options.find("albumid");
    if (option != options.end())
      filter.AppendWhere(PrepareSQL("musicvideo_view.c%02d = (select c%02d from musicvideo where idMVideo = %i)", VIDEODB_ID_MUSICVIDEO_ALBUM, VIDEODB_ID_MUSICVIDEO_ALBUM, (int)option->second.asInteger()));

    AppendIdLinkFilter("tag", "tag", "musicvideo", "musicvideo", "idMVideo", options, filter);
    AppendLinkFilter("tag", "tag", "musicvideo", "musicvideo", "idMVideo", options, filter);
  }
  else
    return false;

  auto option = options.find("xsp");
  if (option != options.end())
  {
    CSmartPlaylist xsp;
    if (!xsp.LoadFromJson(option->second.asString()))
      return false;

    // check if the filter playlist matches the item type
    if (xsp.GetType() == itemType ||
       (xsp.GetGroup() == itemType && !xsp.IsGroupMixed()) ||
        // handle episode listings with videodb://tvshows/titles/ which get the rest
        // of the path (season and episodeid) appended later
       (xsp.GetType() == "episodes" && itemType == "tvshows"))
    {
      std::set<std::string> playlists;
      filter.AppendWhere(xsp.GetWhereClause(*this, playlists));

      if (xsp.GetLimit() > 0)
        sorting.limitEnd = xsp.GetLimit();
      if (xsp.GetOrder() != SortByNone)
        sorting.sortBy = xsp.GetOrder();
      if (xsp.GetOrderDirection() != SortOrderNone)
        sorting.sortOrder = xsp.GetOrderDirection();
      if (CSettings::GetInstance().GetBool(CSettings::SETTING_FILELISTS_IGNORETHEWHENSORTING))
        sorting.sortAttributes = SortAttributeIgnoreArticle;
    }
  }

  option = options.find("filter");
  if (option != options.end())
  {
    CSmartPlaylist xspFilter;
    if (!xspFilter.LoadFromJson(option->second.asString()))
      return false;

    // check if the filter playlist matches the item type
    if (xspFilter.GetType() == itemType)
    {
      std::set<std::string> playlists;
      filter.AppendWhere(xspFilter.GetWhereClause(*this, playlists));
    }
    // remove the filter if it doesn't match the item type
    else
      videoUrl.RemoveOption("filter");
  }

  return true;
}

bool CVideoDatabase::SetVideoUserRating(int dbId, int rating, const MediaType& mediaType)
{
  try
  {
    std::shared_ptr<odb::transaction> odb_transaction (m_cdb.getTransaction());

    if (mediaType == MediaTypeNone)
      return false;

    if (mediaType == MediaTypeMovie)
    {
      CODBMovie movie;
      if (m_cdb.getDB()->query_one<CODBMovie>(odb::query<CODBMovie>::idMovie == dbId, movie))
      {
        movie.m_userrating = rating;
      }
      m_cdb.getDB()->update(movie);
    }
    else if (mediaType == MediaTypeEpisode)
    {
      //TODO: Implement
      //sql = PrepareSQL("UPDATE episode SET userrating=%i WHERE idEpisode = %i", rating, dbId);
    }
    else if (mediaType == MediaTypeMusicVideo)
    {
      //TODO: Implement
      //sql = PrepareSQL("UPDATE musicvideo SET userrating=%i WHERE idMVideo = %i", rating, dbId);
    }
    else if (mediaType == MediaTypeTvShow)
    {
      //TODO: Implement
      //sql = PrepareSQL("UPDATE tvshow SET userrating=%i WHERE idShow = %i", rating, dbId);
    }
    else if (mediaType == MediaTypeSeason)
    {
      //TODO: Implement
      //sql = PrepareSQL("UPDATE seasons SET userrating=%i WHERE idSeason = %i", rating, dbId);
    }

    if(odb_transaction)
      odb_transaction->commit();
    
    return true;
  }
  catch (std::exception& e)
  {
    CLog::Log(LOGERROR, "%s (%i, %s, %i) failed - %s", __FUNCTION__, dbId, mediaType.c_str(), rating, e.what());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "%s (%i, %s, %i) failed", __FUNCTION__, dbId, mediaType.c_str(), rating);
  }
  return false;
}
