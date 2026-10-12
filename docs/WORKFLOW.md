# Workflow

Applies to every component in this repository unless its own `AGENTS.md` or
`INITIAL_IMPLEMENTATION.md` says otherwise.

## Builds

- Do not start an Android build (`m`, `mm`, `make`, `lunch` or `breakfast`)
  on your own. When one is needed, ask the human developer how to start it:
  the setup commands, the target, the environment variables and the build
  targets. Do not guess them, and keep the answer for later sessions.
- Report build results as they are, with the errors.

## Builds and tests on the build host

- Safe builds and tests on the build host need no asking. For example: the
  formatter on the files you touched (`docs/CODE_STYLE.md`), `sh -n` on
  shell scripts, compiling touched sources with the prebuilt toolchain and
  the include paths of the tree, running host tools and scripts on dummy
  files.
- They work in `/tmp`, or in a tmpfs mount point the human developer gave,
  and write nothing to `out/`, the source tree or anywhere else.
- Safe means: no root, no real disks or devices, no change to the system
  or its services. When in doubt, ask.

## Tests on devices and virtual machines

- Do not run tests, install, flash or deploy anything without the human
  developer's confirmation. Ask which environment to use and how: a virtual
  machine, an emulator or a device. The confirmation covers that
  environment only.
- Destructive steps (installing, partitioning, formatting, flashing,
  changing boot entries or firmware variables) run only in a test
  environment the human developer named. Before each one, check that the
  target is the named one: the virtual machine, the adb serial, the size
  and model of the disk.
- Inside a virtual machine, also check that it is one, the way Linux tells:
  `/sys/class/dmi/id/sys_vendor` and `product_name` (`QEMU`, `VMware, Inc.`,
  `innotek GmbH`, `Microsoft Corporation` with `Virtual Machine`), the
  `hypervisor` flag in `/proc/cpuinfo` on x86, `linux,dummy-virt` in
  `/proc/device-tree/compatible` on Arm, virtio devices in
  `/sys/bus/virtio/devices`. These support the human's naming; they do not
  replace it.
- Never run a destructive step on the build host, or on any other machine
  or device, unless the human developer asks for that machine and that
  step.
- Read-only inspection of a confirmed test environment (logs, properties,
  probing) needs no further confirmation.
- A test by an agent does not replace testing by affected users on real
  hardware (`docs/REVIEW.md`, check 6).

## References

- Do not browse outside the AOSP source tree for reference material.
- Search reference material only in a precise, relevant directory and with
  specific terms. AI agents must never run `grep` or `glob` from the AOSP
  source tree root. If the appropriate search path is unclear, ask the human
  developer for a more precise path instead of searching broadly.
- Do not use any proprietary specification from your memory (e.g. codec
  bitstream or transport formats defined by a paywalled standard or a
  vendor's licensed documentation). If one is needed, ask the human
  developer to download the specification document from the internet and
  point you to it, and cite the document in the code and commit message.

## Commits and reviews

- Before presenting a commit to a human maintainer, or when asked to review
  someone else's commit, follow `docs/REVIEW.md`.
- If that review is against a commit that's already merged, don't rewrite
  it — follow `docs/FIXUPS.md` instead.
- If a directory contains `INITIAL_IMPLEMENTATION.md`, treat it as the
  authoritative source for that component's one-time bring-up constraints
  (reference paths, prohibited actions, review checklist). Do not edit it,
  and follow it over this document when it is more specific.
