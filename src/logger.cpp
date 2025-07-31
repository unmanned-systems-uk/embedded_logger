// Core logger implementation
// Generic cross-platform logger implementation
/**
 * @file logger.cpp
 * @brief Cross-platform embedded logging library implementation
 * @version 1.0.0
 * @date 2025-01-31
 * @author Embedded Logger Library
 */

#include "embedded_logger/logger.h"
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define access _access
#define mkdir _mkdir
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#ifdef __linux__
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(_WIN32)
#include <filesystem>
namespace fs = std::filesystem;
#else
// For embedded platforms without std::filesystem
#define NO_FILESYSTEM
#endif

namespace embedded_logger
{

    // Static member initialization
    std::shared_ptr<Logger> Logger::globalLogger_ = nullptr;
    std::mutex Logger::globalLoggerMutex_;

    /**
     * @brief Default time provider using system clock
     */
    class DefaultTimeProvider : public ITimeProvider
    {
    public:
        std::string getCurrentDateTime() override
        {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);

            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
            return std::string(buffer);
        }

        uint64_t getUnixTimestampMs() override
        {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            return static_cast<uint64_t>(ms.count());
        }
    };

    /**
     * @brief Default file system provider
     */
    class DefaultFileSystem : public IFileSystem
    {
    public:
        bool fileExists(const std::string &path) override
        {
            return access(path.c_str(), 0) == 0;
        }

        bool createDirectory(const std::string &path) override
        {
#ifndef NO_FILESYSTEM
            try
            {
                return fs::create_directories(path);
            }
            catch (...)
            {
                return false;
            }
#else
            // Simple directory creation for embedded systems
#ifdef _WIN32
            return mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
            return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
#endif
        }

        size_t getFileSize(const std::string &path) override
        {
#ifndef NO_FILESYSTEM
            try
            {
                return fs::file_size(path);
            }
            catch (...)
            {
                return 0;
            }
#else
            FILE *file = fopen(path.c_str(), "rb");
            if (!file)
                return 0;

            fseek(file, 0, SEEK_END);
            size_t size = ftell(file);
            fclose(file);
            return size;
#endif
        }

        bool deleteFile(const std::string &path) override
        {
#ifndef NO_FILESYSTEM
            try
            {
                return fs::remove(path);
            }
            catch (...)
            {
                return false;
            }
#else
            return remove(path.c_str()) == 0;
#endif
        }

        bool renameFile(const std::string &oldPath, const std::string &newPath) override
        {
            return rename(oldPath.c_str(), newPath.c_str()) == 0;
        }
    };

    Logger::Logger(const LoggerConfig &config,
                   std::unique_ptr<ITimeProvider> timeProvider,
                   std::unique_ptr<IFileSystem> fileSystem)
        : config_(config), timeProvider_(timeProvider ? std::move(timeProvider) : createDefaultTimeProvider()), fileSystem_(fileSystem ? std::move(fileSystem) : createDefaultFileSystem()), initialized_(false), shutdownRequested_(false), currentFileSize_(0), totalLogCount_(0)
    {
    }

    Logger::~Logger()
    {
        shutdown();
    }

    bool Logger::initialize()
    {
        if (initialized_.load())
        {
            return true;
        }

        try
        {
            // Create log directory if it doesn't exist
            if (!fileSystem_->fileExists(config_.logDirectory))
            {
                if (!fileSystem_->createDirectory(config_.logDirectory))
                {
                    printf("Logger: Failed to create log directory: %s\n", config_.logDirectory.c_str());
                    return false;
                }
            }

            // Create initial log file
            if (!createNewLogFile())
            {
                printf("Logger: Failed to create initial log file\n");
                return false;
            }

            // Start async logging thread if enabled
            if (config_.asyncLogging)
            {
                shutdownRequested_.store(false);
                loggerThread_ = std::thread(&Logger::loggerThreadFunction, this);
            }

            initialized_.store(true);

            // Log system startup
            logSystemStartup("Embedded Logger initialized successfully");

            printf("Logger: Initialized successfully\n");
            printf("Logger: Console level: %s, File level: %s\n",
                   logLevelToString(config_.consoleLogLevel).c_str(),
                   logLevelToString(config_.fileLogLevel).c_str());
            printf("Logger: Log directory: %s\n", config_.logDirectory.c_str());
            printf("Logger: Current log file: %s\n", currentLogFile_.c_str());

            return true;
        }
        catch (const std::exception &e)
        {
            printf("Logger: Initialization failed: %s\n", e.what());
            return false;
        }
    }

    void Logger::shutdown()
    {
        if (!initialized_.load() || shutdownRequested_.load())
        {
            return;
        }

        logSystemShutdown();

        shutdownRequested_.store(true);

        // Wake up logger thread and wait for it to finish
        if (config_.asyncLogging && loggerThread_.joinable())
        {
            queueCondition_.notify_all();
            loggerThread_.join();
        }

        // Flush and close file
        std::lock_guard<std::mutex> lock(fileMutex_);
        if (currentLogStream_ && currentLogStream_->is_open())
        {
            currentLogStream_->flush();
            currentLogStream_->close();
        }

        initialized_.store(false);
        printf("Logger: Shutdown completed\n");
    }

    void Logger::updateConfig(const LoggerConfig &config)
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_ = config;
    }

    LoggerConfig Logger::getConfig() const
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        return config_;
    }

    void Logger::debug(const std::string &component, const std::string &message,
                       LogDestination destination)
    {
        LogEntry entry(LogLevel::DEBUG, component, message);
        log(entry, destination);
    }

    void Logger::info(const std::string &component, const std::string &message,
                      LogDestination destination)
    {
        LogEntry entry(LogLevel::INFO, component, message);
        log(entry, destination);
    }

    void Logger::warning(const std::string &component, const std::string &message,
                         LogDestination destination)
    {
        LogEntry entry(LogLevel::WARNING, component, message);
        log(entry, destination);
    }

    void Logger::error(const std::string &component, const std::string &message,
                       LogDestination destination)
    {
        LogEntry entry(LogLevel::ERROR, component, message);
        log(entry, destination);
    }

    void Logger::critical(const std::string &component, const std::string &message,
                          LogDestination destination)
    {
        LogEntry entry(LogLevel::CRITICAL, component, message);
        log(entry, destination);
    }

    void Logger::logf(LogLevel level, const std::string &component, const char *format, ...)
    {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        LogEntry entry(level, component, std::string(buffer));
        log(entry, config_.defaultDestination);
    }

    void Logger::logSystemStartup(const std::string &systemInfo)
    {
        std::string separator(80, '=');

        info("SYSTEM", separator);
        info("SYSTEM", "EMBEDDED LOGGER STARTUP");
        info("SYSTEM", systemInfo);
        info("SYSTEM", "Timestamp: " + getCurrentTimestamp());
        info("SYSTEM", separator);
    }

    void Logger::logSystemShutdown()
    {
        std::string separator(80, '=');

        info("SYSTEM", separator);
        info("SYSTEM", "EMBEDDED LOGGER SHUTDOWN");
        info("SYSTEM", "Total log entries: " + std::to_string(totalLogCount_.load()));
        info("SYSTEM", "Timestamp: " + getCurrentTimestamp());
        info("SYSTEM", separator);
    }

    void Logger::flush()
    {
        if (config_.asyncLogging)
        {
            // For async logging, wait for queue to empty
            std::unique_lock<std::mutex> lock(queueMutex_);
            while (!logQueue_.empty() && !shutdownRequested_.load())
            {
                queueCondition_.notify_all();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Flush file stream
        std::lock_guard<std::mutex> fileLock(fileMutex_);
        if (currentLogStream_ && currentLogStream_->is_open())
        {
            currentLogStream_->flush();
        }
    }

    std::string Logger::getCurrentLogFile() const
    {
        return currentLogFile_;
    }

    size_t Logger::getTotalLogCount() const
    {
        return totalLogCount_.load();
    }

    void Logger::log(const LogEntry &entry, LogDestination destination)
    {
        if (!initialized_.load())
        {
            return;
        }

        // Create complete log entry with timestamp
        LogEntry completeEntry = entry;
        completeEntry.timestamp = getCurrentTimestamp();
        completeEntry.timestampMs = timeProvider_->getUnixTimestampMs();

        if (config_.asyncLogging)
        {
            // Add to queue for background processing
            std::lock_guard<std::mutex> lock(queueMutex_);
            logQueue_.push(completeEntry);
            queueCondition_.notify_one();
        }
        else
        {
            // Process immediately
            processLogEntry(completeEntry, destination);
        }

        totalLogCount_++;
    }

    void Logger::processLogEntry(const LogEntry &entry, LogDestination destination)
    {
        if ((destination & LogDestination::CONSOLE_ONLY) &&
            entry.level >= config_.consoleLogLevel)
        {
            writeToConsole(entry);
        }

        if ((destination & LogDestination::FILE_ONLY) &&
            entry.level >= config_.fileLogLevel)
        {
            writeToFile(entry);
        }
    }

    void Logger::writeToConsole(const LogEntry &entry)
    {
        std::string formatted = formatLogEntry(entry, config_.enableColors);
        printf("%s\n", formatted.c_str());
        fflush(stdout);
    }

    void Logger::writeToFile(const LogEntry &entry)
    {
        std::lock_guard<std::mutex> lock(fileMutex_);

        if (!currentLogStream_ || !currentLogStream_->is_open())
        {
            return;
        }

        std::string formatted = formatLogEntry(entry, false);
        *currentLogStream_ << formatted << std::endl;

        currentFileSize_ += formatted.length() + 1;

        // Check if rotation is needed
        if (currentFileSize_ >= config_.maxFileSize)
        {
            rotateLogFileIfNeeded();
        }
    }

    void Logger::rotateLogFileIfNeeded()
    {
        if (currentFileSize_ < config_.maxFileSize)
        {
            return;
        }

        currentLogStream_->close();

        try
        {
            // Rotate existing backup files
            for (int i = config_.maxBackupFiles - 1; i > 0; --i)
            {
                std::string oldFile = currentLogFile_ + "." + std::to_string(i);
                std::string newFile = currentLogFile_ + "." + std::to_string(i + 1);

                if (fileSystem_->fileExists(oldFile))
                {
                    if (i == config_.maxBackupFiles - 1)
                    {
                        fileSystem_->deleteFile(newFile);
                    }
                    fileSystem_->renameFile(oldFile, newFile);
                }
            }

            // Move current log to .1
            std::string backupFile = currentLogFile_ + ".1";
            fileSystem_->renameFile(currentLogFile_, backupFile);

            // Create new log file
            createNewLogFile();
        }
        catch (const std::exception &e)
        {
            printf("Logger: Failed to rotate log file: %s\n", e.what());
            // Try to reopen current file
            currentLogStream_ = std::make_unique<std::ofstream>(currentLogFile_, std::ios::app);
        }
    }

    bool Logger::createNewLogFile()
    {
        std::string timestamp = getCurrentTimestamp();
        // Replace colons and spaces with underscores for filename
        std::replace(timestamp.begin(), timestamp.end(), ':', '_');
        std::replace(timestamp.begin(), timestamp.end(), ' ', '_');

        currentLogFile_ = config_.logDirectory + "/" + config_.logFilePrefix + "_" +
                          timestamp + config_.logFileExtension;
        currentFileSize_ = 0;

        std::lock_guard<std::mutex> lock(fileMutex_);
        currentLogStream_ = std::make_unique<std::ofstream>(currentLogFile_, std::ios::app);

        if (!currentLogStream_->is_open())
        {
            printf("Logger: Failed to create log file: %s\n", currentLogFile_.c_str());
            return false;
        }

        // Write file header
        *currentLogStream_ << "# Embedded Logger Library Log File" << std::endl;
        *currentLogStream_ << "# Created: " << getCurrentTimestamp() << std::endl;
        *currentLogStream_ << "# Format: [Timestamp] [Level] [Component] Message" << std::endl;
        *currentLogStream_ << std::string(80, '=') << std::endl;

        return true;
    }

    void Logger::loggerThreadFunction()
    {
        while (!shutdownRequested_.load())
        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            // Wait for log entries or shutdown signal
            queueCondition_.wait(lock, [this]
                                 { return !logQueue_.empty() || shutdownRequested_.load(); });

            // Process all queued entries
            while (!logQueue_.empty())
            {
                LogEntry entry = logQueue_.front();
                logQueue_.pop();
                lock.unlock();

                // Process the log entry
                processLogEntry(entry, config_.defaultDestination);

                lock.lock();
            }
        }

        // Process any remaining entries before shutdown
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!logQueue_.empty())
        {
            LogEntry entry = logQueue_.front();
            logQueue_.pop();
            processLogEntry(entry, config_.defaultDestination);
        }
    }

    std::string Logger::formatLogEntry(const LogEntry &entry, bool includeColors)
    {
        std::ostringstream oss;

        if (includeColors)
        {
            oss << getColorForLevel(entry.level);
        }

        oss << "[" << entry.timestamp << "] "
            << "[" << std::setw(8) << logLevelToString(entry.level) << "] "
            << "[" << std::setw(12) << entry.component << "] "
            << entry.message;

        if (config_.includeSourceLocation && !entry.filename.empty() && entry.lineNumber > 0)
        {
            oss << " (" << entry.filename << ":" << entry.lineNumber << ")";
        }

        if (includeColors)
        {
            oss << "\033[0m"; // Reset color
        }

        return oss.str();
    }

    std::string Logger::getCurrentTimestamp()
    {
        return timeProvider_->getCurrentDateTime();
    }

    std::string Logger::logLevelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
        }
    }

    std::string Logger::getColorForLevel(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "\033[36m"; // Cyan
        case LogLevel::INFO:
            return "\033[32m"; // Green
        case LogLevel::WARNING:
            return "\033[33m"; // Yellow
        case LogLevel::ERROR:
            return "\033[31m"; // Red
        case LogLevel::CRITICAL:
            return "\033[35m"; // Magenta
        default:
            return "\033[0m"; // Reset
        }
    }

    std::unique_ptr<ITimeProvider> Logger::createDefaultTimeProvider()
    {
        return std::make_unique<DefaultTimeProvider>();
    }

    std::unique_ptr<IFileSystem> Logger::createDefaultFileSystem()
    {
        return std::make_unique<DefaultFileSystem>();
    }

    // Static methods for global logger
    void Logger::setGlobalLogger(std::shared_ptr<Logger> logger)
    {
        std::lock_guard<std::mutex> lock(globalLoggerMutex_);
        globalLogger_ = logger;
    }

    std::shared_ptr<Logger> Logger::getGlobalLogger()
    {
        std::lock_guard<std::mutex> lock(globalLoggerMutex_);
        return globalLogger_;
    }

    // LoggerFactory implementation
    std::shared_ptr<Logger> LoggerFactory::createDefault(const std::string &logDirectory)
    {
        LoggerConfig config;
        config.logDirectory = logDirectory;
        return std::make_shared<Logger>(config);
    }

    std::shared_ptr<Logger> LoggerFactory::createConsoleOnly()
    {
        LoggerConfig config;
        config.defaultDestination = LogDestination::CONSOLE_ONLY;
        return std::make_shared<Logger>(config);
    }

    std::shared_ptr<Logger> LoggerFactory::createFileOnly(const std::string &logDirectory)
    {
        LoggerConfig config;
        config.logDirectory = logDirectory;
        config.defaultDestination = LogDestination::FILE_ONLY;
        return std::make_shared<Logger>(config);
    }

} // namespace embedded_logger