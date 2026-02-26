#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float64.hpp"
#include "sensor_msgs/msg/temperature.hpp"
#include <libserial/SerialPort.h>

// Include the header we made earlier (ensure it's in your include/sdi12_sensor/ folder)
#include "sdi12_sensor/teros12_sdi12.h" 

using namespace std::chrono_literals;

class Sdi12Node : public rclcpp::Node
{
public:
  Sdi12Node() : Node("sdi12_node"), is_measuring_(false)
  {
    // 1. Initialize Publishers
    info_pub_ = this->create_publisher<std_msgs::msg::String>("teros12/info", 10);
    vwc_pub_  = this->create_publisher<std_msgs::msg::Float64>("teros12/vwc", 10);
    temp_pub_ = this->create_publisher<sensor_msgs::msg::Temperature>("teros12/temperature", 10);
    ec_pub_   = this->create_publisher<std_msgs::msg::Float64>("teros12/ec", 10);

    // 2. Setup Serial Port (Adjust /dev/ttyACM0) as needed for your converter)
    try {
      serial_port_.Open("/dev/ttyACM0"); // Common for USB-SDI12 converters, but check your system
      serial_port_.SetBaudRate(LibSerial::BaudRate::BAUD_9600); // Standard for most USB-SDI12 converters
      serial_port_.SetCharacterSize(LibSerial::CharacterSize::CHAR_SIZE_8);
      serial_port_.SetParity(LibSerial::Parity::PARITY_NONE);
      serial_port_.SetStopBits(LibSerial::StopBits::STOP_BITS_1);
      RCLCPP_INFO(this->get_logger(), "Successfully opened serial port.");
    } catch (const LibSerial::OpenFailed&) {
      RCLCPP_ERROR(this->get_logger(), "Failed to open serial port /dev/ttyUSB0");
      return;
    }

    // 3. Get and Publish Sensor Info Once on Startup
    publish_sensor_info();

    // 4. Create a Timer to trigger measurements every 5 seconds
    last_measurement_time_ = this->now();
    timer_ = this->create_wall_timer(
      5s, std::bind(&Sdi12Node::measurement_cycle, this));
  }

  ~Sdi12Node() {
    if (serial_port_.IsOpen()) {
      serial_port_.Close();
    }
  }

private:
  void publish_sensor_info()
  {
    std::string response;
    // Send Identification Command (e.g., "0I!")
    serial_port_.Write(Teros12::CMD_SEND_ID);
    serial_port_.Write("\r\n");
    
    try {
      serial_port_.ReadLine(response, '\n', 2000); // 2 second timeout
      
      auto msg = std_msgs::msg::String();
      msg.data = response;
      info_pub_->publish(msg);
      RCLCPP_INFO(this->get_logger(), "Sensor Info: %s", response.c_str());
    } catch (const LibSerial::ReadTimeout&) {
      RCLCPP_WARN(this->get_logger(), "Timeout reading sensor info.");
    }
  }

  void measurement_cycle()
  {
    // Calculate time since last measurement
    rclcpp::Time current_time = this->now();
    rclcpp::Duration dt = current_time - last_measurement_time_;
    RCLCPP_INFO(this->get_logger(), "Time since last measurement: %.2f seconds", dt.seconds());
    last_measurement_time_ = current_time;

    std::string response;

    // STEP A: Send Measure Command (e.g., "0M!")
    serial_port_.Write(Teros12::CMD_MEASURE);
    serial_port_.Write("\r\n");
    
    try {
      // Read the acknowledgment (e.g., "00013" -> address 0, 001 seconds, 3 values)
      serial_port_.ReadLine(response, '\n', 2000);
      
      // Give the sensor a moment to process the measurement (TEROS 12 usually takes ~1 sec)
      rclcpp::sleep_for(1s); 

      // STEP B: Send Get Data Command (e.g., "0D0!")
      serial_port_.Write(Teros12::CMD_GET_DATA);
      serial_port_.Write("\r\n");
      
      serial_port_.ReadLine(response, '\n', 2000);
      
      // STEP C: Parse and Publish the data
      parse_and_publish_data(response);

    } catch (const LibSerial::ReadTimeout&) {
      RCLCPP_WARN(this->get_logger(), "Measurement timeout. Is the sensor connected?");
    }
  }

  void parse_and_publish_data(const std::string& data_str)
  {
    // Expected Format: 0+2105.32+23.1+0.102
    // Basic parsing logic using sscanf. 
    // We skip the first character (address) and read the 3 floats separated by + or -
    
    double vwc = 0.0, temp = 0.0, ec = 0.0;
    
    // sscanf is a simple C-style way to extract the numbers, assuming '+' delimiters.
    // Note: If values are negative, the delimiter becomes '-', requiring more robust parsing.
    // For a basic implementation, assuming positive VWC/EC and handling Temp.
    int parsed = sscanf(data_str.c_str(), "%*d+%lf+%lf+%lf", &vwc, &temp, &ec);

    if (parsed == 3) {
      // Publish VWC
      auto vwc_msg = std_msgs::msg::Float64();
      vwc_msg.data = vwc;
      vwc_pub_->publish(vwc_msg);

      // Publish Temperature
      auto temp_msg = sensor_msgs::msg::Temperature();
      temp_msg.temperature = temp;
      temp_pub_->publish(temp_msg);

      // Publish EC
      auto ec_msg = std_msgs::msg::Float64();
      ec_msg.data = ec;
      ec_pub_->publish(ec_msg);

      RCLCPP_INFO(this->get_logger(), "Published -> VWC: %.2f, Temp: %.2f C, EC: %.3f", vwc, temp, ec);
    } else {
      RCLCPP_WARN(this->get_logger(), "Failed to parse sensor string: %s", data_str.c_str());
    }
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr info_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr vwc_pub_;
  rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr temp_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr ec_pub_;
  
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time last_measurement_time_;
  LibSerial::SerialPort serial_port_;
  bool is_measuring_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Sdi12Node>());
  rclcpp::shutdown();
  return 0;
}