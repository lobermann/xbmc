//
//  ODBPerson.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBPERSON_H
#define ODBPERSON_H

#include <odb/core.hxx>

#include <string>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBPerson
{
public:
  CODBPerson()
  {
    m_name = "";
    m_art_urls = "";
  };
  
#pragma db id auto
  unsigned long m_idPerson;
  std::string m_name;
  std::string m_art_urls;
  
private:
  friend class odb::access;
  
};

#endif /* ODBPERSON_H */
