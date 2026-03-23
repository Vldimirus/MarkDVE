set(MARKDVE_APP_SOURCES
    main.cpp
    mainwindow.cpp
    workspacemanager.cpp
    markdownview.cpp
    documenttabwidget.cpp
    documentcontroller.cpp
    editorwindow.cpp
    editorsettings.cpp
    editorsettingsdialog.cpp
    markdownhighlighter.cpp
    viewersettings.cpp
    viewersettingsdialog.cpp
    codemapgenerator.cpp
)

set(MARKDVE_APP_HEADERS
    mainwindow.h
    workspacemanager.h
    markdownview.h
    documenttabwidget.h
    documentcontroller.h
    editorwindow.h
    editorsettings.h
    editorsettingsdialog.h
    markdownhighlighter.h
    viewersettings.h
    viewersettingsdialog.h
    codemapgenerator.h
)

add_executable(MarkDVE
    ${MARKDVE_APP_SOURCES}
    ${MARKDVE_APP_HEADERS}
)

target_link_libraries(MarkDVE PRIVATE Qt6::Widgets)

target_include_directories(MarkDVE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(BUILD_TESTING)
    add_executable(MarkDVEWorkspaceTests
        tests/test_workspacemanager.cpp
        workspacemanager.cpp
        workspacemanager.h
    )

    target_link_libraries(MarkDVEWorkspaceTests PRIVATE Qt6::Test)
    target_include_directories(MarkDVEWorkspaceTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(NAME MarkDVEWorkspaceTests COMMAND MarkDVEWorkspaceTests)

    add_executable(MarkDVECodeMapTests
        tests/test_codemapgenerator.cpp
        codemapgenerator.cpp
        codemapgenerator.h
    )

    target_link_libraries(MarkDVECodeMapTests PRIVATE Qt6::Test)
    target_include_directories(MarkDVECodeMapTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(NAME MarkDVECodeMapTests COMMAND MarkDVECodeMapTests)

    add_executable(MarkDVEViewTests
        tests/test_markdownview.cpp
        markdownview.cpp
        markdownview.h
        viewersettings.cpp
        viewersettings.h
    )

    target_link_libraries(MarkDVEViewTests PRIVATE Qt6::Widgets Qt6::Test)
    target_include_directories(MarkDVEViewTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(NAME MarkDVEViewTests COMMAND MarkDVEViewTests)
    set_tests_properties(MarkDVEViewTests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
