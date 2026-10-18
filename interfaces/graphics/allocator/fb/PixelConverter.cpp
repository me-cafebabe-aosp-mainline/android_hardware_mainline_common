/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
#include "PixelConverter.h"

#include <linux/videodev2.h>

#include <cstring>
#include <utility>

namespace fb {
namespace {

constexpr uint32_t kStaticCubeLevels = 16;

struct Rgba {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
};

uint8_t Expand5(uint32_t value) {
    return static_cast<uint8_t>((value << 3) | (value >> 2));
}

uint8_t Expand6(uint32_t value) {
    return static_cast<uint8_t>((value << 2) | (value >> 4));
}

uint32_t ReverseBits(uint32_t value, uint32_t length) {
    uint32_t reversed = 0;
    for (uint32_t bit = 0; bit < length; ++bit) {
        reversed |= ((value >> bit) & 1U) << (length - bit - 1);
    }
    return reversed;
}

uint32_t Quantize(uint32_t value, uint32_t length) {
    const uint64_t maximum = length == 32 ? UINT32_MAX : (1ULL << length) - 1;
    return static_cast<uint32_t>((static_cast<uint64_t>(value) * maximum + 127) / 255);
}

uint32_t Channel(uint32_t value, const fb_bitfield& field) {
    if (field.length == 0) return 0;
    uint32_t quantized = Quantize(value, field.length);
    if (field.msb_right != 0) quantized = ReverseBits(quantized, field.length);
    return static_cast<uint32_t>(static_cast<uint64_t>(quantized) << field.offset);
}

constexpr fb_bitfield Field(uint32_t offset, uint32_t length) {
    return {.offset = offset, .length = length, .msb_right = 0};
}

// Expresses each whitelisted V4L2 packed RGB layout as little-endian bitfields
// plus constant padding bits.
bool FourccFields(uint32_t fourcc, fb_bitfield* red, fb_bitfield* green, fb_bitfield* blue,
                  fb_bitfield* alpha, uint32_t* constant) {
    *alpha = {};
    *constant = 0;
    switch (fourcc) {
        case V4L2_PIX_FMT_RGB565:
            *red = Field(11, 5);
            *green = Field(5, 6);
            *blue = Field(0, 5);
            return true;
        case V4L2_PIX_FMT_RGB24:
            *red = Field(0, 8);
            *green = Field(8, 8);
            *blue = Field(16, 8);
            return true;
        case V4L2_PIX_FMT_XRGB32:
        case V4L2_PIX_FMT_ARGB32:
            if (fourcc == V4L2_PIX_FMT_XRGB32) {
                *constant = UINT8_MAX;
            } else {
                *alpha = Field(0, 8);
            }
            *red = Field(8, 8);
            *green = Field(16, 8);
            *blue = Field(24, 8);
            return true;
        case V4L2_PIX_FMT_XBGR32:
        case V4L2_PIX_FMT_ABGR32:
            if (fourcc == V4L2_PIX_FMT_XBGR32) {
                *constant = UINT32_C(0xff) << 24;
            } else {
                *alpha = Field(24, 8);
            }
            *blue = Field(0, 8);
            *green = Field(8, 8);
            *red = Field(16, 8);
            return true;
        case V4L2_PIX_FMT_ARGB2101010:
            *alpha = Field(30, 2);
            *red = Field(20, 10);
            *green = Field(10, 10);
            *blue = Field(0, 10);
            return true;
        default:
            return false;
    }
}

template <SourceFormat kSource>
inline Rgba Fetch(const uint8_t* row, uint32_t x) {
    if constexpr (kSource == SourceFormat::kRgba8888) {
        const uint8_t* pixel = row + static_cast<size_t>(x) * 4;
        return {pixel[0], pixel[1], pixel[2], pixel[3]};
    } else if constexpr (kSource == SourceFormat::kRgbx8888) {
        const uint8_t* pixel = row + static_cast<size_t>(x) * 4;
        return {pixel[0], pixel[1], pixel[2], UINT8_MAX};
    } else if constexpr (kSource == SourceFormat::kBgra8888) {
        const uint8_t* pixel = row + static_cast<size_t>(x) * 4;
        return {pixel[2], pixel[1], pixel[0], pixel[3]};
    } else if constexpr (kSource == SourceFormat::kRgb888) {
        const uint8_t* pixel = row + static_cast<size_t>(x) * 3;
        return {pixel[0], pixel[1], pixel[2], UINT8_MAX};
    } else {
        uint16_t value;
        memcpy(&value, row + static_cast<size_t>(x) * 2, sizeof(value));
        return {Expand5((value >> 11) & 0x1f), Expand6((value >> 5) & 0x3f), Expand5(value & 0x1f),
                UINT8_MAX};
    }
}

}  // namespace

uint32_t SourceBytesPerPixel(SourceFormat format) {
    switch (format) {
        case SourceFormat::kRgba8888:
        case SourceFormat::kRgbx8888:
        case SourceFormat::kBgra8888:
            return 4;
        case SourceFormat::kRgb888:
            return 3;
        case SourceFormat::kRgb565:
            return 2;
    }
    return 0;
}

bool PixelConverter::Configure(const Config& config) {
    const uint32_t bpp = config.bits_per_pixel;
    const bool packed_luminance =
            config.output == Output::kMonochrome || config.output == Output::kGrayscale;
    const bool indexed =
            config.output == Output::kRgb332 || config.output == Output::kStaticPalette;
    if (bpp == 0 || bpp > 32 || (indexed && bpp != 8) ||
        (packed_luminance && bpp != 1 && bpp != 2 && bpp != 4 && bpp != 8 && bpp != 16)) {
        return false;
    }
    constant_ = 0;
    final_lookup_.clear();
    final_ = Final::kDirect;
    switch (config.output) {
        case Output::kFourcc: {
            fb_bitfield red;
            fb_bitfield green;
            fb_bitfield blue;
            fb_bitfield alpha;
            if (!FourccFields(config.fourcc, &red, &green, &blue, &alpha, &constant_)) return false;
            for (uint32_t i = 0; i < 256; ++i) {
                red_[i] = Channel(i, red);
                green_[i] = Channel(i, green);
                blue_[i] = Channel(i, blue);
                alpha_[i] = Channel(i, alpha);
            }
            break;
        }
        case Output::kTrueColor:
            for (uint32_t i = 0; i < 256; ++i) {
                red_[i] = Channel(i, config.red);
                green_[i] = Channel(i, config.green);
                blue_[i] = Channel(i, config.blue);
                alpha_[i] = Channel(i, config.transp);
            }
            break;
        case Output::kRgb332:
            for (uint32_t i = 0; i < 256; ++i) {
                red_[i] = i & 0xe0;
                green_[i] = (i >> 3) & 0x1c;
                blue_[i] = i >> 6;
                alpha_[i] = 0;
            }
            break;
        case Output::kStaticPalette:
            if (config.static_palette.size() !=
                kStaticCubeLevels * kStaticCubeLevels * kStaticCubeLevels) {
                return false;
            }
            for (uint32_t i = 0; i < 256; ++i) {
                const uint32_t level = (i * (kStaticCubeLevels - 1) + 127) / 255;
                red_[i] = level << 8;
                green_[i] = level << 4;
                blue_[i] = level;
                alpha_[i] = 0;
            }
            final_ = Final::kPalette;
            final_lookup_.assign(config.static_palette.begin(), config.static_palette.end());
            break;
        case Output::kMonochrome:
        case Output::kGrayscale:
            // Integer BT.601 luminance; the rounding term rides in the constant.
            for (uint32_t i = 0; i < 256; ++i) {
                red_[i] = 77U * i;
                green_[i] = 150U * i;
                blue_[i] = 29U * i;
                alpha_[i] = 0;
            }
            constant_ = 128;
            final_ = Final::kLuminance;
            final_lookup_.resize(256);
            for (uint32_t luminance = 0; luminance < 256; ++luminance) {
                uint32_t value;
                if (config.output == Output::kMonochrome) {
                    value = (luminance >= 128) == config.mono10 ? 1 : 0;
                } else {
                    value = (luminance * ((1U << bpp) - 1) + 127) / 255;
                    if (config.red.msb_right != 0) value = ReverseBits(value, bpp);
                }
                final_lookup_[luminance] = static_cast<uint16_t>(value);
            }
            break;
    }
    // Red/blue swapping exchanges source channels before conversion. Every term
    // is per channel, so exchanging the two tables is equivalent.
    if (config.swap_red_blue) std::swap(red_, blue_);
    if (bpp < 8 && 8 % bpp == 0 && packed_luminance) {
        store_ = Store::kSubByte;
    } else if (bpp == 8) {
        store_ = Store::kBytes1;
    } else if (bpp == 16) {
        store_ = Store::kBytes2;
    } else if (bpp == 24) {
        store_ = Store::kBytes3;
    } else if (bpp == 32) {
        store_ = Store::kBytes4;
    } else {
        store_ = Store::kBits;
    }
    bits_per_pixel_ = bpp;
    reverse_pixels_in_byte_ = config.reverse_pixels_in_byte;
    return true;
}

uint32_t PixelConverter::Pack(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) const {
    const uint32_t sum = red_[red] + green_[green] + blue_[blue] + alpha_[alpha] + constant_;
    switch (final_) {
        case Final::kDirect:
            return sum;
        case Final::kPalette:
            return final_lookup_[sum];
        case Final::kLuminance:
            return final_lookup_[sum >> 8];
    }
    return 0;
}

template <SourceFormat kSource, PixelConverter::Final kFinal, PixelConverter::Store kStore>
void PixelConverter::ConvertRows(const uint8_t* source, uint32_t source_stride,
                                 uint8_t* destination, uint32_t destination_stride,
                                 const DamageRect& rect) const {
    // Locals keep the compiler from reloading members after each byte store,
    // which may alias any object.
    const uint32_t* const red = red_.data();
    const uint32_t* const green = green_.data();
    const uint32_t* const blue = blue_.data();
    const uint32_t* const alpha = alpha_.data();
    const uint32_t constant = constant_;
    const uint16_t* const lookup = final_lookup_.data();
    const uint32_t bpp = bits_per_pixel_;
    const uint32_t pixels_per_byte = kStore == Store::kSubByte ? 8 / bpp : 1;
    const bool reverse_pixels = reverse_pixels_in_byte_;
    for (uint32_t y = rect.top; y < rect.bottom; ++y) {
        const uint8_t* source_row = source + static_cast<size_t>(y) * source_stride;
        uint8_t* row = destination + static_cast<size_t>(y) * destination_stride;
        for (uint32_t x = rect.left; x < rect.right; ++x) {
            const Rgba pixel = Fetch<kSource>(source_row, x);
            uint32_t value = red[pixel.red] + green[pixel.green] + blue[pixel.blue] +
                             alpha[pixel.alpha] + constant;
            if constexpr (kFinal == Final::kPalette) {
                value = lookup[value];
            } else if constexpr (kFinal == Final::kLuminance) {
                value = lookup[value >> 8];
            }
            if constexpr (kStore == Store::kSubByte) {
                uint32_t slot = x % pixels_per_byte;
                if (!reverse_pixels) slot = pixels_per_byte - slot - 1;
                const uint32_t shift = slot * bpp;
                const uint8_t mask = static_cast<uint8_t>(((1U << bpp) - 1) << shift);
                uint8_t& byte = row[x / pixels_per_byte];
                byte = static_cast<uint8_t>((byte & ~mask) | ((value << shift) & mask));
            } else if constexpr (kStore == Store::kBits) {
                const uint64_t first_bit = static_cast<uint64_t>(x) * bpp;
                for (uint32_t bit = 0; bit < bpp; ++bit) {
                    const uint64_t destination_bit = first_bit + bit;
                    uint8_t& byte = row[destination_bit / 8];
                    const uint8_t mask = static_cast<uint8_t>(1U << (destination_bit & 7U));
                    if (((value >> bit) & 1U) != 0) {
                        byte |= mask;
                    } else {
                        byte &= static_cast<uint8_t>(~mask);
                    }
                }
            } else {
                constexpr size_t kBytes = kStore == Store::kBytes1   ? 1
                                          : kStore == Store::kBytes2 ? 2
                                          : kStore == Store::kBytes3 ? 3
                                                                     : 4;
                memcpy(row + static_cast<size_t>(x) * kBytes, &value, kBytes);
            }
        }
    }
}

template <SourceFormat kSource, PixelConverter::Final kFinal>
void PixelConverter::DispatchStore(const uint8_t* source, uint32_t source_stride,
                                   uint8_t* destination, uint32_t destination_stride,
                                   const DamageRect& rect) const {
    // Configure() only pairs palette output with 8-bit stores and luminance
    // output with sub-byte, 8-bit, or 16-bit stores.
    if constexpr (kFinal == Final::kPalette) {
        ConvertRows<kSource, kFinal, Store::kBytes1>(source, source_stride, destination,
                                                     destination_stride, rect);
    } else if constexpr (kFinal == Final::kLuminance) {
        switch (store_) {
            case Store::kSubByte:
                return ConvertRows<kSource, kFinal, Store::kSubByte>(
                        source, source_stride, destination, destination_stride, rect);
            case Store::kBytes2:
                return ConvertRows<kSource, kFinal, Store::kBytes2>(
                        source, source_stride, destination, destination_stride, rect);
            default:
                return ConvertRows<kSource, kFinal, Store::kBytes1>(
                        source, source_stride, destination, destination_stride, rect);
        }
    } else {
        switch (store_) {
            case Store::kBytes1:
                return ConvertRows<kSource, kFinal, Store::kBytes1>(
                        source, source_stride, destination, destination_stride, rect);
            case Store::kBytes2:
                return ConvertRows<kSource, kFinal, Store::kBytes2>(
                        source, source_stride, destination, destination_stride, rect);
            case Store::kBytes3:
                return ConvertRows<kSource, kFinal, Store::kBytes3>(
                        source, source_stride, destination, destination_stride, rect);
            case Store::kBytes4:
                return ConvertRows<kSource, kFinal, Store::kBytes4>(
                        source, source_stride, destination, destination_stride, rect);
            default:
                return ConvertRows<kSource, kFinal, Store::kBits>(
                        source, source_stride, destination, destination_stride, rect);
        }
    }
}

template <SourceFormat kSource>
void PixelConverter::DispatchFinal(const uint8_t* source, uint32_t source_stride,
                                   uint8_t* destination, uint32_t destination_stride,
                                   const DamageRect& rect) const {
    switch (final_) {
        case Final::kDirect:
            return DispatchStore<kSource, Final::kDirect>(source, source_stride, destination,
                                                          destination_stride, rect);
        case Final::kPalette:
            return DispatchStore<kSource, Final::kPalette>(source, source_stride, destination,
                                                           destination_stride, rect);
        case Final::kLuminance:
            return DispatchStore<kSource, Final::kLuminance>(source, source_stride, destination,
                                                             destination_stride, rect);
    }
}

void PixelConverter::Convert(SourceFormat format, const uint8_t* source, uint32_t source_stride,
                             uint8_t* destination, uint32_t destination_stride,
                             const DamageRect& rect) const {
    switch (format) {
        case SourceFormat::kRgba8888:
            return DispatchFinal<SourceFormat::kRgba8888>(source, source_stride, destination,
                                                          destination_stride, rect);
        case SourceFormat::kRgbx8888:
            return DispatchFinal<SourceFormat::kRgbx8888>(source, source_stride, destination,
                                                          destination_stride, rect);
        case SourceFormat::kBgra8888:
            return DispatchFinal<SourceFormat::kBgra8888>(source, source_stride, destination,
                                                          destination_stride, rect);
        case SourceFormat::kRgb888:
            return DispatchFinal<SourceFormat::kRgb888>(source, source_stride, destination,
                                                        destination_stride, rect);
        case SourceFormat::kRgb565:
            return DispatchFinal<SourceFormat::kRgb565>(source, source_stride, destination,
                                                        destination_stride, rect);
    }
}

}  // namespace fb
