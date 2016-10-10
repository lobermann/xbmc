//
//  ODBTag.h
//  kodi
//
//  Created by Lukas Obermann on 07.10.16.
//
//

#ifndef ODBTAG_H
#define ODBTAG_H

#include <odb/core.hxx>

#include <string>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBTag
{
public:
  CODBTag()
  {
    m_name = "";
  };
  
#pragma db id auto
  unsigned long m_idTag;
  std::string m_name;
  
private:
  friend class odb::access;
  
};

#endif /* ODBTAG_H */
