#pragma once

#include <string>
#include <string_view>

#include "nxrpg/types.hpp"

namespace nxrpg::encoding {

/// Returns true if `bytes` is strictly valid UTF-8 (ignoring an optional UTF-8 BOM).
[[nodiscard]] bool is_valid_utf8(std::string_view bytes) noexcept;

/// Returns true if `bytes` contains only 7-bit ASCII characters.
[[nodiscard]] bool is_pure_ascii(std::string_view bytes) noexcept;

/// Detects legacy encoding for INI / LDB metadata strings.
/// Returns `TextEncoding::Utf8` if valid multi-byte UTF-8 (or BOM),
/// `TextEncoding::Auto` if pure 7-bit ASCII, or the highest-scoring legacy CJK/Western codepage.
[[nodiscard]] TextEncoding detect_encoding(
    std::string_view bytes,
    TextEncoding hint = TextEncoding::Auto) noexcept;

/// Converts `bytes` encoded in `encoding` (or auto-detected when `Auto`) to UTF-8.
[[nodiscard]] std::string decode_to_utf8(
    std::string_view bytes,
    TextEncoding encoding = TextEncoding::Auto,
    TextEncoding* out_detected = nullptr);

}  // namespace nxrpg::encoding
