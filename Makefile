.PHONY: help check-env bootstrap-cores clean

help:
	@echo "nx.rpg — Nintendo Switch Multi-Engine RPG Maker Runner (Planning Phase)"
	@echo ""
	@echo "Available targets:"
	@echo "  make check-env        Check devkitPro / CMake / Meson / Node toolchain"
	@echo "  make bootstrap-cores  Clone upstream EasyRPG, nx.js, and mkxp-z into third_party/"
	@echo "  make clean            Remove local build/ and dist/ artifacts"
	@echo ""
	@echo "Planned targets (Phase 1+):"
	@echo "  make launcher         Build nx.rpg.nro frontend"
	@echo "  make core-easyrpg     Build cores/easyrpg.nro (RM2000 / RM2003)"
	@echo "  make core-nxjs-mvz    Build cores/nxjs-mvz.nro (RM MV / MZ)"
	@echo "  make core-mkxp-z      Build cores/mkxp-z.nro (RM XP / VX / VX Ace)"
	@echo "  make sd-pack          Assemble sdmc:/switch/nx.rpg/ release package"

check-env:
	@bash scripts/check-env.sh

bootstrap-cores:
	@bash scripts/bootstrap-cores.sh

clean:
	rm -rf build dist
