# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/alex/esp/v5.5.1/esp-idf/components/bootloader/subproject"
  "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader"
  "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix"
  "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix/tmp"
  "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix/src/bootloader-stamp"
  "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix/src"
  "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/alex/Downloads/Project/embedded_project_Q2/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
