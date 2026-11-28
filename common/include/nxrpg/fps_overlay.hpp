#pragma once

#include <chrono>
#include <cstdio>

namespace nxrpg {

// Count presented frames and draw the label using the caller's pixel backend.
class FpsOverlay {
  using Clock = std::chrono::steady_clock;

 public:
  template <typename FillRect>
  void draw(int width, int height, FillRect fill, Clock::time_point now = Clock::now()) {
    constexpr int pixel = 3;
    constexpr int margin = 8;
    constexpr int boxWidth = 7 * 4 * pixel + 2 * pixel;
    constexpr int boxHeight = 7 * pixel;
    if (width < boxWidth + 2 * margin || height < boxHeight + 2 * margin) return;

    if (since_ == Clock::time_point{}) since_ = now;
    else ++frames_;
    const double seconds = std::chrono::duration<double>(now - since_).count();
    if (seconds >= 0.5) {
      fps_ = static_cast<unsigned>(frames_ / seconds + 0.5);
      since_ = now;
      frames_ = 0;
    }

    char text[8] = "FPS ---";
    if (fps_ <= 999) std::snprintf(text, sizeof text, "FPS %3u", fps_);
    const int x = width - margin - boxWidth;
    fill(x, margin, boxWidth, boxHeight, false);
    for (int i = 0; text[i]; ++i) {
      const auto* rows = glyph(text[i]);
      for (int y = 0; y < 5; ++y) {
        int col = 0;
        while (col < 3) {
          const int start = col++;
          if (!(rows[y] & (4 >> start))) continue;
          while (col < 3 && (rows[y] & (4 >> col))) ++col;
          fill(x + (1 + i * 4 + start) * pixel, margin + (1 + y) * pixel,
               (col - start) * pixel, pixel, true);
        }
      }
    }
  }

 private:
  Clock::time_point since_{};
  unsigned frames_ = 0;
  unsigned fps_ = 1000;

  static const unsigned char* glyph(char ch) {
    static constexpr unsigned char digits[10][5] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
        {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2},
        {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
    };
    static constexpr unsigned char f[] = {7, 4, 6, 4, 4};
    static constexpr unsigned char p[] = {7, 5, 7, 4, 4};
    static constexpr unsigned char s[] = {7, 4, 7, 1, 7};
    static constexpr unsigned char dash[] = {0, 0, 7, 0, 0};
    static constexpr unsigned char blank[] = {0, 0, 0, 0, 0};
    if (ch >= '0' && ch <= '9') return digits[ch - '0'];
    switch (ch) {
      case 'F': return f;
      case 'P': return p;
      case 'S': return s;
      case '-': return dash;
      default: return blank;
    }
  }
};

}  // namespace nxrpg
