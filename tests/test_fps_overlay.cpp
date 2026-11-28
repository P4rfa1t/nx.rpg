#include "nxrpg/fps_overlay.hpp"
#include "test_common.hpp"

#include <array>
#include <chrono>

int main() {
  using namespace std::chrono_literals;
  using Clock = std::chrono::steady_clock;
  constexpr int width = 160;
  constexpr int height = 48;
  std::array<std::array<bool, width>, height> pixels{};
  auto fill = [&](int x, int y, int w, int h, bool white) {
    NXRPG_CHECK(x >= 0 && y >= 0 && x + w <= width && y + h <= height);
    for (int row = y; row < y + h; ++row) {
      for (int col = x; col < x + w; ++col) pixels[row][col] = white;
    }
  };

  nxrpg::FpsOverlay overlay;
  const auto start = Clock::time_point{} + 1s;
  overlay.draw(width, height, fill, start);
  const auto initial = pixels;
  NXRPG_CHECK(initial[11][65]);  // F in the upper-right HUD
  for (int frame = 1; frame <= 30; ++frame) {
    overlay.draw(width, height, fill, start + 500ms * frame / 30);
  }
  NXRPG_CHECK(pixels != initial);
  NXRPG_CHECK(pixels[14][125] && !pixels[14][131]);  // left stroke of 6
  NXRPG_CHECK(pixels[14][137] && pixels[14][143]);   // both sides of 0
  return nxrpg::test::finish();
}
