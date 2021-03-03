#pragma once

#include <microgl/pixel_coder.h>

namespace microgl {
    namespace coder {

        class RGB555_PACKED_16 : public pixel_coder<uint16_t, rgba_t<5,5,5,0>, RGB555_PACKED_16> {
        public:
            using pixel_coder::decode;
            using pixel_coder::encode;

            inline void encode(const color_t &input, uint16_t &output) {
                output = ((input.r & 0b00011111) << 10) + ((input.g & 0b00011111) << 5) + ((input.b & 0b00011111));
            }

            inline void decode(const uint16_t &input, color_t &output) {
                output.r = (input & 0b0111110000000000) >> 10;
                output.g = (input & 0b0000001111100000) >> 5;
                output.b = (input & 0b0000000000011111);
            };

        };
    }
}