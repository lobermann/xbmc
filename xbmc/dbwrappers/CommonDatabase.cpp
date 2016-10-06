//
//  CommonDatabase.cpp
//  kodi
//
//  Created by Lukas Obermann on 30.09.16.
//
//

#include "CommonDatabase.h"

#ifdef HAS_MYSQL
#include <odb/mysql/database.hxx>
#endif
#include <odb/sqlite/database.hxx>

#include <odb/transaction.hxx>
#include <odb/session.hxx>
#include <odb/schema-catalog.hxx>

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

#include "filesystem/SpecialProtocol.h"
#include "profiles/ProfilesManager.h"
#include "settings/AdvancedSettings.h"

CCommonDatabase::CCommonDatabase()
{
  DatabaseSettings settings = &g_advancedSettings.m_databaseCommon ? g_advancedSettings.m_databaseCommon : DatabaseSettings();
  
#ifdef HAS_MYSQL
  if (settings->type == "mysql")
  {
    
  }
  //Default to sqlite
  else (settings->type == "sqlite3")
#endif
  {
    std::string dbfolder = CSpecialProtocol::TranslatePath(CProfilesManager::GetInstance().GetDatabaseFolder());
    m_db = std::shared_ptr<odb::core::database>( new odb::sqlite::database(dbfolder + "/common.db",
                                                                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE));
  }
  if(!odb::session::has_current())
    m_odb_session = std::shared_ptr<odb::session>(new odb::session);
  
}

void CCommonDatabase::init()
{
  {
    odb::core::transaction t (m_db->begin());
    odb::core::schema_catalog::migrate (*m_db);
    t.commit();
  }
}

std::shared_ptr<odb::transaction> CCommonDatabase::getTransaction()
{
  if (!odb::transaction::has_current())
    return std::shared_ptr<odb::transaction>(new odb::transaction(m_db->begin()));
  return nullptr;
}
