// The engine a build with no see-through linked gets.
//
// It fails at open, with the directory it looked in. That is the whole of it, and it is
// deliberately not a stub that returns a plausible layer set: this pipeline is judged by
// alpha-compositing its layers and looking at them, so a fabricated result is not caught by
// the check that a result arrived. A missing model must be distinguishable from a bad one.
//
// SPDX-License-Identifier: Apache-2.0

#include "seethrough/engine.h"

#include <stdio.h>
#include <string.h>

static int absent_open(void *ctx, const char *weights_dir, char *error, size_t error_cap) {
	(void)ctx;
	snprintf(error, error_cap,
			"no see-through engine is linked, and no weights were read from %s", weights_dir);
	return 1;
}

static int absent_decompose(void *ctx, const st_request_t *req, st_result_t *out) {
	(void)ctx;
	(void)req;
	snprintf(out->error, sizeof(out->error), "no see-through engine is linked");
	return 1;
}

static void absent_close(void *ctx) { (void)ctx; }

st_engine_t st_engine_absent(void) {
	st_engine_t e;
	e.open = absent_open;
	e.decompose = absent_decompose;
	e.close = absent_close;
	e.ctx = NULL;
	return e;
}
