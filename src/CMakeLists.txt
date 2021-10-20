set(SRC
    main.cpp

    argparser.cpp
    argparser.h

    str_trim.cpp
    str_trim.h

    emfat.h
    emfat1.h
)

add_executable(${PROJECT_NAME} ${SRC})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD ${CPP_STD})
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        CLI11
        mio::mio
)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        -Dregister=
)

if (NOT WIN32)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            stdc++
            pthread
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SAFESEH:NO")
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD ${CPP_STD})

install(TARGETS ${PROJECT_NAME})

add_custom_target(run ${CMAKE_CURENT_BINARY_DIR}/${PROJECT_NAME})
