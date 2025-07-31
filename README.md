# AirFilter-IoT

A comprehensive IoT-based air filtration system built for the Texas Instruments TM4C123GH6PM microcontroller. This project implements real-time air quality monitoring, automated filtration control, and  connectivity with remote IoT server for remote monitoring and control.

## 🌟 Features

- **Real-time Air Quality Monitoring**: BME680 sensor integration for temperature, humidity, pressure, and gas quality measurement
- **Automated Filtration Control**: Intelligent motor control based on air quality metrics
- **Network Connectivity**: Ethernet-based communication with TCP/IP stack
- **Remote Server Integration**: MQTT protocol for IoT connectivity with server 
- **Modular Architecture**: Well-organized library structure for easy maintenance and extension
- **Professional Communication**: I2C, SPI, and UART protocols for sensor and peripheral communication

## 🏗️ Architecture

### Project Structure
```
AirFilter-IoT/
├── project-framework/     # Core networking and system framework
│   ├── arp.c/h          # Address Resolution Protocol
│   ├── eth0.c/h         # Ethernet driver
│   ├── icmp.c/h         # Internet Control Message Protocol
│   ├── ip.c/h           # Internet Protocol stack
│   ├── socket.c/h       # Socket abstraction layer
│   └── eeprom.c/h       # EEPROM management
├── clock/               # Timing and clock management
│   ├── clock.c/h        # System clock configuration
│   ├── timer.c/h        # Timer functionality
│   └── wait.c/h         # Delay and wait functions
├── sensor/              # Sensor integration
│   └── bme680.c/h       # BME680 air quality sensor
├── communication/       # Communication protocols
│   ├── I2C3.c/h        # I2C communication
│   ├── spi0.c/h        # SPI communication
│   └── uart0.c/h       # UART communication
├── protocol/           # Network protocols
│   ├── tcp.c/h         # TCP protocol implementation
│   ├── udp.c/h         # UDP protocol implementation
│   └── mqtt.c/h        # MQTT client for IoT
├── gpio/               # General Purpose I/O
│   └── gpio.c/h        # GPIO control and configuration
├── ethernet.c          # Main Ethernet application
└── motor.c             # Motor control application
```

### Hardware Requirements

- **Microcontroller**: Texas Instruments TM4C123GH6PM
- **Sensors**: BME680 (Temperature, Humidity, Pressure, Gas Quality)
- **Communication**: Ethernet PHY for network connectivity
- **Actuators**: DC motor for air filtration control
- **Development Board**: TM4C123GH6PM LaunchPad or compatible

## 🚀 Getting Started

### Prerequisites

- Code Composer Studio (CCS) or compatible IDE
- TM4C123GH6PM development board
- BME680 sensor module
- Ethernet connection
- Motor driver circuit

### Installation

1. **Clone the Repository**
   ```bash
   git clone git@github.com:sanketkoirala/AirFilter-IoT.git
   cd AirFilter-IoT
   ```

2. **Import Project**
   - Open Code Composer Studio
   - Import existing project from the cloned directory
   - Configure target settings for TM4C123GH6PM

3. **Hardware Setup**
   - Connect BME680 sensor via I2C
   - Connect motor driver circuit to GPIO pins
   - Connect Ethernet PHY to the microcontroller
   - Power the system

4. **Configuration**
   - Update network settings in `ethernet.c`
   - Configure MQTT broker details in `protocol/mqtt.c`
   - Set motor control parameters in `motor.c`

### Building and Flashing

1. **Build the Project**
   - Clean and rebuild the project in CCS
   - Ensure all dependencies are resolved

2. **Flash to Device**
   - Connect TM4C123GH6PM via USB
   - Flash the compiled binary to the device

## 📊 Usage

### Air Quality Monitoring

The system continuously monitors air quality using the BME680 sensor:

- **Temperature**: Ambient temperature measurement
- **Humidity**: Relative humidity levels
- **Pressure**: Atmospheric pressure
- **Gas Quality**: VOC (Volatile Organic Compounds) detection

### Automated Control

The system automatically controls the air filtration motor based on:

- Air quality thresholds
- Time-based schedules
- Manual override commands

### Network Features

- **Ethernet Connectivity**: Real-time data transmission
- **TCP/IP Stack**: Full network protocol support
- **MQTT Integration**: Cloud connectivity for remote monitoring


## 🔧 Configuration

### Network Settings

Edit `ethernet.c` to configure:
- IP address and subnet mask
- Gateway and DNS settings
- MQTT broker connection details

### Sensor Calibration

Configure BME680 sensor parameters in `sensor/bme680.c`:
- Temperature offset
- Humidity calibration
- Gas sensor baseline


## 📈 Monitoring and Control

### Real-time Data

The system provides real-time data through:
- Ethernet network interface
- MQTT topic publishing
- Local GPIO status indicators

### Remote Access

Access the system remotely via:
- MQTT client applications by subscribing to topic


## 📝 License

This project is licensed under the MIT License.

## 📞 Support

For support and questions:
- Create an issue in the GitHub repository
- Check the documentation in the code comments

## 🔄 Version History

- **v1.0.0** - Initial release with core functionality
  - Basic air quality monitoring
  - Network connectivity
  - MQTT integration
