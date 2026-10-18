/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <linux/fb.h>

#include <array>
#include <cstdint>
#include <vector>

namespace fb {

struct DamageRect {
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
};

enum class SourceFormat { kRgba8888, kRgbx8888, kBgra8888, kRgb888, kRgb565 };

uint32_t SourceBytesPerPixel(SourceFormat format);

// Converts packed RGB client pixels to one validated fbdev pixel layout. Every
// supported output is a sum of independent per-channel terms followed by an
// optional final lookup, so the per-pixel work is a few table reads.
class PixelConverter {
  public:
    enum class Output { kTrueColor, kFourcc, kRgb332, kStaticPalette, kMonochrome, kGrayscale };

    struct Config {
        Output output = Output::kTrueColor;
        uint32_t bits_per_pixel = 0;
        fb_bitfield red{};
        fb_bitfield green{};
        fb_bitfield blue{};
        fb_bitfield transp{};
        uint32_t fourcc = 0;
        bool mono10 = false;
        bool reverse_pixels_in_byte = false;
        bool swap_red_blue = false;
        // 16x16x16 nearest-color cube for kStaticPalette.
        std::vector<uint8_t> static_palette;
    };

    bool Configure(const Config& config);

    // Converts the rectangle from source rows to destination rows. Both buffers
    // must cover the rectangle; the caller validates bounds.
    void Convert(SourceFormat format, const uint8_t* source, uint32_t source_stride,
                 uint8_t* destination, uint32_t destination_stride, const DamageRect& rect) const;

    uint32_t Pack(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) const;

    enum class Final { kDirect, kPalette, kLuminance };
    enum class Store { kSubByte, kBytes1, kBytes2, kBytes3, kBytes4, kBits };

  private:
    using Table = std::array<uint32_t, 256>;

    template <SourceFormat kSource, Final kFinal, Store kStore>
    void ConvertRows(const uint8_t* source, uint32_t source_stride, uint8_t* destination,
                     uint32_t destination_stride, const DamageRect& rect) const;
    template <SourceFormat kSource>
    void DispatchFinal(const uint8_t* source, uint32_t source_stride, uint8_t* destination,
                       uint32_t destination_stride, const DamageRect& rect) const;
    template <SourceFormat kSource, Final kFinal>
    void DispatchStore(const uint8_t* source, uint32_t source_stride, uint8_t* destination,
                       uint32_t destination_stride, const DamageRect& rect) const;

    Table red_{};
    Table green_{};
    Table blue_{};
    Table alpha_{};
    uint32_t constant_ = 0;
    Final final_ = Final::kDirect;
    Store store_ = Store::kBytes4;
    std::vector<uint16_t> final_lookup_;
    uint32_t bits_per_pixel_ = 32;
    bool reverse_pixels_in_byte_ = false;
};

}  // namespace fb
