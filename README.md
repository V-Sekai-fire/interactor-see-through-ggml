# interactor-see-through-ggml

The C++ interactor for single-image layer decomposition of anime characters, answering one command on the bus.

## What it is for

It parses a `decompose` command, refuses a run below production resolution or step count before it reaches the engine, and replies in CBOR with where the layers were written rather than the pixels. The engine is a seam: this build links an absent engine that fails at open, so the command, the gate and the reply are tested with no GPU and no weights. The Containerfile packages it with a transport worker as one container image. The design follows the paper cited in CITATION.cff. CLAUDE.md in manuals-weftspun records why the workspace manifest does not place this repository.

## Build and test

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build
```

## Licence

LICENSE is MIT, while the source files' SPDX headers name Apache-2.0.
