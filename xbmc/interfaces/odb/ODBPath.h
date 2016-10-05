//
//  ODBPath.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBPATH_H
#define ODBPATH_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <string>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBPath
{
public:
  CODBPath()
  {
    m_path = "";
    m_content = "";
    m_scraper = "";
    m_hash = "";
    m_scanRecursive = 0;
    m_useFolderNames = false;
    m_settings = "";
    m_noUpdate = false;
    m_exclude =false;
    m_dateAdded = "";
  };
  
#pragma db id auto
  unsigned long m_idPath;
  std::string m_path;
  std::string m_content;
  std::string m_scraper;
  std::string m_hash;
  int m_scanRecursive;
  bool m_useFolderNames;
  std::string m_settings;
  bool m_noUpdate;
  bool m_exclude;
  std::string m_dateAdded;
  odb::lazy_shared_ptr<CODBPath> m_parentPath;
  
private:
  friend class odb::access;
};


#endif /* ODBPATH_H */
