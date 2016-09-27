//
// Created by a1rwulf on 25.09.16.
//

#ifndef CMAKE_VIDEOTEST_H
#define CMAKE_VIDEOTEST_H

#include <odb/core.hxx>

#pragma db object
class VideoTest
{
  public:
    VideoTest();

    const std::string& getTitle () const;
    void setTitle (const std::string&);

  private:
    friend class odb::access;
    #pragma db id auto
    unsigned long id_;

    std::string m_title;
};

#endif //CMAKE_VIDEOTEST_H
