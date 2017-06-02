find_path(ATF_INCLUDE_DIR atf-c++.hpp)

include_directories("${ATF_INCLUDE_DIR}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Kyuafile.in
  ${CMAKE_CURRENT_BINARY_DIR}/Kyuafile)

add_definitions(-DDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(t_metadata t_metadata.cpp)
target_link_libraries(t_metadata ${ATF_LIB} ${PCAP_LIB} rb ${Boost_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT})

if(REMATCHCOMP_LIB AND REMATCHEXEC_LIB AND PCRE2_LIB)
  add_executable(t_session t_session.cpp)
  target_link_libraries(t_session engines ${ATF_LIB} ${PCAP_LIB} ${PCRE2_LIB}
    ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB} rb ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT})
endif()

if(RE2_LIB)
  add_executable(t_re2 t_re2.cpp)
  target_link_libraries(t_re2 engines ${ATF_LIB} ${PCAP_LIB} ${RE2_LIB}
    rb ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

add_executable(t_option t_option.cpp ${ENGINE_SRCS})
target_link_libraries(t_option
  engines ${ATF_LIB} ${PCAP_LIB} ${Boost_LIBRARIES} rb ${CMAKE_THREAD_LIBS_INIT})
if(HYPERSCAN_LIB)
  target_link_libraries(t_option ${HYPERSCAN_LIB})
endif()
if(PCRE2_LIB)
  target_link_libraries(t_option ${PCRE2_LIB})
endif()
if(RE2_LIB)
  target_link_libraries(t_option ${RE2_LIB})
endif()
if(REMATCHCOMP_LIB AND REMATCHEXEC_LIB)
  target_link_libraries(t_option ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB})
endif()

add_executable(t_libtest t_libtest.cpp)
#target_link_libraries(t_libtest
#    engines rb ${ATF_LIB} ${Boost_LIBRARIES} ${PCAP_LIB} ${CMAKE_THREAD_LIBS_INIT}
#    ${HYPERSCAN_LIB} ${PCRE2_LIB} ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB})
target_link_libraries(t_libtest
    engines ${ATF_LIB} ${Boost_LIBRARIES} ${PCAP_LIB} ${CMAKE_THREAD_LIBS_INIT})
if(HYPERSCAN_LIB)
  target_link_libraries(t_libtest ${HYPERSCAN_LIB})
endif()
if(PCRE2_LIB)
  target_link_libraries(t_libtest ${PCRE2_LIB})
endif()
if(REMATCHCOMP_LIB AND REMATCHEXEC_LIB)
  target_link_libraries(t_libtest ${REMATCHCOMP_LIB} ${REMATCHEXEC_LIB})
endif()
if(RE2_LIB)
  target_link_libraries(t_libtest ${RE2_LIB})
endif()
target_link_libraries(t_libtest rb)
