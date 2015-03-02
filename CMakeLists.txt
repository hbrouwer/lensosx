# (c) 2015, Harm Brouwer

cmake_minimum_required(VERSION 2.8)

project(LensOSX)

set(VERSION "OSX-1.0a")
set(CMAKE_VERBOSE_MAKEFILE 1)

# setup build for OSX
if(APPLE)
        set(PROGNAME LensOSX)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;i386")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.5")
endif()

# X11 for linking
set(X11_INCLUDE_DIR 
        /opt/X11/include
        )
set(X11_LIBRARIES 
        /opt/X11/lib/libX11.dylib)
include_directories(${X11_INCLUDE_DIR})
link_libraries(${X11_LIBRARIES})

# TCL/TK, so that we can link against it later
find_package(TCL REQUIRED)
if(TCL_FOUND)
        include_directories(${TCL_INCLUDE_PATH})
endif()
if(TK_FOUND)
        include_directories(${TK_INCLUDE_PATH})
endif()

# source files
set(LensOSX_SRC
        src/act.c
        src/canvRect.c
        src/command.c
        src/connect.c
        src/connectCom.c
        src/control.c
        src/display.c
        src/displayCom.c
        src/example.c
        src/exampleCom.c
        src/extension.c
        src/graph.c
        src/graphCom.c
        src/main.c
        src/network.c
        src/networkCom.c
        src/object.c
        src/objectCom.c
        src/parallel.c
        src/parallelCom.c
        src/tkConsolePrint.c
        src/train.c
        src/trainCom.c
        src/type.c
        src/util.c
        )

# MacOSX resources
set(LensOSX_MACOSX_RESOURCE_FILES 
        ${LensOSX_SOURCE_DIR}/LensOSX.icns)

set_source_files_properties(
        ${LensOSX_MACOSX_RESOURCE_FILES}
        PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)

# executable that we want to build
add_executable(${PROGNAME} MACOSX_BUNDLE 
        ${LensOSX_SRC}
        ${LensOSX_MACOSX_RESOURCE_FILES})
target_link_libraries(${PROGNAME} ${X11_LIBRARY})
target_link_libraries(${PROGNAME} ${TCL_LIBRARY})
target_link_libraries(${PROGNAME} ${TK_LIBRARY})

install(TARGETS ${PROGNAME}
        BUNDLE DESTINATION ${LensOSX_SOURCE_DIR} COMPONENT Runtime)

# wrap everything into an application bundle
if(APPLE)
        set(APP ${LensOSX_SOURCE_DIR}/${PROGNAME}.app)

        # bundle properties
        set(MACOSX_BUNDLE_NAME "LensOSX")
        set(MACOSX_BUNDLE_VERSION ${VERSION})
        set(MACOSX_BUNDLE_ICON_FILE "LensOSX.icns")
        set(MACOSX_BUNDLE_GUI_IDENTIFIER "nl.rug.lensosx")

        set(DIRS
                ${X11_LIBRARY_PATH}
                ${TCL_LIBRARY_PATH}
                ${TK_LIBRARY_PATH})

        # install
        install(CODE "
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                \"${LensOSX_SOURCE_DIR}/commands/\" 
                \"${APP}/Contents/MacOS/commands/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                \"${LensOSX_SOURCE_DIR}/examples\" 
                \"${APP}/Contents/MacOS/examples\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory
                \"${APP}/Contents/MacOS/src\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/console.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/display.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/file.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/fileselect.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/graph.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/interface.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/lensrc.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/src/object.tcl\"
                \"${APP}/Contents/MacOS/src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy
                \"${LensOSX_SOURCE_DIR}/Src/shell.tcl\"
                \"${APP}/Contents/MacOS/Src/\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
                \"${LensOSX_SOURCE_DIR}/src/images\" 
                \"${APP}/Contents/MacOS/src/images\")
        include(BundleUtilities) 
        fixup_bundle(\"${APP}\" \"\" \"${DIRS}\")"
        COMPONENT Runtime)

set(CPACK_BINARY_DRAGNDROP "ON")
include(CPack)

endif()
