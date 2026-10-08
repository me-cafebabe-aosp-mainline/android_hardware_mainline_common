/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.mainline.audio;

import vendor.mainline.audio.CardStatus;
import vendor.mainline.audio.EndpointStatus;

@VintfStability
parcelable Status {
    /** The sound cards the HAL uses, in card index order. */
    CardStatus[] cards;
    /** One entry per device port of IModule/default, USB templates excluded. */
    EndpointStatus[] endpoints;
}
