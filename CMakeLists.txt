cmake_minimum_required(VERSION 3.16)
project(Beachhead2000ParserViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_LINKER_PREFERRED ON)

# Size optimization flags for MSVC
if(MSVC)
    # C++ conformance
    add_compile_options(/Zc:__cplusplus /permissive-)
    # Size optimization (safe with Qt's dynamic runtime):
    #   /Os        favor small code
    #   /Gy        function-level linking (lets /OPT:REF drop unused funcs)
    #   /Zc:inline remove unreferenced COMDATs
    # NOTE: /GL (LTCG), /GR- (no RTTI), /EHs-c- (no exceptions), /MT (static
    # runtime) all caused crashes with Qt, so they stay disabled.
    add_compile_options(/Os /Gy /Zc:inline)
    # Linker: eliminate unused refs, merge identical COMDATs, strip debug info
    add_link_options(/OPT:REF /OPT:ICF /DEBUG:NONE)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt location. Overridable in order of precedence:
#   1) -DCMAKE_PREFIX_PATH=<dir>   (command line / CMake Tools configure setting)
#   2) -DQt6_DIR=<dir>/lib/cmake/Qt6
#   3) QTDIR environment variable  (used by the CMake preset)
# Falls back to the original hard-coded path only when none of the above is set.
if(NOT DEFINED CMAKE_PREFIX_PATH AND DEFINED ENV{QTDIR})
    set(CMAKE_PREFIX_PATH "$ENV{QTDIR}")
elseif(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64")
endif()

# Use static Qt libraries if available
set(QT_STATIC ON CACHE BOOL "Use static Qt")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(Beachhead2000ParserViewer
    main.cpp
    mainwindow.h
    mainwindow.cpp
    texparser.h
    texparser.cpp
    localization.h
    localization.cpp
)

target_link_libraries(Beachhead2000ParserViewer PRIVATE Qt6::Widgets)
