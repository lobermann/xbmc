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
    odb::core::schema_catalog::create_schema (*m_db);
    t.commit();
  }
}

std::shared_ptr<odb::transaction> CCommonDatabase::getTransaction()
{
  if (!odb::transaction::has_current())
    return std::shared_ptr<odb::transaction>(new odb::transaction(m_db->begin()));
  return nullptr;
}
