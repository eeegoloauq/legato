find_package(Qt6 REQUIRED COMPONENTS Test)

# One executable per file under QtTest, linked against the module the app loads.
foreach(name subsonic iteminfo lrclib palette crossfade automix instantmix playqueue scrobbletracker serverlist keychain models)
  qt_add_executable(${name}_test ${name}_test.cpp)
  target_link_libraries(${name}_test PRIVATE legato Qt6::Test)
  add_test(NAME ${name} COMMAND ${name}_test)
endforeach()

# MPRIS on a bus of its own, so the test never meets the desktop's players.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_program(LEGATO_DBUS_RUN_SESSION dbus-run-session)
  qt_add_executable(mpris_test mpris_test.cpp)
  target_link_libraries(mpris_test PRIVATE legato Qt6::Test)
  if(LEGATO_DBUS_RUN_SESSION)
    add_test(NAME mpris COMMAND ${LEGATO_DBUS_RUN_SESSION} -- $<TARGET_FILE:mpris_test>)
  else()
    add_test(NAME mpris COMMAND mpris_test)
  endif()
endif()

# Pictures are made under a QGuiApplication, which needs a platform.
set_tests_properties(palette PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Exercise actual pointer delivery through the viewport above the scrolling grid.
find_program(LEGATO_QMLTESTRUNNER qmltestrunner HINTS "${QT6_INSTALL_PREFIX}/bin")
if(LEGATO_QMLTESTRUNNER)
  add_test(NAME wheel_delivery COMMAND "${LEGATO_QMLTESTRUNNER}" -input "${CMAKE_CURRENT_SOURCE_DIR}/qml")
  set_tests_properties(wheel_delivery PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
