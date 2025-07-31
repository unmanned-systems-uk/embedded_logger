

// Main logger interface - generic cross-platform logging
// Supports ESP32, STM32, Arduino, and desktop platforms
/**
 * @file logger.h
 * @brief Cross-platform embedded logging library
 * @details Provides asynchronous, thread-safe logging with file rotation
 * @version 1.0.0
 * @date 2025-01-31
 * @author Embedded Logger Library
 *
 * @copyright Copyright (c) 2025 Unmanned Systems UK. All rights reserved.
 * Licensed under the MIT License.
 */

#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include <functional>

namespace embedded_logger
{

    /**
     * @brief Log levels for filtering and categorization
     */
    enum class LogLevel : uint8_t
    {
        DEBUG = 0,   ///< Detailed debug information
        INFO = 1,    ///< General information messages
        WARNING = 2, ///< Warning messages
        ERROR = 3,   ///< Error messages
        CRITICAL = 4 ///< Critical system errors
    };

    /**
     * @brief Log destinations for output control
     */
    enum class LogDestination : uint8_t
    {
        CONSOLE_ONLY = 1, ///< Output only to console/serial
        FILE_ONLY = 2,    ///< Output only to file
        BOTH = 3          ///< Output to both console and file
    };

    /**
     * @brief Individual log entry structure
     */
    struct LogEntry
    {
        LogLevel level;        ///< Log level
        std::string timestamp; ///< Formatted timestamp
        std::string component; ///< Component/module name
        std::string message;   ///< Log message
        std::string filename;  ///< Source file name (optional)
        int lineNumber;        ///< Source line number (optional)
        uint64_t timestampMs;  ///< Unix timestamp in milliseconds

        /**
         * @brief Default constructor
         */
        LogEntry() = default;

        /**
         * @brief Construct a log entry
         * @param lvl Log level
         * @param comp Component name
         * @param msg Log message
         * @param file Source file name (optional)
         * @param line Source line number (optional)
         */
        LogEntry(LogLevel lvl, const std::string &comp, const std::string &msg,
                 const std::string &file = "", int line = 0)
            : level(lvl), component(comp), message(msg), filename(file),
              lineNumber(line), timestampMs(0) {}
    };

    /**
     * @brief Logger configuration structure
     */
    struct LoggerConfig
    {
        LogLevel consoleLogLevel = LogLevel::DEBUG;               ///< Console log level
        LogLevel fileLogLevel = LogLevel::INFO;                   ///< File log level
        LogDestination defaultDestination = LogDestination::BOTH; ///< Default output destination

        std::string logDirectory = "/logs"; ///< Log file directory
        size_t maxFileSize = 1024 * 1024;   ///< Max file size (1MB)
        int maxBackupFiles = 5;             ///< Number of backup files

        bool asyncLogging = true;           ///< Enable async logging
        bool enableColors = true;           ///< Enable console colors
        bool includeTimestamp = true;       ///< Include timestamps
        bool includeSourceLocation = false; ///< Include file:line info

        std::string timestampFormat = "%Y-%m-%d %H:%M:%S"; ///< Timestamp format
        std::string logFilePrefix = "embedded_log";        ///< Log file prefix
        std::string logFileExtension = ".txt";             ///< Log file extension
    };

    /**
     * @brief Platform-specific time provider interface
     */
    class ITimeProvider
    {
    public:
        virtual ~ITimeProvider() = default;

        /**
         * @brief Get current date/time string
         * @return Formatted timestamp string
         */
        virtual std::string getCurrentDateTime() = 0;

        /**
         * @brief Get unix timestamp in milliseconds
         * @return Milliseconds since epoch
         */
        virtual uint64_t getUnixTimestampMs() = 0;
    };

    /**
     * @brief Platform-specific file system interface
     */
    class IFileSystem
    {
    public:
        virtual ~IFileSystem() = default;

        /**
         * @brief Check if file exists
         * @param path File path to check
         * @return true if file exists
         */
        virtual bool fileExists(const std::string &path) = 0;

        /**
         * @brief Create directory recursively
         * @param path Directory path to create
         * @return true if successful
         */
        virtual bool createDirectory(const std::string &path) = 0;

        /**
         * @brief Get file size
         * @param path File path
         * @return File size in bytes
         */
        virtual size_t getFileSize(const std::string &path) = 0;

        /**
         * @brief Delete file
         * @param path File path to delete
         * @return true if successful
         */
        virtual bool deleteFile(const std::string &path) = 0;

        /**
         * @brief Rename/move file
         * @param oldPath Source path
         * @param newPath Destination path
         * @return true if successful
         */
        virtual bool renameFile(const std::string &oldPath, const std::string &newPath) = 0;
    };

    /**
     * @brief Cross-platform embedded logging library
     * @details Provides asynchronous, thread-safe logging with file rotation,
     *          multiple output destinations, and platform abstraction.
     *
     * Features:
     * - Multiple log levels (DEBUG, INFO, WARNING, ERROR, CRITICAL)
     * - Dual output (console + file)
     * - Automatic file rotation by size
     * - Thread-safe operation
     * - Asynchronous logging to prevent blocking
     * - Configurable formatting
     * - Component-based filtering
     * - Cross-platform support (ESP32, STM32, Arduino, Linux, Windows)
     *
     * @example Basic usage
     * @code
     * using namespace embedded_logger;
     *
     * // Create and configure logger
     * LoggerConfig config;
     * config.logDirectory = "/sdcard/logs";
     * auto logger = std::make_shared<Logger>(config);
     *
     * // Initialize and set as global
     * logger->initialize();
     * Logger::setGlobalLogger(logger);
     *
     * // Use logging macros
     * EL_INFO("APP", "Application started");
     * EL_ERROR("SENSOR", "Failed to read temperature");
     * @endcode
     */
    class Logger
    {
    public:
        /**
         * @brief Constructor with configuration
         * @param config Logger configuration
         * @param timeProvider Custom time provider (optional)
         * @param fileSystem Custom file system (optional)
         */
        explicit Logger(const LoggerConfig &config = LoggerConfig{},
                        std::unique_ptr<ITimeProvider> timeProvider = nullptr,
                        std::unique_ptr<IFileSystem> fileSystem = nullptr);

        /**
         * @brief Destructor - ensures all logs are flushed
         */
        ~Logger();

        // Delete copy constructor and assignment
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        /**
         * @brief Initialize the logging system
         * @return true if initialization successful
         * @note Must be called before logging
         */
        bool initialize();

        /**
         * @brief Shutdown the logging system gracefully
         * @note Flushes all pending logs and stops background thread
         */
        void shutdown();

        /**
         * @brief Update logger configuration
         * @param config New configuration
         * @note Some settings require restart to take effect
         */
        void updateConfig(const LoggerConfig &config);

        /**
         * @brief Get current configuration
         * @return Current logger configuration
         */
        LoggerConfig getConfig() const;

        // Main logging methods

        /**
         * @brief Log a debug message
         * @param component Component name (e.g., "WiFiManager", "BatteryMonitor")
         * @param message Log message
         * @param destination Output destination override
         */
        void debug(const std::string &component, const std::string &message,
                   LogDestination destination = LogDestination::BOTH);

        /**
         * @brief Log an info message
         * @param component Component name
         * @param message Log message
         * @param destination Output destination override
         */
        void info(const std::string &component, const std::string &message,
                  LogDestination destination = LogDestination::BOTH);

        /**
         * @brief Log a warning message
         * @param component Component name
         * @param message Log message
         * @param destination Output destination override
         */
        void warning(const std::string &component, const std::string &message,
                     LogDestination destination = LogDestination::BOTH);

        /**
         * @brief Log an error message
         * @param component Component name
         * @param message Log message
         * @param destination Output destination override
         */
        void error(const std::string &component, const std::string &message,
                   LogDestination destination = LogDestination::BOTH);

        /**
         * @brief Log a critical error message
         * @param component Component name
         * @param message Log message
         * @param destination Output destination override
         */
        void critical(const std::string &component, const std::string &message,
                      LogDestination destination = LogDestination::BOTH);

        /**
         * @brief Log formatted message with printf-style formatting
         * @param level Log level
         * @param component Component name
         * @param format Printf-style format string
         * @param ... Format arguments
         */
        void logf(LogLevel level, const std::string &component,
                  const char *format, ...);

        /**
         * @brief Log system startup information
         * @param systemInfo System information to log
         */
        void logSystemStartup(const std::string &systemInfo);

        /**
         * @brief Log system shutdown information
         */
        void logSystemShutdown();

        /**
         * @brief Force flush all pending log entries
         * @note Blocks until all logs are written
         */
        void flush();

        // Status and statistics

        /**
         * @brief Get current log file path
         * @return Path to current log file
         */
        std::string getCurrentLogFile() const;

        /**
         * @brief Get total number of log entries written
         * @return Total log entry count
         */
        size_t getTotalLogCount() const;

        /**
         * @brief Check if logger is initialized
         * @return true if initialized and ready
         */
        bool isInitialized() const { return initialized_; }

        // Static convenience methods for global logger

        /**
         * @brief Set global logger instance
         * @param logger Shared pointer to logger instance
         * @note Thread-safe
         */
        static void setGlobalLogger(std::shared_ptr<Logger> logger);

        /**
         * @brief Get global logger instance
         * @return Shared pointer to global logger (may be null)
         * @note Thread-safe
         */
        static std::shared_ptr<Logger> getGlobalLogger();

    private:
        // Configuration and platform providers
        LoggerConfig config_;
        std::unique_ptr<ITimeProvider> timeProvider_;
        std::unique_ptr<IFileSystem> fileSystem_;

        // State management
        std::atomic<bool> initialized_;
        std::atomic<bool> shutdownRequested_;
        mutable std::mutex configMutex_;

        // File management
        std::string currentLogFile_;
        size_t currentFileSize_;
        std::unique_ptr<std::ofstream> currentLogStream_;
        mutable std::mutex fileMutex_;

        // Asynchronous logging
        std::queue<LogEntry> logQueue_;
        std::mutex queueMutex_;
        std::condition_variable queueCondition_;
        std::thread loggerThread_;

        // Statistics
        std::atomic<size_t> totalLogCount_;

        // Static global logger
        static std::shared_ptr<Logger> globalLogger_;
        static std::mutex globalLoggerMutex_;

        // Internal methods
        void log(const LogEntry &entry, LogDestination destination);
        void processLogEntry(const LogEntry &entry, LogDestination destination);
        void writeToConsole(const LogEntry &entry);
        void writeToFile(const LogEntry &entry);
        void rotateLogFileIfNeeded();
        bool createNewLogFile();
        void loggerThreadFunction();
        LogEntry createLogEntry(LogLevel level, const std::string &component,
                                const std::string &message);
        std::string formatLogEntry(const LogEntry &entry, bool includeColors = false);
        std::string getCurrentTimestamp();

        // Utility methods
        static std::string logLevelToString(LogLevel level);
        static std::string getColorForLevel(LogLevel level);

        // Default platform providers
        std::unique_ptr<ITimeProvider> createDefaultTimeProvider();
        std::unique_ptr<IFileSystem> createDefaultFileSystem();
    };

    /**
     * @brief Simple logger factory for common use cases
     */
    class LoggerFactory
    {
    public:
        /**
         * @brief Create logger with default configuration
         * @param logDirectory Directory for log files
         * @return Configured logger instance
         */
        static std::shared_ptr<Logger> createDefault(const std::string &logDirectory = "/logs");

        /**
         * @brief Create console-only logger (no file output)
         * @return Console-only logger instance
         */
        static std::shared_ptr<Logger> createConsoleOnly();

        /**
         * @brief Create file-only logger (no console output)
         * @param logDirectory Directory for log files
         * @return File-only logger instance
         */
        static std::shared_ptr<Logger> createFileOnly(const std::string &logDirectory = "/logs");
    };

} // namespace embedded_logger

// Convenience macros (can be disabled by defining EMBEDDED_LOGGER_NO_MACROS)
#ifndef EMBEDDED_LOGGER_NO_MACROS

/**
 * @brief Log debug message using global logger
 * @param component Component name
 * @param message Log message
 */
#define EL_DEBUG(component, message)                              \
    if (auto logger = embedded_logger::Logger::getGlobalLogger()) \
    {                                                             \
        logger->debug(component, message);                        \
    }

/**
 * @brief Log info message using global logger
 * @param component Component name
 * @param message Log message
 */
#define EL_INFO(component, message)                               \
    if (auto logger = embedded_logger::Logger::getGlobalLogger()) \
    {                                                             \
        logger->info(component, message);                         \
    }

/**
 * @brief Log warning message using global logger
 * @param component Component name
 * @param message Log message
 */
#define EL_WARNING(component, message)                            \
    if (auto logger = embedded_logger::Logger::getGlobalLogger()) \
    {                                                             \
        logger->warning(component, message);                      \
    }

/**
 * @brief Log error message using global logger
 * @param component Component name
 * @param message Log message
 */
#define EL_ERROR(component, message)                              \
    if (auto logger = embedded_logger::Logger::getGlobalLogger()) \
    {                                                             \
        logger->error(component, message);                        \
    }

/**
 * @brief Log critical message using global logger
 * @param component Component name
 * @param message Log message
 */
#define EL_CRITICAL(component, message)                           \
    if (auto logger = embedded_logger::Logger::getGlobalLogger()) \
    {                                                             \
        logger->critical(component, message);                     \
    }

// Formatted logging macros

/**
 * @brief Log formatted debug message using global logger
 * @param component Component name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define ELF_DEBUG(component, format, ...)                                                 \
    if (auto logger = embedded_logger::Logger::getGlobalLogger())                         \
    {                                                                                     \
        logger->logf(embedded_logger::LogLevel::DEBUG, component, format, ##__VA_ARGS__); \
    }

/**
 * @brief Log formatted info message using global logger
 * @param component Component name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define ELF_INFO(component, format, ...)                                                 \
    if (auto logger = embedded_logger::Logger::getGlobalLogger())                        \
    {                                                                                    \
        logger->logf(embedded_logger::LogLevel::INFO, component, format, ##__VA_ARGS__); \
    }

/**
 * @brief Log formatted error message using global logger
 * @param component Component name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
#define ELF_ERROR(component, format, ...)                                                 \
    if (auto logger = embedded_logger::Logger::getGlobalLogger())                         \
    {                                                                                     \
        logger->logf(embedded_logger::LogLevel::ERROR, component, format, ##__VA_ARGS__); \
    }

#endif // EMBEDDED_LOGGER_NO_MACROS