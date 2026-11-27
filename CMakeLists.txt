cmake_minimum_required(VERSION 3.21)
project(Clipify VERSION 0.1.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2 Sql Widgets Network Concurrent)
qt_standard_project_setup()

qt_add_executable(clipify WIN32
    src/ScreenCapture.cpp
    src/ScreenCapture.h
    src/TextRecognition.cpp
    src/TextRecognition.h
    src/ReleaseCheck.cpp
    src/ReleaseCheck.h
    src/HistoryPause.cpp
    src/HistoryPause.h
    src/LlmService.cpp
    src/LlmService.h
    src/WhisperModule.cpp
    src/WhisperModule.h
    src/CommandShortcuts.cpp
    src/CommandShortcuts.h
    src/Converter.cpp
    src/Converter.h
    src/AppConfig.cpp
    src/AppConfig.h
    src/QuickNotes.cpp
    src/QuickNotes.h
    src/main.cpp
    src/ClipboardModule.cpp
    src/ClipboardModule.h
    src/DesktopShortcut.cpp
    src/DesktopShortcut.h
)
qt_add_qml_module(clipify
    URI Clipify
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES qml/Main.qml qml/HistoryPopup.qml qml/HistoryAction.qml qml/ScreenshotGallery.qml qml/ClipboardTypeIcon.qml qml/QuickNotePopup.qml qml/QuickNotesView.qml qml/ShortcutKeys.qml qml/LineIcon.qml qml/MessageBox.qml qml/ConverterView.qml qml/CommandShortcutsView.qml qml/WhisperView.qml qml/LlmSettings.qml qml/AppTextField.qml qml/WhisperOverlay.qml qml/Dropdown.qml qml/RecordingIcon.qml qml/RowRemoval.qml qml/CursorTooltip.qml qml/HistoryToolbar.qml qml/HistorySizeInput.qml
)
qt_add_resources(clipify "clipify_branding"
    PREFIX "/clipify"
    FILES assets/clipify-app-icon.png
)
target_link_libraries(clipify PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Sql Qt6::Widgets Qt6::Network Qt6::Concurrent)
target_compile_definitions(clipify PRIVATE CLIPIFY_VERSION="${PROJECT_VERSION}")

set(dictation_prompt prompts/dictation-polishing.md)
set(prompt_build_dirs "${CMAKE_CURRENT_BINARY_DIR}")
if(CMAKE_CONFIGURATION_TYPES)
    set(prompt_build_dirs "")
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        list(APPEND prompt_build_dirs "${CMAKE_CURRENT_BINARY_DIR}/${config}")
    endforeach()
endif()
foreach(prompt_build_dir IN LISTS prompt_build_dirs)
    if(NOT EXISTS "${prompt_build_dir}/${dictation_prompt}")
        configure_file(${dictation_prompt} "${prompt_build_dir}/${dictation_prompt}" COPYONLY)
    endif()
endforeach()

if(WIN32)
    install(TARGETS clipify RUNTIME DESTINATION .)
    install(FILES ${dictation_prompt} DESTINATION prompts)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    configure_file(scripts/setup-gnome-shortcut.py clipify-setup-gnome COPYONLY)
    find_package(X11 REQUIRED)
    find_package(Qt6 6.4 REQUIRED COMPONENTS DBus)
    target_compile_definitions(clipify PRIVATE CLIPIFY_X11)
    target_link_libraries(clipify PRIVATE X11::X11 Qt6::DBus)

    include(GNUInstallDirs)
    file(RELATIVE_PATH clipify_prompt_dir "${CMAKE_INSTALL_FULL_BINDIR}"
         "${CMAKE_INSTALL_FULL_DATADIR}/clipify/prompts")
    target_compile_definitions(clipify PRIVATE CLIPIFY_INSTALLED_PROMPT_DIR="${clipify_prompt_dir}")
    install(FILES ${dictation_prompt} DESTINATION ${CMAKE_INSTALL_DATADIR}/clipify/prompts)
    install(TARGETS clipify RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/io.github.riccify.Clipify.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/clipify-app-icon.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
        RENAME io.github.riccify.Clipify.png
    )
    install(FILES packaging/io.github.riccify.Clipify-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/clipify
    )
    install(PROGRAMS scripts/setup-gnome-shortcut.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME clipify-setup-gnome
    )
    install(PROGRAMS scripts/install-ocr-best.sh
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME clipify-install-ocr-best
    )
    install(FILES LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/clipify
        RENAME copyright
    )

    set(CPACK_GENERATOR DEB)
    set(CPACK_PACKAGE_NAME clipify)
    set(CPACK_PACKAGE_CONTACT "Rick <67060146+riccify@users.noreply.github.com>")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/riccify/Clipify")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Desktop toolbox for clipboard history, screenshots, and local OCR")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_SECTION utils)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_STRIP_FILES ON)

    # Qt loads QML imports and plugins at runtime, so shlibdeps cannot discover them.
    string(JOIN ", " CPACK_DEBIAN_PACKAGE_DEPENDS
        qt6-qpa-plugins
        qt6-image-formats-plugins
        qml6-module-qtqml
        qml6-module-qtqml-workerscript
        qml6-module-qtquick
        qml6-module-qtquick-controls
        qml6-module-qtquick-layouts
        qml6-module-qtquick-templates
        qml6-module-qtquick-window
        libqt6sql6-sqlite
        libxtst6
        hicolor-icon-theme
        pkexec
        python3-gi
        libglib2.0-bin
        tesseract-ocr
        tesseract-ocr-deu
        tesseract-ocr-eng
    )
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS curl)
    include(CPack)
endif()
