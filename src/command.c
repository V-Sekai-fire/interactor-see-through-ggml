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

// A refusal names its reason and the numbers behind it. There is no message, because a caller
// matches the atom and a sentence is the thing it cannot match.
static int refuse(st_refusal_t *why, const char *reason) {
	memset(why, 0, sizeof(*why));
	why->reason = reason;
	return 1;
}

static int refuse_flag(st_refusal_t *why, const char *reason, const char *flag) {
	refuse(why, reason);
	why->flag = flag;
	return 1;
}

static int refuse_bound(st_refusal_t *why, const char *reason, long long got, long long min) {
	refuse(why, reason);
	why->got = got;
	why->minimum = min;
	why->has_numbers = 1;
	return 1;
}

int st_parse(const char *line, st_request_t *req, char *store, size_t store_cap,
		st_refusal_t *why) {
	char *sp = store;
	char *send = store + store_cap;

	memset(req, 0, sizeof(*req));
	memset(why, 0, sizeof(*why));
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
		refuse(why, "unknown_command");
		why->text = take(verb, (size_t)(at - verb), &sp, send);
		return 1;
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
				return refuse_flag(why, "missing_value", take(flag, flag_n, &sp, send));
			}
			if (flag_n == 3 && memcmp(flag, "res", 3) == 0) {
				req->res = atoi(val);
			} else if (flag_n == 5 && memcmp(flag, "steps", 5) == 0) {
				req->steps = atoi(val);
			} else if (flag_n == 3 && memcmp(flag, "out", 3) == 0) {
				req->out_dir = take(val, val_n, &sp, send);
			} else {
				return refuse_flag(why, "unknown_flag", take(flag, flag_n, &sp, send));
			}
			continue;
		}

		if (req->in_path) {
			return refuse(why, "too_many_input_paths");
		}
		req->in_path = take(tok, n, &sp, send);
		if (!req->in_path) {
			return refuse(why, "too_many_input_paths");
		}
	}

	if (!req->in_path) {
		return refuse(why, "missing_input_path");
	}
	// The gate. Both bounds are refused with the number that was asked for, because a refusal
	// that does not say what it saw is one the caller retries unchanged.
	if (req->res < ST_MIN_RES) {
		return refuse_bound(why, "res_below_minimum", req->res, ST_MIN_RES);
	}
	if (req->steps < ST_MIN_STEPS) {
		return refuse_bound(why, "steps_below_minimum", req->steps, ST_MIN_STEPS);
	}
	return 0;
}

// `{:error, :reason}` when there is nothing to add, and `{:error, {:reason, %{...}}}` when the
// reason has numbers behind it. What a caller matches is the atom either way.
static size_t refusal_reply(unsigned char *reply, size_t cap, const st_refusal_t *why) {
	weft_cbor_t c = weft_cbor_to(reply, cap);
	unsigned pairs = 0;
	if (why->has_numbers) {
		pairs = 2;
	} else if (why->flag) {
		pairs = 1;
	} else if (why->text) {
		pairs = 1;
	}

	if (pairs == 0) {
		weft_cbor_error(&c, why->reason);
		return c.n;
	}

	weft_cbor_error_detail(&c, why->reason, pairs);
	if (why->has_numbers) {
		weft_cbor_atom(&c, "got");
		weft_cbor_int(&c, why->got);
		weft_cbor_atom(&c, "minimum");
		weft_cbor_int(&c, why->minimum);
	} else if (why->flag) {
		weft_cbor_atom(&c, "flag");
		weft_cbor_text(&c, why->flag);
	} else {
		weft_cbor_atom(&c, "verb");
		weft_cbor_text(&c, why->text);
	}
	return c.n;
}

static size_t bare_error(unsigned char *reply, size_t cap, const char *reason) {
	weft_cbor_t c = weft_cbor_to(reply, cap);
	weft_cbor_error(&c, reason);
	return c.n;
}

size_t st_ask(void *ctx, const char *command, unsigned char *reply, size_t cap, int *stop) {
	st_state_t *st = (st_state_t *)ctx;
	(void)stop;

	char store[2048];
	st_refusal_t why;
	st_request_t req;

	if (st_parse(command, &req, store, sizeof(store), &why) != 0) {
		return refusal_reply(reply, cap, &why);
	}
	if (!req.out_dir) {
		req.out_dir = st->out_root;
	}
	if (!st->opened) {
		return bare_error(reply, cap, "no_engine");
	}

	st_result_t res;
	memset(&res, 0, sizeof(res));
	if (st->engine.decompose(st->engine.ctx, &req, &res) != 0) {
		// The engine's own message is a person's to read, so it goes under :detail where no
		// program looks. What a caller matches on is the atom.
		weft_cbor_t c = weft_cbor_to(reply, cap);
		weft_cbor_error_detail(&c, "decompose_failed", 1);
		weft_cbor_atom(&c, "detail");
		weft_cbor_text(&c, res.error[0] ? res.error : "");
		return c.n;
	}

	// What the caller gets: where the layers are, how many, and how long the engine says it
	// took. Not the pixels -- the bus carries at most one value and a layer set is larger than
	// that, so the artefacts stay on the volume and this reply says where.
	// `{:ok, %{...}}`, and every key an atom, so a caller matches %{layers: n} rather than
	// reaching into a map with binary keys.
	weft_cbor_t c = weft_cbor_to(reply, cap);
	weft_cbor_ok_map(&c, 5);
	weft_cbor_atom(&c, "in");
	weft_cbor_text(&c, req.in_path);
	weft_cbor_atom(&c, "out");
	weft_cbor_text(&c, req.out_dir);
	weft_cbor_atom(&c, "sidecar");
	weft_cbor_text(&c, res.sidecar);
	weft_cbor_atom(&c, "layers");
	weft_cbor_int(&c, res.layers);
	weft_cbor_atom(&c, "ms");
	weft_cbor_int(&c, res.ms);
	return c.n;
}
