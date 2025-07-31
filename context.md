# Embedded Logger Library - Project Context

**Repository**: https://github.com/unmanned-systems-uk/embedded_logger.git  
**Project Started**: January 2025  
**Status**: Architecture Complete, Implementation Phase Starting

## Project Overview

### Vision
Create a generic, cross-platform logging library for embedded and desktop applications, extracted from a working forestry research device logger implementation.

### Key Goals
- **Zero Regression**: Maintain all functionality from the original forestry logger
- **Cross-Platform**: Support ESP32, STM32, Arduino, Linux, Windows
- **Specialized Logging**: BMS, IoT, Safety-Critical patterns
- **Production Ready**: Memory optimized, real-time capable
- **Reusable**: Generic library usable across multiple projects

## Current Status

### ✅ Completed
- [x] Complete architecture design
- [x] Platform abstraction interface design
- [x] Directory structure definition
- [x] Build system architecture (CMake)
- [x] Repository creation: https://github.com/unmanned-systems-uk/embedded_logger.git
- [x] Batch script for directory structure generation
- [x] Doxygen documentation standards defined

### 🔄 In Progress
- [ ] Setting up Git repository structure
- [ ] Initial commit with directory structure
- [ ] Doxyfile configuration

### ⏳ Next Steps
- [ ] Configure Doxygen documentation system
- [ ] Implement ESP32 platform providers (with full Doxygen docs)
- [ ] Port existing forestry logger implementation
- [ ] Create STM32 platform providers
- [ ] Add BMS-specific logging features

## Platform Priorities

### Primary Platforms (Production)
1. **ESP32** - Forestry Research Device (Active project, zero regression required)
2. **STM32** - Battery Management System (New project, BMS-specific features needed)

### Secondary Platforms (Future)
3. **Arduino** - Community support
4. **Linux** - Development and testing
5. **Windows** - Development and testing

## Technical Architecture

### Core Library Structure
```
embedded-logger-lib/
├── include/embedded_logger/
│   ├── logger.h                    # Main logger interface
│   ├── log_entry.h                 # Log entry structures
│   ├── platform_interfaces.h      # Platform abstraction
│   ├── logger_config.h             # Configuration structures
│   └── specialized_logging.h      # Domain-specific utilities
├── src/
│   ├── logger.cpp                  # Core implementation
│   ├── log_formatter.cpp           # Formatting utilities
│   ├── platform_factory.cpp       # Platform detection/creation
│   ├── platform/                  # Platform implementations
│   │   ├── esp32/                 # ESP32 providers
│   │   ├── stm32/                 # STM32 providers
│   │   ├── arduino/               # Arduino providers
│   │   ├── posix/                 # Linux/macOS providers
│   │   └── windows/               # Windows providers
│   └── specialized/               # Domain-specific loggers
│       ├── bms_logger_factory.cpp
│       ├── iot_logger_factory.cpp
│       └── safety_critical_logger.cpp
```

### Platform Abstraction Interfaces
```cpp
namespace embedded_logger {
    class ITimeProvider;      // Cross-platform time management
    class IFileSystem;        // Cross-platform file operations
    class IConsoleOutput;     // Cross-platform console output
    class IThreadSync;        // Cross-platform synchronization
    class PlatformFactory;    // Auto-detection and creation
}
```

### Key Features
- **Asynchronous Logging**: Thread-safe with configurable queue sizes
- **File Rotation**: Automatic rotation by size with configurable backup count
- **Multiple Destinations**: Console, file, or both
- **Memory Optimized**: Platform-specific memory constraints
- **Specialized Patterns**: BMS, IoT, safety-critical logging macros

## Source Material

### Original Implementation
The library is based on a working logger from a forestry research device with these files:
- `logger.h` - Sophisticated async logger with file rotation
- `logger.cpp` - Complete implementation with ESP-IDF integration
- Dependencies: `ITimeManager`, `IDataStorage` (to be abstracted)

### Original Features to Preserve
- Multiple log levels (DEBUG, INFO, WARNING, ERROR, CRITICAL)
- Dual output (console + SD card)
- Automatic file rotation by size and time
- Thread-safe operation
- Asynchronous logging to prevent blocking
- Configurable formatting
- Component-based filtering
- Global logger instance with convenience macros

## Platform-Specific Considerations

### ESP32 (Forestry Research Device)
- **Framework**: ESP-IDF
- **Storage**: SPIFFS/FAT filesystem on SD card
- **Memory**: 520KB RAM, abundant flash
- **Features**: WiFi logging capability, color console output
- **Current Status**: Working implementation to be migrated
- **Zero Regression Required**: Must maintain exact current functionality

### STM32 (Battery Management System)
- **Families**: STM32F1/F4/F7/H7 support
- **RTOS**: FreeRTOS or bare metal
- **Memory**: Highly constrained (16-64KB RAM)
- **Storage**: Optional (none/external flash/SD card)
- **Transport**: UART, SWD, future CAN support
- **Special Requirements**: Safety-critical logging, minimal latency

#### STM32 Memory Optimization
- **F1**: 16-item queue, 64-byte messages, 16KB files
- **F4**: 32-item queue, 128-byte messages, 64KB files  
- **F7/H7**: 64-item queue, 256-byte messages, 256KB files

### BMS-Specific Features
```cpp
// BMS-specific log levels
enum class BMSLogLevel { CRITICAL_FAULT, SAFETY_WARNING, SYSTEM_EVENT, DIAGNOSTIC, DEBUG_TRACE };

// BMS logging macros
BMS_LOG_CELL_VOLTAGE(cell_id, voltage_mv)
BMS_LOG_TEMPERATURE(sensor_id, temp_c)
BMS_SAFETY_CRITICAL(event, value)  // <10μs latency, always logged
```

## Build System

### CMake Architecture
- **Main**: `CMakeLists.txt` with platform auto-detection
- **Platform Configs**: `platform_configs/esp32_config.cmake`, `stm32_config.cmake`
- **Package**: `embedded-logger-config.cmake.in` for find_package() support
- **Cross-Platform**: Supports ESP-IDF, STM32CubeIDE, Arduino IDE, standard CMake

### Platform Detection
```cmake
if(ESP_PLATFORM)          # ESP-IDF
elseif(STM32_FAMILY)       # STM32CubeIDE
elseif(ARDUINO)            # Arduino
elseif(UNIX)               # Linux/macOS
elseif(WIN32)              # Windows
```

## Migration Strategy (5-Week Plan)

### Phase 1: ESP32 Platform Implementation (Week 1)
- **Goal**: Zero regression for forestry device
- **Tasks**: 
  - Configure Doxygen documentation system
  - Extract ESP32 platform providers from current implementation (with full docs)
  - Create compatibility wrappers for existing `ITimeManager`/`IDataStorage`
  - Validate forestry device functionality
  - Generate initial API documentation
- **Success Criteria**: Forestry device works identically with new logger + complete Doxygen docs

### Phase 2: STM32 Platform Implementation (Week 2-3)
- **Goal**: Production-ready STM32 BMS logger
- **Tasks**:
  - Implement STM32 platform providers (HAL-based) with comprehensive documentation
  - Create BMS-specific logging patterns and factories
  - Memory optimization for different STM32 families
  - Safety-critical logging features
  - Document platform-specific constraints and limitations
- **Success Criteria**: Complete BMS logging with <8KB RAM usage + full API documentation

### Phase 3: Core Unification (Week 4)
- **Goal**: Unified core with platform abstraction
- **Tasks**:
  - Refactor Logger class to use platform interfaces
  - Implement LoggerFactory with auto-detection
  - Create comprehensive configuration system
  - Cross-platform testing
  - Complete cross-platform documentation with examples
- **Success Criteria**: Both ESP32 and STM32 using shared core + comprehensive docs

### Phase 4: Production Deployment (Week 5)
- **Goal**: v1.0.0 release ready for both projects
- **Tasks**:
  - Integration with forestry and BMS projects
  - Complete documentation and examples
  - Performance validation and optimization
  - CI/CD setup with automated Doxygen generation
  - Documentation hosting setup (GitHub Pages)
- **Success Criteria**: Both projects using shared library in production + published documentation

## Example Projects

### Forestry Research Device Integration
```cpp
// examples/forestry_device/main.cpp
#include "embedded_logger/logger.h"

void app_main() {
    auto config = embedded_logger::LoggerConfig{};
    config.logDirectory = "/sdcard/logs";
    config.maxFileSize = 1024 * 1024;  // 1MB
    
    auto logger = embedded_logger::LoggerFactory::createForESP32(config);
    logger->initialize();
    embedded_logger::Logger::setGlobalLogger(logger);
    
    EL_INFO("FORESTRY", "Device started");
}
```

### STM32 BMS Integration
```cpp
// examples/bms_system/main.cpp
#include "embedded_logger/specialized_logging.h"

extern UART_HandleTypeDef huart1;

void bms_init_logging() {
    embedded_logger::BMSLoggerConfig config;
    config.maxAsyncQueueSize = 32;  // STM32F4 optimized
    
    auto logger = embedded_logger::BMSLoggerFactory::createForSTM32BMS(
        config, &huart1, STM32FileSystem::NONE);
    
    embedded_logger::Logger::setGlobalLogger(logger);
    BMS_SYSTEM_EVENT(STARTUP, "BMS Logger initialized");
}

void monitor_cells() {
    for(int i = 0; i < 12; i++) {
        uint16_t voltage = read_cell_voltage(i);
        BMS_LOG_CELL_VOLTAGE(i, voltage);
        
        if(voltage < 2500) {
            BMS_SAFETY_CRITICAL("CELL_UNDERVOLTAGE", voltage);
        }
    }
}
```

## Documentation Requirements

### Doxygen Documentation Standard
**Requirement**: All code must have comprehensive Doxygen-compliant documentation

#### Documentation Standards
- **All public APIs**: Complete Doxygen comments with `@brief`, `@param`, `@return`, `@note`
- **All classes**: Class-level documentation with purpose, usage examples, thread safety
- **All enums**: Document each enum value with its purpose and usage
- **All macros**: Document parameters, return values, and usage examples
- **Platform-specific code**: Document platform limitations and requirements
- **Examples**: All example code must be fully documented

#### Doxygen Configuration
```cpp
// Required Doxygen comment format
/**
 * @brief Brief description of the function/class
 * @details Detailed description with usage patterns, constraints, etc.
 * @param paramName Description of parameter, including valid ranges
 * @return Description of return value and possible error conditions
 * @note Important notes about thread safety, platform limitations, etc.
 * @warning Any important warnings or limitations
 * @example
 * @code
 * // Example usage code here
 * @endcode
 */
```

#### Documentation Generation
- **Doxyfile**: Configure for embedded systems documentation
- **Output Formats**: HTML (primary), PDF (optional)
- **Integration**: Automated generation in CI/CD pipeline
- **Hosting**: GitHub Pages or documentation site
- **Examples**: Include all example projects in generated docs

#### Platform-Specific Documentation
- **ESP32**: Document ESP-IDF integration, SPIFFS/FAT usage, FreeRTOS considerations
- **STM32**: Document HAL integration, memory constraints, RTOS/bare metal differences
- **Arduino**: Document library compatibility, memory limitations
- **Cross-Platform**: Document feature availability matrix

## Development Environment

### Repository Structure
- **Main Branch**: `main` - Production releases
- **Development**: `develop` - Integration branch
- **Features**: `feature/platform-name` - Platform-specific work
- **Examples**: Complete working examples for each platform
- **Documentation**: `docs/` - Generated Doxygen documentation
- **Doxyfile**: Root-level Doxygen configuration

### VSCode Configuration
- Multi-platform IntelliSense (ESP32, STM32, Arduino)
- Platform-specific compiler paths and definitions
- Integrated build tasks for each platform
- Doxygen extensions for documentation generation and preview

### Testing Strategy
- **Unit Tests**: Core logger functionality, platform interfaces
- **Integration Tests**: Full platform testing (ESP32, STM32)
- **Performance Tests**: Memory usage, latency benchmarks
- **Regression Tests**: Forestry device compatibility

## Key Design Decisions

### Platform Abstraction Approach
- **Dependency Injection**: Constructor takes platform providers
- **Factory Pattern**: Auto-detection with platform-specific optimization
- **Configuration-Driven**: Single config struct instead of many setters
- **Zero Dependencies**: Core logger has no external dependencies

### Memory Management
- **Platform-Specific**: Different limits for each platform
- **Configurable**: All buffers and limits configurable at compile time
- **Optimization**: Platform-specific compiler flags and optimizations

### Thread Safety
- **Async by Default**: Non-blocking logging with background thread
- **Platform Adapters**: FreeRTOS, std::thread, or bare metal critical sections
- **Configurable**: Can disable async for memory-constrained platforms

## Success Metrics

### Technical Metrics
- **ESP32**: Zero performance regression vs current implementation
- **STM32**: <8KB RAM, <32KB flash for full-featured logger
- **Latency**: <10μs for safety-critical logging on STM32
- **Reliability**: 99.9%+ uptime for safety-critical applications
- **Documentation**: 100% Doxygen coverage for public APIs
- **Examples**: All example code fully documented and tested

### Project Metrics
- **Migration Time**: ESP32 migration <1 week
- **BMS Integration**: Full STM32 BMS logging <2 weeks
- **Test Coverage**: 90%+ for core functionality
- **Documentation**: Complete setup guides for all platforms + generated API docs
- **Doxygen Quality**: All warnings resolved, comprehensive examples included

## Future Roadmap

### v1.0.0 (Current Goal)
- ESP32 and STM32 production ready
- Complete BMS logging patterns
- Basic Arduino support

### v1.1.0
- Advanced IoT logging patterns
- WiFi/Network logging for ESP32
- CAN bus logging for STM32

### v2.0.0
- Real-time logging guarantees
- Distributed logging (multiple devices)
- Cloud integration patterns

## Contact & Collaboration

### Repository
- **URL**: https://github.com/unmanned-systems-uk/embedded_logger.git
- **Organization**: Unmanned Systems UK
- **License**: MIT

### Key Stakeholders
- **Forestry Project**: Active user, zero regression required
- **BMS Project**: New integration, safety-critical requirements
- **Community**: Open source contributors (future)

## Files Generated

### Setup Script
- `setup_embedded_logger.bat` - Creates complete directory structure
- Generates all placeholder files with implementation comments
- Sets up VSCode configuration for multi-platform development

### Next Actions Required
1. Run setup script to create directory structure
2. Initialize Git repository with generated structure
3. Begin ESP32 platform provider implementation
4. Extract and port existing forestry logger code

---

*This context document should be referenced in any future conversations about the embedded logger library project to maintain continuity and understanding of the current state and goals.*