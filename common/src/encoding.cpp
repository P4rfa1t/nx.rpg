#include "nxrpg/encoding.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "encoding_tables.hpp"

namespace nxrpg::encoding {

namespace {

bool has_utf8_bom(std::string_view bytes) noexcept {
  return bytes.size() >= 3 &&
         static_cast<unsigned char>(bytes[0]) == 0xEF &&
         static_cast<unsigned char>(bytes[1]) == 0xBB &&
         static_cast<unsigned char>(bytes[2]) == 0xBF;
}

std::string_view strip_utf8_bom(std::string_view bytes) noexcept {
  return has_utf8_bom(bytes) ? bytes.substr(3) : bytes;
}

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

std::uint16_t lookup_2byte(const std::uint16_t* table, std::uint8_t b1, std::uint8_t b2) noexcept {
  if (b1 < 0x81u || b1 > 0xFEu || b2 < 0x40u || b2 > 0xFEu || b2 == 0x7Fu) {
    return 0;
  }
  std::size_t row = static_cast<std::size_t>(b1 - 0x81u);
  std::size_t col = static_cast<std::size_t>(b2 < 0x7Fu ? (b2 - 0x40u) : (b2 - 0x41u));
  return table[row * 190u + col];
}

int score_shift_jis(std::string_view bytes, bool& all_valid) noexcept {
  int score = 0;
  all_valid = true;
  for (std::size_t i = 0; i < bytes.size();) {
    auto b1 = static_cast<std::uint8_t>(bytes[i]);
    if (b1 < 0x80u) {
      ++i;
      continue;
    }
    if (b1 >= 0xA1u && b1 <= 0xDFu) {
      // Half-width katakana: valid, but rare in titles compared to full-width kana/kanji
      score += 1;
      ++i;
      continue;
    }
    if (((b1 >= 0x81u && b1 <= 0x9Fu) || (b1 >= 0xE0u && b1 <= 0xFCu)) && i + 1 < bytes.size()) {
      auto b2 = static_cast<std::uint8_t>(bytes[i + 1]);
      std::uint16_t cp = lookup_2byte(detail::kCp932Table, b1, b2);
      if (cp != 0) {
        if (b1 == 0x82u || b1 == 0x83u) {
          score += 12;  // Hiragana / Katakana — very strong Shift_JIS indicator
        } else if (b1 >= 0x81u && b1 <= 0x9Fu) {
          score += 9;   // Symbols + JIS Level 1 kanji
        } else {
          score += 6;
        }
        i += 2;
        continue;
      }
    }
    all_valid = false;
    score -= 30;
    ++i;
  }
  return score;
}

int score_gbk(std::string_view bytes, bool& all_valid) noexcept {
  int score = 0;
  all_valid = true;
  for (std::size_t i = 0; i < bytes.size();) {
    auto b1 = static_cast<std::uint8_t>(bytes[i]);
    if (b1 < 0x80u) {
      ++i;
      continue;
    }
    if (b1 >= 0x81u && b1 <= 0xFEu && i + 1 < bytes.size()) {
      auto b2 = static_cast<std::uint8_t>(bytes[i + 1]);
      std::uint16_t cp = lookup_2byte(detail::kCp936Table, b1, b2);
      if (cp != 0) {
        if (b1 >= 0xB0u && b1 <= 0xF7u && b2 >= 0xA1u && b2 <= 0xFEu) {
          score += 10;  // Standard GB2312 common Simplified Chinese area
        } else if (b1 >= 0xA1u && b1 <= 0xA9u && b2 >= 0xA1u && b2 <= 0xFEu) {
          score += 8;   // GB2312 punctuation / full-width ASCII
        } else {
          score += 3;   // Extended GBK/3/4/5
        }
        i += 2;
        continue;
      }
    }
    all_valid = false;
    score -= 30;
    ++i;
  }
  return score;
}

int score_big5(std::string_view bytes, bool& all_valid) noexcept {
  int score = 0;
  all_valid = true;
  for (std::size_t i = 0; i < bytes.size();) {
    auto b1 = static_cast<std::uint8_t>(bytes[i]);
    if (b1 < 0x80u) {
      ++i;
      continue;
    }
    if (b1 >= 0xA1u && b1 <= 0xF9u && i + 1 < bytes.size()) {
      auto b2 = static_cast<std::uint8_t>(bytes[i + 1]);
      std::uint16_t cp = lookup_2byte(detail::kCp950Table, b1, b2);
      if (cp != 0) {
        if (b1 >= 0xA4u && b1 <= 0xC6u && ((b2 >= 0x40u && b2 <= 0x7Eu) || (b2 >= 0xA1u && b2 <= 0xFEu))) {
          score += (b2 <= 0x7Eu) ? 10 : 8;  // Big5 Level 1 common Traditional Chinese
        } else {
          score += 4;
        }
        i += 2;
        continue;
      }
    }
    all_valid = false;
    score -= 30;
    ++i;
  }
  return score;
}

int score_cp949(std::string_view bytes, bool& all_valid) noexcept {
  int score = 0;
  all_valid = true;
  for (std::size_t i = 0; i < bytes.size();) {
    auto b1 = static_cast<std::uint8_t>(bytes[i]);
    if (b1 < 0x80u) {
      ++i;
      continue;
    }
    if (b1 >= 0x81u && b1 <= 0xFEu && i + 1 < bytes.size()) {
      auto b2 = static_cast<std::uint8_t>(bytes[i + 1]);
      std::uint16_t cp = lookup_2byte(detail::kCp949Table, b1, b2);
      if (cp != 0) {
        if (b1 >= 0xB0u && b1 <= 0xC8u && b2 >= 0xA1u && b2 <= 0xFEu) {
          score += 7;  // KS X 1001 common Hangul syllables
        } else {
          score += 3;
        }
        i += 2;
        continue;
      }
    }
    all_valid = false;
    score -= 30;
    ++i;
  }
  return score;
}

int score_cp1252(std::string_view bytes) noexcept {
  int score = 0;
  for (unsigned char ch : bytes) {
    if (ch >= 0x80u) {
      std::uint16_t cp = detail::kCp1252Table[ch - 0x80u];
      score += (cp != 0xFFFDu) ? 2 : -10;
    }
  }
  return score;
}

}  // namespace

bool is_pure_ascii(std::string_view bytes) noexcept {
  for (unsigned char c : bytes) {
    if (c >= 0x80u) return false;
  }
  return true;
}

bool is_valid_utf8(std::string_view bytes) noexcept {
  bytes = strip_utf8_bom(bytes);
  std::size_t i = 0;
  while (i < bytes.size()) {
    auto c = static_cast<std::uint8_t>(bytes[i]);
    if (c <= 0x7Fu) {
      ++i;
      continue;
    }
    std::size_t len = 0;
    std::uint32_t cp = 0;
    std::uint32_t min_cp = 0;
    if ((c & 0xE0u) == 0xC0u) {
      len = 2;
      cp = c & 0x1Fu;
      min_cp = 0x80u;
    } else if ((c & 0xF0u) == 0xE0u) {
      len = 3;
      cp = c & 0x0Fu;
      min_cp = 0x800u;
    } else if ((c & 0xF8u) == 0xF0u) {
      len = 4;
      cp = c & 0x07u;
      min_cp = 0x10000u;
    } else {
      return false;
    }
    if (i + len > bytes.size()) return false;
    for (std::size_t k = 1; k < len; ++k) {
      auto cont = static_cast<std::uint8_t>(bytes[i + k]);
      if ((cont & 0xC0u) != 0x80u) return false;
      cp = (cp << 6) | (cont & 0x3Fu);
    }
    if (cp < min_cp || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
      return false;
    }
    i += len;
  }
  return true;
}

TextEncoding detect_encoding(std::string_view bytes, TextEncoding hint) noexcept {
  if (has_utf8_bom(bytes)) {
    return TextEncoding::Utf8;
  }
  if (is_pure_ascii(bytes)) {
    return TextEncoding::Auto;
  }
  if (is_valid_utf8(bytes)) {
    return TextEncoding::Utf8;
  }

  bool sjis_ok = false, gbk_ok = false, big5_ok = false, cp949_ok = false;
  int sjis_score = score_shift_jis(bytes, sjis_ok);
  int gbk_score = score_gbk(bytes, gbk_ok);
  int big5_score = score_big5(bytes, big5_ok);
  int cp949_score = score_cp949(bytes, cp949_ok);
  int cp1252_score = score_cp1252(bytes);

  if (hint == TextEncoding::ShiftJis) sjis_score += 4;
  if (hint == TextEncoding::Gbk) gbk_score += 4;
  if (hint == TextEncoding::Big5) big5_score += 4;
  if (hint == TextEncoding::Cp949) cp949_score += 4;
  if (hint == TextEncoding::Cp1252) cp1252_score += 4;

  struct Candidate {
    TextEncoding enc;
    int score;
    bool valid;
  };
  std::array<Candidate, 5> candidates = {{
      {TextEncoding::ShiftJis, sjis_score, sjis_ok},
      {TextEncoding::Gbk, gbk_score, gbk_ok},
      {TextEncoding::Big5, big5_score, big5_ok},
      {TextEncoding::Cp949, cp949_score, cp949_ok},
      {TextEncoding::Cp1252, cp1252_score, true},
  }};

  const Candidate* best = &candidates[0];
  for (const auto& c : candidates) {
    if (c.valid != best->valid) {
      if (c.valid) best = &c;
    } else if (c.score > best->score) {
      best = &c;
    }
  }
  return best->enc;
}

std::string decode_to_utf8(
    std::string_view bytes,
    TextEncoding encoding,
    TextEncoding* out_detected) {
  TextEncoding resolved = encoding;
  if (resolved == TextEncoding::Auto) {
    resolved = detect_encoding(bytes);
  }
  if (out_detected) {
    *out_detected = resolved;
  }

  std::string_view src = strip_utf8_bom(bytes);
  if (resolved == TextEncoding::Auto || resolved == TextEncoding::Utf8) {
    if (is_valid_utf8(src)) {
      return std::string(src);
    }
    resolved = detect_encoding(src);
    if (out_detected) *out_detected = resolved;
  }

  std::string out;
  out.reserve(src.size() * 2);

  if (resolved == TextEncoding::Cp1252) {
    for (unsigned char ch : src) {
      if (ch < 0x80u) {
        out.push_back(static_cast<char>(ch));
      } else {
        append_utf8(out, detail::kCp1252Table[ch - 0x80u]);
      }
    }
    return out;
  }

  const std::uint16_t* table = detail::kCp932Table;
  switch (resolved) {
    case TextEncoding::ShiftJis: table = detail::kCp932Table; break;
    case TextEncoding::Gbk: table = detail::kCp936Table; break;
    case TextEncoding::Big5: table = detail::kCp950Table; break;
    case TextEncoding::Cp949: table = detail::kCp949Table; break;
    default: table = detail::kCp932Table; break;
  }

  for (std::size_t i = 0; i < src.size();) {
    auto b1 = static_cast<std::uint8_t>(src[i]);
    if (b1 < 0x80u) {
      out.push_back(static_cast<char>(b1));
      ++i;
      continue;
    }
    if (resolved == TextEncoding::ShiftJis && b1 >= 0xA1u && b1 <= 0xDFu) {
      // Half-width katakana U+FF61..U+FF9F
      append_utf8(out, 0xFF61u + (b1 - 0xA1u));
      ++i;
      continue;
    }
    if (resolved == TextEncoding::Gbk && b1 == 0x80u) {
      append_utf8(out, 0x20ACu);  // Euro sign
      ++i;
      continue;
    }
    if (i + 1 < src.size()) {
      auto b2 = static_cast<std::uint8_t>(src[i + 1]);
      std::uint16_t cp = lookup_2byte(table, b1, b2);
      if (cp != 0) {
        append_utf8(out, cp);
        i += 2;
        continue;
      }
    }
    append_utf8(out, 0xFFFDu);
    ++i;
  }

  return out;
}

}  // namespace nxrpg::encoding
