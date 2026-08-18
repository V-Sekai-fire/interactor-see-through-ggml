// The command cycle, with no GPU, no weights and no bus.
//
// What is proved here is the gate and the parse, which are the two things that decide whether
// a job runs at all. A recording engine stands in for see-through, so the reply's shape is
// checked without a model: what the interactor promises a caller is that a successful reply
// names the sidecar and carries the engine's own timing, and that a refusal carries the number
// that was refused.
//
// SPDX-License-Identifier: Apache-2.0

#include "seethrough/command.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int ok, const char *what) {
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++failures;
	}
}

// Records what it was asked for, and answers with a fixed result. It never looks at a file.
static st_request_t seen;
static int calls = 0;

static int rec_open(void *ctx, const char *dir, char *error, size_t cap) {
	(void)ctx;
	(void)dir;
	(void)error;
	(void)cap;
	return 0;
}

static int rec_decompose(void *ctx, const st_request_t *req, st_result_t *out) {
	(void)ctx;
	seen = *req;
	++calls;
	out->layers = 7;
	out->ms = 359000;
	snprintf(out->sidecar, sizeof(out->sidecar), "res.psd.json");
	return 0;
}

static void rec_close(void *ctx) { (void)ctx; }

static int is_error(const unsigned char *r, size_t n) {
	return n > 7 && r[0] == 0xa1 && memcmp(r + 2, "error", 5) == 0;
}

// The refusal text, so a case can check which bound was hit rather than only that one was.
static int mentions(const unsigned char *r, size_t n, const char *needle) {
	const size_t len = strlen(needle);
	for (size_t i = 0; i + len <= n; ++i) {
		if (memcmp(r + i, needle, len) == 0) {
			return 1;
		}
	}
	return 0;
}

int main(void) {
	unsigned char reply[4096];
	char store[2048];
	char error[256];
	st_request_t req;
	int stop = 0;

	st_state_t st;
	st.engine.open = rec_open;
	st.engine.decompose = rec_decompose;
	st.engine.close = rec_close;
	st.engine.ctx = NULL;
	st.out_root = "/runpod-volume/see-through/out";
	st.opened = 1;

	check(st_parse("decompose /in.png --res 1280 --steps 30", &req, store, sizeof(store), error,
				   sizeof(error)) == 0,
			"the production settings parse");
	check(req.res == 1280 && req.steps == 30 && strcmp(req.in_path, "/in.png") == 0,
			"the path and both settings are read");

	check(st_parse("decompose /in.png --res 512 --steps 30", &req, store, sizeof(store), error,
				   sizeof(error)) != 0,
			"512px is refused");
	check(strstr(error, "512") != NULL, "the refusal names the resolution it saw");

	check(st_parse("decompose /in.png --res 1280 --steps 8", &req, store, sizeof(store), error,
				   sizeof(error)) != 0,
			"8 steps is refused");
	check(strstr(error, "8") != NULL, "the refusal names the step count it saw");

	check(st_parse("decompose /in.png", &req, store, sizeof(store), error, sizeof(error)) != 0,
			"a command with no settings is refused, not defaulted");
	check(st_parse("decompose --res 1280 --steps 30", &req, store, sizeof(store), error,
				   sizeof(error)) != 0,
			"a command with no input path is refused");
	check(st_parse("decompose /in.png --res 1280 --steps", &req, store, sizeof(store), error,
				   sizeof(error)) != 0,
			"a flag with no value is refused as a missing argument");
	check(strstr(error, "needs a value") != NULL, "and says so, rather than blaming the count");
	check(st_parse("render /in.png --res 1280 --steps 30", &req, store, sizeof(store), error,
				   sizeof(error)) != 0,
			"another verb is refused");

	{ // A refused command never reaches the engine, which is the whole point of the gate.
		calls = 0;
		const size_t n = st_ask(&st, "decompose /in.png --res 512 --steps 8", reply,
				sizeof(reply), &stop);
		check(is_error(reply, n), "a refusal is a CBOR error reply");
		check(calls == 0, "a refused command never reaches the engine");
	}

	{ // A production command reaches it, with the out dir defaulted to the volume.
		calls = 0;
		const size_t n = st_ask(&st, "decompose /in/a.png --res 1280 --steps 30", reply,
				sizeof(reply), &stop);
		check(calls == 1, "a production command reaches the engine");
		check(strcmp(seen.out_dir, "/runpod-volume/see-through/out") == 0,
				"output defaults to the network volume");
		check(!is_error(reply, n), "the reply is not an error");
		check(mentions(reply, n, "res.psd.json"), "the reply names the sidecar");
		check(mentions(reply, n, "sidecar") && mentions(reply, n, "layers"),
				"the reply carries the keys a caller decodes");
	}

	{ // With no engine loaded, every command is answered rather than dropped.
		st_state_t none = st;
		none.opened = 0;
		const size_t n = st_ask(&none, "decompose /in/a.png --res 1280 --steps 30", reply,
				sizeof(reply), &stop);
		check(is_error(reply, n), "a worker with no weights still answers its job");
	}

	printf("%s\n", failures ? "decompose: FAILED" : "decompose: all checks passed");
	return failures ? 1 : 0;
}
