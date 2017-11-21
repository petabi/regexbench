# These suppress some warning caused by auto generated pcrecheckdb sources
# TODO!! : separate those sources into a library
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-global-constructors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-exit-time-destructors")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")

if (REMATCHCOMP_LIB AND HYPERSCAN_LIB AND PCRE2_LIB)
  set(CONTINUE TRUE)
endif()

if (CONTINUE)
  find_package(Lua) # TODO revisit!!!
  if (Lua_FOUND)
    include_directories(SYSTEM ${LUA_INCLUDE_DIR})
  else()
    set(CONTINUE FALSE)
  endif()
endif()

if (CONTINUE)
  find_package(PkgConfig)
  pkg_check_modules(JSONCPP jsoncpp)
  if (JSONCPP_FOUND)
    include_directories(SYSTEM ${JSONCPP_INCLUDEDIR})
    link_directories(${JSONCPP_LIBRARY_DIRS})
  else()
    set(CONTINUE FALSE)
  endif()
endif()

if (CONTINUE)
  find_path(AUTO_OPT_INCLUDE_DIR pius/auto-opt.h HINTS ${CMAKE_INSTALL_PREFIX}/include)
  if (AUTO_OPT_INCLUDE_DIR)
    message(STATUS "${AUTO_OPT_INCLUDE_DIR}")
    include_directories(SYSTEM ${AUTO_OPT_INCLUDE_DIR})
  else()
    set(CONTINUE FALSE)
  endif()
endif()

if (CONTINUE)
find_library(LIB_SQLITE3 NAMES sqlite3)
find_library(LIB_LITESQL NAMES litesql PATH_SUFFIXES static)
find_library(LIB_LITESQL_UTIL NAMES litesql-util PATH_SUFFIXES static)
find_library(LIB_LITESQL_SQLITE NAMES litesql_sqlite PATH_SUFFIXES static)
find_library(EDITLINE_LIB NAMES edit)

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
add_executable(pcre_checker
  main.cpp PcreChecker.cpp CheckerShell.cpp pcrecheckdb.cpp ../Rule.cpp
  $<TARGET_OBJECTS:engines>)

if (USE_SYSTEM_LITESQL)
  target_link_libraries(pcre_checker ${LITESQL_LIBRARIES})
else()
  target_link_libraries(pcre_checker litesql litesql-util litesql_sqlite)
endif()

target_link_libraries(pcre_checker
  ${Boost_LIBRARIES} ${LIB_SQLITE3}
  ${PCRE2_LIB} ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB} ${HYPERSCAN_LIB}
  ${EDITLINE_LIB} ${JSONCPP_LIBRARIES} ${PCAP_LIB}
)

if (OS_IS_LINUX)
  target_link_libraries(pcre_checker ${CMAKE_DL_LIBS})
endif()

if(RE2_INCLUDE_DIR)
  # this is for the case when RE2 library is found
  # (in that case engine objects are referencing RE2 library)
  target_link_libraries(pcre_checker ${RE2_LIB})
endif()

endif(CONTINUE)
