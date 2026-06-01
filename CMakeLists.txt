cmake_minimum_required(VERSION 3.15)

project(FallingSand LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Paths
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(RES_DIR ${SRC_DIR}/res)

set(DEPS_INCLUDE ${CMAKE_SOURCE_DIR}/externals/include)
set(DEPS_LIB ${CMAKE_SOURCE_DIR}/externals/lib)
set(DEPS_BIN ${CMAKE_SOURCE_DIR}/externals/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Sources
file(GLOB_RECURSE SRC_FILES
    CONFIGURE_DEPENDS
    ${SRC_DIR}/*.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Includes
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${SRC_DIR}
        ${DEPS_INCLUDE}
)

# Warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -g)
endif()

# Link dependencies based on OS
if(WIN32)
     target_link_libraries(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            ${DEPS_LIB}/sfml-graphics-d.lib
            ${DEPS_LIB}/sfml-window-d.lib
            ${DEPS_LIB}/sfml-system-d.lib
        >

        $<$<CONFIG:Release>:
            ${DEPS_LIB}/sfml-graphics.lib
            ${DEPS_LIB}/sfml-window.lib
            ${DEPS_LIB}/sfml-system.lib
        >
    )

    # Copy all DLLs after build
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${DEPS_BIN}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
elseif(APPLE)
    
elseif(UNIX)
    
endif()

# Copy resource folder after build
set(RES_DIR ${CMAKE_SOURCE_DIR}/res)

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RES_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
    COMMENT "Copying Resources folder to output directory..."
)