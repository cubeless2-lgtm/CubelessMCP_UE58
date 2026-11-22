# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# Modified by Epic Games, Inc. (2026): removed the native _motion_correction
# import; the C++ now lives in the UE Kimodo module.

# This package only carries the motion_postprocess shim; the correction itself is
# native UE C++ reached through the file exchange in KIMODO_CORRECTION_DIR.
