/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsp/OutputProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aidl::android::hardware::audio::core::mainline::dsp {

namespace {

constexpr float kMinus3Db = 0.70710678f;
// Bass of every main speaker summed into the subwoofer, kept below clipping
// for correlated (mostly mono) bass.
constexpr float kBassSumGain = 0.5f;
// The test tone's level: pink noise at about -20 dBFS.
constexpr float kToneGain = 0.1f;

bool IsSupported(snd_pcm_format_t format) {
    switch (format) {
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_FLOAT_LE:
            return true;
        default:
            return false;
    }
}

size_t SampleBytes(snd_pcm_format_t format) {
    return static_cast<size_t>(snd_pcm_format_physical_width(format) / 8);
}

float ReadSample(const uint8_t* p, snd_pcm_format_t format) {
    switch (format) {
        case SND_PCM_FORMAT_S16_LE: {
            int16_t v;
            std::memcpy(&v, p, sizeof(v));
            return v / 32768.0f;
        }
        case SND_PCM_FORMAT_S24_3LE: {
            const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(p[2]) << 24 |
                                                   static_cast<uint32_t>(p[1]) << 16 |
                                                   static_cast<uint32_t>(p[0]) << 8) >>
                              8;
            return v / 8388608.0f;
        }
        case SND_PCM_FORMAT_S24_LE: {
            int32_t v;
            std::memcpy(&v, p, sizeof(v));
            v = static_cast<int32_t>(static_cast<uint32_t>(v) << 8) >> 8;
            return v / 8388608.0f;
        }
        case SND_PCM_FORMAT_S32_LE: {
            int32_t v;
            std::memcpy(&v, p, sizeof(v));
            return static_cast<float>(v / 2147483648.0);
        }
        case SND_PCM_FORMAT_FLOAT_LE: {
            float v;
            std::memcpy(&v, p, sizeof(v));
            return v;
        }
        default:
            return 0.0f;
    }
}

void WriteSample(float value, uint8_t* p, snd_pcm_format_t format) {
    const double v = std::clamp(static_cast<double>(value), -1.0, 1.0);
    switch (format) {
        case SND_PCM_FORMAT_S16_LE: {
            const int16_t s =
                    static_cast<int16_t>(std::clamp(std::lround(v * 32768.0), -32768L, 32767L));
            std::memcpy(p, &s, sizeof(s));
            break;
        }
        case SND_PCM_FORMAT_S24_3LE: {
            const int32_t s = static_cast<int32_t>(
                    std::clamp(std::lround(v * 8388608.0), -8388608L, 8388607L));
            p[0] = static_cast<uint8_t>(s & 0xff);
            p[1] = static_cast<uint8_t>((s >> 8) & 0xff);
            p[2] = static_cast<uint8_t>((s >> 16) & 0xff);
            break;
        }
        case SND_PCM_FORMAT_S24_LE: {
            const int32_t s = static_cast<int32_t>(
                    std::clamp(std::lround(v * 8388608.0), -8388608L, 8388607L));
            std::memcpy(p, &s, sizeof(s));
            break;
        }
        case SND_PCM_FORMAT_S32_LE: {
            const int32_t s = static_cast<int32_t>(
                    std::clamp(std::llround(v * 2147483648.0), -2147483648LL, 2147483647LL));
            std::memcpy(p, &s, sizeof(s));
            break;
        }
        case SND_PCM_FORMAT_FLOAT_LE: {
            const float f = static_cast<float>(v);
            std::memcpy(p, &f, sizeof(f));
            break;
        }
        default:
            break;
    }
}

// Position of a role in ALSA's channel order.
int AlsaRank(Role role) {
    switch (role) {
        case Role::kFrontLeft:
            return 0;
        case Role::kFrontRight:
            return 1;
        case Role::kRearLeft:
            return 2;
        case Role::kRearRight:
            return 3;
        case Role::kCenter:
            return 4;
        case Role::kLfe:
            return 5;
        case Role::kSideLeft:
            return 6;
        case Role::kSideRight:
            return 7;
        case Role::kOther:
            return 8;
    }
    return 8;
}

}  // namespace

std::unique_ptr<OutputProcessor> OutputProcessor::Create(const OutputConfig& config,
                                                         std::vector<Role> input_roles,
                                                         snd_pcm_format_t format,
                                                         unsigned int rate) {
    if (!IsSupported(format) || input_roles.empty() || input_roles.size() > kMaxChannels ||
        rate == 0) {
        return nullptr;
    }
    std::vector<Role> output_roles;
    if (config.layout == Layout::kAuto) {
        // The stream's own channels, in the order the hardware expects them.
        output_roles = input_roles;
        std::stable_sort(output_roles.begin(), output_roles.end(),
                         [](Role a, Role b) { return AlsaRank(a) < AlsaRank(b); });
    } else {
        output_roles = LayoutRoles(config.layout);
    }
    return std::unique_ptr<OutputProcessor>(new OutputProcessor(
            config, std::move(input_roles), std::move(output_roles), format, rate));
}

OutputProcessor::OutputProcessor(const OutputConfig& config, std::vector<Role> input_roles,
                                 std::vector<Role> output_roles, snd_pcm_format_t format,
                                 unsigned int rate)
    : config_(config),
      input_roles_(std::move(input_roles)),
      output_roles_(std::move(output_roles)),
      format_(format),
      rate_(rate) {
    BuildMatrix();

    const bool explicit_layout = config_.layout != Layout::kAuto;
    lfe_ = config_.subwoofer ? IndexOf(Role::kLfe) : -1;
    const bool stream_has_lfe =
            std::find(input_roles_.begin(), input_roles_.end(), Role::kLfe) != input_roles_.end();
    bass_to_sub_ = explicit_layout && lfe_ >= 0 &&
                   (config_.small_speakers || (config_.fill && !stream_has_lfe));
    for (size_t o = 0; o < output_roles_.size(); ++o) {
        if (static_cast<int>(o) != lfe_ && output_roles_[o] != Role::kLfe) {
            mains_.push_back(static_cast<int>(o));
        }
    }
    if (bass_to_sub_) {
        low_pass_ = LinkwitzRiley4(config_.crossover_hz, rate_, true);
        if (config_.small_speakers) {
            high_passes_.assign(mains_.size(), LinkwitzRiley4(config_.crossover_hz, rate_, false));
        }
    }

    const size_t outputs = output_roles_.size();
    gains_.assign(outputs, 1.0f);
    delays_.assign(outputs, 0);
    delay_lines_.resize(outputs);
    delay_positions_.assign(outputs, 0);
    if (explicit_layout) {
        for (size_t o = 0; o < outputs; ++o) {
            gains_[o] = std::pow(10.0f, config_.levels_db[o] / 20.0f);
            delays_[o] = static_cast<size_t>(std::lround(config_.delays_ms[o] * rate_ / 1000.0f));
            delay_lines_[o].assign(delays_[o] + 1, 0.0f);
        }
    }
    in_.resize(input_roles_.size());
    out_.resize(outputs);
}

int OutputProcessor::IndexOf(Role role) const {
    const auto it = std::find(output_roles_.begin(), output_roles_.end(), role);
    return it != output_roles_.end() ? static_cast<int>(it - output_roles_.begin()) : -1;
}

void OutputProcessor::BuildMatrix() {
    const size_t outputs = output_roles_.size();
    const size_t inputs = input_roles_.size();
    matrix_.assign(outputs, std::vector<float>(inputs, 0.0f));

    if (config_.layout == Layout::kAuto) {
        // The outputs are the inputs in another order: a plain permutation.
        std::vector<bool> used(inputs, false);
        for (size_t o = 0; o < outputs; ++o) {
            for (size_t i = 0; i < inputs; ++i) {
                if (!used[i] && input_roles_[i] == output_roles_[o]) {
                    matrix_[o][i] = 1.0f;
                    used[i] = true;
                    break;
                }
            }
        }
        return;
    }

    // A mono stream is a centre channel, whatever its mask calls it.
    std::vector<Role> roles = input_roles_;
    if (roles.size() == 1) roles[0] = Role::kCenter;

    // The output roles that exist; LFE only with a subwoofer behind it.
    auto output_of = [&](Role role) -> int {
        if (role == Role::kLfe && !config_.subwoofer) return -1;
        return IndexOf(role);
    };
    auto add = [&](int output, size_t input, float gain) {
        if (output >= 0) matrix_[output][input] += gain;
    };
    const int fl = output_of(Role::kFrontLeft), fr = output_of(Role::kFrontRight);

    for (size_t i = 0; i < inputs; ++i) {
        const Role role = roles[i];
        if (role == Role::kOther) {
            // Discrete channels (index masks) keep their position.
            if (i < outputs) add(static_cast<int>(i), i, 1.0f);
            continue;
        }
        if (const int o = output_of(role); o >= 0) {
            add(o, i, 1.0f);
            continue;
        }
        // Downmix what the speakers lack.
        switch (role) {
            case Role::kCenter:
                add(fl, i, kMinus3Db);
                add(fr, i, kMinus3Db);
                break;
            case Role::kLfe:
                add(fl, i, 0.5f);
                add(fr, i, 0.5f);
                break;
            case Role::kRearLeft:
                add(output_of(Role::kSideLeft) >= 0 ? output_of(Role::kSideLeft) : fl, i,
                    output_of(Role::kSideLeft) >= 0 ? 1.0f : kMinus3Db);
                break;
            case Role::kRearRight:
                add(output_of(Role::kSideRight) >= 0 ? output_of(Role::kSideRight) : fr, i,
                    output_of(Role::kSideRight) >= 0 ? 1.0f : kMinus3Db);
                break;
            case Role::kSideLeft:
                add(output_of(Role::kRearLeft) >= 0 ? output_of(Role::kRearLeft) : fl, i,
                    output_of(Role::kRearLeft) >= 0 ? 1.0f : kMinus3Db);
                break;
            case Role::kSideRight:
                add(output_of(Role::kRearRight) >= 0 ? output_of(Role::kRearRight) : fr, i,
                    output_of(Role::kRearRight) >= 0 ? 1.0f : kMinus3Db);
                break;
            default:
                break;
        }
    }

    if (!config_.fill) return;
    // Speaker fill: speakers without a channel of their own play a mix of the
    // fronts (or of the rears for the sides). The subwoofer is filled by the
    // bass management.
    auto has_input = [&](Role role) {
        return std::find(roles.begin(), roles.end(), role) != roles.end();
    };
    auto input_of = [&](Role role) -> int {
        const auto it = std::find(roles.begin(), roles.end(), role);
        return it != roles.end() ? static_cast<int>(it - roles.begin()) : -1;
    };
    const int in_fl = input_of(Role::kFrontLeft), in_fr = input_of(Role::kFrontRight);
    const int in_rl = input_of(Role::kRearLeft), in_rr = input_of(Role::kRearRight);
    const int in_c = input_of(Role::kCenter);
    // Left and right sources for the fill; a mono stream feeds both.
    const int left = in_fl >= 0 ? in_fl : in_c;
    const int right = in_fr >= 0 ? in_fr : in_c;
    if (left < 0 || right < 0) return;
    for (size_t o = 0; o < outputs; ++o) {
        const Role role = output_roles_[o];
        if (has_input(role)) continue;
        switch (role) {
            case Role::kFrontLeft:
            case Role::kFrontRight:
                break;  // Covered by the centre downmix above.
            case Role::kRearLeft:
                matrix_[o][left] += kMinus3Db;
                break;
            case Role::kRearRight:
                matrix_[o][right] += kMinus3Db;
                break;
            case Role::kSideLeft:
                matrix_[o][in_rl >= 0 ? in_rl : left] += kMinus3Db;
                break;
            case Role::kSideRight:
                matrix_[o][in_rr >= 0 ? in_rr : right] += kMinus3Db;
                break;
            case Role::kCenter:
                if (left != right) {
                    matrix_[o][left] += 0.5f;
                    matrix_[o][right] += 0.5f;
                }
                break;
            default:
                break;
        }
    }
}

void OutputProcessor::SetTestTone(std::optional<unsigned int> channel) {
    if (channel.has_value() && *channel >= output_roles_.size()) channel.reset();
    tone_channel_ = channel;
}

const void* OutputProcessor::Process(const void* input, size_t frames) {
    const size_t inputs = input_roles_.size();
    const size_t outputs = output_roles_.size();
    const size_t sample_bytes = SampleBytes(format_);
    for (auto& channel : in_) {
        if (channel.size() < frames) channel.resize(frames);
    }
    for (auto& channel : out_) {
        if (channel.size() < frames) channel.resize(frames);
    }
    if (output_.size() < frames * outputs * sample_bytes) {
        output_.resize(frames * outputs * sample_bytes);
    }

    const uint8_t* src = static_cast<const uint8_t*>(input);
    for (size_t f = 0; f < frames; ++f) {
        for (size_t i = 0; i < inputs; ++i) {
            in_[i][f] = ReadSample(src, format_);
            src += sample_bytes;
        }
    }

    Mix(frames);
    if (bass_to_sub_) ManageBass(frames);
    SwapAndMono(frames);
    LevelsAndDelays(frames);
    if (tone_channel_.has_value()) TestTone(frames);

    uint8_t* dst = output_.data();
    for (size_t f = 0; f < frames; ++f) {
        for (size_t o = 0; o < outputs; ++o) {
            WriteSample(out_[o][f], dst, format_);
            dst += sample_bytes;
        }
    }
    return output_.data();
}

void OutputProcessor::Mix(size_t frames) {
    for (size_t o = 0; o < output_roles_.size(); ++o) {
        float* out = out_[o].data();
        std::fill(out, out + frames, 0.0f);
        for (size_t i = 0; i < input_roles_.size(); ++i) {
            const float gain = matrix_[o][i];
            if (gain == 0.0f) continue;
            const float* in = in_[i].data();
            for (size_t f = 0; f < frames; ++f) out[f] += gain * in[f];
        }
    }
}

void OutputProcessor::ManageBass(size_t frames) {
    float* sub = out_[lfe_].data();
    for (size_t f = 0; f < frames; ++f) {
        float sum = 0.0f;
        for (const int m : mains_) sum += out_[m][f];
        sub[f] += kBassSumGain * low_pass_.Process(sum);
    }
    if (high_passes_.empty()) return;
    for (size_t k = 0; k < mains_.size(); ++k) {
        float* out = out_[mains_[k]].data();
        for (size_t f = 0; f < frames; ++f) out[f] = high_passes_[k].Process(out[f]);
    }
}

void OutputProcessor::SwapAndMono(size_t frames) {
    if (!config_.swap_left_right && !config_.mono) return;
    for (const auto& [left_role, right_role] : {std::pair{Role::kFrontLeft, Role::kFrontRight},
                                                std::pair{Role::kRearLeft, Role::kRearRight},
                                                std::pair{Role::kSideLeft, Role::kSideRight}}) {
        const int l = IndexOf(left_role);
        const int r = IndexOf(right_role);
        if (l < 0 || r < 0) continue;
        float* left = out_[l].data();
        float* right = out_[r].data();
        for (size_t f = 0; f < frames; ++f) {
            if (config_.mono) {
                const float mid = 0.5f * (left[f] + right[f]);
                left[f] = mid;
                right[f] = mid;
            } else {
                std::swap(left[f], right[f]);
            }
        }
    }
}

void OutputProcessor::LevelsAndDelays(size_t frames) {
    for (size_t o = 0; o < output_roles_.size(); ++o) {
        float* out = out_[o].data();
        const float gain = gains_[o];
        if (gain != 1.0f) {
            for (size_t f = 0; f < frames; ++f) out[f] *= gain;
        }
        if (delays_[o] == 0) continue;
        std::vector<float>& line = delay_lines_[o];
        size_t pos = delay_positions_[o];
        for (size_t f = 0; f < frames; ++f) {
            line[pos] = out[f];
            pos = (pos + 1) % line.size();
            // The oldest sample: written `delays_[o]` frames ago.
            out[f] = line[pos];
        }
        delay_positions_[o] = pos;
    }
}

void OutputProcessor::TestTone(size_t frames) {
    for (size_t o = 0; o < output_roles_.size(); ++o) {
        float* out = out_[o].data();
        if (o == *tone_channel_) {
            for (size_t f = 0; f < frames; ++f) out[f] = kToneGain * NextPinkNoise();
        } else {
            std::fill(out, out + frames, 0.0f);
        }
    }
}

float OutputProcessor::NextPinkNoise() {
    // White noise from xorshift32, shaped to -3 dB per octave with Paul
    // Kellet's filter (public domain, musicdsp.org).
    noise_state_ ^= noise_state_ << 13;
    noise_state_ ^= noise_state_ >> 17;
    noise_state_ ^= noise_state_ << 5;
    const float white = static_cast<float>(noise_state_) / 2147483648.0f - 1.0f;
    pink_[0] = 0.99886f * pink_[0] + white * 0.0555179f;
    pink_[1] = 0.99332f * pink_[1] + white * 0.0750759f;
    pink_[2] = 0.96900f * pink_[2] + white * 0.1538520f;
    pink_[3] = 0.86650f * pink_[3] + white * 0.3104856f;
    pink_[4] = 0.55000f * pink_[4] + white * 0.5329522f;
    pink_[5] = -0.7616f * pink_[5] - white * 0.0168980f;
    const float pink = pink_[0] + pink_[1] + pink_[2] + pink_[3] + pink_[4] + pink_[5] + pink_[6] +
                       white * 0.5362f;
    pink_[6] = white * 0.115926f;
    // The filter has a gain of about 9.
    return pink * 0.11f;
}

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
