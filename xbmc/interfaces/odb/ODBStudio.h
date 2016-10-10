//
//  ODBStudio.h
//  kodi
//
//  Created by Lukas Obermann on 07.10.16.
//
//

#ifndef ODBSTUDIO_H
#define ODBSTUDIO_H

#include <odb/core.hxx>

#include <string>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBStudio
{
public:
  CODBStudio()
  {
    m_name = "";
  };
  
#pragma db id auto
  unsigned long m_idStudio;
  std::string m_name;
  
private:
  friend class odb::access;
  
};

#endif /* ODBSTUDIO_H */
