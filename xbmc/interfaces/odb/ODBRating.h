//
//  ODBRating.h
//  kodi
//
//  Created by Lukas Obermann on 03.10.16.
//
//

#ifndef ODBRATING_H
#define ODBRATING_H

#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include <string>

#include "ODBFile.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db object pointer(std::shared_ptr)
class CODBRating
{
public:
  CODBRating()
  {
    m_ratingType = "";
    m_rating = 0.0f;
    m_votes = 0;
  };
  
#pragma db id auto
  unsigned long m_idRating;
  odb::lazy_shared_ptr<CODBFile> m_file;
  std::string m_ratingType;
  float m_rating;
  int m_votes;
  
private:
  friend class odb::access;
  
};

#endif /* ODBRATING_H */
