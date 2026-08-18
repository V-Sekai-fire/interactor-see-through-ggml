// Parse, gate, decompose, reply.
//
// SPDX-License-Identifier: Apache-2.0

#include "seethrough/command.h"

#include "weft/cbor.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// One token, copied into the caller's store and NUL-terminated. The command bytes are
// borrowed from the transport, which may still own them, so nothing here points into `line`.
static char *take(const char *at, size_t n, char **store, char *store_end) {
	if ((size_t)(store_end - *store) < n + 1) {
		return NULL;
	}
	char *out = *store;
	memcpy(out, at, n);
	out[n] = '\0';
	*store += n + 1;
	return out;
}

static int fail(char *error, size_t cap, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(error, cap, fmt, ap);
	va_end(ap);
	return 1;
}

int st_parse(const char *line, st_request_t *req, char *store, size_t store_cap, char *error,
		size_t error_cap) {
	char *sp = store;
	char *send = store + store_cap;

	memset(req, 0, sizeof(*req));
	req->res = 0;
	req->steps = 0;

	const char *at = line;
	while (*at == ' ') {
		++at;
	}
	const char *verb = at;
	while (*at && *at != ' ') {
		++at;
	}
	if ((size_t)(at - verb) != 9 || memcmp(verb, "decompose", 9) != 0) {
		return fail(error, error_cap, "unknown command; this interactor answers `decompose`");
	}

	while (*at) {
		while (*at == ' ') {
			++at;
		}
		if (!*at) {
			break;
		}
		const char *tok = at;
		while (*at && *at != ' ') {
			++at;
		}
		const size_t n = (size_t)(at - tok);

		if (n > 2 && tok[0] == '-' && tok[1] == '-') {
			// A flag and its value. The value is required: `--steps` with nothing after it
			// used to read as steps=0, which the gate below refuses, but with a message about
			// step count rather than about the missing argument.
			const char *flag = tok + 2;
			const size_t flag_n = n - 2;
			while (*at == ' ') {
				++at;
			}
			const char *val = at;
			while (*at && *at != ' ') {
				++at;
			}
			const size_t val_n = (size_t)(at - val);
			if (val_n == 0) {
				return fail(error, error_cap, "--%.*s needs a value", (int)flag_n, flag);
			}
			if (flag_n == 3 && memcmp(flag, "res", 3) == 0) {
				req->res = atoi(val);
			} else if (flag_n == 5 && memcmp(flag, "steps", 5) == 0) {
				req->steps = atoi(val);
			} else if (flag_n == 3 && memcmp(flag, "out", 3) == 0) {
				req->out_dir = take(val, val_n, &sp, send);
			} else {
				return fail(error, error_cap, "unknown flag --%.*s", (int)flag_n, flag);
			}
			continue;
		}

		if (req->in_path) {
			return fail(error, error_cap, "decompose takes one input path");
		}
		req->in_path = take(tok, n, &sp, send);
		if (!req->in_path) {
			return fail(error, error_cap, "input path too long");
		}
	}

	if (!req->in_path) {
		return fail(error, error_cap, "decompose needs an input path");
	}
	// The gate. Both bounds are refused with the number that was asked for, because a refusal
	// that does not say what it saw is one the caller retries unchanged.
	if (req->res < ST_MIN_RES) {
		return fail(error, error_cap,
				"--res %d is below the production setting %d; a smaller run is not evidence",
				req->res, ST_MIN_RES);
	}
	if (req->steps < ST_MIN_STEPS) {
		return fail(error, error_cap,
				"--steps %d is below the production setting %d; a smaller run is not evidence",
				req->steps, ST_MIN_STEPS);
	}
	return 0;
}

static size_t error_reply(unsigned char *reply, size_t cap, const char *text) {
	weft_cbor_t c = weft_cbor_to(reply, cap);
	weft_cbor_map(&c, 1);
	weft_cbor_kv_text(&c, "error", text);
	return c.n;
}

size_t st_ask(void *ctx, const char *command, unsigned char *reply, size_t cap, int *stop) {
	st_state_t *st = (st_state_t *)ctx;
	(void)stop;

	char store[2048];
	char error[256];
	st_request_t req;

	if (st_parse(command, &req, store, sizeof(store), error, sizeof(error)) != 0) {
		return error_reply(reply, cap, error);
	}
	if (!req.out_dir) {
		req.out_dir = st->out_root;
	}
	if (!st->opened) {
		return error_reply(reply, cap, "no engine: the weights were never loaded");
	}

	st_result_t res;
	memset(&res, 0, sizeof(res));
	if (st->engine.decompose(st->engine.ctx, &req, &res) != 0) {
		return error_reply(reply, cap, res.error[0] ? res.error : "decompose failed");
	}

	// What the caller gets: where the layers are, how many, and how long the engine says it
	// took. Not the pixels -- the bus carries at most one value and a layer set is larger than
	// that, so the artefacts stay on the volume and this reply says where.
	weft_cbor_t c = weft_cbor_to(reply, cap);
	weft_cbor_map(&c, 5);
	weft_cbor_kv_text(&c, "in", req.in_path);
	weft_cbor_kv_text(&c, "out", req.out_dir);
	weft_cbor_kv_text(&c, "sidecar", res.sidecar);
	weft_cbor_kv_int(&c, "layers", res.layers);
	weft_cbor_kv_int(&c, "ms", res.ms);
	return c.n;
}
