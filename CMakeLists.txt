cmake_minimum_required(VERSION 3.21)
project(WRAM VERSION 1.1.0 LANGUAGES CXX RC)

if(NOT MSVC)
  message(FATAL_ERROR "WRAM se compila con MSVC (Visual Studio 2022).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# CRT estática: el exe no depende de vcruntime140.dll.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

# SDK de WebView2 (encabezados y cargador estático). Se descarga una vez desde NuGet y se verifica su hash.
set(WEBVIEW2_VERSION 1.0.4370.26)
set(WEBVIEW2_SHA256 1C0DACAC84B70109B3FD42F90C53A0BA942ABC3A87590D7A4E0CCD86E4A06CF4)
set(WEBVIEW2_DIR ${CMAKE_SOURCE_DIR}/third_party/webview2)
if(NOT EXISTS ${WEBVIEW2_DIR}/version-${WEBVIEW2_VERSION})
  set(_wv2_zip ${CMAKE_BINARY_DIR}/webview2-${WEBVIEW2_VERSION}.zip)
  message(STATUS "Descargando Microsoft.Web.WebView2 ${WEBVIEW2_VERSION}")
  file(DOWNLOAD https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION} ${_wv2_zip}
       EXPECTED_HASH SHA256=${WEBVIEW2_SHA256} TLS_VERIFY ON)
  set(_wv2_pkg ${CMAKE_BINARY_DIR}/webview2-pkg-${WEBVIEW2_VERSION})
  file(ARCHIVE_EXTRACT INPUT ${_wv2_zip} DESTINATION ${_wv2_pkg})
  # Se reemplaza cualquier versión anterior del SDK para no mezclar encabezados.
  file(REMOVE_RECURSE ${WEBVIEW2_DIR})
  file(COPY ${_wv2_pkg}/build/native/include/ DESTINATION ${WEBVIEW2_DIR}/include)
  file(COPY ${_wv2_pkg}/build/native/x64/WebView2LoaderStatic.lib DESTINATION ${WEBVIEW2_DIR}/lib)
  file(COPY ${_wv2_pkg}/LICENSE.txt DESTINATION ${WEBVIEW2_DIR})
  file(WRITE ${WEBVIEW2_DIR}/version-${WEBVIEW2_VERSION} "${WEBVIEW2_VERSION}\n")
endif()

# Núcleo sin dependencias de interfaz: lo usan la app y las pruebas.
add_library(wram_core STATIC
  src/core/Config.cpp
  src/core/Format.cpp
  src/core/I18n.cpp
  src/core/MetricCatalog.cpp
  src/core/Text.cpp)
target_include_directories(wram_core PUBLIC src)

set(WRAM_WARNINGS /W4 /permissive- /utf-8 /Zc:__cplusplus)
set(WRAM_HARDENING /GS /guard:cf /sdl)
set(WRAM_DEFINES UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_compile_options(wram_core PRIVATE ${WRAM_WARNINGS} ${WRAM_HARDENING} "$<$<CONFIG:MinSizeRel,Release>:/O1;/Gy;/Gw;/GL>")
target_compile_definitions(wram_core PUBLIC ${WRAM_DEFINES})

# Interfaz de configuración: se minifica y se incrusta como recurso.
set(SETTINGS_SRC ${CMAKE_SOURCE_DIR}/src/ui/settings.html)
set(SETTINGS_MIN ${CMAKE_BINARY_DIR}/res/settings.min.html)
add_custom_command(
  OUTPUT ${SETTINGS_MIN}
  COMMAND ${CMAKE_COMMAND} -DIN=${SETTINGS_SRC} -DOUT=${SETTINGS_MIN} -DASSETS=${CMAKE_SOURCE_DIR}/assets -P ${CMAKE_SOURCE_DIR}/tools/minify-html.cmake
  DEPENDS ${SETTINGS_SRC} ${CMAKE_SOURCE_DIR}/tools/minify-html.cmake ${CMAKE_SOURCE_DIR}/assets/logo-horizontal-light.b64 ${CMAKE_SOURCE_DIR}/assets/logo-mark.b64
  COMMENT "Minificando settings.html")
add_custom_target(wram_settings_html DEPENDS ${SETTINGS_MIN})

add_executable(WRAM WIN32
  src/main.cpp
  src/app/App.cpp
  src/platform/Autostart.cpp
  src/platform/ConfigStore.cpp
  src/sensors/GpuThermalProvider.cpp
  src/sensors/PdhProvider.cpp
  src/sensors/SystemProviders.cpp
  src/ui/SettingsWindow.cpp
  src/ui/TaskbarBand.cpp
  src/ui/TrayIcon.cpp
  res/wram.rc)
add_dependencies(WRAM wram_settings_html)
set_source_files_properties(res/wram.rc PROPERTIES
  OBJECT_DEPENDS "${SETTINGS_MIN};${CMAKE_SOURCE_DIR}/res/wram.ico;${CMAKE_SOURCE_DIR}/res/wram.manifest")
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_BINARY_DIR}/res\" /I\"${CMAKE_SOURCE_DIR}/res\"")

target_include_directories(WRAM PRIVATE src ${WEBVIEW2_DIR}/include)
target_compile_options(WRAM PRIVATE ${WRAM_WARNINGS} ${WRAM_HARDENING} /EHsc /GR- "$<$<CONFIG:MinSizeRel,Release>:/O1;/Gy;/Gw;/GL>")
target_link_libraries(WRAM PRIVATE wram_core
  ${WEBVIEW2_DIR}/lib/WebView2LoaderStatic.lib
  d2d1 dwrite pdh dwmapi shcore version advapi32 shell32 ole32 gdi32 user32)
target_link_options(WRAM PRIVATE
  /MANIFEST:NO /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /GUARD:CF /CETCOMPAT
  /DEPENDENTLOADFLAG:0x800
  "$<$<CONFIG:MinSizeRel,Release>:/LTCG;/OPT:REF;/OPT:ICF;/INCREMENTAL:NO;/DEBUG:NONE>")

# Pruebas del núcleo (sin framework externo).
enable_testing()
add_executable(wram_tests tests/test_main.cpp)
target_link_libraries(wram_tests PRIVATE wram_core)
target_compile_options(wram_tests PRIVATE ${WRAM_WARNINGS} /EHsc $<$<CONFIG:MinSizeRel,Release>:/GL>)
target_link_options(wram_tests PRIVATE $<$<CONFIG:MinSizeRel,Release>:/LTCG>)
add_test(NAME core COMMAND wram_tests)
