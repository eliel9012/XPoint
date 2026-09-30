add_executable(SkyTokenStoreTest
  SkyTokenStoreTest.cpp
  ${REPO_ROOT}/src/sky/SkyTokenStore.cpp
)
target_include_directories(SkyTokenStoreTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src/sky
)
target_link_libraries(SkyTokenStoreTest PRIVATE
  crosspoint_test_common
  ArduinoJson
  GTest::gtest_main
)
gtest_discover_tests(SkyTokenStoreTest)
