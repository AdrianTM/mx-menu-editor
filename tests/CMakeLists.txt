if(NOT Qt6Test_FOUND)
    message(STATUS "Qt6::Test not found - skipping test suite")
    return()
endif()

add_executable(test_desktoputils
    test_desktoputils.cpp
    ${CMAKE_SOURCE_DIR}/src/desktoputils.cpp
    ${CMAKE_SOURCE_DIR}/src/docviewer.cpp
    ${CMAKE_SOURCE_DIR}/src/addappdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/addappdialog.ui
    ${CMAKE_SOURCE_DIR}/src/mainwindow.cpp
    ${CMAKE_SOURCE_DIR}/src/mainwindow.ui
)
target_compile_definitions(test_desktoputils PRIVATE VERSION="test")
target_include_directories(test_desktoputils PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_desktoputils PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_compile_options(test_desktoputils PRIVATE
    -Wpedantic
    -Werror=return-type
    -Werror=switch
    -Werror=uninitialized
    -Werror
)
add_test(NAME test_desktoputils COMMAND test_desktoputils)
