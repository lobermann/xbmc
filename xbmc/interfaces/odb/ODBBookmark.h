//
//  ODBBookmark.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBBOOKMARK_H
#define ODBBOOKMARK_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <string>

#include "ODBFile.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBBookmark
{
public:
  CODBBookmark()
  {
    m_timeInSeconds = 0;
    m_totalTimeInSeconds = 0;
    m_thumbNailImage = "";
    m_player = "";
    m_type = 0;
  };
  
#pragma db id auto
  unsigned long m_idBookmark;
  odb::lazy_shared_ptr<CODBFile> m_file;
  double m_timeInSeconds;
  double m_totalTimeInSeconds;
  std::string m_thumbNailImage;
  std::string m_player;
  int m_type;
  
private:
  friend class odb::access;
  
};

#endif /* ODBBOOKMARK_H */
