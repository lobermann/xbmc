//
//  ODBFile.h
//  kodi
//
//  Created by Lukas Obermann on 29.09.16.
//
//

#ifndef ODBFILE_H
#define ODBFILE_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <string>

#include "ODBPath.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBFile
{
public:
  CODBFile()
  {
    m_filename = "";
    m_playCount = 0;
    m_lastPlayed = "";
    m_dateAdded = "";
  };
  
#pragma db id auto
  unsigned long m_idFile;
  
  odb::lazy_shared_ptr<CODBPath> m_path;
  std::string m_filename;
  unsigned int m_playCount;
  std::string m_lastPlayed;
  std::string m_dateAdded;
  
private:
    friend class odb::access;
  
};


#endif /* ODBFile_h */
