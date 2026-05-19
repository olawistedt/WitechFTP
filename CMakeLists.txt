cmake_minimum_required(VERSION 3.16)
project(WitechFTP LANGUAGES CXX)

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
add_executable(WitechFTP WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/layout.cpp
    src/local_browser.cpp
    src/remote_browser.cpp
    src/transfers.cpp
    src/saved_sites.cpp
    src/langstrings.cpp
    src/langstrings.h
    src/ftpcommunicator.cpp
    src/ftpcommunicator.h
    src/filetreewidget.cpp
    src/filetreewidget.h
    src/resources.qrc
    src/appicon.rc
)

if(APPLE)
    set_target_properties(WitechFTP PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "se.witech.WitechFTP"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
endif()

# Länka mot Qt-moduler
target_link_libraries(WitechFTP PRIVATE
    Qt6::Widgets
    Qt6::Network
)
