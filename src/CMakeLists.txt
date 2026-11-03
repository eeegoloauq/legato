# The executable is made here, in a directory without QML, as in lumeo: a deploy scans the target's
# own directory for QML imports, and the project root holds the tests and the build tree.

set(LEGATO_VERSION "dev" CACHE STRING "The version legato reports; a release passes its tag's")
qt_add_executable(legato-app main.cpp)
target_link_libraries(legato-app PRIVATE legato legatoplugin)
target_compile_definitions(legato-app PRIVATE LEGATO_VERSION="${LEGATO_VERSION}")
set_target_properties(legato-app PROPERTIES
  OUTPUT_NAME legato
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE TRUE
)
set_property(TARGET legato-app PROPERTY QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS ${PROJECT_SOURCE_DIR}/qml)

if(WIN32)
  # The icon Explorer, the taskbar and the installer show.
  target_sources(legato-app PRIVATE ${PROJECT_SOURCE_DIR}/packaging/legato.rc)
endif()

install(TARGETS legato-app RUNTIME DESTINATION bin BUNDLE DESTINATION .)
# Qt creates the attached ToolTip from its own "import QtQuick.Controls", which no scan of our QML
# sees: the generic Controls module, without its other styles, goes in by hand.
set(controls ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}/QtQuick/Controls)
if(WIN32)
  # windeployqt lays Qt out beside the binary, the compiler's runtime with it. libmpv and QtKeychain
  # are not Qt's and go in by hand.
  qt_generate_deploy_qml_app_script(TARGET legato-app OUTPUT_SCRIPT deploy NO_TRANSLATIONS
    EXCLUDE_PLUGIN_TYPES qmltooling generic networkinformation)
  install(SCRIPT ${deploy})
  install(FILES ${MPV_DIR}/libmpv-2.dll DESTINATION bin)
  install(IMPORTED_RUNTIME_ARTIFACTS Qt6Keychain::Qt6Keychain RUNTIME DESTINATION bin)
  install(FILES ${controls}/qmldir ${controls}/qtquickcontrols2plugin.dll ${controls}/plugins.qmltypes
    DESTINATION qml/QtQuick/Controls)
elseif(NOT APPLE)
  # The install is a bundle: the binary and the Qt it was built and tested with. libmpv and
  # QtKeychain stay the system's. Wayland beside X11; no QML debugger and no GTK theme (the
  # portal's serves).
  qt_generate_deploy_qml_app_script(TARGET legato-app OUTPUT_SCRIPT deploy NO_TRANSLATIONS
    INCLUDE_PLUGINS qwayland
    EXCLUDE_PLUGINS qgtk3
    EXCLUDE_PLUGIN_TYPES qmltooling generic egldeviceintegrations networkinformation)
  install(CODE "set(QT_DEPLOY_IGNORED_LIB_DIRS \"${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES};${MPV_LIBRARY_DIRS}\")")
  install(SCRIPT ${deploy})
  install(FILES ${controls}/qmldir ${controls}/libqtquickcontrols2plugin.so ${controls}/plugins.qmltypes
    DESTINATION qml/QtQuick/Controls)
endif()
