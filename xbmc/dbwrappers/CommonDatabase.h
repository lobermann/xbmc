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
