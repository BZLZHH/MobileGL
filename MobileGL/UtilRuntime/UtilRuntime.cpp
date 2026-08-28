// MobileGL - MobileGL/UtilRuntime/UtilRuntime.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include <Includes.h>
#include "Protocol/mgruntime_api.h"
#include "MG_Util/Math/HalfFloat.h"

namespace {
    float Math_HalfFloatToFloat(uint16_t value) {
        return MobileGL::MG_Util::DecodeHalfBitsToFloat(value);
    }

    uint16_t Math_FloatToHalfFloat(float value) {
        return MobileGL::MG_Util::EncodeFloatToHalfBits(value);
    }

    uint16_t Math_FixedToHalfFloat(int32_t value) {
        return MobileGL::MG_Util::EncodeFloatToHalfBits(static_cast<float>(value));
    }

    int32_t Math_HalfFloatToFixed(uint16_t value) {
        return static_cast<int32_t>(MobileGL::MG_Util::DecodeHalfBitsToFloat(value));
    }

    const MobileGLMathApi g_mathApi = {
        sizeof(MobileGLMathApi),
        (MOBILEGL_RUNTIME_ABI_MAJOR << 16) | MOBILEGL_RUNTIME_ABI_MINOR,
        &Math_HalfFloatToFloat,
        &Math_FloatToHalfFloat,
        &Math_FixedToHalfFloat,
        &Math_HalfFloatToFixed
    };

    // Phase 0/1 skeleton: root table is allocated and the Math domain is live;
    // the remaining domain vtables are filled in by the C ABI adapter work
    // (Phase 1).
    const MobileGLUtilApi g_utilApi = {
        sizeof(MobileGLUtilApi),
        MOBILEGL_RUNTIME_ABI_MAJOR,
        MOBILEGL_RUNTIME_ABI_MINOR,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &g_mathApi,
        nullptr,
        nullptr
    };
} // namespace

extern "C" const MobileGLUtilApi* mobilegl_util_api(uint32_t requestedStructSize,
                                                    uint32_t requestedAbiMajor) {
    if (requestedStructSize < sizeof(MobileGLUtilApi) ||
        requestedAbiMajor != MOBILEGL_RUNTIME_ABI_MAJOR) {
        return nullptr;
    }
    return &g_utilApi;
}

// End of File
