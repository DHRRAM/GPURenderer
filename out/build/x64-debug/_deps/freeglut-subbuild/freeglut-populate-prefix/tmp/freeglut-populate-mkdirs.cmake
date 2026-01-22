# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-src")
  file(MAKE_DIRECTORY "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-src")
endif()
file(MAKE_DIRECTORY
  "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-build"
  "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix"
  "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix/tmp"
  "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix/src/freeglut-populate-stamp"
  "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix/src"
  "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix/src/freeglut-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix/src/freeglut-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Dhruv/UofU/UofU_Assignments/CS_6610/GPURenderer/out/build/x64-debug/_deps/freeglut-subbuild/freeglut-populate-prefix/src/freeglut-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
