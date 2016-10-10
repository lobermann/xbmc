//
//  ODBMovie.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBMOVIE_H
#define ODBMOVIE_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ODBFile.h"
#include "ODBRating.h"
#include "ODBGenre.h"
#include "ODBPersonLink.h"
#include "ODBSet.h"
#include "ODBBookmark.h"
#include "ODBUniqueID.h"
#include "ODBArt.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBMovie
{
public:
  CODBMovie()
  {
    m_title = "";
    m_plot = "";
    m_plotoutline = "";
    m_tagline = "";
    m_credits = "";
    m_thumbUrl = "";
    m_sortTitle = "";
    m_runtime = 0;
    m_mpaa = "";
    m_top250 = 0;
    m_originalTitle = "";
    m_thumbUrl_spoof = "";
    m_trailer = "";
    m_fanart = "";
    m_userrating = 0;
    m_premiered = "";
  };
  
#pragma db id auto
  unsigned long m_idMovie;
  odb::lazy_shared_ptr<CODBFile> m_file;
  std::string m_title;
  std::string m_plot;
  std::string m_plotoutline;
  std::string m_tagline;
  odb::lazy_shared_ptr<CODBRating> m_defaultRating;
  std::vector< odb::lazy_shared_ptr<CODBRating> > m_ratings;
  std::string m_credits;
  std::string m_thumbUrl;
  std::string m_sortTitle;
  int m_runtime;
  std::string m_mpaa;
  int m_top250;
  std::vector< odb::lazy_shared_ptr<CODBGenre> > m_genres;
  std::vector< odb::lazy_shared_ptr<CODBPersonLink> > m_directors;
  std::vector< odb::lazy_shared_ptr<CODBPersonLink> > m_actors;
  std::vector< odb::lazy_shared_ptr<CODBPersonLink> > m_writingCredits;
  std::string m_originalTitle;
  std::string m_thumbUrl_spoof;
  std::vector<std::string> m_studios;
  std::vector<std::string> m_tags;
  std::string m_trailer;
  std::string m_fanart;
  std::vector< odb::lazy_shared_ptr<CODBArt> > m_artwork;
  std::vector<std::string> m_countries;
  odb::lazy_shared_ptr<CODBPath> m_basePath;
  odb::lazy_shared_ptr<CODBPath> m_parentPath;
  odb::lazy_shared_ptr<CODBSet> m_set;
  int m_userrating;
  std::string m_premiered;
  std::vector< odb::lazy_shared_ptr<CODBBookmark> > m_bookmarks;
  odb::lazy_shared_ptr<CODBBookmark> m_resumeBookmark;
  std::vector< odb::lazy_shared_ptr<CODBUniqueID> > m_ids;
  odb::lazy_shared_ptr<CODBUniqueID> m_defaultID;
  
private:
  friend class odb::access;
};

#pragma db view object(CODBMovie) \
  object(CODBFile: CODBMovie::m_file) \
  object(CODBPath: CODBFile::m_path)
struct ODBView_Movie_File_Path
{
  std::shared_ptr<CODBMovie> movie;
  std::shared_ptr<CODBFile> file;
  std::shared_ptr<CODBPath> path;
};

#pragma db view object(CODBMovie) \
  object(CODBFile: CODBMovie::m_file) \
  object(CODBUniqueID: CODBMovie::m_file)
struct ODBView_Movie_File_UID
{
  std::shared_ptr<CODBMovie> movie;
  std::shared_ptr<CODBFile> file;
  std::shared_ptr<CODBUniqueID> uid;
};

#endif /* ODBMOVIE_H */
