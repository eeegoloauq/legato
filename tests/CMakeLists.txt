find_package(Qt6 REQUIRED COMPONENTS Test)

# One executable per file under QtTest, linked against the module the app loads.
foreach(name subsonic coverstore diskcache iteminfo lrclib palette crossfade automix instantmix playqueue scrobbletracker serverlist keychain models)
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

# The app on a virtual X display against the fake server (tests/app_test.sh).
find_program(LEGATO_XVFB_RUN xvfb-run)
find_package(Python3 COMPONENTS Interpreter)
if(LEGATO_XVFB_RUN AND Python3_FOUND)
  add_test(NAME app COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/app_test.sh $<TARGET_FILE:legato-app>
    ${PROJECT_SOURCE_DIR}/scripts/fake-subsonic.py)
  set_tests_properties(app PROPERTIES TIMEOUT 120)
endif()

# Under the sanitize preset every test runs with the sanitizers' settings: a leak or undefined
# behaviour fails it, and what is not ours is in sanitizers/.
if(LEGATO_SANITIZE)
  get_property(tests DIRECTORY PROPERTY TESTS)
  set_property(TEST ${tests} APPEND PROPERTY ENVIRONMENT
    "ASAN_OPTIONS=detect_leaks=1:strict_init_order=1:check_initialization_order=1"
    "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/sanitizers/lsan.supp:print_suppressions=0"
    "UBSAN_OPTIONS=print_stacktrace=1")
endif()
