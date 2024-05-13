cmake_minimum_required(VERSION 3.19) # Set the minimum required CMake version
project(VoxelSpace C)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

set(CMAKE_C_STANDARD 23)
set(SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c)

include(FetchContent)

option(FILE_OPEN "use file opener" ON)

if(EMSCRIPTEN)
  # remove --closure-args="--compilation_level SIMPLE" when
  # https://github.com/emscripten-core/emscripten/issues/16706 is fixed
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --no-heap-copy")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")

  set(LINK_FLAGS CMAKE_EXE_LINKER_FLAGS)

  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sGL_ENABLE_GET_PROC_ADDRESS")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sUSE_WEBGL2=1")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sFULL_ES3=1")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sUSE_GLFW=3")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sWASM=1")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sASYNCIFY")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sMINIFY_HTML=1")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sENVIRONMENT=web")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sFILESYSTEM=0")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} -sALLOW_MEMORY_GROWTH=1")
  set(${LINK_FLAGS} "${${LINK_FLAGS}} --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm-shell/pre.js")
  set(${LINK_FLAGS}
      "${${LINK_FLAGS}} --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm-shell/shell.html"
  )

  # Remove the -rdynamic flag because otherwise emscripten does not generate
  # HTML+JS+WASM files, only a non-working and fat HTML
  string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_C_FLAGS
                 "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

if(EMSCRIPTEN)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES LINK_DEPENDS
               ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm-shell/shell.html)
endif()

target_include_directories(${PROJECT_NAME}
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(SUPPORT_CLANGD "Support clangd" ON)
if(SUPPORT_CLANGD)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CLANGD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/cmake/.clangd.in)
  set(CLANGD_DEST ${CMAKE_CURRENT_SOURCE_DIR}/.clangd)
  configure_file(${CLANGD_SRC} ${CLANGD_DEST} @ONLY)
endif()
