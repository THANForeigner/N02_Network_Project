cmake_minimum_required(VERSION 3.10)
project(NetworkProjectServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Project Include ===
include_directories(${PROJECT_SOURCE_DIR}/include)

# === libcurl (manual setup) ===
set(CURL_INCLUDE_DIR "D:/C++lib/curl-8.14.1/curl-8.14.1/include")
set(CURL_LIBRARY "D:/C++lib/curl-8.14.1/curl-8.14.1/lib/libcurl.dll.a")

include_directories(${CURL_INCLUDE_DIR})

# === Sources ===
file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
set(MAIN_FILE ${PROJECT_SOURCE_DIR}/main.cpp)

# === Executable ===
add_executable(${PROJECT_NAME} ${MAIN_FILE} ${SRC_FILES})

# === Link Libraries ===
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    ${CURL_LIBRARY}
    PowrProf  
    ws2_32    
)
