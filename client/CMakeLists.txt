set(IRODS_EXECUTABLE_NAME itruncate)

add_executable(
  ${IRODS_EXECUTABLE_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_compile_definitions(
  ${IRODS_EXECUTABLE_NAME}
  PRIVATE
  ${IRODS_COMPILE_DEFINITIONS}
  ${IRODS_COMPILE_DEFINITIONS_PRIVATE})

target_include_directories(
  ${IRODS_EXECUTABLE_NAME}
  PRIVATE
  $<BUILD_INTERFACE:${CMAKE_IRODS_PLUGIN_SOURCE_DIR}/include>
  ${IRODS_INCLUDE_DIRS}
  ${IRODS_EXTERNALS_FULLPATH_BOOST}/include
  ${IRODS_EXTERNALS_FULLPATH_FMT}/include)

target_link_libraries(
  ${IRODS_EXECUTABLE_NAME}
  PRIVATE
  irods_client
  ${IRODS_EXTERNALS_FULLPATH_BOOST}/lib/libboost_program_options.so
  ${IRODS_EXTERNALS_FULLPATH_FMT}/lib/libfmt.so)

add_dependencies(${IRODS_EXECUTABLE_NAME} irods_api_plugin_replica_truncate_client)

install(
  TARGETS ${IRODS_EXECUTABLE_NAME}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
