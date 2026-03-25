#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>

enum class ConfigStatus {
    OK,
    FILE_ERR,
    PARSE_ERR,
    WRITE_ERR,
    RESTORED_BACKUP,
    RESTORED_DEFAULT,
    RESTORING_ERR,
    VALIDATION_ERR
};

// struct AudioProfile {
//     uint32_t freq;
//     uint32_t step;
//     uint32_t cycle_count;
// };

// struct BleConfig {
//     String service_uuid;
//     String char_uuid;
//     uint32_t pin;
// };

struct AppSerialConfig {
    bool read_enabled;
    bool write_enabled;
};

struct AppConfig {
private:
    JsonDocument _doc;

    static bool isSeparator(char c);
    static bool isDigitToken(const String& token);
    static String sha256Hex(const String& input);

    std::vector<String> splitPath(const String& path) const;
    bool getVariantByPath(JsonVariantConst root, const String& path, JsonVariantConst& out) const;
    bool ensureParentObject(const std::vector<String>& tokens, JsonObject& parent, String& leaf) ;
    bool validateDocument(const JsonDocument& doc) const;
    static void mergeObjects(JsonObject dst, JsonObjectConst src);

public:
    AppConfig();

    void loadFactoryDefaults();
    ConfigStatus deserialize(const String& jsonPayload);
    String serialize() const;

    bool validate() const;
    bool operator==(const AppConfig& other) const;
    String hash() const;

    bool clear();
    const JsonDocument& raw() const;

    bool exists(const String& path) const;
    bool removeValue(const String& path);

    bool getValue(const String& path, String& out) const;
    bool getValue(const String& path, bool& out) const;
    bool getValue(const String& path, uint32_t& out) const;
    bool getValue(const String& path, int32_t& out) const;
    bool getValue(const String& path, double& out) const;

    bool setValue(const String& path, const String& value);
    bool setValue(const String& path, const char* value);
    bool setValue(const String& path, bool value);
    bool setValue(const String& path, uint32_t value);
    bool setValue(const String& path, int32_t value);
    bool setValue(const String& path, double value);
    bool setRawJson(const String& path, const String& jsonValue);

    bool getSubtreeJson(const String& path, String& out) const;
};

class BackupManager {
public:
    ConfigStatus readRaw(const String& path, String& outputBuffer);
    ConfigStatus writeAtomic(const String& path, const String& payload);
};

class ConfigOrchestrator {
private:
    AppConfig _state;
    BackupManager _ioManager;

    const String _mainPath;
    const String _backupPath;

    String _mainSnapshot;
    String _backupSnapshot;

    ConfigOrchestrator();
    ConfigStatus processLoadLogic();
    static ConfigOrchestrator* _instance;

public:
    static ConfigOrchestrator* getInstance();

    ConfigStatus begin();
    ConfigStatus reload();
    ConfigStatus save(bool updateBackup = true);
    ConfigStatus proccesFactoryReset();

    const AppConfig& getConfig() const;
    ConfigStatus updateConfig(const AppConfig& newConfig);

    const String& getMainSnapshot() const;
    const String& getBackupSnapshot() const;
};