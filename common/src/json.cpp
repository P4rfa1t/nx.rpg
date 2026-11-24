#include "nxrpg/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace nxrpg {

namespace {

void append_utf8(std::string& out, std::uint32_t cp) {
  if (cp <= 0x7Fu) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FFu) {
    out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  } else if (cp <= 0xFFFFu) {
    out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
    out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  } else if (cp <= 0x10FFFFu) {
    out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
    out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
  }
}

void dump_string(std::string& out, std::string_view s) {
  out.push_back('"');
  for (unsigned char ch : s) {
    switch (ch) {
      case '"':
        out.append("\\\"");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      default:
        if (ch < 0x20u) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(ch));
          out.append(buf);
        } else {
          out.push_back(static_cast<char>(ch));
        }
        break;
    }
  }
  out.push_back('"');
}

void dump_impl(std::string& out, const JsonValue& val, bool pretty, int indent) {
  if (val.is_null()) {
    out.append("null");
  } else if (auto b = val.as_bool()) {
    out.append(*b ? "true" : "false");
  } else if (val.is_int()) {
    out.append(std::to_string(*val.as_int64()));
  } else if (auto d = val.as_double()) {
    if (!std::isfinite(*d)) {
      out.append("null");
    } else {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%.15g", *d);
      out.append(buf);
    }
  } else if (const auto* s = val.as_string()) {
    dump_string(out, *s);
  } else if (const auto* arr = val.as_array()) {
    if (arr->empty()) {
      out.append("[]");
      return;
    }
    out.push_back('[');
    for (std::size_t i = 0; i < arr->size(); ++i) {
      if (i > 0) out.push_back(',');
      if (pretty) {
        out.push_back('\n');
        out.append(static_cast<std::size_t>((indent + 1) * 2), ' ');
      }
      dump_impl(out, (*arr)[i], pretty, indent + 1);
    }
    if (pretty) {
      out.push_back('\n');
      out.append(static_cast<std::size_t>(indent * 2), ' ');
    }
    out.push_back(']');
  } else if (const auto* obj = val.as_object()) {
    if (obj->empty()) {
      out.append("{}");
      return;
    }
    out.push_back('{');
    for (std::size_t i = 0; i < obj->size(); ++i) {
      if (i > 0) out.push_back(',');
      if (pretty) {
        out.push_back('\n');
        out.append(static_cast<std::size_t>((indent + 1) * 2), ' ');
      }
      dump_string(out, (*obj)[i].first);
      out.append(pretty ? ": " : ":");
      dump_impl(out, (*obj)[i].second, pretty, indent + 1);
    }
    if (pretty) {
      out.push_back('\n');
      out.append(static_cast<std::size_t>(indent * 2), ' ');
    }
    out.push_back('}');
  }
}

class Parser {
 public:
  explicit Parser(std::string_view src) : src_(src) {
    if (src_.size() >= 3 &&
        static_cast<unsigned char>(src_[0]) == 0xEF &&
        static_cast<unsigned char>(src_[1]) == 0xBB &&
        static_cast<unsigned char>(src_[2]) == 0xBF) {
      pos_ = 3;
    }
  }

  std::optional<JsonValue> parse_root() {
    skip_ws();
    auto val = parse_value(0);
    if (!val) return std::nullopt;
    skip_ws();
    if (pos_ != src_.size()) return std::nullopt;
    return val;
  }

 private:
  static constexpr int kMaxDepth = 128;

  void skip_ws() noexcept {
    while (pos_ < src_.size()) {
      char c = src_[pos_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool consume(char expected) noexcept {
    if (pos_ < src_.size() && src_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  std::optional<JsonValue> parse_value(int depth) {
    if (depth > kMaxDepth || pos_ >= src_.size()) return std::nullopt;
    char c = src_[pos_];
    if (c == '{') return parse_object(depth + 1);
    if (c == '[') return parse_array(depth + 1);
    if (c == '"') {
      auto s = parse_string();
      if (!s) return std::nullopt;
      return JsonValue(std::move(*s));
    }
    if (c == 't') {
      if (src_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return JsonValue(true);
      }
      return std::nullopt;
    }
    if (c == 'f') {
      if (src_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return JsonValue(false);
      }
      return std::nullopt;
    }
    if (c == 'n') {
      if (src_.substr(pos_, 4) == "null") {
        pos_ += 4;
        return JsonValue(nullptr);
      }
      return std::nullopt;
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parse_number();
    }
    return std::nullopt;
  }

  std::optional<std::uint32_t> parse_hex4() noexcept {
    if (pos_ + 4 > src_.size()) return std::nullopt;
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = src_[pos_ + i];
      v <<= 4;
      if (c >= '0' && c <= '9') {
        v |= static_cast<std::uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        v |= static_cast<std::uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        v |= static_cast<std::uint32_t>(c - 'A' + 10);
      } else {
        return std::nullopt;
      }
    }
    pos_ += 4;
    return v;
  }

  std::optional<std::string> parse_string() {
    if (!consume('"')) return std::nullopt;
    std::string out;
    while (pos_ < src_.size()) {
      unsigned char ch = static_cast<unsigned char>(src_[pos_++]);
      if (ch == '"') {
        return out;
      }
      if (ch < 0x20u) {
        return std::nullopt;
      }
      if (ch == '\\') {
        if (pos_ >= src_.size()) return std::nullopt;
        char esc = src_[pos_++];
        switch (esc) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'u': {
            auto u1 = parse_hex4();
            if (!u1) return std::nullopt;
            std::uint32_t cp = *u1;
            if (cp >= 0xD800u && cp <= 0xDBFFu) {
              if (pos_ + 2 <= src_.size() && src_[pos_] == '\\' && src_[pos_ + 1] == 'u') {
                pos_ += 2;
                auto u2 = parse_hex4();
                if (!u2 || *u2 < 0xDC00u || *u2 > 0xDFFFu) return std::nullopt;
                cp = 0x10000u + (((cp - 0xD800u) << 10) | (*u2 - 0xDC00u));
              } else {
                return std::nullopt;
              }
            } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
              return std::nullopt;
            }
            append_utf8(out, cp);
            break;
          }
          default:
            return std::nullopt;
        }
      } else {
        out.push_back(static_cast<char>(ch));
      }
    }
    return std::nullopt;
  }

  std::optional<JsonValue> parse_number() {
    std::size_t start = pos_;
    if (src_[pos_] == '-') ++pos_;
    if (pos_ >= src_.size()) return std::nullopt;
    if (src_[pos_] == '0') {
      ++pos_;
    } else if (src_[pos_] >= '1' && src_[pos_] <= '9') {
      while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
    } else {
      return std::nullopt;
    }
    bool is_float = false;
    if (pos_ < src_.size() && src_[pos_] == '.') {
      is_float = true;
      ++pos_;
      if (pos_ >= src_.size() || src_[pos_] < '0' || src_[pos_] > '9') return std::nullopt;
      while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
    }
    if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
      is_float = true;
      ++pos_;
      if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
      if (pos_ >= src_.size() || src_[pos_] < '0' || src_[pos_] > '9') return std::nullopt;
      while (pos_ < src_.size() && src_[pos_] >= '0' && src_[pos_] <= '9') ++pos_;
    }

    std::string_view token = src_.substr(start, pos_ - start);
    if (!is_float) {
      std::int64_t iv = 0;
      auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), iv);
      if (ec == std::errc() && ptr == token.data() + token.size()) {
        return JsonValue(iv);
      }
    }
    std::string tmp(token);
    char* endptr = nullptr;
    double dv = std::strtod(tmp.c_str(), &endptr);
    if (endptr == tmp.c_str() + tmp.size() && std::isfinite(dv)) {
      return JsonValue(dv);
    }
    return std::nullopt;
  }

  std::optional<JsonValue> parse_array(int depth) {
    if (!consume('[')) return std::nullopt;
    JsonValue::Array arr;
    skip_ws();
    if (consume(']')) return JsonValue(std::move(arr));
    while (true) {
      skip_ws();
      auto elem = parse_value(depth);
      if (!elem) return std::nullopt;
      arr.push_back(std::move(*elem));
      skip_ws();
      if (consume(']')) return JsonValue(std::move(arr));
      if (!consume(',')) return std::nullopt;
    }
  }

  std::optional<JsonValue> parse_object(int depth) {
    if (!consume('{')) return std::nullopt;
    JsonValue::Object obj;
    skip_ws();
    if (consume('}')) return JsonValue(std::move(obj));
    while (true) {
      skip_ws();
      auto key = parse_string();
      if (!key) return std::nullopt;
      skip_ws();
      if (!consume(':')) return std::nullopt;
      skip_ws();
      auto val = parse_value(depth);
      if (!val) return std::nullopt;

      // Replace duplicate key if present, preserving deterministic order
      bool replaced = false;
      for (auto& [k, v] : obj) {
        if (k == *key) {
          v = std::move(*val);
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        obj.emplace_back(std::move(*key), std::move(*val));
      }

      skip_ws();
      if (consume('}')) return JsonValue(std::move(obj));
      if (!consume(',')) return std::nullopt;
    }
  }

  std::string_view src_;
  std::size_t pos_ = 0;
};

}  // namespace

std::optional<bool> JsonValue::as_bool() const noexcept {
  if (const auto* p = std::get_if<bool>(&data_)) return *p;
  return std::nullopt;
}

std::optional<std::int64_t> JsonValue::as_int64() const noexcept {
  if (const auto* p = std::get_if<std::int64_t>(&data_)) return *p;
  if (const auto* d = std::get_if<double>(&data_)) {
    if (std::isfinite(*d) &&
        *d >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
        *d <= static_cast<double>(std::numeric_limits<std::int64_t>::max()) &&
        std::trunc(*d) == *d) {
      return static_cast<std::int64_t>(*d);
    }
  }
  return std::nullopt;
}

std::optional<double> JsonValue::as_double() const noexcept {
  if (const auto* d = std::get_if<double>(&data_)) return *d;
  if (const auto* i = std::get_if<std::int64_t>(&data_)) return static_cast<double>(*i);
  return std::nullopt;
}

const std::string* JsonValue::as_string() const noexcept {
  return std::get_if<std::string>(&data_);
}

const JsonValue::Array* JsonValue::as_array() const noexcept {
  return std::get_if<Array>(&data_);
}

JsonValue::Array* JsonValue::as_array_mut() noexcept {
  return std::get_if<Array>(&data_);
}

const JsonValue::Object* JsonValue::as_object() const noexcept {
  return std::get_if<Object>(&data_);
}

JsonValue::Object* JsonValue::as_object_mut() noexcept {
  return std::get_if<Object>(&data_);
}

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
  const auto* obj = as_object();
  if (!obj) return nullptr;
  for (const auto& [k, v] : *obj) {
    if (k == key) return &v;
  }
  return nullptr;
}

JsonValue* JsonValue::find_mut(std::string_view key) noexcept {
  auto* obj = as_object_mut();
  if (!obj) return nullptr;
  for (auto& [k, v] : *obj) {
    if (k == key) return &v;
  }
  return nullptr;
}

void JsonValue::set(std::string key, JsonValue value) {
  if (is_null()) {
    data_ = Object{};
  }
  auto* obj = as_object_mut();
  if (!obj) return;
  for (auto& [k, v] : *obj) {
    if (k == key) {
      v = std::move(value);
      return;
    }
  }
  obj->emplace_back(std::move(key), std::move(value));
}

void JsonValue::push_back(JsonValue value) {
  if (is_null()) {
    data_ = Array{};
  }
  auto* arr = as_array_mut();
  if (!arr) return;
  arr->push_back(std::move(value));
}

std::string JsonValue::dump(bool pretty) const {
  std::string out;
  dump_impl(out, *this, pretty, 0);
  if (pretty) out.push_back('\n');
  return out;
}

std::optional<JsonValue> JsonValue::parse(std::string_view text) {
  Parser parser(text);
  return parser.parse_root();
}

}  // namespace nxrpg
