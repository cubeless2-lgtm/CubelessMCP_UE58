# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Modified by Epic Games, Inc. (2026)
# Modifications to upstream nv-tlabs/kimodo (baseline commit 301d3a1d0d2d):
#   1. Removed DEMO_ASSETS_ROOT, DEMO_EXAMPLES_ROOT and demo_asset_path, because the
#      demo asset tree they pointed at is not shipped with the plugin.
#   2. Removed SOMA_ASSETS_ROOT, which had no remaining callers.

from pathlib import Path

PACKAGE_ROOT = Path(__file__).resolve().parent
ASSETS_ROOT = PACKAGE_ROOT / "assets"
SKELETONS_ROOT = ASSETS_ROOT / "skeletons"


def skeleton_asset_path(*parts: str) -> Path:
    return SKELETONS_ROOT.joinpath(*parts)
