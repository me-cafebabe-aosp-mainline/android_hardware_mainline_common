# Code Style

Applies to every component in this repository unless its own `AGENTS.md` or
`INITIAL_IMPLEMENTATION.md` says otherwise. Written for both AI agents and
human contributors.

## Language

C++ is preferred, since it's what most of this repository is written in and
what most of the AOSP reference HAL implementations use, but it is not a hard
requirement — pick whatever language best fits the component (this
repository already carries a root `rustfmt.toml` for Rust). The C++-specific
rules below apply when you do use C++; use the equivalent idiomatic
conventions of whatever language you pick otherwise (e.g. `Result`/`Option`
and no `panic!()` on recoverable errors in Rust).

## C++

- Google C++ Style Guide naming: `CamelCase` types and functions,
  `snake_case_` member variables (trailing underscore), `kCamelCase`
  constants.
- No exceptions: `try`/`catch` is forbidden. Report failures through the
  interface's native error type instead (`ndk::ScopedAStatus`, `status_t`,
  `RetCode`, or `std::optional`, depending on the API you're implementing).
- Prefer the C++ standard library and `libbase`
  (`system/libbase/include/android-base`) over raw C APIs for logging,
  properties, string parsing (`android::base::Parse*`), and file descriptors
  (`unique_fd`).

## Kotlin (apps)

Apps (settings apps of a HAL, see `docs/HAL_APPS.md`) are written in Kotlin;
Java only appears as generated AIDL code.

- **Structure.** An activity hosts the pages; a page is a fragment that only
  renders. Its state lives in a `ViewModel` as a `StateFlow`, so that it
  survives configuration changes, and the fragment collects it with
  `repeatOnLifecycle(Lifecycle.State.STARTED)`.
- **One owner per side effect.** Every call to the HAL goes through one
  class (`HalControl`), every change of a system setting through one class
  (e.g. `AudioRouting`), so that no page can bypass its checks, storage or
  confirmation flow.
- **No blocking on the main thread.** Binder calls, to the HAL or to
  framework services that may block, run on `Dispatchers.IO` from
  `viewModelScope` / `lifecycleScope`. A `BroadcastReceiver` that has work
  to do uses `goAsync()` and a worker thread.
- **Failures degrade, never crash.** The HAL may be restarting or absent:
  its owner class catches `RemoteException`, `SecurityException` and the
  runtime exceptions Binder turns the HAL's error codes into
  (`IllegalArgumentException`, `IllegalStateException`,
  `ServiceSpecificException`, ...; `Parcel.createExceptionOrNull()`), and
  returns null, and the page shows less. Code that runs inside another
  process's call (a `ContentProvider` serving a Settings tile) must not
  throw at all.
- **Resources.** Every user visible text is a string resource, as a whole
  sentence with `xliff:g` placeholders and a comment giving an example; no
  sentences assembled in code. Counts use `plurals`. Escape apostrophes
  (`\'`). Units belong in the string ("80 Hz"), not in code.
- **Preferences built in code** are `isPersistent = false`: their values
  live in the HAL or the framework, not in the app's SharedPreferences. Give
  them stable keys, and update a page in place when its structure did not
  change, so that a value change does not reset the scroll position or a
  moving slider.
- **Edge to edge.** Pages handle window insets (the shared
  `SettingsFragment` does), since apps targeting the current SDK draw behind
  the system bars.
- **R8.** Keep classes instantiated by name (manifest, preference
  `fragment`) and AIDL parcelables in `proguard.flags`.

## Comments

- Comment why, not what. If the name and types already say it, add nothing.
- Comment what the code cannot show: a hardware or framework quirk, an
  ordering or locking requirement, an invariant, a non-obvious workaround.
- One line where one line is enough. Longer is for genuinely complicated
  logic.
- No comment that restates the next line, narrates a class that is clear
  from its name, or lists parameters that are self-explanatory.
- Doc comments (KDoc, Doxygen) on public API only when the contract is not
  obvious from the signature.
- Design rationale that spans a component belongs in its `AGENTS.md` or
  `README.md`, not in the code.

## Copyright header

Every new source file, in any language, starts with the SPDX license header;
copy it from an existing file in the same directory rather than retyping it.
For a directory's first file(s), use:

`Android.bp`:
```
//
// SPDX-FileCopyrightText: The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//
```

Source files:
```
/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */
```

XML files (manifests, resources), after the XML declaration if there is one:
```
<!--
    SPDX-FileCopyrightText: The LineageOS Project
    SPDX-License-Identifier: Apache-2.0
-->
```

## Formatting tools

Run a formatter on the files you actually touched before committing:

- C++: `clang-format -i --style=file <files>` using the repository's
  `.clang-format` (`hardware/mainline/common/.clang-format`), or the
  prebuilt toolchain binary if available, e.g.
  `prebuilts/clang/host/linux-x86/clang-r*/bin/clang-format`.
- Rust: `rustfmt <files>` using the repository's `rustfmt.toml`
  (`hardware/mainline/common/rustfmt.toml`).
- Kotlin: `java -jar prebuilts/build-tools/common/framework/ktfmt.jar
  --kotlinlang-style <files>` (relative to the AOSP root).
- `Android.bp`: `out/host/linux-x86/bin/bpfmt -w <files>` (relative to the
  AOSP root; it's a build output, not a prebuilt). If it doesn't exist yet,
  building it yourself would violate `docs/WORKFLOW.md` — ask the maintainer
  to build it, or leave `Android.bp` formatting to them for that pass.

Only format the files you changed. Do not go searching the tree for other
formatters or linters to run, and do not reformat unrelated files.

### Externally imported components

Some components are tracked against an external upstream (see the root
`README.md`'s Upstreams table) and deliberately keep the upstream's own
formatting instead of this repository's, to keep diffs against upstream
small and syncs/cherry-picks easy. Forcing this repository's formatter on
such a component reformats the whole file and buries your actual change in
unrelated noise.

Before editing a file, run the formatter on it unmodified first:

- No diff: the component already follows this repository's style; format
  your touched files as usual before committing.
- Non-trivial diff: the component doesn't use this repository's formatting
  (typically true for anything in the Upstreams table). Don't run the
  formatter on your touched files for this change — match the file's
  existing style by hand instead, so your diff stays limited to the actual
  change.

## Documentation

Keep the relevant `README.md` (including any per-backend/per-module README)
and `AGENTS.md` in sync with behavior, property, or ABI changes you make.

### Section names

Name a section with a short noun phrase that says what it is about, not
with a sentence or a description of why it exists. This applies to the
section comments in code and configuration files, such as `.mk`, `.rc`,
SELinux policy (`.te`, `file_contexts`, ...) and source files. It does not
apply to documentation files.

- Good: `# Graphics (Mesa)`, `# Modem`, `# Audio`.
- Bad: `# Graphics, detected by the quirks service`, `# Set the GLES
  version for Mesa`.

Put the explanation in the comments under the section, and keep the name
stable so that sections can be found and sorted.
