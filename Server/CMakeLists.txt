cmake_minimum_required(VERSION 3.10)
project(NetworkProjectServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === OpenCV (local folder) ===
# Set the hint for find_package to look in your local install directory.
set(OpenCV_DIR ${PROJECT_SOURCE_DIR}/lib/opencv/install)
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV ${OpenCV_VERSION} using C++ compiler: ${CMAKE_CXX_COMPILER_ID}")

# === Sources ===
file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)
set(MAIN_FILE ${PROJECT_SOURCE_DIR}/main.cpp)

# === Executable ===
add_executable(${PROJECT_NAME} ${MAIN_FILE} ${SRC_FILES})

# === Modern way to set include directories for a specific target ===
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# === Link Libraries ===
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}
    PowrProf
    ws2_32
)

# === Post-Build Step to copy DLLs (with corrected path) ===
# This copies the needed DLL to your build folder so the .exe can run.

if(MSVC)
    # The path for Visual Studio built DLLs
    set(OPENCV_DLL_DIR ${PROJECT_SOURCE_DIR}/lib/opencv/install/x64/vc16/bin)
else() # Assumes MinGW/GCC
    # The CORRECT path for MinGW built DLLs
    set(OPENCV_DLL_DIR ${PROJECT_SOURCE_DIR}/lib/opencv/install/x64/mingw/bin)
endif()

# Find all DLLs in the chosen directory
file(GLOB OPENCV_DLLS "${OPENCV_DLL_DIR}/*.dll")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${OPENCV_DLLS}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying OpenCV DLLs to executable directory..."
)