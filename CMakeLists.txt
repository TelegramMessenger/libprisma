cmake_minimum_required(VERSION 3.16)
project(libprisma LANGUAGES CXX)

include(GNUInstallDirs)

# Boost.Regex is header-only since 1.76. A parent project may already provide the target.
if(NOT TARGET Boost::regex)
    find_package(Boost 1.76 REQUIRED COMPONENTS regex)
endif()

# Static, because nothing is exported for a Windows DLL.
add_library(prisma STATIC
    libprisma/Highlight.cpp
    libprisma/LanguageTree.cpp
    libprisma/SyntaxHighlighter.cpp
    libprisma/TokenList.cpp
)
add_library(libprisma::prisma ALIAS prisma)

target_compile_features(prisma PUBLIC cxx_std_20)
target_include_directories(prisma PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libprisma>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libprisma>
)
target_link_libraries(prisma PUBLIC Boost::regex)

install(TARGETS prisma)
install(FILES
    libprisma/Highlight.h
    libprisma/LanguageTree.h
    libprisma/SyntaxHighlighter.h
    libprisma/TokenList.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libprisma
)
# Not compiled in: the application reads it and passes the contents to SyntaxHighlighter.
install(FILES libprisma/grammars.dat DESTINATION ${CMAKE_INSTALL_DATADIR}/libprisma)
