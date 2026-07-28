cmake_minimum_required(VERSION 3.21)
# LockDoors : tiny CommonLibSSE-NG SKSE plugin — one TESActivateEvent sink that pops
# "Door is Locked" when the player tries to open a door our mod holds shut (BlockActivation).
project(LockDoors VERSION 1.1.0 LANGUAGES CXX)

set(OUTPUT_FOLDER "X:/MODDINGSSE/modorganizer2/mods/Lock Doors")

find_package(CommonLibSSE CONFIG REQUIRED)
add_commonlibsse_plugin(${PROJECT_NAME} SOURCES src/plugin.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
message(STATUS "SKSE plugin output: ${DLL_FOLDER}")
add_custom_command(
    TARGET "${PROJECT_NAME}" POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    VERBATIM
)
