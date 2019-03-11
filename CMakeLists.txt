cmake_minimum_required (VERSION 3.0)
project (glsexpand)

find_package (Boost 1.69 REQUIRED)

add_executable (glsexpand
  glsexpand.cpp
)

target_link_libraries (glsexpand PRIVATE Boost::boost)
