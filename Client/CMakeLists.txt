cmake_minimum_required(VERSION 3.10)
project(NetworkProjectClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Project Include ===
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/lib
)

# === libcurl (manual setup) ===
set(PROJECT_LIBS_DIR ${PROJECT_SOURCE_DIR}/lib)
set(CURL_ROOT ${PROJECT_LIBS_DIR}/curl-8.14.1) 
include_directories(${CURL_INCLUDE_DIR})

# === Sources ===
file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
set(MAIN_FILE ${PROJECT_SOURCE_DIR}/main.cpp)

# === Executable ===
add_executable(${PROJECT_NAME} ${MAIN_FILE} ${SRC_FILES})

# === Link Libraries ===
target_include_directories(${PROJECT_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${CURL_ROOT}/include" 
)
target_link_libraries(${PROJECT_NAME}
    ${CURL_LIBRARY}
    "${CURL_ROOT}/lib/libcurl.dll.a"
    PowrProf     
    ws2_32       
)

set(CURL_DLL_PATH "${CURL_ROOT}/bin/libcurl-x64.dll")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CURL_DLL_PATH}"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying libcurl DLL to executable directory..."
)