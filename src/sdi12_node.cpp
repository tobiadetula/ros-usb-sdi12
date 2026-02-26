#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <regex>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/float64.hpp"
#include "sensor_msgs/msg/temperature.hpp"
#include <libserial/SerialPort.h>

#include "sdi12_sensor/teros12_sdi12.h" 

using namespace std::chrono_literals;

// A struct to hold the publishers for a specific sensor address
struct SensorPublishers {
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr info;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr vwc;
  rclcpp::Publisher<sensor_msgs::msg::Temperature>::SharedPtr temp;
  rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr ec;
};

class Sdi12MultiNode : public rclcpp::Node
{
public:
  Sdi12MultiNode() : Node("sdi12_node")
  {
    // 1. Declare parameter as a list of strings. Default is just address "0".
    this->declare_parameter<std::vector<std::string>>("sensor_addresses", {"0"});
    std::vector<std::string> addresses_param = this->get_parameter("sensor_addresses").as_string_array();
    
    // Extract the characters from the parameter
    for (const auto& addr_str : addresses_param) {
      if (!addr_str.empty()) {
        active_addresses_.push_back(addr_str[0]);
      }
    }

    // 2. Setup Publishers for EACH sensor dynamically
    for (char addr : active_addresses_) {
      std::string prefix = "teros12/sensor_" + std::string(1, addr) + "/";
      
      SensorPublishers pubs;
      pubs.info = this->create_publisher<std_msgs::msg::String>(prefix + "info", 10);
      pubs.vwc  = this->create_publisher<std_msgs::msg::Float64>(prefix + "vwc", 10);
      pubs.temp = this->create_publisher<sensor_msgs::msg::Temperature>(prefix + "temperature", 10);
      pubs.ec   = this->create_publisher<std_msgs::msg::Float64>(prefix + "ec", 10);
      
      sensor_pubs_[addr] = pubs;
      RCLCPP_INFO(this->get_logger(), "Configured publishers for SDI-12 Address: '%c'", addr);
    }

    // 3. Setup Serial Port
    try {
      serial_port_.Open("/dev/serial/by-id/usb-Apogee_Instruments__Inc._Sensor_Interface_B16392460F001A00-if00"); 
      serial_port_.SetBaudRate(LibSerial::BaudRate::BAUD_9600); 
      serial_port_.SetCharacterSize(LibSerial::CharacterSize::CHAR_SIZE_8);
      serial_port_.SetParity(LibSerial::Parity::PARITY_NONE);
      serial_port_.SetStopBits(LibSerial::StopBits::STOP_BITS_1);
    } catch (const LibSerial::OpenFailed&) {
      RCLCPP_ERROR(this->get_logger(), "Failed to open serial port /dev/ttyACM0");
      return; 
    }

    // 4. Get Info for all connected sensors
    for (char addr : active_addresses_) {
      publish_sensor_info(addr);
    }

    // 5. Timer: Loops through all sensors every 10 seconds
    last_measurement_time_ = this->now();
    timer_ = this->create_wall_timer(
      10s, std::bind(&Sdi12MultiNode::measurement_cycle, this));
  }

  ~Sdi12MultiNode() {
    if (serial_port_.IsOpen()) {
      serial_port_.Close();
    }
  }

private:
  // --- THE PLACEHOLDER REPLACEMENT LOGIC ---
  // This function takes the target address and a command from the header file,
  // and replaces the 'a' at the start with the actual address.
  std::string build_command(char addr, const char* base_cmd) 
  {
    std::string cmd(base_cmd);
    if (!cmd.empty() && cmd[0] == 'a') {
      cmd[0] = addr; // Replace 'a' with the real address (e.g., '0' or '1')
    }
    return cmd;
  }

  void publish_sensor_info(char addr)
  {
    std::string response;
    std::string cmd = build_command(addr, Teros12::CMD_SEND_ID);
    
    serial_port_.Write(cmd + "\r\n");
    
    try {
      serial_port_.ReadLine(response, '\n', 2000); 
      auto msg = std_msgs::msg::String();
      msg.data = response;
      sensor_pubs_[addr].info->publish(msg);
    } catch (const LibSerial::ReadTimeout&) {
      RCLCPP_WARN(this->get_logger(), "Timeout info for sensor %c", addr);
    }
  }

void measurement_cycle()
  {
    rclcpp::Time current_time = this->now();
    rclcpp::Duration dt = current_time - last_measurement_time_;
    RCLCPP_INFO(this->get_logger(), "--- Starting Measurement Cycle (dt: %.2fs) ---", dt.seconds());
    last_measurement_time_ = current_time;

    // Iterate through every sensor address configured
    for (char addr : active_addresses_) {
      std::string response;

      try {
        std::string get_data_cmd = build_command(addr, Teros12::CMD_EXTENDED_READ_FORMAT_1);
        
        // FIX 1: Flush the serial buffer before writing to drop any stale noise
        serial_port_.FlushIOBuffers(); 
        
        serial_port_.Write(get_data_cmd + "\r\n");
        
        serial_port_.ReadLine(response, '\n', 2000);
        RCLCPP_INFO(this->get_logger(), "Raw response from sensor %c: '%s'", addr, response.c_str());
        
        // STEP C: Parse and Publish the data
        parse_and_publish_data(addr, response);

      } catch (const LibSerial::ReadTimeout&) {
        RCLCPP_WARN(this->get_logger(), "Measurement timeout for sensor %c", addr);
      }
    }
  }
  
  void parse_and_publish_data(char addr, const std::string& data_str)
  {
    // FIX 2: Truncate the string at the first carriage return (\r) or newline (\n).
    // This strictly ignores trailing garbage that might contain extra numbers.
    std::string clean_str = data_str;
    size_t pos = clean_str.find_first_of("\r\n");
    if (pos != std::string::npos) {
      clean_str = clean_str.substr(0, pos);
    }

    // Use the cleaned string for regex evaluation
    std::regex float_regex("[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?");
    auto words_begin = std::sregex_iterator(clean_str.begin(), clean_str.end(), float_regex);
    auto words_end = std::sregex_iterator();

    std::vector<double> values;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        try {
            values.push_back(std::stod((*i).str()));
        } catch(...) {
            // Ignore anything that fails to convert
        }
    }

    // Evaluate what we extracted.
    if (values.size() >= 3) {
      size_t n = values.size();
      double vwc  = values[n-3];
      double temp = values[n-2];
      double ec   = values[n-1];

      // Publish to the specific sensor's topics
      auto vwc_msg = std_msgs::msg::Float64();
      vwc_msg.data = vwc;
      sensor_pubs_[addr].vwc->publish(vwc_msg);

      auto temp_msg = sensor_msgs::msg::Temperature();
      temp_msg.temperature = temp;
      sensor_pubs_[addr].temp->publish(temp_msg);

      auto ec_msg = std_msgs::msg::Float64();
      ec_msg.data = ec;
      sensor_pubs_[addr].ec->publish(ec_msg);

      RCLCPP_INFO(this->get_logger(), "Sensor %c Parsed -> VWC: %.2f, Temp: %.2f, EC: %.3f", addr, vwc, temp, ec);
    } else {
      RCLCPP_WARN(this->get_logger(), "Could not find 3 valid numbers in response: '%s'", clean_str.c_str());
    }
  }
  
  std::vector<char> active_addresses_;
  std::map<char, SensorPublishers> sensor_pubs_;
  
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time last_measurement_time_;
  LibSerial::SerialPort serial_port_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Sdi12MultiNode>());
  rclcpp::shutdown();
  return 0;
}