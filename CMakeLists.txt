cmake_minimum_required(VERSION 3.24)

project(Notera VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Quick QuickControls2 QuickDialogs2 Pdf Sql)

qt_standard_project_setup(REQUIRES 6.8)

set_source_files_properties(src/ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(Notera
    MACOSX_BUNDLE
    WIN32
    src/main.cpp
    src/app/ApplicationController.h
    src/app/ApplicationController.cpp
    src/platform/AppDataPaths.h
    src/platform/AppDataPaths.cpp
    src/services/DatabaseService.h
    src/services/DatabaseService.cpp
    src/services/FileService.h
    src/services/FileService.cpp
    src/features/library/Score.h
    src/features/library/ScoreListModel.h
    src/features/library/ScoreListModel.cpp
    src/features/library/NamedListModel.h
    src/features/library/NamedListModel.cpp
    src/features/library/LibraryEntryModel.h
    src/features/library/LibraryEntryModel.cpp
    src/features/library/ScoreRepository.h
    src/features/library/ScoreRepository.cpp
    src/features/library/ThumbnailGenerator.h
    src/features/library/ThumbnailGenerator.cpp
    src/features/library/LibraryService.h
    src/features/library/LibraryService.cpp
)

qt_add_qml_module(Notera
    URI Notera
    VERSION 1.0
    QML_FILES
        src/ui/Main.qml
        src/ui/AppShell.qml
        src/ui/Theme.qml
        src/ui/components/AppButton.qml
        src/ui/components/IconButton.qml
        src/ui/components/AppDialog.qml
        src/ui/components/ConfirmDialog.qml
        src/ui/components/AppMenu.qml
        src/ui/components/AppMenuItem.qml
        src/ui/components/AppMenuSeparator.qml
        src/ui/components/Sidebar.qml
        src/ui/pages/LibraryPage.qml
        src/ui/pages/ReaderPage.qml
        src/ui/pages/SettingsPage.qml
)

target_include_directories(Notera PRIVATE src)

target_link_libraries(Notera
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Concurrent
        Qt6::Pdf
        Qt6::Sql
)

set_target_properties(Notera PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.notera.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Notera"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(Notera)

# Qt generates an install-time deployment script. It bundles the required Qt
# frameworks on macOS and the required DLLs on Windows before CPack packages them.
qt_generate_deploy_qml_app_script(
    TARGET Notera
    OUTPUT_SCRIPT deploy_script
)

install(TARGETS Notera
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
install(SCRIPT ${deploy_script})

set(CPACK_PACKAGE_NAME "Notera")
set(CPACK_PACKAGE_VENDOR "Notera")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "本地优先的乐谱阅读器")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Notera")
    # CPack runs this after Qt deployment has copied all nested frameworks and
    # plug-ins, but before the DMG is assembled.
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SignMacBundle.cmake")
elseif(WIN32)
    # Requires WiX Toolset on the Windows packaging machine.
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "C2A4B1D0-76FB-4D37-94B3-32D3A9FD44F1")
endif()

include(CPack)
