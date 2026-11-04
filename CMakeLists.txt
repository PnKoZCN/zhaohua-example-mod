cmake_minimum_required(VERSION 3.20)
project(example_mod C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ══════════════════════════════════════════════════════════
#  配置
# ══════════════════════════════════════════════════════════

# Nexus 引擎源码目录（相对本工程或绝对路径）
set(ZH_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zh_engine"
    CACHE PATH "ZhaoHua Nexus 引擎源码目录")

# 链接模式：shared（动态依赖引擎）| static（静态打包引擎）
set(ZH_LINK_MODE "shared" CACHE STRING "shared or static")

# 是否包含示例功能
option(BUILD_FEATURE_TICK  "tick 计数示例"      ON)
option(BUILD_FEATURE_HP    "血量示例"          ON)
option(BUILD_FEATURE_LOG   "调用统计示例"      ON)
option(BUILD_FEATURE_MULTI "多实例示例"        ON)

# ══════════════════════════════════════════════════════════
#  引擎
# ══════════════════════════════════════════════════════════

if(NOT EXISTS "${ZH_ENGINE_DIR}/include/zh/zh_layer.h")
    message(FATAL_ERROR
        "找不到 Nexus 引擎头文件。\n"
        "请设置 -DZH_ENGINE_DIR=/path/to/zh_engine\n"
        "或把引擎源码放到 ${CMAKE_CURRENT_SOURCE_DIR}/../zh_engine")
endif()

if(ZH_LINK_MODE STREQUAL "static")
    message(STATUS "链接模式: 静态打包引擎")
    add_subdirectory(${ZH_ENGINE_DIR} zh_engine_build)
    set(ZH_LINK_TARGET zh_engine)
else()
    message(STATUS "链接模式: 动态依赖引擎（运行时从 libzh_engine.so 解析）")
    set(ZH_LINK_TARGET "")
endif()

# ══════════════════════════════════════════════════════════
#  Mod 源码
# ══════════════════════════════════════════════════════════

set(MOD_SOURCES
    src/entry.c
    src/worker.c
    src/util/util.c
)

if(BUILD_FEATURE_TICK)
    list(APPEND MOD_SOURCES src/features/feature_tick.c)
endif()
if(BUILD_FEATURE_HP)
    list(APPEND MOD_SOURCES src/features/feature_hp.c)
endif()
if(BUILD_FEATURE_LOG)
    list(APPEND MOD_SOURCES src/features/feature_log.c)
endif()
if(BUILD_FEATURE_MULTI)
    list(APPEND MOD_SOURCES src/features/feature_multi.c)
endif()

add_library(example_mod SHARED ${MOD_SOURCES})

target_include_directories(example_mod PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ZH_ENGINE_DIR}/include
)

if(ZH_LINK_TARGET)
    target_link_libraries(example_mod PRIVATE ${ZH_LINK_TARGET})
endif()

# Android
if(ANDROID)
    target_link_libraries(example_mod PRIVATE log android)
    target_compile_options(example_mod PRIVATE
        -Wall -Wextra -Werror
        -fvisibility=hidden           # 只导出显式标记的符号
        -fno-omit-frame-pointer
    )
    target_compile_definitions(example_mod PRIVATE
        ZH_MOD_NAME="example_mod"
    )
    # 显式导出：constructor/destructor 自动可见，
    # 其他符号默认隐藏
    target_link_options(example_mod PRIVATE
        -Wl,--exclude-libs,ALL
    )
else()
    target_compile_options(example_mod PRIVATE -Wall -Wextra)
endif()

set_target_properties(example_mod PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "example_mod"
)

# ══════════════════════════════════════════════════════════
#  输出
# ══════════════════════════════════════════════════════════

set_target_properties(example_mod PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out
)

install(TARGETS example_mod
    LIBRARY DESTINATION out
)

message(STATUS "─────────────────────────────────")
message(STATUS "Mod 名称:  libexample_mod.so")
message(STATUS "引擎目录:  ${ZH_ENGINE_DIR}")
message(STATUS "链接模式:  ${ZH_LINK_MODE}")
message(STATUS "示例功能:  tick=${BUILD_FEATURE_TICK} hp=${BUILD_FEATURE_HP} "
               "log=${BUILD_FEATURE_LOG} multi=${BUILD_FEATURE_MULTI}")
message(STATUS "─────────────────────────────────")
