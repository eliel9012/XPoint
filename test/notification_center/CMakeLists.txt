add_executable(NotificationCenterTest
  NotificationCenterTest.cpp
  ${REPO_ROOT}/src/notifications/NotificationCenter.cpp
)

target_include_directories(NotificationCenterTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/I18n
)

target_link_libraries(NotificationCenterTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(NotificationCenterTest)
