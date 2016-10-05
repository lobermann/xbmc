//
//  ODBPersonLink.h
//  kodi
//
//  Created by Lukas Obermann on 04.10.16.
//
//

#ifndef ODBPERSONLINK_H
#define ODBPERSONLINK_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <string>

#include "ODBPerson.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBPersonLink
{
public:
  CODBPersonLink()
  {
    m_role = "";
  };
  
#pragma db id auto
  unsigned long m_idPersonLink;
  odb::lazy_shared_ptr<CODBPerson> m_person;
  std::string m_role;
  
private:
  friend class odb::access;
  
};

#endif /* ODBPERSONLINK_H */
