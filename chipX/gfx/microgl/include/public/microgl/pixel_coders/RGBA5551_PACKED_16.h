#pragma once

#include <microgl/PixelCoder.h>

namespace coder {


    class RGBA5551_PACKED_16 : public PixelCoder<uint16_t, RGBA5551_PACKED_16> {
    public:

        static
        inline void encode(const color_t &input, uint16_t &output) {
            output= ((input.r & 0x1F) << 11) + ((input.g & 0x1F) << 6) + ((input.b & 0x1F) << 1) + (input.a & 0x1);
        }

        static
        inline void decode(const uint16_t &input, color_t &output) {

            output.r = (input & 0b1111100000000000) >> 11;
            output.g = (input & 0b0000011111000000) >> 6;
            output.b = (input & 0b0000000000111110) >> 1;
            output.a = (input & 0b0000000000000001);

            update_channel_bit(output);
        };

        static
        channel red_bits() {
            return 5;
        }
        static
        channel green_bits() {
            return 5;
        }
        static
        channel blue_bits() {
            return 5;
        }
        static
        channel alpha_bits() {
            return 1;
        }

        static
        inline const char * format() {
            return "RGBA5551_PACKED_16";
        }

    };

}
