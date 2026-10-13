cmake_minimum_required(VERSION 3.16)
project(gcs_tile_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
# Gui: QImage + QPainter to draw tiles. Sql: SQLite (MBTiles and maptiles.db).
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Sql)
qt_standard_project_setup()

# Draws a synthetic test tile pack (.mbtiles)
qt_add_executable(make_test_mbtiles src/make_test_mbtiles.cpp src/TileMath.h)
target_link_libraries(make_test_mbtiles PRIVATE Qt6::Core Qt6::Gui Qt6::Sql)

# Imports an .mbtiles pack into maptiles.db (HLD schema); also --list and --remove
qt_add_executable(import_mbtiles src/import_mbtiles.cpp src/TileMath.h)
target_link_libraries(import_mbtiles PRIVATE Qt6::Core Qt6::Sql)

# "ros2 run gcs_tile_tools make_test_mbtiles" / "ros2 run gcs_tile_tools import_mbtiles"
install(TARGETS make_test_mbtiles import_mbtiles RUNTIME DESTINATION lib/${PROJECT_NAME})

ament_package()
