cmake_minimum_required(VERSION 3.10)
project(poco_template_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск POCO библиотек
find_package(Poco REQUIRED COMPONENTS 
    Foundation 
    Net 
    Util 
    JSON 
    JWT 
    Prometheus
    Crypto
)

# Добавляем директории для include
include_directories(${CMAKE_SOURCE_DIR}/src)

# Собираем исходники
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

# Создаем статическую библиотеку из основного кода (без main.cpp)
list(REMOVE_ITEM SOURCES ${CMAKE_SOURCE_DIR}/src/main.cpp)

add_library(car_rental_lib STATIC ${SOURCES} ${HEADERS})

# Линкуем библиотеку с POCO
target_link_libraries(car_rental_lib
    Poco::Foundation
    Poco::Net
    Poco::Util
    Poco::JSON
    Poco::JWT
    Poco::Prometheus
    Poco::Crypto
    OpenSSL::Crypto
    OpenSSL::SSL
)

# Создаем исполняемый файл из main.cpp и библиотеки
add_executable(poco_template_server src/main.cpp)
target_link_libraries(poco_template_server car_rental_lib)
