# usage:
#   mkdir build && cd build && cmake ..
#   cmake --build . --config Release
# rebuild:
#   cmake --build . --config Release --clean-first

cmake_minimum_required(VERSION 3.10)

project(tkvideo VERSION 1.4.1)
enable_language(RC)

set (PKG_VERSION ${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR}${PROJECT_VERSION_PATCH})
set (PKG_DOT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

find_package(TclStub REQUIRED)

set(TARGETNAME ${PROJECT_NAME}${PKG_VERSION})
set(SOURCE
     generic/tkvideo.c
     win/winvideo.cpp
     win/graph.cpp
     win/dshow_utils.cpp
     win/tkvideo.rc)
     
add_library(${TARGETNAME} SHARED ${SOURCE})

target_compile_features(${TARGETNAME} PRIVATE c_std_11)

include_directories(${TCL_INCLUDE_PATH} ${TK_INCLUDE_PATH})
include_directories(generic win)
target_link_libraries(${TARGETNAME} ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY})
if (MSVC)
    add_definitions(-W3 -Ot -Oi -fp:strict -Gm- -Gs -GS -GL)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_NON_CONFORMING_WCSTOK)
    target_link_libraries(${TARGETNAME} -ltcg)
endif(MSVC)
add_definitions(-DUNICODE -D_UNICODE -DUSE_TCL_STUBS -DUSE_TK_STUBS)
add_definitions(-DPACKAGE_NAME="${PROJECT_NAME}")
add_definitions(-DPACKAGE_VERSION="${PKG_DOT_VERSION}")
add_definitions(-DCOMMAVERSION=${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0)
add_definitions(-DDOTVERSION="${PKG_DOT_VERSION}")
add_definitions(-DVERSION="${PKG_VERSION}")

set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
set(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL "${CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL} /SAFESEH:NO")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /SAFESEH:NO")
set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /SAFESEH:NO")
set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} /SAFESEH:NO")
set(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL "${CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL} /SAFESEH:NO")
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /SAFESEH:NO")
set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO} /SAFESEH:NO")

# Perform the pkgIndex.tcl.in substitutions and copy to the output directory.
#set (PACKAGE_NAME "${PROJECT_NAME}")
#set (PACKAGE_VERSION "${PKG_DOT_VERSION}")
#set (PKG_LIB_FILE "${TARGETNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
#configure_file( pkgIndex.tcl.in pkgIndex.tcl @ONLY)
#add_custom_command(TARGET ${TARGETNAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
#    "${PROJECT_BINARY_DIR}/pkgIndex.tcl" $<TARGET_FILE_DIR:${TARGETNAME}>)

# Generate the pkgIndex.tcl file
file(GENERATE
    OUTPUT "$<TARGET_FILE_DIR:${TARGETNAME}>/pkgIndex.tcl"
    CONTENT "package ifneeded ${PROJECT_NAME} ${PKG_DOT_VERSION} [list load [file join $dir $<TARGET_FILE_NAME:${TARGETNAME}>]]\n")
