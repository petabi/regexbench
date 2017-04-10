# These suppress some warning caused by auto generated pcrecheckdb sources
# TODO!! : separate those sources into a library
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-global-constructors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-exit-time-destructors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")

find_package(Lua REQUIRED) # TODO revisit!!!
find_library(LIB_SQLITE3 NAMES sqlite3)
find_library(LIB_JSONCPP NAMES jsoncpp)
find_library(LIB_LITESQL NAMES litesql)
find_library(LIB_LITESQL_UTIL NAMES litesql-util)
find_library(LIB_LITESQL_SQLITE NAMES litesql_sqlite)
find_library(EDITLINE_LIB NAMES edit)

include_directories(SYSTEM ${LUA_INCLUDE_DIR})

if (LIB_LITESQL AND LIB_LITESQL_UTIL AND LIB_LITESQL_SQLITE)
  set(USE_SYSTEM_LITESQL TRUE)
  set(LITESQL_LIBRARIES "")
  list(APPEND LITESQL_LIBRARIES ${LIB_LITESQL})
  list(APPEND LITESQL_LIBRARIES ${LIB_LITESQL_UTIL})
  list(APPEND LITESQL_LIBRARIES ${LIB_LITESQL_SQLITE})
else()
  include_directories(SYSTEM library/litesql-src-0.3.17/include)
  add_subdirectory(library/litesql-src-0.3.17)
endif()

add_custom_command(
   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pcrecheckdb.cpp ${CMAKE_CURRENT_SOURCE_DIR}/pcrecheckdb.hpp
   COMMAND litesql-gen -v -t c++ pcre_check_db.xml
   MAIN_DEPENDENCY pcre_check_db.xml
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
   DEPENDS pcre_check_db.xml
   )
add_executable(pcre_checker PcreChecker.cpp pcrecheckdb.cpp ../Rule.cpp CheckerShell.cpp db_setup.cpp)

if (USE_SYSTEM_LITESQL)
  target_link_libraries(pcre_checker ${LITESQL_LIBRARIES})
else()
  target_link_libraries(pcre_checker litesql litesql-util litesql_sqlite)
endif()

target_link_libraries(pcre_checker engines
  ${Boost_LIBRARIES} ${LIB_SQLITE3}
  ${PCRE2_LIB} ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB} ${HYPERSCAN_LIB}
  ${EDITLINE_LIB} ${LIB_JSONCPP}
)
