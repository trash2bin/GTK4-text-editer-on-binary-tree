# src/CMakeLists.txt
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

# Опции оптимизации
option(ENABLE_MAX_OPTIM "Enable maximal optimizations for Release builds (march=native, flto, strip)" OFF)
option(ENABLE_PORTABLE_OPTIM "Enable portable optimized build (no march=native, safe flags like -O2 -flto)" OFF)


# Попытка подставить gcc-ar/gcc-ranlib для LTO
if((ENABLE_MAX_OPTIM OR ENABLE_PORTABLE_OPTIM) AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
  find_program(GCC_AR NAMES gcc-ar ${CMAKE_C_COMPILER}-ar ar)
  find_program(GCC_RANLIB NAMES gcc-ranlib ${CMAKE_C_COMPILER}-ranlib ranlib)
  if(GCC_AR AND GCC_RANLIB)
    message(STATUS "Using ${GCC_AR} and ${GCC_RANLIB} for LTO-capable archives")
    set(CMAKE_AR "${GCC_AR}" CACHE FILEPATH "Archiver" FORCE)
    set(CMAKE_RANLIB "${GCC_RANLIB}" CACHE FILEPATH "Ranlib" FORCE)
  else()
    message(WARNING "gcc-ar/gcc-ranlib not found: static archive with LTO may fail. Consider disabling -flto in Nix.")
  endif()
endif()

# Отключаем march=native если NIX_ENFORCE_NO_NATIVE установлена в окружении
if(DEFINED ENV{NIX_ENFORCE_NO_NATIVE})
  message(STATUS "NIX_ENFORCE_NO_NATIVE detected — disabling -march=native and similar impure flags")
  # выключаем ENABLE_MAX_OPTIM автоматически (или можно только убрать march)
  set(ENABLE_MAX_OPTIM OFF CACHE BOOL "Disable max optim because Nix forbids native" FORCE)
endif()


# --- библиотека с логикой ---
add_library(tree_lib STATIC
    Tree.cpp
    BinaryTreeFile.cpp
)

target_include_directories(tree_lib
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Базовые warning flags
target_compile_options(tree_lib PRIVATE -Wall -Wextra -Wpedantic)

# --- исполняемый файл GUI ---
add_executable(editor
    main.cpp
)

# include dirs от pkg-config (SYSTEM, чтобы не показывать warning-ы из внешнего кода)
target_include_directories(editor PRIVATE SYSTEM ${GTKMM_INCLUDE_DIRS})
target_include_directories(editor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# дополнительные compile flags и линковка от pkg-config
if(GTKMM_CFLAGS_OTHER)
  target_compile_options(editor PRIVATE ${GTKMM_CFLAGS_OTHER})
endif()

target_link_libraries(editor PRIVATE tree_lib ${GTKMM_LIBRARIES} ${GTKMM_LDFLAGS_OTHER})
target_compile_features(editor PRIVATE cxx_std_17)

# Портативный оптимизированный профиль (без march=native)
if(ENABLE_PORTABLE_OPTIM)
  message(STATUS "ENABLE_PORTABLE_OPTIM=ON: using portable -O2 -flto and hardening flags")
  target_compile_options(tree_lib PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-O2 -flto -fstack-protector-strong -D_FORTIFY_SOURCE=2>
    $<$<CONFIG:Release>:-O2 -flto -fstack-protector-strong -D_FORTIFY_SOURCE=2>
  )
  target_compile_options(editor PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-O2 -flto -fstack-protector-strong -D_FORTIFY_SOURCE=2>
    $<$<CONFIG:Release>:-O2 -flto -fstack-protector-strong -D_FORTIFY_SOURCE=2>
  )
  target_link_options(tree_lib PRIVATE $<$<CONFIG:RelWithDebInfo>:-flto> $<$<CONFIG:Release>:-flto>)
  target_link_options(editor PRIVATE $<$<CONFIG:RelWithDebInfo>:-flto> $<$<CONFIG:Release>:-flto>)
endif()

# Максимальная агрессивная оптимизация (опционально)
if(ENABLE_MAX_OPTIM)
  message(STATUS "ENABLE_MAX_OPTIM=ON: enabling -O3 -march=native -flto and strip for Release/RelWithDebInfo")
  target_compile_options(tree_lib PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-O3 -march=native -ffast-math -funroll-loops -flto>
    $<$<CONFIG:Release>:-O3 -march=native -ffast-math -funroll-loops -flto>
  )
  target_compile_options(editor PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-O3 -march=native -ffast-math -funroll-loops -flto>
    $<$<CONFIG:Release>:-O3 -march=native -ffast-math -funroll-loops -flto>
  )
  target_link_options(tree_lib PRIVATE $<$<CONFIG:RelWithDebInfo>:-flto> $<$<CONFIG:Release>:-flto>)
  target_link_options(editor PRIVATE $<$<CONFIG:RelWithDebInfo>:-flto> $<$<CONFIG:Release>:-flto>)
endif()

# Strip binary after build (safely)
if(ENABLE_MAX_OPTIM)
  if(DEFINED CMAKE_STRIP AND CMAKE_STRIP)
    add_custom_command(TARGET editor POST_BUILD
      COMMAND ${CMAKE_STRIP} $<TARGET_FILE:editor>
      COMMENT "Strip editor binary"
      VERBATIM
    )
  else()
    add_custom_command(TARGET editor POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Strip tool not found; skipping strip step"
      COMMENT "Strip not available"
      VERBATIM
    )
  endif()
endif()
