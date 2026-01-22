# CMakeList.txt : CMake project for GPURenderer, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (GPURenderer "GPURenderer.cpp" "shader.vert" "shader.frag")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GPURenderer PROPERTY CXX_STANDARD 20)
endif()

find_package(OpenGL REQUIRED)
find_package(GLUT QUIET)

target_compile_definitions(GPURenderer PRIVATE GPURENDERER_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(GPURenderer PRIVATE ${OPENGL_INCLUDE_DIR})
if (TARGET OpenGL::GL)
  target_link_libraries(GPURenderer PRIVATE OpenGL::GL)
else()
  target_link_libraries(GPURenderer PRIVATE ${OPENGL_LIBRARIES})
endif()

if (GLUT_FOUND)
  target_include_directories(GPURenderer PRIVATE ${GLUT_INCLUDE_DIR})
  target_link_libraries(GPURenderer PRIVATE ${GLUT_LIBRARIES})
else()
  if (CMAKE_VERSION VERSION_LESS 3.11)
    message(FATAL_ERROR "GLUT not found and FetchContent requires CMake 3.11+. Install FreeGLUT or upgrade CMake.")
  endif()

  include(FetchContent)
  set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
  set(FREEGLUT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(FREEGLUT_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

  FetchContent_Declare(
    freeglut
    GIT_REPOSITORY https://github.com/FreeGLUTProject/freeglut.git
    GIT_TAG v3.4.0
  )

  if (CMAKE_VERSION VERSION_LESS 3.14)
    FetchContent_GetProperties(freeglut)
    if (NOT freeglut_POPULATED)
      FetchContent_Populate(freeglut)
      add_subdirectory(${freeglut_SOURCE_DIR} ${freeglut_BINARY_DIR})
    endif()
  else()
    FetchContent_MakeAvailable(freeglut)
  endif()

  if (TARGET freeglut_static)
    target_link_libraries(GPURenderer PRIVATE freeglut_static)
  elseif (TARGET freeglut)
    target_link_libraries(GPURenderer PRIVATE freeglut)
  else()
    message(FATAL_ERROR "FreeGLUT build did not produce an expected target.")
  endif()

  target_include_directories(GPURenderer PRIVATE ${freeglut_SOURCE_DIR}/include)
endif()
