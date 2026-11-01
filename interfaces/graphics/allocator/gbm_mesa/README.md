# gralloc_gbm_mesa
This project provides the implements of Android Hardware Graphics Interfaces.

## General
The code is tested on the platform with RK3588 (Mali G610 GPU) and using the libgbm_mesa from Mesa 25.1.4.

We still need to do more things, but it seems that the basic function is fine.

### Components
Provided:

- Allocator AIDL V2 Service
- Mapper Stable-C (IMapper V5) HAL Module
- Gralloc 4 HAL Module
- Library for using gralloc with GBM (Provided by Mesa)

## Usage
Add packages to `PRODUCT_PACKAGES`, and build the code with AOSP.

## Properties

| Property | Meaning |
|----------|---------|
| `vendor.gralloc.device` | The DRM node to allocate on, `/dev/dri/renderD128` by default. Point it at the primary node of the display (`/dev/dri/card0`) when the display and the GPU are separate devices: Mesa then renders on the GPU through kmsro and allocates scanout buffers on the display, so the display can always import them. The allocator drops DRM master after opening the node, so the composer can still take it |
| `vendor.gralloc.scanout_linear` | `true` allocates the buffers for scanout as linear, for a display that is on another device than the allocation and cannot use the modifiers of the GPU |
