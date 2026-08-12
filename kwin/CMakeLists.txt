cmake_minimum_required(VERSION 3.25)
project(lumasave-kwin VERSION 0.1.0 LANGUAGES CXX)
set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
include(KDECMakeSettings)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Quick Widgets)
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons Config)
find_package(KWin 6.7 REQUIRED)

kcoreaddons_add_plugin(lumasave INSTALL_NAMESPACE "kwin/effects/plugins"
    SOURCES main.cpp lumasaveeffect.cpp resources.qrc)
target_link_libraries(lumasave PRIVATE KWin::kwin Qt6::DBus KF6::ConfigCore)
