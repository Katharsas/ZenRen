cmake_minimum_required(VERSION 3.25)
project(zenren)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO fix VS generator

message(STATUS "############## CMAKE START ##############")

if (NOT WIN32)
    message(FATAL_ERROR "Windows only project!" )
endif()

if (CMAKE_SIZEOF_VOID_P GREATER 4)
    message(STATUS "Architecture: 64bit" )
    set(ARCHITECTURE 64bit)
else()
    message(STATUS "Architecture: 32bit" )
    set(ARCHITECTURE 32bit)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_MODE Debug)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(BUILD_MODE Release)
endif ()

if (BUILD_MODE STREQUAL Debug)
    message(STATUS "BuildMode: Debug" )
elseif (BUILD_MODE STREQUAL Release)
    message(STATUS "BuildMode: Release" )
else()
    message(FATAL_ERROR "Only 'Debug' or 'Release' build type supported! Make sure to pass -DCMAKE_CONFIGURATION_TYPES=<type>")
endif()

####### General options

if(MSVC)
    add_compile_options(/arch:AVX2)
else()
    #set(CMAKE_EXE_LINKER_FLAGS "-Wl,--large-address-aware")
    message(FATAL_ERROR "MSVC compiler required!" )
endif()

####### ImGui

# Set where the ImGui files are stored
set(IMGUI_PATH  "lib/imgui")
    
# Compile as static library 
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library("imgui" STATIC ${IMGUI_SOURCES})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH})

####### Configure CMake Libraries
message(STATUS "############## THIRDPARTY LIBRARIES ##############")

option(G3_SHARED_LIB "" OFF)
option(ADD_FATAL_EXAMPLE "" OFF)
option(ADD_G3LOG_UNIT_TEST "" OFF)
add_subdirectory(lib/g3log EXCLUDE_FROM_ALL)

add_subdirectory(lib/DirectXTex EXCLUDE_FROM_ALL)

add_subdirectory(lib/meshoptimizer EXCLUDE_FROM_ALL)

add_subdirectory(lib/bvh)

#set(ZK_ENABLE_MMAP OFF)
set(ZK_BUILD_TESTS OFF)
set(ZK_ENABLE_ASAN OFF)
add_subdirectory(lib/ZenKit EXCLUDE_FROM_ALL)

####### ZenRen Executable
message(STATUS "############## EXECUTABLE ##############")

link_directories(${DXSDK_LIB_DIR})

file(GLOB_RECURSE PROJECT_SRC CONFIGURE_DEPENDS "src/*.h" "src/*.cpp" "src/*.rc" "src/viewer/win_resources/app.manifest")
add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SRC})

 if (MSVC)
    # Advanced -> Character Set -> Use Unicode
    target_compile_definitions(${PROJECT_NAME} PUBLIC UNICODE _UNICODE)

    # Linker -> System -> Enable Large Addresses -> Yes
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/LARGEADDRESSAWARE")

    if (BUILD_MODE STREQUAL Release)
        # Advanced -> Whole Program Optimization -> Use Link Time Code Generation
        # (see https://stackoverflow.com/questions/48431918/wholeprogramoptimization-in-cmake)
        # Not using generator expression here since it does not work with ninja ($ has to be replaced with $$)
        set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/GL")
        set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/LTCG")

        # Enable debug information in release mode
        # In theory it would be better to use set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase) than to directly
        # apply /Zi to CXX flags, however that does not work for Ninja generator (creates much smaller PDB, not picked up by VS)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
    else()
        # speed up debug builds by disabling basic runtime checks and enabling limited inlining
        # (see https://dirtyhandscoding.github.io/posts/fast-debug-in-visual-c.html)

        # C/C++ -> Code Generation -> Basic Runtime Checks -> Default
        string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

        # C/C++ -> Optimization -> Inline Function Expansion -> Only __inline
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1")
    endif()

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        # set visual studio's working directory for debugging since launch.vs.json > currentDir does not work for MS project files
        set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources")
        
        # set command args since it seems like launch.vs.json does basically not work at all
        #set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--noLog --rdp --level \"WORLD.ZEN\"")
    endif()
 endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/stdafx.h")

#include header files for target compilation
target_include_directories(${PROJECT_NAME} PRIVATE "src")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/g3log/src") 
target_include_directories(${PROJECT_NAME} PRIVATE "lib/DirectXMath/Inc")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/DirectXTex/DirectXTex")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/meshoptimizer/src")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/bvh/src")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/ZenKit/include")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/magic_enum")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/octree_attcs")
target_include_directories(${PROJECT_NAME} PRIVATE "lib/tinyobj")
# target_include_directories(${PROJECT_NAME} PRIVATE "${DXSDK_DIR}/Include")

target_include_directories(${PROJECT_NAME} PUBLIC "lib/ZenKit/vendor/glm")

#link static library into target (compiled source files)
target_link_libraries(${PROJECT_NAME} PRIVATE g3log)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE DirectXTex)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)
target_link_libraries(${PROJECT_NAME} PRIVATE bvh)
target_link_libraries(${PROJECT_NAME} PRIVATE zenkit)

target_link_libraries(${PROJECT_NAME} PRIVATE
    dxgi.lib dxguid.lib uuid.lib
    kernel32.lib user32.lib
    comdlg32.lib advapi32.lib shell32.lib
    ole32.lib oleaut32.lib
    ntdll.lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11.lib d3dcompiler.lib
)

message(STATUS "############## INSTALL ##############")

# Allow users to create ready-to-use program in bin folder with install command
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/ DESTINATION . FILES_MATCHING PATTERN "*")

message(STATUS "############## CMAKE END ##############")