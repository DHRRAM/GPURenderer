# CMakeList.txt : CMake project for GPURenderer, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable(
  GPURenderer
  "GPURenderer.cpp"
  "RendererApp.cpp"
  "RendererApp.h"
  "shader.vert"
  "shader.frag"
  "stb_image.h"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GPURenderer PROPERTY CXX_STANDARD 20)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 QUIET)
find_package(glm QUIET)
find_package(assimp QUIET)

if (CMAKE_VERSION VERSION_LESS 3.11)
  message(FATAL_ERROR "This project requires CMake 3.11+ for FetchContent dependencies.")
endif()
include(FetchContent)

if (NOT glfw3_FOUND)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
endif()

if (NOT glm_FOUND)
  FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
  )
endif()

if (NOT assimp_FOUND)
  set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
  set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
  set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.3.1
  )
endif()

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
)

if (NOT glfw3_FOUND OR NOT glm_FOUND OR NOT assimp_FOUND)
  if (CMAKE_VERSION VERSION_LESS 3.14)
    if (NOT glfw3_FOUND)
      FetchContent_GetProperties(glfw)
      if (NOT glfw_POPULATED)
        FetchContent_Populate(glfw)
        add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})
      endif()
    endif()
    if (NOT glm_FOUND)
      FetchContent_GetProperties(glm)
      if (NOT glm_POPULATED)
        FetchContent_Populate(glm)
        add_subdirectory(${glm_SOURCE_DIR} ${glm_BINARY_DIR})
      endif()
    endif()
    if (NOT assimp_FOUND)
      FetchContent_GetProperties(assimp)
      if (NOT assimp_POPULATED)
        FetchContent_Populate(assimp)
        add_subdirectory(${assimp_SOURCE_DIR} ${assimp_BINARY_DIR})
      endif()
    endif()
  else()
    FetchContent_MakeAvailable(glfw glm assimp)
  endif()
endif()

if (CMAKE_VERSION VERSION_LESS 3.14)
  FetchContent_GetProperties(imgui)
  if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
  endif()
else()
  FetchContent_MakeAvailable(imgui)
endif()

add_library(
  imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(
  imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(GPURenderer PRIVATE GPURENDERER_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(GPURenderer PRIVATE ${OPENGL_INCLUDE_DIR})
if (TARGET OpenGL::GL)
  target_link_libraries(imgui PRIVATE OpenGL::GL)
  target_link_libraries(GPURenderer PRIVATE OpenGL::GL)
else()
  target_link_libraries(imgui PRIVATE ${OPENGL_LIBRARIES})
  target_link_libraries(GPURenderer PRIVATE ${OPENGL_LIBRARIES})
endif()

if (TARGET glfw)
  target_link_libraries(imgui PRIVATE glfw)
  target_link_libraries(GPURenderer PRIVATE glfw)
elseif (TARGET glfw::glfw)
  target_link_libraries(imgui PRIVATE glfw::glfw)
  target_link_libraries(GPURenderer PRIVATE glfw::glfw)
elseif (TARGET glfw3)
  target_link_libraries(imgui PRIVATE glfw3)
  target_link_libraries(GPURenderer PRIVATE glfw3)
else()
  message(FATAL_ERROR "GLFW target not found. Ensure glfw is installed or FetchContent succeeds.")
endif()

if (TARGET glm::glm)
  target_link_libraries(GPURenderer PRIVATE glm::glm)
elseif (TARGET glm)
  target_link_libraries(GPURenderer PRIVATE glm)
endif()

if (TARGET assimp::assimp)
  target_link_libraries(GPURenderer PRIVATE assimp::assimp)
elseif (TARGET Assimp::assimp)
  target_link_libraries(GPURenderer PRIVATE Assimp::assimp)
elseif (TARGET assimp)
  target_link_libraries(GPURenderer PRIVATE assimp)
else()
  message(FATAL_ERROR "Assimp target not found. Ensure assimp is installed or FetchContent succeeds.")
endif()

target_link_libraries(GPURenderer PRIVATE imgui)
