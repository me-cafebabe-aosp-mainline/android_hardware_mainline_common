/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

/**
 * How an output processes what it plays. Applies right away, also to playing
 * streams. The defaults play every stream as it is.
 */
@VintfStability
parcelable OutputSettings {
    /** The stream's own channels, unprocessed but for swap and mono. */
    const int LAYOUT_AUTO = 0;
    const int LAYOUT_STEREO = 1;
    const int LAYOUT_QUAD = 2;
    const int LAYOUT_5_1 = 3;
    const int LAYOUT_7_1 = 4;

    /** One of LAYOUT_*. A layout needs as many channels as the output has. */
    int layout;
    /**
     * Speakers the stream has no channel for play a mix of the others (stereo
     * on 5.1: rears from the fronts, centre from both, subwoofer the bass).
     */
    boolean fill;
    boolean swapLeftRight;
    /** Left and right of each speaker pair play their average. */
    boolean mono;
    /** The LFE channel drives a subwoofer; without one its content goes to the fronts. */
    boolean subwoofer = true;
    /** Bass below the crossover goes to the subwoofer instead of the other speakers. */
    boolean smallSpeakers;
    /** 40 to 250 Hz. */
    int crossoverHz = 80;
    /**
     * Per speaker of the layout, in the order FL FR RL RR FC LFE SL SR (as far
     * as the layout has them): level in dB (-30 to +10) and delay in ms (0 to
     * 20). Ignored for LAYOUT_AUTO. Missing entries are 0.
     */
    float[] levelsDb;
    float[] delaysMs;
}
