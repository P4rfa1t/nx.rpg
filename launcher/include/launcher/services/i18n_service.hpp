#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nxrpg::launcher {

/// Lightweight localization loader backed by `launcher/resources/i18n/<lang>.json`.
class I18nService {
 public:
  bool load_from_directory(const std::filesystem::path& i18n_dir, std::string_view language_code);
  bool load_from_json_string(std::string_view json_text);

  /// Looks up a dotted key such as `"app.applet_warning"` or `"tabs.library"`.
  [[nodiscard]] std::string translate(std::string_view dotted_key, std::string_view fallback = "") const;

  [[nodiscard]] const std::string& active_language() const noexcept { return language_; }

 private:
  std::string language_ = "zh-CN";
  std::unordered_map<std::string, std::string> entries_;
};

}  // namespace nxrpg::launcher
