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
find_package(KF6 6.0 REQUIRED COMPONENTS CoreAddons Config KCMUtils)
find_package(KWin 6.7 REQUIRED)
find_package(Qt6 6.7 REQUIRED COMPONENTS DBus)

set(LUMASAVE_RUST_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../target/release/liblumasave_core.a)
add_custom_command(OUTPUT ${LUMASAVE_RUST_LIB}
    COMMAND cargo build --release --package lumasave-core
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../crates/lumasave-core/src/lib.rs
            ${CMAKE_CURRENT_SOURCE_DIR}/../crates/lumasave-core/Cargo.toml
            ${CMAKE_CURRENT_SOURCE_DIR}/../Cargo.toml
    VERBATIM)
add_custom_target(lumasave_rust_core DEPENDS ${LUMASAVE_RUST_LIB})

kcoreaddons_add_plugin(lumasave INSTALL_NAMESPACE "kwin/effects/plugins"
    SOURCES main.cpp lumasaveeffect.cpp resources.qrc)
target_link_libraries(lumasave PRIVATE KWin::kwin KF6::ConfigCore)
add_dependencies(lumasave lumasave_rust_core)
target_link_libraries(lumasave PRIVATE ${LUMASAVE_RUST_LIB})

qt_add_executable(lumasave-calibrate
    ../tools/calibration/main.cpp
    ../tools/calibration/calibrationcontroller.cpp
    ../tools/calibration/calibration.qrc)
target_link_libraries(lumasave-calibrate PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Quick)
install(TARGETS lumasave-calibrate RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})

kcoreaddons_add_plugin(kcm_lumasave
    SOURCES ../kcm/lumasavekcm.cpp
    INSTALL_NAMESPACE "plasma/kcms/systemsettings")
target_include_directories(kcm_lumasave PRIVATE ../kcm)
target_link_libraries(kcm_lumasave PRIVATE KF6::KCMUtils KF6::ConfigCore Qt6::DBus Qt6::Widgets)
install(FILES ../kcm/kcm_lumasave.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(PROGRAMS ../scripts/lumasave-calibrate-preview DESTINATION ${KDE_INSTALL_BINDIR})
