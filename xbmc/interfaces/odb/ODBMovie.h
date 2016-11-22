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

#include "ODBBookmark.h"
#include "ODBCountry.h"
#include "ODBFile.h"
#include "ODBRating.h"
#include "ODBGenre.h"
#include "ODBPersonLink.h"
#include "ODBSet.h"
#include "ODBStudio.h"
#include "ODBStreamDetails.h"
#include "ODBTag.h"
#include "ODBUniqueID.h"
#include "ODBArt.h"
#include "ODBDate.h"


#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr) \
                  table("movie")
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
    m_synced = false;
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
  std::vector< odb::lazy_shared_ptr<CODBStudio> > m_studios;
  std::vector< odb::lazy_shared_ptr<CODBTag> > m_tags;
  std::string m_trailer;
  std::string m_fanart;
  std::vector< odb::lazy_shared_ptr<CODBArt> > m_artwork;
  std::vector< odb::lazy_shared_ptr<CODBCountry> > m_countries;
  odb::lazy_shared_ptr<CODBPath> m_basePath;
  odb::lazy_shared_ptr<CODBPath> m_parentPath;
  odb::lazy_shared_ptr<CODBSet> m_set;
  int m_userrating;
  CODBDate m_premiered;
  std::vector< odb::lazy_shared_ptr<CODBBookmark> > m_bookmarks;
  odb::lazy_shared_ptr<CODBBookmark> m_resumeBookmark;
  std::vector< odb::lazy_shared_ptr<CODBUniqueID> > m_ids;
  odb::lazy_shared_ptr<CODBUniqueID> m_defaultID;
  
  //Members not stored in the db, used for sync ...
#pragma db transient
  bool m_synced;
  
private:
  friend class odb::access;
  
#pragma db index member(m_file)
#pragma db index member(m_title)
#pragma db index member(m_defaultRating)
#pragma db index member(m_sortTitle)
#pragma db index member(m_mpaa)
#pragma db index member(m_originalTitle)
#pragma db index member(m_basePath)
#pragma db index member(m_parentPath)
#pragma db index member(m_set)
#pragma db index member(m_userrating)
#pragma db index member(m_premiered)
#pragma db index member(m_defaultID)
};

#pragma db view \
  object(CODBMovie) \
  object(CODBGenre = genre: CODBMovie::m_genres) \
  object(CODBPersonLink = director_link: CODBMovie::m_directors) \
  object(CODBPerson = director: director_link::m_person) \
  object(CODBPersonLink = actor_ink: CODBMovie::m_actors) \
  object(CODBPerson = actor: actor_ink::m_person) \
  object(CODBPersonLink = writingCredit_link: CODBMovie::m_writingCredits) \
  object(CODBPerson = writingCredit: writingCredit_link::m_person) \
  object(CODBStudio = studio: CODBMovie::m_studios) \
  object(CODBTag = tag: CODBMovie::m_tags) \
  object(CODBCountry = country: CODBMovie::m_countries) \
  object(CODBSet = set: CODBMovie::m_set) \
  object(CODBBookmark = bookmark: CODBMovie::m_bookmarks) \
  object(CODBFile = fileView: CODBMovie::m_file) \
  object(CODBStreamDetails: CODBMovie::m_file == CODBStreamDetails::m_file) \
  object(CODBRating = defaultRating: CODBMovie::m_defaultRating) \
  query(distinct)
struct ODBView_Movie
{
  std::shared_ptr<CODBMovie> movie;
};

#pragma db view \
  object(CODBMovie) \
  object(CODBGenre = genre inner: CODBMovie::m_genres) \
  object(CODBFile = file inner: CODBMovie::m_file) \
  object(CODBPath = path inner: file::m_path) \
  query(distinct)
struct ODBView_Movie_Genre
{
  #pragma db column(genre::m_idGenre)
  unsigned long m_idGenre;
  #pragma db column(genre::m_name)
  std::string m_name;
  #pragma db column(file::m_playCount)
  unsigned int m_playCount;
  #pragma db column(path::m_path)
  std::string m_path;
};

#pragma db view \
  object(CODBMovie) \
  object(CODBCountry = country inner: CODBMovie::m_countries) \
  object(CODBFile = file inner: CODBMovie::m_file) \
  object(CODBPath = path inner: file::m_path) \
  query(distinct)
struct ODBView_Movie_Country
{
#pragma db column(country::m_idCountry)
  unsigned long m_idCountry;
#pragma db column(country::m_name)
  std::string m_name;
#pragma db column(file::m_playCount)
  unsigned int m_playCount;
#pragma db column(path::m_path)
  std::string m_path;
};

#pragma db view \
  object(CODBMovie) \
  object(CODBPersonLink = person_link: CODBMovie::m_directors) \
  object(CODBPerson = person: person_link::m_person) \
  object(CODBFile = file inner: CODBMovie::m_file) \
  object(CODBPath = path inner: file::m_path) \
  query(distinct)
struct ODBView_Movie_Director
{
  std::shared_ptr<CODBPerson> person;
#pragma db column(file::m_playCount)
  unsigned int m_playCount;
#pragma db column(path::m_path)
  std::string m_path;
};

#pragma db view \
  object(CODBMovie) \
  object(CODBPersonLink = person_link: CODBMovie::m_actors) \
  object(CODBPerson = person: person_link::m_person) \
  object(CODBFile = file inner: CODBMovie::m_file) \
  object(CODBPath = path inner: file::m_path) \
  query(distinct)
struct ODBView_Movie_Actor
{
  std::shared_ptr<CODBPerson> person;
#pragma db column(file::m_playCount)
  unsigned int m_playCount;
#pragma db column(path::m_path)
  std::string m_path;
};

#pragma db view \
  object(CODBMovie) \
  object(CODBStudio = studio inner: CODBMovie::m_studios) \
  object(CODBFile = file inner: CODBMovie::m_file) \
  object(CODBPath = path inner: file::m_path) \
  query(distinct)
struct ODBView_Movie_Studio
{
#pragma db column(studio::m_idStudio)
  unsigned long m_idStudio;
#pragma db column(studio::m_name)
  std::string m_name;
#pragma db column(file::m_playCount)
  unsigned int m_playCount;
#pragma db column(path::m_path)
  std::string m_path;
};

#pragma db view object(CODBMovie)
struct ODBView_Movie_Count
{
#pragma db column("count(1)")
  std::size_t count;
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

#pragma db view object(CODBMovie) \
  object(CODBArt: CODBMovie::m_artwork) \
  query(distinct)
struct ODBView_Movie_Art
{
  std::shared_ptr<CODBArt> art;
};

#endif /* ODBMOVIE_H */
