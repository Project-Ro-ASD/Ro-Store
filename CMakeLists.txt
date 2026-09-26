cmake_minimum_required(VERSION 3.21)

project(RoStore
    VERSION 0.2.0
    DESCRIPTION "Ro-ASD application store"
    HOMEPAGE_URL "https://github.com/Project-Ro-ASD/Ro-Store"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network DBus)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(ro-store
    src/main.cpp
    src/CatalogModel.h
    src/CatalogModel.cpp
    src/PackageStatus.h
    src/PackageStatus.cpp
    src/packages/PackageTransaction.h
    src/packages/PackageTransaction.cpp
    src/packages/PackageTransactionModel.h
    src/packages/PackageTransactionModel.cpp
    src/packages/PackageTransactionManager.h
    src/packages/PackageTransactionManager.cpp
    src/AppLauncher.h
    src/AppLauncher.cpp
    src/packages/backends/Dnf5Backend.h
    src/packages/backends/Dnf5Backend.cpp
)

qt_add_qml_module(ro-store
    URI RoStore
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/HomePage.qml
        qml/pages/AppDetailPage.qml
        qml/pages/DownloadsPage.qml
        qml/components/AppCard.qml
)

target_link_libraries(ro-store
    PRIVATE
        Qt6::Core
        Qt6::Quick
        Qt6::Network
        Qt6::DBus
)

target_include_directories(ro-store
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(GNUInstallDirs)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/org.projectro.RoStore.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.projectro.RoStore.desktop
    @ONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/org.projectro.RoStore.metainfo.xml.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.projectro.RoStore.metainfo.xml
    @ONLY
)

install(TARGETS ro-store
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org.projectro.RoStore.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/ro-store.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org.projectro.RoStore.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

set(CPACK_GENERATOR "RPM")

set(CPACK_PACKAGE_NAME "ro-store")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Project Ro-ASD")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ro-ASD application store")
set(CPACK_PACKAGE_DESCRIPTION
    "Ro-Store is the graphical application store for Ro-ASD. It allows users to browse, install, update, remove and launch applications distributed through Ro-ASD repositories."
)
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")

set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_AUTOPROV ON)

# Ro-Store package transactions use the DNF5 daemon over D-Bus.
set(CPACK_RPM_PACKAGE_REQUIRES "dnf5daemon-server, dnf5daemon-server-polkit")

set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

include(CPack)
