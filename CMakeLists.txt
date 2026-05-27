cmake_minimum_required(VERSION 3.20)
project(DataManagement_System LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 让 MSVC 把源文件按 UTF-8 解析,避免中文字符串字面量乱码
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_executable(DataManagement_System
        src/main.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        src/SqlHighlighter.cpp
        src/SqlHighlighter.h
        src/storage/Table.cpp
        src/storage/Table.h
        src/storage/CsvIO.cpp
        src/storage/CsvIO.h
        src/sql/Token.h
        src/sql/Lexer.cpp
        src/sql/Lexer.h
        src/sql/Ast.h
        src/sql/Parser.cpp
        src/sql/Parser.h
        src/sql/Executor.cpp
        src/sql/Executor.h
        src/sql/SqlError.h
        resources/resources.qrc
)

target_include_directories(DataManagement_System PRIVATE src)
target_link_libraries(DataManagement_System PRIVATE Qt6::Widgets Qt6::Svg)

# 把示例 CSV 拷到运行目录旁
add_custom_command(TARGET DataManagement_System POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:DataManagement_System>/data"
        COMMENT "Copying CSV data files to build directory"
)

if(WIN32)
    add_custom_command(TARGET DataManagement_System POST_BUILD
            COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
            --no-translations --no-system-d3d-compiler --no-opengl-sw
            "$<TARGET_FILE:DataManagement_System>"
            COMMENT "Deploying Qt runtime DLLs"
    )
endif()
