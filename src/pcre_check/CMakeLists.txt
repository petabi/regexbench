add_custom_command(
   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/pcrecheckdb.cpp ${CMAKE_CURRENT_SOURCE_DIR}/pcrecheckdb.hpp
   COMMAND litesql-gen -v -t c++ pcre_check_db.xml
   MAIN_DEPENDENCY pcre_check_db.xml
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
   DEPENDS pcre_check_db.xml
   )

add_executable(db_setup db_setup.cpp pcrecheckdb.cpp)

find_library(LIB_SQLITE3 NAMES sqlite3)
find_library(LIB_JSONCPP NAMES jsoncpp)
find_library(LIB_LITESQL NAMES liblitesql.a)
find_library(LIB_LITESQL_UTIL NAMES liblitesql-util.a)
find_library(LIB_LITESQL_SQLITE NAMES liblitesql_sqlite.a)
target_link_libraries(db_setup
  ${Boost_LIBRARIES} ${LIB_LITESQL} ${LIB_LITESQL_UTIL} ${LIB_LITESQL_SQLITE} ${LIB_SQLITE3} ${LIB_JSONCPP})
