add_executable(BrowserHtmlParserTest
  BrowserHtmlParserTest.cpp
  ${REPO_ROOT}/src/browser/BrowserHtmlParser.cpp
)
target_include_directories(BrowserHtmlParserTest PRIVATE ${REPO_ROOT}/src)
target_link_libraries(BrowserHtmlParserTest PRIVATE crosspoint_test_common)
add_test(NAME BrowserHtmlParserTest COMMAND BrowserHtmlParserTest)
