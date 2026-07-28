//
//  Config.cpp
//  OpenKey cho Linux
//

#include "Config.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#include "AppState.h"
#include "Engine.h"
#include "Macro.h"
#include "SmartSwitchKey.h"

namespace openkey {
namespace {

struct Field {
    const char* name;
    int* value;
};

// Danh sach nay la nguon su that duy nhat cho viec doc/ghi cau hinh: them mot
// tuy chon moi chi can them mot dong o day.
std::vector<Field> settingFields() {
    return {
        {"language", &vLanguage},
        {"inputType", &vInputType},
        {"freeMark", &vFreeMark},
        {"codeTable", &vCodeTable},
        {"switchKeyStatus", &vSwitchKeyStatus},
        {"checkSpelling", &vCheckSpelling},
        {"useModernOrthography", &vUseModernOrthography},
        {"quickTelex", &vQuickTelex},
        {"restoreIfWrongSpelling", &vRestoreIfWrongSpelling},
        {"fixRecommendBrowser", &vFixRecommendBrowser},
        {"useMacro", &vUseMacro},
        {"useMacroInEnglishMode", &vUseMacroInEnglishMode},
        {"autoCapsMacro", &vAutoCapsMacro},
        {"useSmartSwitchKey", &vUseSmartSwitchKey},
        {"upperCaseFirstChar", &vUpperCaseFirstChar},
        {"tempOffSpelling", &vTempOffSpelling},
        {"allowConsonantZFWJ", &vAllowConsonantZFWJ},
        {"quickStartConsonant", &vQuickStartConsonant},
        {"quickEndConsonant", &vQuickEndConsonant},
        {"rememberCode", &vRememberCode},
        {"otherLanguage", &vOtherLanguage},
        {"tempOffOpenKey", &vTempOffOpenKey},
    };
}

bool makeDirs(const std::string& path) {
    std::string acc;
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            acc = path.substr(0, i);
            if (mkdir(acc.c_str(), 0700) != 0 && errno != EEXIST) {
                return false;
            }
        }
    }
    return true;
}

// Doc mot doi tuong JSON phang gom cac cap "khoa": so hoac "khoa": "chuoi".
// Du dung cho file cau hinh cua chung ta, va tranh keo them mot thu vien.
bool parseFlatJson(const std::string& text,
                   std::vector<std::pair<std::string, std::string>>& out) {
    size_t i = 0;
    auto skipSpace = [&] {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) i++;
    };

    skipSpace();
    if (i >= text.size() || text[i] != '{') return false;
    i++;

    while (true) {
        skipSpace();
        if (i < text.size() && text[i] == '}') return true;
        if (i >= text.size()) return false;

        if (text[i] != '"') return false;
        i++;
        std::string key;
        while (i < text.size() && text[i] != '"') key.push_back(text[i++]);
        if (i >= text.size()) return false;
        i++;

        skipSpace();
        if (i >= text.size() || text[i] != ':') return false;
        i++;
        skipSpace();

        std::string value;
        if (i < text.size() && text[i] == '"') {
            i++;
            while (i < text.size() && text[i] != '"') value.push_back(text[i++]);
            if (i >= text.size()) return false;
            i++;
        } else {
            while (i < text.size() && text[i] != ',' && text[i] != '}' &&
                   !std::isspace(static_cast<unsigned char>(text[i]))) {
                value.push_back(text[i++]);
            }
            if (value.empty()) return false;
        }

        out.emplace_back(key, value);

        skipSpace();
        if (i < text.size() && text[i] == ',') {
            i++;
            continue;
        }
        if (i < text.size() && text[i] == '}') return true;
        return false;
    }
}

bool readWholeFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

std::string Config::configDir() {
    std::string base;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = std::string(home) + "/.config";
    } else {
        base = ".";
    }
    const std::string dir = base + "/openkey";
    makeDirs(dir);
    return dir;
}

std::string Config::configPath() const { return configDir() + "/config.json"; }
std::string Config::macroPath() const { return configDir() + "/macro.dat"; }
std::string Config::smartSwitchPath() const { return configDir() + "/smartswitch.dat"; }

bool Config::load() {
    std::string text;
    if (!readWholeFile(configPath(), text)) {
        // Lan chay dau tien: giu mac dinh va ghi ra de nguoi dung co file de sua.
        save();
        return true;
    }

    std::vector<std::pair<std::string, std::string>> pairs;
    if (!parseFlatJson(text, pairs)) {
        return false;
    }

    const auto fields = settingFields();
    for (const auto& [key, value] : pairs) {
        if (key == "backend") {
            backend = backendKindFromString(value);
            continue;
        }
        for (const auto& f : fields) {
            if (key == f.name) {
                *f.value = std::atoi(value.c_str());
                break;
            }
        }
    }
    return true;
}

bool Config::save() const {
    std::ofstream out(configPath(), std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out << "{\n";
    out << "  \"backend\": \"" << backendKindToString(backend) << "\"";
    for (const auto& f : settingFields()) {
        out << ",\n  \"" << f.name << "\": " << *f.value;
    }
    out << "\n}\n";
    return out.good();
}

void Config::loadMacroTable() const {
    readFromFile(macroPath(), false);
    onTableCodeChange();
}

void Config::saveMacroTable() const { saveToFile(macroPath()); }

void Config::loadSmartSwitchTable() const {
    std::string blob;
    if (!readWholeFile(smartSwitchPath(), blob) || blob.empty()) {
        return;
    }
    initSmartSwitchKey(reinterpret_cast<const Byte*>(blob.data()),
                       static_cast<int>(blob.size()));
}

void Config::saveSmartSwitchTable() const {
    std::vector<Byte> data;
    getSmartSwitchKeySaveData(data);
    std::ofstream out(smartSwitchPath(), std::ios::binary | std::ios::trunc);
    if (!out) return;
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
}

} // namespace openkey
