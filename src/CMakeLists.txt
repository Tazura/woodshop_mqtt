cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 14)

project(woodshop_mqtt VERSION 1.0)

include(GNUInstallDirs)

find_package(Boost REQUIRED COMPONENTS system filesystem program_options)

SET(CONFIG_FILENAME "${CMAKE_PROJECT_NAME}.conf")
SET(CONFIG_PATH "${CMAKE_INSTALL_SYSCONFDIR}/${CONFIG_FILENAME}")
SET(CONFIG_PATH_DEFAULT "${CONFIG_PATH}.default")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/woodshop_mqtt.conf ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_FILENAME} COPYONLY)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(woodshop_mqtt main.cpp)
target_link_libraries(woodshop_mqtt mosquittopp Boost::system Boost::filesystem Boost::program_options dl)

install(TARGETS woodshop_mqtt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/woodshop_mqtt.conf DESTINATION /${CMAKE_INSTALL_SYSCONFDIR} RENAME ${CONFIG_FILENAME}.default)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/woodshop_mqtt.service DESTINATION /${CMAKE_INSTALL_SYSCONFDIR}/systemd/system )
