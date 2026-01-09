README.md

# U3D-Android-X11

An X11-based **OpenGL ES 2.0** 3D prototyping environment built with **EGL + GLES2**, designed for rapid experimentation with rendering, input, and scene structure before porting to Android.

This project is the **canonical exploration layer** for U3D. Ideas are developed, tested, and stabilized here before being mechanically adapted to the Android NDK release build.

---

## Overview

U3D-Android-X11 provides a minimal but complete 3D rendering loop running under X11, intentionally avoiding engines and heavy abstractions. It mirrors the Android rendering pipeline closely while remaining fast to iterate on in a desktop or Termux + X11 environment.

The code focuses on clarity and correctness:
- explicit EGL setup
- hand-written matrix math
- raw X11 input handling
- simple but expressive scene construction

---

## Features

- **X11 + EGL + OpenGL ES 2.0**
  - Desktop/X11 rendering using the same GLES target as Android
  - Explicit EGL display, surface, and context creation

- **Manual rendering pipeline**
  - Minimal vertex and fragment shaders
  - Single VBO with per-vertex color
  - Depth testing enabled

- **Hand-rolled math**
  - Identity, translation, rotation, scale, and perspective matrices
  - Explicit matrix multiplication
  - No external math or graphics libraries

- **Mouse-driven rotation with inertia**
  - Click-and-drag rotation
  - Velocity accumulation, clamping, and damping
  - Behavior closely matches the Android touch model

- **Hierarchical scene composition**
  - Multi-part humanoid character constructed from cube primitives
  - Separate transforms for torso, head, arms, and legs
  - Shared world rotation for cohesive motion

---

## Controls

- **Left mouse button drag**
  - Horizontal drag → Y-axis rotation
  - Vertical drag → X-axis rotation
- Rotation continues with inertia and slows over time

---

## Project Structure

U3D-Android-X11/ ├── main.c        # Full rendering loop, math, input, scene ├── build.sh      # Simple build script ├── .gitignore └── README.md

This repository is intentionally small. The entire system can be understood by reading `main.c`.

---

## Build Requirements

- Linux environment with X11
- EGL development headers
- OpenGL ES 2.0 headers
- Common toolchains:
  - GCC or Clang
  - Termux + X11 (supported and tested)

---

## Building and Running

Example build command (adjust as needed for your system):


gcc main.c -o u3d-x11 \
    -lX11 -lEGL -lGLESv2 -lm

Run:

./u3d-x11


---

Relationship to Other Projects

This repository exists as part of a deliberate pipeline:

U3D-Android-X11

Primary design and experimentation environment

Fast iteration, minimal friction

Canonical logic and structure


U3D-Android-Release

Android NDK + GameActivity deployment

JNI input bridge

Lifecycle and platform integration



The goal is not duplication, but mechanical translation from X11 to Android.


---

Design Philosophy

Prefer explicit systems over hidden abstractions

Prototype behavior before platform compliance

Keep rendering and math understandable at a glance

Use the same graphics API across environments

Treat Android as a target, not a thinking space


This project is intentionally direct, readable, and extensible.


---

License

MIT License. See LICENSE for details.
