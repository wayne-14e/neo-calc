cmake_minimum_required(VERSION 3.21)

project(Neo
    VERSION 1.0.0
    DESCRIPTION "Neo, a cross-platform Qt 6 calculator"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

qt_add_executable(calculator
    main.cpp
    calculator.cpp
    calculator.h
    calcengine.cpp
    calcengine.h
    formula.cpp
    formula.h
)

target_link_libraries(calculator PRIVATE Qt6::Widgets)

target_compile_definitions(calculator PRIVATE
    NEO_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(calculator PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(calculator PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(calculator PROPERTIES
    OUTPUT_NAME "Neo"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Neo"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.neo.Neo"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
)

include(GNUInstallDirs)

install(TARGETS calculator
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
