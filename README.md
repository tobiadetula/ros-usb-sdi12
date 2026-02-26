# ros-usb-sdi12
A ros2 repository to get data from an sdi12 sensor over a usb to sdi 12 converter 



## ROS Setup 


`
ros2 pkg create sdi12_sensor \
  --build-type ament_cmake \
  --node-name sdi12_node \
  --dependencies rclcpp std_msgs
`




## Dependencies

The following dependencies are required to run the project:
`
sudo apt update
sudo apt install libserial-dev
sudo usermod -a -G dialout $USER
`