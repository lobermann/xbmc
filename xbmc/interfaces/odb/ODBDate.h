//
//  ODBDate.h
//  kodi
//
//  Created by Lukas Obermann on 08.11.16.
//
//

#ifndef ODBDATE_H
#define ODBDATE_H

#include <odb/core.hxx>

#include <string>

#include "../../linux/PlatformDefs.h"

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db value
class CODBDate
{
public:
  CODBDate()
  {
    m_ulong_date = 0;
  }
  
  /*
   * ulong_date = CDateTime::GetAsULongLong()
   * date = CDateTime::GetAsDBDateTime()
   */
  CODBDate(uint64_t ulong_date, std::string date)
  {
    m_ulong_date = ulong_date;
    m_date = date;
  }
  
  void setDateTime(uint64_t ulong_date, std::string date)
  {
    m_ulong_date = ulong_date;
    m_date = date;
  }
  
  uint64_t m_ulong_date;
  std::string m_date;
  
private:
  friend class odb::access;
  
};


#endif /* ODBDATE_H */
