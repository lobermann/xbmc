//
//  ODBGenre.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBGENRE_H
#define ODBGENRE_H

#include <odb/core.hxx>

#include <string>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBGenre
{
public:
  CODBGenre()
  {
    m_name = "";
  };
  
#pragma db id auto
  unsigned long m_idGenre;
  std::string m_name;
  
private:
  friend class odb::access;
  
};

#endif /* ODBGENRE_H */
