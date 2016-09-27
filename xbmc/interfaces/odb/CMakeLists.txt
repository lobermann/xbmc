find_package(ODB REQUIRED OPTIONAL_COMPONENTS sqlite)

include(${ODB_USE_FILE})

#set(OdbCMake_SOURCES dummy.cpp)

set(OdbCMake_ODB_HEADERS
        VideoTest.h)

odb_compile(OdbCMake_SOURCES FILES ${OdbCMake_ODB_HEADERS} DB sqlite GENERATE_QUERY GENERATE_SESSION)

#set(SOURCES ${OdbCMake_SOURCES})
set(HEADERS ${OdbCMake_ODB_HEADERS})

add_library(odb_binding STATIC ${SOURCES} ${HEADERS})
set_target_properties(odb_binding PROPERTIES LINKER_LANGUAGE CXX)

set(core_DEPENDS odb_binding ${core_DEPENDS} CACHE STRING "" FORCE)