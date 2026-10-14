cmake_minimum_required(VERSION 3.8)
project(gps_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

# ROS 2 packages this node uses (all from /opt/ros/jazzy).
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

# The node
add_executable(gps_sim src/gps_sim_node.cpp)
ament_target_dependencies(gps_sim rclcpp sensor_msgs)

# "ros2 run gps_sim gps_sim" looks in lib/<package>
install(TARGETS gps_sim RUNTIME DESTINATION lib/${PROJECT_NAME})

# "ros2 launch gps_sim gps_sim.launch.xml" and its parameter file
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

ament_package()
