#pragma once

#include <microgl/pixel_coder.h>

namespace microgl {
    namespace coder {

        class RGBA8888_PACKED_32 : public pixel_coder<uint32_t, rgba_t<8,8,8,8>, RGBA8888_PACKED_32> {
        public:
            using pixel_coder::decode;
            using pixel_coder::encode;

            inline void encode(const color_t &input, uint32_t &output) const {
                output = (input.r << 24) + (input.g << 16) + (input.b << 8) + input.a;
            }

            inline void decode(const uint32_t &input, color_t &output) const {
                output.r = (input & 0xFF000000) >> 24;
                output.g = (input & 0x00FF0000) >> 16;
                output.b = (input & 0x0000FF00) >> 8;
                output.a = (input & 0x000000FF);
            };

        };
    }
}