set(HASHTABLE_SRCS hashtable.c hashtable_itr.c)
set(ULAKEFS_SRCS Ulakefs.c options.c debug.c 
    general.c readrmdir.c
    fuse_operations.c http.c network.c)

find_package(PkgConfig)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(Jansson jansson)
set(CURL_LIBRARY "-lcurl")
find_package(CURL REQUIRED)
add_executable(ulakefs ${ULAKEFS_SRCS} ${HASHTABLE_SRCS})

if (UNIX AND NOT APPLE)
    target_link_libraries(ulakefs fuse pthread rt ${CURL_LIBRARIES} ${SSL_LIB} OpenSSL::Crypto ${LIBCONFIG_LOCATION} ${CMAKE_THREAD_LIBS_INIT} Threads::Threads ${Jansson_LIBRARIES} uuid)
    target_include_directories(ulakefs PRIVATE ${Jansson_INCLUDE_DIRS} ${CURL_INCLUDE_DIR})
else()
    target_link_libraries(ulakefs fuse pthread ${Curl_LIBRARIES} ${SSL_LIB} OpenSSL::Crypto ${LIBCONFIG_LOCATION} ${CMAKE_THREAD_LIBS_INIT} Threads::Threads ${Jansson_LIBRARIES} uuid)
endif()

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ulakefs DESTINATION bin)
