// The decomposition engine, as a seam.
//
// The model is not in this repository and cannot be: see-through is a diffusion pipeline over
// ggml with several gigabytes of weights, and those live on the network volume this
// repository's README names rather than in an image or a git history. So the interactor holds
// the command, the gate and the reply, and the engine arrives through these four pointers.
//
// `src/engine_absent.c` is what a build with no engine linked gets. It fails with the path it
// looked in, which is honest; a stub that returned a plausible layer set would make a missing
// model indistinguishable from a bad one, and this pipeline is judged by looking at layers.
//
// SPDX-License-Identifier: Apache-2.0
#ifndef SEETHROUGH_ENGINE_H
#define SEETHROUGH_ENGINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// What one decomposition was asked for. Every field is decided by `st_parse`, so the engine
// never sees the command text and cannot disagree with the gate about what the settings were.
typedef struct {
	const char *in_path;
	const char *out_dir;
	int res;
	int steps;
} st_request_t;

// What one decomposition produced. The layer files and the `.psd.json` sidecar are written by
// the engine under `out_dir`; this reports where and how long, and nothing about the pixels.
//
// `ms` is measured by the engine around its own work. It is not measured by the caller and
// never inferred from a wall clock somewhere else -- a timing taken outside the program that
// did the work has been wrong here before, in the direction of confident and fabricated.
typedef struct {
	int layers;
	long long ms;
	char sidecar[512]; // the .psd.json path, relative to out_dir
	char error[256];   // empty when the run succeeded
} st_result_t;

typedef struct {
	// Loads the weights. Returns 0 on success, and on failure writes into `error` the path it
	// looked in, because "model not found" without one is the least actionable line a
	// serverless worker can put in a log it may be the only copy of.
	int (*open)(void *ctx, const char *weights_dir, char *error, size_t error_cap);

	// One image in, layers out. Returns 0 on success.
	int (*decompose)(void *ctx, const st_request_t *req, st_result_t *out);

	void (*close)(void *ctx);
	void *ctx;
} st_engine_t;

// The engine a build with no see-through linked gets.
st_engine_t st_engine_absent(void);

#ifdef __cplusplus
}
#endif

#endif
