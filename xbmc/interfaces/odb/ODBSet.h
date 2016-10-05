//
//  ODBSet.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBSET_H
#define ODBSET_H

#include <odb/core.hxx>

#include <map>
#include <string>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBSet
{
public:
  CODBSet()
  {
    m_name = "";
    m_overview = "";
  };
  
#pragma db id auto
  unsigned long m_idSet;
  std::string m_name;
  std::string m_overview;
  std::map<std::string, std::string> m_artwork;
  
private:
  friend class odb::access;
  
};


#endif /* ODBSET_H */
