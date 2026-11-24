# frozen_string_literal: false
# cores/mkxp-z/preload/01_nxrpg_win32_ext.rb
#
# Loaded AFTER upstream `scripts/preload/win32_wrap.rb` when
# `manifest.coreOptions.mkxp.enableWin32ApiStubs == true`.
#
# Upstream `win32_wrap.rb` already implements `User32`:
#   Keybd_event, GetKeyState, GetAsyncKeyState, GetKeyboardState,
#   ShowCursor, GetCursorPos, GetClientRect, ScreenToClient, FindWindowA.
#
# This Switch extension module adds frequently used `Kernel32` & `User32` stubs
# found in Japanese/Chinese RGSS1/2/3 doujin games:
#   - Kernel32::GetPrivateProfileStringA / GetPrivateProfileIntA
#   - Kernel32::WritePrivateProfileStringA (redirected to `NXRPG_SAVE_DIR`)
#   - User32::GetSystemMetrics (SM_CXSCREEN=1280, SM_CYSCREEN=720)
#   - User32::MessageBoxA / MessageBoxW (logs to `last_run.log` and returns IDOK=1)
