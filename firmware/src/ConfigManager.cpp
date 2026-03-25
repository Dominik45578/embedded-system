#include "ConfigManager.hpp"
#include <mbedtls/sha256.h>

namespace {
static const char FACTORY_DEFAULT_JSON[] = R"json(
{
  "alarm": {
    "freq": 4000,
    "step": 500,
    "cycle_count": 10
  },
  "enabled": {
    "freq": 2000,
    "step": 200,
    "cycle_count": 2
  },
  "disabled": {
    "freq": 2500,
    "step": 100,
    "cycle_count": 1
  },
  "ble": {
    "service_uuid": "4fafc201-1fb5-459e-8fcc-c5c9c331914b",
    "char_uuid": "beb5483e-36e1-4688-b7f5-ea07361b26a8",
    "pin": 123456,
    "device_name": "ESP32 Lock",
    "enabled": true
  },
  "serial": {
    "read": true,
    "write": true
  },
  "servo": {
    "pin": 23,
    "channel": 1,
    "freq": 50,
    "res": 16,
    "bounds": {
      "min": 500,
      "mid": 1500,
      "max": 2500
    },
    "angles": {
      "min": 0,
      "mid": 90,
      "max": 180
    },
    "move": {
      "open_ms": 1500,
      "close_ms": 1500
    }
  },
  "lock": {
    "pin": "123456",
    "blocked_ms": 30000,
    "auto_relock_ms": 5000
  },
  "log": {
    "enabled": true,
    "level": "INFO"
  }
}
)json";

static bool isNumericToken(const String& token) {
    if (token.length() == 0) return false;
    for (size_t i = 0; i < token.length(); ++i) {
        if (token[i] < '0' || token[i] > '9') return false;
    }
    return true;
}
}

ConfigOrchestrator* ConfigOrchestrator::_instance = nullptr;

AppConfig::AppConfig() {
    loadFactoryDefaults();
}

bool AppConfig::isSeparator(char c) {
    return c == '.' || c == ',' || c == '/' || c == ':';
}

bool AppConfig::isDigitToken(const String& token) {
    return isNumericToken(token);
}

String AppConfig::sha256Hex(const String& input) {
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const unsigned char*>(input.c_str()), input.length());
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    String out;
    out.reserve(64);
    for (size_t i = 0; i < 32; ++i) {
        if (hash[i] < 16) out += '0';
        out += String(hash[i], HEX);
    }
    out.toLowerCase();
    return out;
}

std::vector<String> AppConfig::splitPath(const String& path) const {
    std::vector<String> tokens;
    String token;

    for (size_t i = 0; i < path.length(); ++i) {
        char c = path[i];
        if (isSeparator(c)) {
            if (token.length() > 0) {
                tokens.push_back(token);
                token = "";
            }
        } else if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            token += c;
        }
    }

    if (token.length() > 0) {
        tokens.push_back(token);
    }

    return tokens;
}

bool AppConfig::getVariantByPath(JsonVariantConst root, const String& path, JsonVariantConst& out) const {
    std::vector<String> tokens = splitPath(path);
    if (tokens.empty()) return false;

    JsonVariantConst current = root;

    for (size_t i = 0; i < tokens.size(); ++i) {
        const String& token = tokens[i];

        if (current.is<JsonObjectConst>()) {
            JsonObjectConst obj = current.as<JsonObjectConst>();
            if (!obj.containsKey(token.c_str())) return false;
            current = obj[token.c_str()];
        } else if (current.is<JsonArrayConst>()) {
            if (!isDigitToken(token)) return false;
            size_t index = static_cast<size_t>(token.toInt());
            JsonArrayConst arr = current.as<JsonArrayConst>();
            if (index >= arr.size()) return false;
            current = arr[index];
        } else {
            return false;
        }
    }

    out = current;
    return true;
}

bool AppConfig::ensureParentObject(const std::vector<String>& tokens, JsonObject& parent, String& leaf) {
    if (tokens.empty()) return false;

    if (!_doc.is<JsonObject>()) {
        _doc.to<JsonObject>();
    }
    JsonObject current = _doc.as<JsonObject>();

    if (tokens.size() == 1) {
        parent = current;
        leaf = tokens[0];
        return true;
    }

    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        const char* key = tokens[i].c_str();
        JsonVariant member = current[key];
        if (!member.is<JsonObject>()) {
            member.to<JsonObject>();
        }
        current = member.as<JsonObject>();
    }

    parent = current;
    leaf = tokens.back();
    return true;
}

void AppConfig::mergeObjects(JsonObject dst, JsonObjectConst src) {
    for (JsonPairConst kv : src) {
        const char* key = kv.key().c_str();
        JsonVariantConst srcValue = kv.value();

        if (srcValue.is<JsonObjectConst>()) {
            if (!dst[key].is<JsonObject>()) {
                dst[key].to<JsonObject>();
            }
            mergeObjects(dst[key].as<JsonObject>(), srcValue.as<JsonObjectConst>());
        } else {
            dst[key] = srcValue;
        }
    }
}

bool AppConfig::validateDocument(const JsonDocument& doc) const {
    auto getU32 = [&](const char* path, uint32_t& out) -> bool {
        JsonVariantConst node;
        if (!getVariantByPath(doc.as<JsonVariantConst>(), path, node)) return false;
        if (node.is<uint32_t>()) { out = node.as<uint32_t>(); return true; }
        if (node.is<int32_t>()) { out = static_cast<uint32_t>(node.as<int32_t>()); return true; }
        if (node.is<const char*>()) { out = static_cast<uint32_t>(String(node.as<const char*>()).toInt()); return true; }
        if (node.is<bool>()) { out = node.as<bool>() ? 1 : 0; return true; }
        return false;
    };

    auto getI32 = [&](const char* path, int32_t& out) -> bool {
        JsonVariantConst node;
        if (!getVariantByPath(doc.as<JsonVariantConst>(), path, node)) return false;
        if (node.is<int32_t>()) { out = node.as<int32_t>(); return true; }
        if (node.is<uint32_t>()) { out = static_cast<int32_t>(node.as<uint32_t>()); return true; }
        if (node.is<const char*>()) { out = static_cast<int32_t>(String(node.as<const char*>()).toInt()); return true; }
        if (node.is<bool>()) { out = node.as<bool>() ? 1 : 0; return true; }
        return false;
    };

    auto getBool = [&](const char* path, bool& out) -> bool {
        JsonVariantConst node;
        if (!getVariantByPath(doc.as<JsonVariantConst>(), path, node)) return false;
        if (node.is<bool>()) { out = node.as<bool>(); return true; }
        if (node.is<const char*>()) {
            String s = node.as<const char*>();
            s.toLowerCase();
            if (s == "true" || s == "1") { out = true; return true; }
            if (s == "false" || s == "0") { out = false; return true; }
            return false;
        }
        if (node.is<uint32_t>()) { out = node.as<uint32_t>() != 0; return true; }
        if (node.is<int32_t>()) { out = node.as<int32_t>() != 0; return true; }
        return false;
    };

    auto getString = [&](const char* path, String& out) -> bool {
        JsonVariantConst node;
        if (!getVariantByPath(doc.as<JsonVariantConst>(), path, node)) return false;
        if (node.is<const char*>()) { out = node.as<const char*>(); return true; }
        if (node.is<String>()) { out = node.as<String>(); return true; }
        if (node.is<JsonObjectConst>() || node.is<JsonArrayConst>()) {
            String payload;
            serializeJson(node, payload);
            out = payload;
            return true;
        }
        String payload;
        serializeJson(node, payload);
        out = payload;
        return true;
    };

    uint32_t u32 = 0; int32_t i32 = 0; bool b = false; String s;

    if (!getU32("alarm.freq", u32) || u32 < 2000 || u32 > 5000) return false;
    if (!getU32("alarm.step", u32) || u32 == 0) return false;
    if (!getU32("alarm.cycle_count", u32) || u32 == 0) return false;

    if (!getU32("enabled.freq", u32) || u32 < 2000 || u32 > 5000) return false;
    if (!getU32("enabled.step", u32) || u32 == 0) return false;
    if (!getU32("enabled.cycle_count", u32) || u32 == 0) return false;

    if (!getU32("disabled.freq", u32) || u32 < 2000 || u32 > 5000) return false;
    if (!getU32("disabled.step", u32) || u32 == 0) return false;
    if (!getU32("disabled.cycle_count", u32) || u32 == 0) return false;

    if (!getString("ble.service_uuid", s) || s.length() == 0) return false;
    if (!getString("ble.char_uuid", s) || s.length() == 0) return false;
    if (!getU32("ble.pin", u32) || u32 == 0) return false;
    if (!getString("ble.device_name", s) || s.length() == 0) return false;
    if (!getBool("ble.enabled", b)) return false;

    if (!getBool("serial.read", b)) return false;
    if (!getBool("serial.write", b)) return false;

    if (!getU32("servo.pin", u32) || u32 > 39) return false;
    if (!getU32("servo.channel", u32) || u32 > 15) return false;
    if (!getU32("servo.freq", u32) || u32 < 40 || u32 > 400) return false;
    if (!getU32("servo.res", u32) || u32 < 1 || u32 > 20) return false;

    uint32_t minPulse = 0, midPulse = 0, maxPulse = 0;
    if (!getU32("servo.bounds.min", minPulse) || minPulse < 200 || minPulse > 3000) return false;
    if (!getU32("servo.bounds.mid", midPulse) || midPulse < 200 || midPulse > 3000) return false;
    if (!getU32("servo.bounds.max", maxPulse) || maxPulse < 200 || maxPulse > 3000) return false;
    if (!(minPulse < midPulse && midPulse < maxPulse)) return false;

    if (!getI32("servo.angles.min", i32) || i32 < 0 || i32 > 180) return false;
    if (!getI32("servo.angles.mid", i32) || i32 < 0 || i32 > 180) return false;
    if (!getI32("servo.angles.max", i32) || i32 < 0 || i32 > 180) return false;

    if (!getU32("servo.move.open_ms", u32) || u32 == 0) return false;
    if (!getU32("servo.move.close_ms", u32) || u32 == 0) return false;

    if (!getString("lock.pin", s) || s.length() == 0) return false;
    if (!getU32("lock.blocked_ms", u32) || u32 == 0) return false;
    if (!getU32("lock.auto_relock_ms", u32) || u32 == 0) return false;

    if (!getBool("log.enabled", b)) return false;
    if (!getString("log.level", s) || s.length() == 0) return false;

    return true;
}

void AppConfig::loadFactoryDefaults() {
    _doc.clear();
    deserializeJson(_doc, FACTORY_DEFAULT_JSON);
}

ConfigStatus AppConfig::deserialize(const String& jsonPayload) {
    JsonDocument incoming;
    DeserializationError error = deserializeJson(incoming, jsonPayload);
    if (error) return ConfigStatus::PARSE_ERR;

    if (!incoming.is<JsonObjectConst>()) return ConfigStatus::PARSE_ERR;

    JsonDocument candidate;
    DeserializationError baseErr = deserializeJson(candidate, FACTORY_DEFAULT_JSON);
    if (baseErr) return ConfigStatus::RESTORING_ERR;

    mergeObjects(candidate.as<JsonObject>(), incoming.as<JsonObjectConst>());

    if (!validateDocument(candidate)) return ConfigStatus::VALIDATION_ERR;

    _doc = candidate;
    return ConfigStatus::OK;
}

String AppConfig::serialize() const {
    String output;
    serializeJson(_doc, output);
    return output;
}

bool AppConfig::validate() const {
    return validateDocument(_doc);
}

bool AppConfig::operator==(const AppConfig& other) const {
    return hash() == other.hash();
}

String AppConfig::hash() const {
    return sha256Hex(serialize());
}

bool AppConfig::clear() {
    _doc.clear();
    _doc.to<JsonObject>();
    return true;
}

const JsonDocument& AppConfig::raw() const {
    return _doc;
}

bool AppConfig::exists(const String& path) const {
    JsonVariantConst node;
    return getVariantByPath(_doc.as<JsonVariantConst>(), path, node);
}

bool AppConfig::removeValue(const String& path) {
    std::vector<String> tokens = splitPath(path);
    if (tokens.empty()) return false;

    if (!_doc.is<JsonObject>()) return false;
    JsonObject current = _doc.as<JsonObject>();

    if (tokens.size() == 1) {
        current.remove(tokens[0].c_str());
        return true;
    }

    for (size_t i = 0; i + 1 < tokens.size(); ++i) {
        const char* key = tokens[i].c_str();
        if (!current.containsKey(key) || !current[key].is<JsonObject>()) {
            return false;
        }
        current = current[key].as<JsonObject>();
    }

    current.remove(tokens.back().c_str());
    return true;
}

bool AppConfig::getValue(const String& path, String& out) const {
    JsonVariantConst node;
    if (!getVariantByPath(_doc.as<JsonVariantConst>(), path, node)) return false;
    if (node.is<const char*>()) { out = node.as<const char*>(); return true; }
    if (node.is<String>()) { out = node.as<String>(); return true; }
    String payload;
    serializeJson(node, payload);
    out = payload;
    return true;
}

bool AppConfig::getValue(const String& path, bool& out) const {
    JsonVariantConst node;
    if (!getVariantByPath(_doc.as<JsonVariantConst>(), path, node)) return false;
    if (node.is<bool>()) { out = node.as<bool>(); return true; }
    if (node.is<const char*>()) {
        String s = node.as<const char*>();
        s.toLowerCase();
        if (s == "true" || s == "1") { out = true; return true; }
        if (s == "false" || s == "0") { out = false; return true; }
        return false;
    }
    if (node.is<uint32_t>()) { out = node.as<uint32_t>() != 0; return true; }
    if (node.is<int32_t>()) { out = node.as<int32_t>() != 0; return true; }
    return false;
}

bool AppConfig::getValue(const String& path, uint32_t& out) const {
    JsonVariantConst node;
    if (!getVariantByPath(_doc.as<JsonVariantConst>(), path, node)) return false;
    if (node.is<uint32_t>()) { out = node.as<uint32_t>(); return true; }
    if (node.is<int32_t>()) { out = static_cast<uint32_t>(node.as<int32_t>()); return true; }
    if (node.is<bool>()) { out = node.as<bool>() ? 1 : 0; return true; }
    if (node.is<const char*>()) { out = static_cast<uint32_t>(String(node.as<const char*>()).toInt()); return true; }
    return false;
}

bool AppConfig::getValue(const String& path, int32_t& out) const {
    JsonVariantConst node;
    if (!getVariantByPath(_doc.as<JsonVariantConst>(), path, node)) return false;
    if (node.is<int32_t>()) { out = node.as<int32_t>(); return true; }
    if (node.is<uint32_t>()) { out = static_cast<int32_t>(node.as<uint32_t>()); return true; }
    if (node.is<bool>()) { out = node.as<bool>() ? 1 : 0; return true; }
    if (node.is<const char*>()) { out = static_cast<int32_t>(String(node.as<const char*>()).toInt()); return true; }
    return false;
}

bool AppConfig::getValue(const String& path, double& out) const {
    JsonVariantConst node;
    if (!getVariantByPath(_doc.as<JsonVariantConst>(), path, node)) return false;
    if (node.is<double>()) { out = node.as<double>(); return true; }
    if (node.is<float>()) { out = static_cast<double>(node.as<float>()); return true; }
    if (node.is<uint32_t>()) { out = static_cast<double>(node.as<uint32_t>()); return true; }
    if (node.is<int32_t>()) { out = static_cast<double>(node.as<int32_t>()); return true; }
    if (node.is<bool>()) { out = node.as<bool>() ? 1.0 : 0.0; return true; }
    if (node.is<const char*>()) { out = String(node.as<const char*>()).toFloat(); return true; }
    return false;
}

bool AppConfig::setValue(const String& path, const String& value) {
    std::vector<String> tokens = splitPath(path);
    JsonObject parent; String leaf;
    if (!ensureParentObject(tokens, parent, leaf)) return false;
    parent[leaf.c_str()] = value;
    return true;
}

bool AppConfig::setValue(const String& path, const char* value) {
    return setValue(path, String(value ? value : ""));
}

bool AppConfig::setValue(const String& path, bool value) {
    std::vector<String> tokens = splitPath(path);
    JsonObject parent; String leaf;
    if (!ensureParentObject(tokens, parent, leaf)) return false;
    parent[leaf.c_str()] = value;
    return true;
}

bool AppConfig::setValue(const String& path, uint32_t value) {
    std::vector<String> tokens = splitPath(path);
    JsonObject parent; String leaf;
    if (!ensureParentObject(tokens, parent, leaf)) return false;
    parent[leaf.c_str()] = value;
    return true;
}

bool AppConfig::setValue(const String& path, int32_t value) {
    std::vector<String> tokens = splitPath(path);
    JsonObject parent; String leaf;
    if (!ensureParentObject(tokens, parent, leaf)) return false;
    parent[leaf.c_str()] = value;
    return true;
}

bool AppConfig::setValue(const String& path, double value) {
    std::vector<String> tokens = splitPath(path);
    JsonObject parent; String leaf;
    if (!ensureParentObject(tokens, parent, leaf)) return false;
    parent[leaf.c_str()] = value;
    return true;
}

bool AppConfig::setRawJson(const String& path, const String& jsonValue) {
    JsonDocument fragment;
    DeserializationError error = deserializeJson(fragment, jsonValue);
    if (error) return false;

    std::vector<String> tokens = splitPath(path);
    if (tokens.empty()) return false;

    JsonObject parent; String leaf;
    if (!ensureParentObject(tokens, parent, leaf)) return false;

    parent[leaf.c_str()] = fragment.as<JsonVariantConst>();
    return true;
}

bool AppConfig::getSubtreeJson(const String& path, String& out) const {
    JsonVariantConst node;
    if (!getVariantByPath(_doc.as<JsonVariantConst>(), path, node)) return false;
    out = "";
    serializeJson(node, out);
    return true;
}

ConfigStatus BackupManager::readRaw(const String& path, String& outputBuffer) {
    if (!LittleFS.exists(path)) return ConfigStatus::FILE_ERR;

    File file = LittleFS.open(path, "r");
    if (!file) return ConfigStatus::FILE_ERR;

    outputBuffer = "";
    outputBuffer.reserve(file.size());
    outputBuffer = file.readString();
    file.close();
    return ConfigStatus::OK;
}

ConfigStatus BackupManager::writeAtomic(const String& path, const String& payload) {
    String tempPath = path + ".tmp";

    File file = LittleFS.open(tempPath, "w");
    if (!file) return ConfigStatus::WRITE_ERR;

    size_t bytesWritten = file.print(payload);
    file.flush();
    file.close();

    if (bytesWritten != payload.length()) {
        LittleFS.remove(tempPath);
        return ConfigStatus::WRITE_ERR;
    }

    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
    }

    if (!LittleFS.rename(tempPath, path)) {
        LittleFS.remove(tempPath);
        return ConfigStatus::WRITE_ERR;
    }

    return ConfigStatus::OK;
}

ConfigOrchestrator::ConfigOrchestrator()
    : _mainPath("/config.json"),
      _backupPath("/backup.json") {
}

ConfigOrchestrator* ConfigOrchestrator::getInstance() {
    if (_instance == nullptr) {
        _instance = new ConfigOrchestrator();
    }
    return _instance;
}

ConfigStatus ConfigOrchestrator::begin() {
    if (!LittleFS.begin(true)) {
        _state.loadFactoryDefaults();
        _mainSnapshot = _state.serialize();
        _backupSnapshot = _mainSnapshot;
        return ConfigStatus::FILE_ERR;
    }
    return processLoadLogic();
}

ConfigStatus ConfigOrchestrator::reload() {
    return processLoadLogic();
}

ConfigStatus ConfigOrchestrator::processLoadLogic() {
    String payload;

    if (_ioManager.readRaw(_mainPath, payload) == ConfigStatus::OK) {
        if (_state.deserialize(payload) == ConfigStatus::OK) {
            _mainSnapshot = payload;
            
            // Cicha weryfikacja backupu podczas udanego odczytu z maina
            String backupPayload;
            if (_ioManager.readRaw(_backupPath, backupPayload) == ConfigStatus::OK) {
                _backupSnapshot = backupPayload;
            } else {
                _ioManager.writeAtomic(_backupPath, _mainSnapshot);
                _backupSnapshot = _mainSnapshot;
            }
            return ConfigStatus::OK;
        }
    }

    if (_ioManager.readRaw(_backupPath, payload) == ConfigStatus::OK) {
        if (_state.deserialize(payload) == ConfigStatus::OK) {
            _ioManager.writeAtomic(_mainPath, _state.serialize());
            _mainSnapshot = payload;
            _backupSnapshot = payload;
            return ConfigStatus::RESTORED_BACKUP;
        }
    }

    return proccesFactoryReset();
}

ConfigStatus ConfigOrchestrator::save(bool updateBackup) {
    if (!_state.validate()) return ConfigStatus::VALIDATION_ERR;

    String payload = _state.serialize();

    ConfigStatus status = _ioManager.writeAtomic(_mainPath, payload);
    if (status != ConfigStatus::OK) return status;

    _mainSnapshot = payload;

    if (updateBackup) {
        status = _ioManager.writeAtomic(_backupPath, payload);
        if (status != ConfigStatus::OK) return status;
        _backupSnapshot = payload;
    }

    return ConfigStatus::OK;
}

ConfigStatus ConfigOrchestrator::proccesFactoryReset() {
    _state.loadFactoryDefaults();
    String defaultPayload = _state.serialize();

    ConfigStatus mainStatus = _ioManager.writeAtomic(_mainPath, defaultPayload);
    ConfigStatus backupStatus = _ioManager.writeAtomic(_backupPath, defaultPayload);

    if (mainStatus != ConfigStatus::OK || backupStatus != ConfigStatus::OK) {
        return ConfigStatus::RESTORING_ERR;
    }

    _mainSnapshot = defaultPayload;
    _backupSnapshot = defaultPayload;
    return ConfigStatus::RESTORED_DEFAULT;
}

const AppConfig& ConfigOrchestrator::getConfig() const {
    return _state;
}

ConfigStatus ConfigOrchestrator::updateConfig(const AppConfig& newConfig) {
    if (!newConfig.validate()) return ConfigStatus::VALIDATION_ERR;
    if (_state == newConfig) return ConfigStatus::OK;

    _state = newConfig;
    return save(true);
}

const String& ConfigOrchestrator::getMainSnapshot() const {
    return _mainSnapshot;
}

const String& ConfigOrchestrator::getBackupSnapshot() const {
    return _backupSnapshot;
}