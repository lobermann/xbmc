//
//  CommonDatabase.hpp
//  kodi
//
//  Created by Lukas Obermann on 30.09.16.
//
//

#ifndef COMMONDATABASE_HPP
#define COMMONDATABASE_HPP

#include <memory>

#include <odb/database.hxx>

#include <odb/odb_gen/ODBBookmark.h>
#include <odb/odb_gen/ODBBookmark_odb.h>
#include <odb/odb_gen/ODBFile.h>
#include <odb/odb_gen/ODBFile_odb.h>
#include <odb/odb_gen/ODBGenre.h>
#include <odb/odb_gen/ODBGenre_odb.h>
#include <odb/odb_gen/ODBMovie.h>
#include <odb/odb_gen/ODBMovie_odb.h>
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

class DatabaseSettings;

class CCommonDatabase
{
public:
  CCommonDatabase();
  void init();
  
  std::shared_ptr<odb::database> getDB(){ return m_db; };
  std::shared_ptr<odb::transaction> getTransaction();
  
private:
  std::shared_ptr<odb::database> m_db;
  std::shared_ptr<odb::session> m_odb_session;
};

#endif /* COMMONDATABASE_HPP */
