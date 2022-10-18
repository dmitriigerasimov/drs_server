# Минимально потребная версия Cmake
cmake_minimum_required(VERSION 3.0)


# ----  Общие настройки -----
# Название проекта и указание основного языка
project(drs_server LANGUAGES C)
# Этот инклуд реализует парсинг файлов и всасывание результатов в переменную
include(sources/dap-sdk/cmake/ReadMKFile.cmake)
# Общие настройки
set(CMAKE_ENABLE_EXPORTS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)
set(CMAKE_C_STANDARD 11)

SET( CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
SET( CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
SET( CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
add_definitions("-DDAP_VERSION=\"${VERSION_MAJOR}-${VERSION_MINOR}.${VERSION_PATCH}\"")

# Инклуд с определением операционки и выставлением различных дефайнов общего назначения
# А так же выставление флагов для релиза и для отладки
include(sources/dap-sdk/cmake/OS_Detection.cmake)
# Перечисление модулей DAP SDK которые нам нужны
set(DAPSDK_MODULES "core crypto io app-cli")
# Подключение DAP SDK
add_subdirectory(sources/dap-sdk)

# Подключаем библиотеку DRS
add_subdirectory(sources/drs)

# ---- Непосредственно подключение файлов проекта ---

# Сервер
set (SERVER_SOURCES
    sources/main_server.c
    sources/sig_unix_handler.c
#    sources/eth_srv.c
    )
set (SERVER_HEADERS
#    sources/eth_srv.h
    sources/sig_unix_handler.h
    )
# Консолька к серверу
set (CLI_SOURCES
    sources/main_cli.c
    )

# Тесты
set (TESTS_SOURCES
    sources/main_server.c
    )
set (TESTS_HEADERS
    sources/main.c
    )


#-- Определяем исполняемый файл и библиотеки к нему
add_executable(${PROJECT_NAME} ${SERVER_SOURCES} ${SERVER_HEADERS})
target_link_libraries(${PROJECT_NAME} dap_core dap_crypto dap_io dap_cli_server)

#add_executable(${PROJECT_NAME}-cli ${CLI_SOURCES})
#target_link_libraries(${PROJECT_NAME}-cli dap_core )
