cmake_minimum_required(VERSION 3.16)

# This is the line that's likely missing!
set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Q2_Project)