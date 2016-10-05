//
//  ODBUniqueID.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBUNIQUEID_H
#define ODBUNIQUEID_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <string>

#include "ODBFile.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBUniqueID
{
public:
  CODBUniqueID()
  {
    m_value = "";
    m_type = "";
  };
  
#pragma db id auto
  unsigned long m_idUniqueID;
  odb::lazy_shared_ptr<CODBFile> m_file;
  std::string m_value;
  std::string m_type;
  
private:
  friend class odb::access;
  
};

#endif /* ODBUNIQUEID_H */
