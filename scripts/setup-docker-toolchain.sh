#!/usr/bin/env bash
set -euo pipefail

# Builds/refreshes a persistent local Docker image `nxrpg-devkita64:latest`
# on top of `devkitpro/devkita64:latest` with all missing Switch portlibs
# (fmt, inih, pixman, nlohmann_json, icu, liblcf), Borealis, Meson, Node, and pnpm.

IMAGE_OUT="${NXRPG_DOCKER_IMAGE:-nxrpg-devkita64:latest}"
BASE_IMAGE="${NXRPG_DOCKER_BASE:-devkitpro/devkita64:latest}"
CONTAINER_NAME="nxrpg-toolchain-build-$$"
DNS_ARG="${NXRPG_DOCKER_DNS:-223.5.5.5}"

cleanup() {
  docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo ">>> Starting builder container ($CONTAINER_NAME) from $BASE_IMAGE..."
docker run -d --name "$CONTAINER_NAME" --dns "$DNS_ARG" "$BASE_IMAGE" sleep 3600 >/dev/null

docker exec "$CONTAINER_NAME" bash -lc '
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
export DEVKITPRO=/opt/devkitpro
export PATH="$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH"
PREFIX="$DEVKITPRO/portlibs/switch"
TARGET_HOST="aarch64-none-elf"
NPROC="$(nproc)"

echo ">>> [1/6] Installing host build tools (meson, autotools, node 22, pnpm)..."
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
  autoconf automake libtool pkg-config perl python3 python3-pip python3-setuptools \
  bison ninja-build curl ca-certificates unzip xz-utils bzip2 ruby-full >/dev/null

# Provide lowercase pkg-config aliases expected by mkxp-z Meson wrap/dependency lookups
ln -sf SDL2_image.pc "$PREFIX/lib/pkgconfig/sdl2_image.pc"
ln -sf SDL2_ttf.pc "$PREFIX/lib/pkgconfig/sdl2_ttf.pc"
ln -sf glesv2.pc "$PREFIX/lib/pkgconfig/gl.pc"

pip3 install --no-cache-dir --break-system-packages "meson>=1.5.0" >/dev/null
hash -r

if ! command -v node >/dev/null 2>&1 || [[ "$(node -v | cut -d. -f1 | tr -d v)" -lt 20 ]]; then
  curl -fsSL https://deb.nodesource.com/setup_22.x | bash - >/dev/null
  apt-get install -y -qq nodejs >/dev/null
fi
npm install -g pnpm >/dev/null 2>&1

mkdir -p /tmp/deps /opt/nxrpg
cd /tmp/deps

ARCH_FLAGS="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec"
CROSS_CFLAGS="-O2 -g0 $ARCH_FLAGS -ffunction-sections -fdata-sections -D__SWITCH__ -I$PREFIX/include -isystem $DEVKITPRO/libnx/include"
CROSS_LDFLAGS="$ARCH_FLAGS -L$PREFIX/lib -L$DEVKITPRO/libnx/lib"

cat > /tmp/deps/meson-switch.txt <<EOF
[binaries]
c = '\''aarch64-none-elf-gcc'\''
cpp = '\''aarch64-none-elf-g++'\''
ar = '\''aarch64-none-elf-gcc-ar'\''
strip = '\''aarch64-none-elf-strip'\''
pkg-config = '\''aarch64-none-elf-pkg-config'\''

[built-in options]
c_args = ['\''-O2'\'', '\''-g0'\'', '\''-march=armv8-a+crc+crypto'\'', '\''-mtune=cortex-a57'\'', '\''-mtp=soft'\'', '\''-fPIC'\'', '\''-ftls-model=local-exec'\'', '\''-ffunction-sections'\'', '\''-fdata-sections'\'', '\''-D__SWITCH__'\'', '\''-I/opt/devkitpro/portlibs/switch/include'\'', '\''-isystem'\'', '\''/opt/devkitpro/libnx/include'\'']
cpp_args = ['\''-O2'\'', '\''-g0'\'', '\''-march=armv8-a+crc+crypto'\'', '\''-mtune=cortex-a57'\'', '\''-mtp=soft'\'', '\''-fPIC'\'', '\''-ftls-model=local-exec'\'', '\''-ffunction-sections'\'', '\''-fdata-sections'\'', '\''-D__SWITCH__'\'', '\''-I/opt/devkitpro/portlibs/switch/include'\'', '\''-isystem'\'', '\''/opt/devkitpro/libnx/include'\'']
c_link_args = ['\''-march=armv8-a+crc+crypto'\'', '\''-mtune=cortex-a57'\'', '\''-L/opt/devkitpro/portlibs/switch/lib'\'', '\''-L/opt/devkitpro/libnx/lib'\'']
cpp_link_args = ['\''-march=armv8-a+crc+crypto'\'', '\''-mtune=cortex-a57'\'', '\''-L/opt/devkitpro/portlibs/switch/lib'\'', '\''-L/opt/devkitpro/libnx/lib'\'']

[host_machine]
system = '\''horizon'\''
cpu_family = '\''aarch64'\''
cpu = '\''cortex-a57'\''
endian = '\''little'\''
EOF

echo ">>> [2/6] Building fmt + nlohmann_json + inih + pixman for Switch..."
curl -fsSLO https://github.com/fmtlib/fmt/releases/download/12.1.0/fmt-12.1.0.zip
unzip -q fmt-12.1.0.zip
cmake -S fmt-12.1.0 -B fmt-build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_SHARED_LIBS=OFF -DFMT_DOC=OFF -DFMT_TEST=OFF
cmake --build fmt-build -j"$NPROC"
cmake --install fmt-build

curl -fsSLO https://github.com/nlohmann/json/archive/v3.12.0.tar.gz
tar xf v3.12.0.tar.gz
perl -pi -e "s/CMAKE_INSTALL_DATADIR/CMAKE_INSTALL_LIBDIR/" json-3.12.0/CMakeLists.txt
cmake -S json-3.12.0 -B json-build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DJSON_BuildTests=OFF
cmake --build json-build -j"$NPROC"
cmake --install json-build

curl -fsSLO https://github.com/benhoyt/inih/archive/refs/tags/r62.tar.gz
tar xf r62.tar.gz
PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" meson setup inih-build inih-r62 \
  --cross-file /tmp/deps/meson-switch.txt \
  --prefix="$PREFIX" --libdir=lib --buildtype=release -Ddefault_library=static
meson compile -C inih-build -j"$NPROC"
meson install -C inih-build

curl -fsSLO https://cairographics.org/releases/pixman-0.46.4.tar.gz
tar xf pixman-0.46.4.tar.gz
PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig" meson setup pixman-build pixman-0.46.4 \
  --cross-file /tmp/deps/meson-switch.txt \
  --prefix="$PREFIX" --libdir=lib --buildtype=release -Ddefault_library=static \
  -Dtests=disabled -Ddemos=disabled -Dlibpng=disabled
meson compile -C pixman-build -j"$NPROC"
meson install -C pixman-build

echo ">>> [3/6] Building ICU 78.3 (native tools + Switch static libs)..."
curl -fsSLO https://github.com/unicode-org/icu/releases/download/release-78.3/icu4c-78.3-sources.tgz
tar xf icu4c-78.3-sources.tgz
if curl -fsSLO https://ci.easyrpg.org/job/icudata/lastSuccessfulBuild/artifact/icudata78_all.tar.gz 2>/dev/null; then
  tar xf icudata78_all.tar.gz
  cp -f icudt*.dat icu/source/data/in/
fi
chmod +x icu/source/configure
cp icu/source/config/mh-linux icu/source/config/mh-unknown
perl -pi -e "s/SMALL_BUFFER_MAX_SIZE 512/SMALL_BUFFER_MAX_SIZE 2048/" icu/source/tools/toolutil/pkg_genc.h
perl -pi -e "s/LIBCPPFLAGS =/LIBCPPFLAGS = -DUCONFIG_ONLY_COLLATION/" icu/source/config/mh-unknown

ICU_ARGS="--enable-strict=no --disable-tests --disable-samples --disable-dyload --disable-extras --disable-icuio --with-data-packaging=static --disable-layout --disable-layoutex --enable-draft=no"

mkdir -p icu-native icu-cross
(cd icu-native && ../icu/source/configure --enable-static --disable-shared $ICU_ARGS >/dev/null && make -j"$NPROC" >/dev/null)

(cd icu-cross && \
  CC="${TARGET_HOST}-gcc" CXX="${TARGET_HOST}-g++" \
  CFLAGS="$CROSS_CFLAGS" CXXFLAGS="$CROSS_CFLAGS" \
  LDFLAGS="$CROSS_LDFLAGS" LIBS="-lnx" \
  ../icu/source/configure --enable-static --disable-shared --prefix="$PREFIX" \
    --host="$TARGET_HOST" --with-cross-build=/tmp/deps/icu-native --disable-tools $ICU_ARGS >/dev/null && \
  make -j"$NPROC" >/dev/null && \
  make install >/dev/null)

echo ">>> [4/6] Building liblcf for Switch..."
git clone --depth=1 https://github.com/EasyRPG/liblcf.git /tmp/deps/liblcf
cmake -S /tmp/deps/liblcf -B /tmp/deps/liblcf-build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_SHARED_LIBS=OFF \
  -DLIBLCF_UPDATE_MIMEDB=OFF \
  -DLIBLCF_ENABLE_TOOLS=OFF \
  -DLIBLCF_ENABLE_TESTS=OFF
cmake --build /tmp/deps/liblcf-build -j"$NPROC"
cmake --install /tmp/deps/liblcf-build

echo ">>> [5/6] Cloning xfangfang/borealis (--depth=1 --shallow-submodules) to /opt/nxrpg/borealis..."
GIT_HTTP_VERSION=HTTP/1.1 git clone --depth=1 --recursive --shallow-submodules https://github.com/xfangfang/borealis.git /opt/nxrpg/borealis

echo ">>> [6/6] Cleaning temporary build directories..."
rm -rf /tmp/deps /var/lib/apt/lists/*
'

echo ">>> Committing persistent toolchain image $IMAGE_OUT..."
docker commit "$CONTAINER_NAME" "$IMAGE_OUT" >/dev/null
echo "Done! Built image: $IMAGE_OUT"
