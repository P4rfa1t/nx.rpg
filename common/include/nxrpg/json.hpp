#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nxrpg {

/// Lightweight, zero-dependency RFC 8259 JSON value used across `common/` and `launcher/`.
class JsonValue {
 public:
  using Array = std::vector<JsonValue>;
  using Object = std::vector<std::pair<std::string, JsonValue>>;
  using Storage = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object>;

  JsonValue() noexcept : data_(nullptr) {}
  JsonValue(std::nullptr_t) noexcept : data_(nullptr) {}
  JsonValue(bool value) noexcept : data_(value) {}
  JsonValue(int value) noexcept : data_(static_cast<std::int64_t>(value)) {}
  JsonValue(std::int64_t value) noexcept : data_(value) {}
  JsonValue(std::uint32_t value) noexcept : data_(static_cast<std::int64_t>(value)) {}
  JsonValue(std::uint64_t value) noexcept : data_(static_cast<std::int64_t>(value)) {}
  JsonValue(double value) noexcept : data_(value) {}
  JsonValue(const char* value) : data_(std::string(value ? value : "")) {}
  JsonValue(std::string_view value) : data_(std::string(value)) {}
  JsonValue(std::string value) : data_(std::move(value)) {}
  JsonValue(Array value) : data_(std::move(value)) {}
  JsonValue(Object value) : data_(std::move(value)) {}

  [[nodiscard]] static JsonValue make_object() { return JsonValue(Object{}); }
  [[nodiscard]] static JsonValue make_array() { return JsonValue(Array{}); }

  [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(data_); }
  [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(data_); }
  [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<std::int64_t>(data_); }
  [[nodiscard]] bool is_number() const noexcept {
    return std::holds_alternative<std::int64_t>(data_) || std::holds_alternative<double>(data_);
  }
  [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(data_); }
  [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<Array>(data_); }
  [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<Object>(data_); }

  [[nodiscard]] std::optional<bool> as_bool() const noexcept;
  [[nodiscard]] std::optional<std::int64_t> as_int64() const noexcept;
  [[nodiscard]] std::optional<double> as_double() const noexcept;
  [[nodiscard]] const std::string* as_string() const noexcept;
  [[nodiscard]] const Array* as_array() const noexcept;
  [[nodiscard]] Array* as_array_mut() noexcept;
  [[nodiscard]] const Object* as_object() const noexcept;
  [[nodiscard]] Object* as_object_mut() noexcept;

  /// Finds a member by key in an object, or returns nullptr if not an object / missing.
  [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;
  [[nodiscard]] JsonValue* find_mut(std::string_view key) noexcept;

  /// Sets or updates a key on an object (converts null to object if needed).
  void set(std::string key, JsonValue value);

  /// Appends an element to an array (converts null to array if needed).
  void push_back(JsonValue value);

  [[nodiscard]] std::string dump(bool pretty = true) const;
  [[nodiscard]] static std::optional<JsonValue> parse(std::string_view text);

 private:
  Storage data_;
};

}  // namespace nxrpg
