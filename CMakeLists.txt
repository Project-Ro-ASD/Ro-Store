cmake_minimum_required(VERSION 3.21)

project(RoStore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network)

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(ro-store
    src/main.cpp
    src/CatalogModel.h
    src/CatalogModel.cpp
    src/PackageStatus.h
    src/PackageStatus.cpp
    src/PackageInstaller.h
    src/PackageInstaller.cpp
    src/AppLauncher.h
    src/AppLauncher.cpp
)

qt_add_qml_module(ro-store
    URI RoStore
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/pages/HomePage.qml
        qml/pages/AppDetailPage.qml
        qml/components/AppCard.qml
)

target_link_libraries(ro-store
    PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::Network
)

target_include_directories(ro-store
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
