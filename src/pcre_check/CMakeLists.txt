add_custom_command(
   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pcrecheckdb.cpp ${CMAKE_CURRENT_SOURCE_DIR}/pcrecheckdb.hpp
   COMMAND litesql-gen -v -t c++ pcre_check_db.xml
   MAIN_DEPENDENCY pcre_check_db.xml
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
   DEPENDS pcre_check_db.xml
   )

# These suppress some warning caused by auto generated pcrecheckdb sources
# TODO!! : separate those sources into a library
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-global-constructors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-exit-time-destructors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")


#add_executable(db_setup db_setup.cpp pcrecheckdb.cpp)

find_package(Lua REQUIRED) # TODO revisit!!!
find_library(LIB_SQLITE3 NAMES sqlite3)
find_library(LIB_JSONCPP NAMES jsoncpp)
find_library(LIB_LITESQL NAMES liblitesql.a)
find_library(LIB_LITESQL_UTIL NAMES liblitesql-util.a)
find_library(LIB_LITESQL_SQLITE NAMES liblitesql_sqlite.a)
find_library(EDITLINE_LIB NAMES edit)
#target_link_libraries(db_setup
#  ${Boost_LIBRARIES} ${LIB_LITESQL} ${LIB_LITESQL_UTIL} ${LIB_LITESQL_SQLITE} ${LIB_SQLITE3} ${LIB_JSONCPP})

include_directories(SYSTEM ${LUA_INCLUDE_DIR})

add_executable(pcre_checker PcreChecker.cpp pcrecheckdb.cpp ../Rule.cpp CheckerShell.cpp db_setup.cpp)
target_link_libraries(pcre_checker engines
  ${Boost_LIBRARIES} ${LIB_LITESQL} ${LIB_LITESQL_UTIL} ${LIB_LITESQL_SQLITE} ${LIB_SQLITE3}
  ${PCRE2_LIB} ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB} ${HYPERSCAN_LIB}
  ${EDITLINE_LIB} ${LIB_JSONCPP}
)
