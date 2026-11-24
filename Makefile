.PHONY: help validate check-env bootstrap-cores apply-patches clean

PYTHON ?= $(shell command -v python3 2>/dev/null || command -v python 2>/dev/null)

help:
	@echo "nx.rpg — Nintendo Switch Multi-Engine RPG Maker Runner"
	@echo ""
	@echo "Available targets (Phase 0):"
	@echo "  make validate         Validate JSON Schemas, example configs, lockfile, SERIES, and scripts"
	@echo "  make check-env        Check devkitPro / CMake / Meson / Node / pnpm toolchain"
	@echo "  make bootstrap-cores  Fetch upstream EasyRPG, nx.js, and mkxp-z at locked commits"
	@echo "  make apply-patches    Dry-run check patch queues in cores/*/patches/SERIES"
	@echo "  make clean            Remove local build/ and dist/ artifacts"
	@echo ""
	@echo "Planned build targets (Phase 1 ~ Phase 3):"
	@echo "  make test-host        Build & run host C++20 unit tests under tests/"
	@echo "  make launcher         Build nx.rpg.nro (C++20 + Borealis frontend)"
	@echo "  make core-easyrpg     Build cores/easyrpg.nro (RM2000 / RM2003)"
	@echo "  make core-nxjs-mvz    Build cores/nxjs-mvz.nro (RM MV / MZ, --fat)"
	@echo "  make core-mkxp-z      Build cores/mkxp-z.nro (RM XP / VX / VX Ace)"
	@echo "  make sd-pack          Assemble sdmc:/switch/nx.rpg/ release zip + GPL source bundle"

validate:
	@"$(PYTHON)" scripts/validate-repo.py
	@bash -n scripts/check-env.sh
	@bash -n scripts/bootstrap-cores.sh
	@bash -n scripts/apply-patches.sh

check-env:
	@bash scripts/check-env.sh

bootstrap-cores:
	@bash scripts/bootstrap-cores.sh

apply-patches:
	@bash scripts/apply-patches.sh --check all

clean:
	rm -rf build dist out
