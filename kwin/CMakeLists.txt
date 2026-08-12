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
find_package(KF6Screen 6.0 REQUIRED)
find_package(KWin 6.7 REQUIRED)
find_package(Qt6 6.7 REQUIRED COMPONENTS DBus)

qt_add_qml_module(lumasaveqml
    URI org.kde.lumasave
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/org/kde/lumasave
    SOURCES ../plasmoid/plugin/lumasavecontroller.cpp ../plasmoid/plugin/lumasavecontroller.h)
target_link_libraries(lumasaveqml PRIVATE Qt6::Core Qt6::Qml Qt6::DBus KF6::ConfigCore KF6::Screen)
target_include_directories(lumasaveqml PRIVATE ../plasmoid/plugin)
set_target_properties(lumasaveqmlplugin PROPERTIES
    PREFIX "lib"
    INSTALL_RPATH "$ORIGIN")
install(TARGETS lumasaveqml lumasaveqmlplugin
    LIBRARY DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/lumasave)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org/kde/lumasave/qmldir
    ${CMAKE_CURRENT_BINARY_DIR}/org/kde/lumasave/lumasaveqml.qmltypes
    DESTINATION ${KDE_INSTALL_QMLDIR}/org/kde/lumasave)

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
set_target_properties(lumasave-calibrate PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
target_link_libraries(lumasave-calibrate PRIVATE Qt6::Core Qt6::DBus Qt6::Gui Qt6::Quick)
install(TARGETS lumasave-calibrate RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})

kcoreaddons_add_plugin(kcm_lumasave
    SOURCES ../kcm/lumasavekcm.cpp
    INSTALL_NAMESPACE "plasma/kcms/systemsettings")
target_include_directories(kcm_lumasave PRIVATE ../kcm)
target_link_libraries(kcm_lumasave PRIVATE KF6::KCMUtils KF6::ConfigCore KF6::Screen Qt6::DBus Qt6::Widgets)
install(FILES ../kcm/kcm_lumasave.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(PROGRAMS ../scripts/lumasave-calibrate-preview DESTINATION ${KDE_INSTALL_BINDIR})
install(PROGRAMS ../scripts/lumasave-check DESTINATION ${KDE_INSTALL_BINDIR})
install(DIRECTORY ../plasmoid/ DESTINATION ${KDE_INSTALL_DATADIR}/plasma/plasmoids/com.github.lkarlslund.lumasave
    PATTERN "plugin" EXCLUDE)
