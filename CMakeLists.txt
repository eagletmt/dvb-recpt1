project(dvb-recpt1)
cmake_minimum_required(VERSION 2.8)

add_definitions(-Wall -W)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")

find_package(Boost COMPONENTS program_options)
if (NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found")
endif()

set(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc" CACHE STRING "read-only single-machine data")
message(STATUS "SYSCONFDIR=${SYSCONFDIR}")
configure_file(config.h.in config.h)

add_executable(recpt1
  record.cc
  tuner.cc
  oneshot_timer.cc
  )
target_link_libraries(recpt1 boost_program_options)

add_executable(recpt1ctl recpt1ctl.c)

install(PROGRAMS recpt1 recpt1ctl DESTINATION bin)
install(FILES adapters.conf channels.conf DESTINATION ${SYSCONFDIR})
