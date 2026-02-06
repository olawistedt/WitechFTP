cmake_minimum_required(VERSION 3.16)
project(QtFtpClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Workaround for MinGW GCC internal compiler error with AVX512
if(MINGW)
    add_compile_options(-mno-avx512f -fno-ipa-cp -fno-ipa-sra)
endif()

# Hitta Qt-moduler
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Aktivera Qt-specifika verktyg
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Lägg till källkodsfiler
add_executable(QtFtpClient
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/resources.qrc
)

# Länka mot Qt-moduler
target_link_libraries(QtFtpClient PRIVATE
    Qt6::Widgets
    Qt6::Network
)
