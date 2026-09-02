cmake_minimum_required(VERSION 3.24)

project(Notera VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Quick QuickControls2 QuickDialogs2 Pdf Sql)

qt_standard_project_setup(REQUIRES 6.8)

set_source_files_properties(src/ui/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(src/ui/Motion.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

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
    src/services/MetronomeService.h
    src/services/MetronomeService.cpp
    src/services/MetronomeAudio.h
    src/services/MetronomeAudio.cpp
    src/features/library/Score.h
    src/features/library/ScoreListModel.h
    src/features/library/ScoreListModel.cpp
    src/features/library/NamedListModel.h
    src/features/library/NamedListModel.cpp
    src/features/library/LibraryEntryModel.h
    src/features/library/LibraryEntryModel.cpp
    src/features/library/LibrarySelectionModel.h
    src/features/library/LibrarySelectionModel.cpp
    src/features/library/ScoreRepository.h
    src/features/library/ScoreRepository.cpp
    src/features/library/ThumbnailGenerator.h
    src/features/library/ThumbnailGenerator.cpp
    src/features/library/LibraryService.h
    src/features/library/LibraryService.cpp
    src/assets/Notera.icns
)

set_source_files_properties(src/assets/Notera.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

qt_add_resources(Notera "notera_assets"
    PREFIX "/"
    FILES src/assets/notera-icon.png
)

if(WIN32)
    target_sources(Notera PRIVATE src/assets/notera.rc)
endif()

qt_add_qml_module(Notera
    URI Notera
    VERSION 1.0
    QML_FILES
        src/ui/Main.qml
        src/ui/AppShell.qml
        src/ui/Theme.qml
        src/ui/Motion.qml
        src/ui/components/AppButton.qml
        src/ui/components/AppIcon.qml
        src/ui/components/IconButton.qml
        src/ui/components/AppDialog.qml
        src/ui/components/ConfirmDialog.qml
        src/ui/components/AppMenu.qml
        src/ui/components/AppMenuItem.qml
        src/ui/components/AppMenuSeparator.qml
        src/ui/components/FolderIcon.qml
        src/ui/components/TagIcon.qml
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
        Qt6::CorePrivate
)

if(APPLE)
    target_link_options(Notera PRIVATE "-Wl,-weak_framework,AGL"
        "-Wl,-framework,CoreAudio" "-Wl,-framework,CoreFoundation"
        "-Wl,-framework,AudioToolbox")
endif()

set_target_properties(Notera PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.notera.app"
    MACOSX_BUNDLE_BUNDLE_NAME "Notera"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "Notera.icns"
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(Notera)

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
    set(CPACK_PACKAGE_FILE_NAME "Notera-v${PROJECT_VERSION}-macOS-arm64")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_BINARY_DIR}/dmg-background.png")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/configure-dmg.applescript")
    execute_process(
        COMMAND /usr/bin/sips -s format png
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/dmg-background.svg"
            --out "${CPACK_DMG_BACKGROUND_IMAGE}"
        RESULT_VARIABLE dmg_background_result
        OUTPUT_QUIET
        ERROR_VARIABLE dmg_background_error
    )
    if(NOT dmg_background_result EQUAL 0)
        message(FATAL_ERROR "Failed to render DMG background: ${dmg_background_error}")
    endif()
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SignMacBundle.cmake")
elseif(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "C2A4B1D0-76FB-4D37-94B3-32D3A9FD44F1")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/Notera.ico")
    set(CPACK_WIX_CULTURES "zh-CN")
    set(CPACK_WIX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/WIX.template.in")
    set(CPACK_WIX_UI_REF "NoteraUI_InstallDir")
    set(CPACK_WIX_EXTRA_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/NoteraInstallerUI.wxs"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/DesktopShortcut.wxs"
    )
endif()

include(CPack)
