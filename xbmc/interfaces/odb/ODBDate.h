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

#include <ctime>

#ifdef ODB_COMPILER
#pragma db model version(1, 1, open)
#endif

#pragma db value
class CODBDate
{
public:
  CODBDate()
  {
    m_year = 0;
    m_month = 0;
    m_wday = 0;
    m_mday = 0;
    m_hour = 0;
    m_minute = 0;
    m_second = 0;
  };
  
  CODBDate(tm& time)
  {
    m_year = time.tm_year;
    m_month = time.tm_mon;
    m_wday = time.tm_wday;
    m_mday = time.tm_mday;
    m_hour = time.tm_hour;
    m_minute = time.tm_min;
    m_second = time.tm_sec;
  };
  
  void setDateFromTm(const tm& time)
  {
    m_year = time.tm_year;
    m_month = time.tm_mon;
    m_wday = time.tm_wday;
    m_mday = time.tm_mday;
    m_hour = time.tm_hour;
    m_minute = time.tm_min;
    m_second = time.tm_sec;
  };
  
  tm getDateAsTm()
  {
    struct tm *stm;
    time_t tim;
    time(&tim);
    stm = localtime(&tim);
    
    stm->tm_year = m_year;
    stm->tm_mon = m_month;
    stm->tm_wday = m_wday;
    stm->tm_mday = m_mday;
    stm->tm_hour = m_hour;
    stm->tm_min = m_minute;
    stm->tm_sec = m_second;
    
    return *stm;
  };
  
  int m_year;
  int m_month;
  int m_wday;
  int m_mday;
  int m_hour;
  int m_minute;
  int m_second;
  
private:
  friend class odb::access;
  
};


#endif /* ODBDATE_H */
