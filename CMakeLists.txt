cmake_minimum_required(VERSION 3.14)

project(industrial-camera-viewer VERSION 0.1 LANGUAGES CXX)

include_directories("/usr/local/include/aravis-0.8/")

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO IMPORTED_TARGET gio-2.0)
pkg_check_modules(GLIB IMPORTED_TARGET glib-2.0)
pkg_check_modules(GOBJECT IMPORTED_TARGET gobject-2.0)
pkg_check_modules(GTHREAD IMPORTED_TARGET gthread-2.0)
pkg_check_modules(LIBXML IMPORTED_TARGET libxml-2.0)
pkg_check_modules(jsoncpp REQUIRED jsoncpp)
include_directories(${GLIB_INCLUDE_DIRS})

find_package(glog REQUIRED)
find_package(fmt REQUIRED)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror ")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=format")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=sign-compare ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=array-bounds")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=missing-field-initializers")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=pointer-arith ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=sizeof-pointer-div")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=unused-result")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pthread -fopenmp ")

# Force using Qt5 because of otherwise QtQuick.Controls 1.4 is not available
find_package(QT REQUIRED COMPONENTS Core NAMES Qt5)

message(STATUS "Found Qt ${QT_VERSION_MAJOR}")

find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS DBus REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Quick REQUIRED)
include_directories(${Qt5Widgets_INCLUDE_DIRS})

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(Boost COMPONENTS system filesystem iostreams REQUIRED)

find_package(RunningStats REQUIRED)
include_directories(${RUNNINGSTATS_INCLUDE_DIRS})

find_package(Ceres REQUIRED)

find_package(GTest REQUIRED)

set(COMMON_LIBS
    ceres
    glog::glog
    ${RUNNINGSTATS_LIBRARIES}
    ${OpenCV_LIBRARIES}
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::DBus
    fmt::fmt
    libaravis-0.8.so
    glib-2.0 gio-2.0 gobject-2.0 gthread-2.0
    ParallelTime
)

set(LIB_SOURCES
    arv_ext.cpp
    buffer.cpp
    cameramanager.cpp
    dbusreceiver.cpp
    misc.cpp
    setfalseondestruct.cpp
    whitebalance.cpp
)

set(PROJECT_SOURCES
        main.cpp
        qml.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(industrial-camera-viewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${LIB_SOURCES}
    )
else()
    add_executable(industrial-camera-viewer
        ${PROJECT_SOURCES}
        ${LIB_SOURCES}
    )
    add_executable(tests tests.cpp
        ${LIB_SOURCES}
    )
endif()

target_link_libraries(tests
    GTest::gtest
    ${COMMON_LIBS}
)


target_link_libraries(industrial-camera-viewer
  ${COMMON_LIBS}
)

add_executable(industrial-camera-viewer-dbus-sender camera-viewer-dbus-sender.cpp)

target_link_libraries(industrial-camera-viewer-dbus-sender
    ${COMMON_LIBS}
)

set_target_properties(industrial-camera-viewer PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS industrial-camera-viewer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_import_qml_plugins(industrial-camera-viewer)
    qt_finalize_executable(industrial-camera-viewer)

    qt_import_qml_plugins(tests)
    qt_finalize_executable(tests)
endif()
