# Runs immediately after project(EasyRPG_Player) and devkitPro Platform/NintendoSwitch.cmake
# Ensures libbz2 is appended after Findfreetype's libfreetype.a.
set(CMAKE_CXX_STANDARD_LIBRARIES "-lbz2 -lnx -lm" CACHE STRING "" FORCE)
