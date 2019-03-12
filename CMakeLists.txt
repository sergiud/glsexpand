cmake_minimum_required (VERSION 3.0)
project (glsexpand LANGUAGES CXX)

find_package (Boost 1.69 REQUIRED)

add_executable (glsexpand
  glsexpand.cpp
)

target_compile_features (glsexpand
  PRIVATE cxx_generic_lambdas)

target_link_libraries (glsexpand PRIVATE Boost::boost)
