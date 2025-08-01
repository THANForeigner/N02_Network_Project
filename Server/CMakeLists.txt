cmake_minimum_required(VERSION 3.10)
project(NetworkProjectServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Project Include ===
include_directories(${PROJECT_SOURCE_DIR}/include)

# === OpenCV (local folder) ===
# This path is correct based on your screenshot.
set(OpenCV_DIR ${PROJECT_SOURCE_DIR}/lib/opencv/install)

# This command will now succeed because you will have chosen
# a generator (MinGW or VS 2019) that matches a library folder.
find_package(OpenCV REQUIRED)

message(STATUS "Found OpenCV ${OpenCV_VERSION} using C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
include_directories(${OpenCV_INCLUDE_DIRS})

# === libcurl (manual setup) ===
set(CURL_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/lib/curl-8.14.1/curl-8.14.1/include)
set(CURL_LIBRARY ${PROJECT_SOURCE_DIR}/lib/curl-8.14.1/lib/libcurl.dll.a)
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

# === Post-Build Step to copy DLLs (Optional but Recommended) ===
# This copies the needed DLL to your build folder so the .exe can run.
# NOTE: The path to the DLL depends on whether you chose MinGW or vc16!
# CMake will handle this logic for you.

if(MSVC)
    set(OPENCV_DLL_DIR ${PROJECT_SOURCE_DIR}/lib/opencv/build/x64/vc16/bin)
else() # Assumes MinGW/GCC
    set(OPENCV_DLL_DIR ${PROJECT_SOURCE_DIR}/lib/opencv/build/x64/mingw/bin)
endif()

# Find all DLLs in the chosen directory
file(GLOB OPENCV_DLLS "${OPENCV_DLL_DIR}/*.dll")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${OPENCV_DLLS}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying OpenCV DLLs to executable directory..."
)