find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(NOTIFICATION_I18N_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
  OUTPUT "${NOTIFICATION_I18N_DIR}/I18nKeys.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${NOTIFICATION_I18N_DIR}"
  COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/scripts/gen_i18n.py"
          "${REPO_ROOT}/lib/I18n/translations" "${NOTIFICATION_I18N_DIR}"
          --builtin-langs EN,PT
  DEPENDS "${REPO_ROOT}/scripts/gen_i18n.py"
          "${REPO_ROOT}/lib/I18n/translations/english.yaml"
          "${REPO_ROOT}/lib/I18n/translations/portuguese-BR.yaml"
  WORKING_DIRECTORY "${REPO_ROOT}"
  VERBATIM
)

add_executable(NotificationCenterTest
  "${NOTIFICATION_I18N_DIR}/I18nKeys.h"
  NotificationCenterTest.cpp
  ${REPO_ROOT}/src/notifications/NotificationCenter.cpp
)

target_include_directories(NotificationCenterTest PRIVATE
  "${NOTIFICATION_I18N_DIR}"
  ${REPO_ROOT}/src
)

target_link_libraries(NotificationCenterTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(NotificationCenterTest)
