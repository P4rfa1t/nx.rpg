#include "launcher/services/i18n_service.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

#include "nxrpg/json.hpp"

namespace nxrpg::launcher {

namespace {

void flatten_json(
    const JsonValue& node,
    const std::string& prefix,
    std::unordered_map<std::string, std::string>& out) {
  if (const auto* s = node.as_string()) {
    if (!prefix.empty()) {
      out[prefix] = *s;
    }
    return;
  }
  if (const auto* obj = node.as_object()) {
    for (const auto& [k, v] : *obj) {
      std::string next = prefix.empty() ? k : (prefix + "." + k);
      flatten_json(v, next, out);
    }
  }
}

}  // namespace

bool I18nService::load_from_json_string(std::string_view json_text) {
  auto parsed = JsonValue::parse(json_text);
  if (!parsed || !parsed->is_object()) return false;
  entries_.clear();
  flatten_json(*parsed, "", entries_);
  return true;
}

bool I18nService::load_from_directory(
    const std::filesystem::path& i18n_dir,
    std::string_view language_code) {
  namespace fs = std::filesystem;
  std::error_code ec;

  std::filesystem::path target = i18n_dir / (std::string(language_code) + ".json");
  if (!fs::is_regular_file(target, ec)) {
    target = i18n_dir / "en-US.json";
    if (!fs::is_regular_file(target, ec)) {
      return false;
    }
    language_ = "en-US";
  } else {
    language_ = std::string(language_code);
  }

  std::ifstream ifs(target, std::ios::binary);
  if (!ifs) return false;
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  return load_from_json_string(content);
}

std::string I18nService::translate(std::string_view dotted_key, std::string_view fallback) const {
  auto it = entries_.find(std::string(dotted_key));
  if (it != entries_.end()) {
    return it->second;
  }
  return fallback.empty() ? std::string(dotted_key) : std::string(fallback);
}

}  // namespace nxrpg::launcher
