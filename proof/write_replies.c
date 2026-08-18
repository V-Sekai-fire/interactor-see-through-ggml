// Writes one file per reply shape, so a real virtual machine can decode what this interactor
// produces. The C proof beside it asserts on bytes; only Erlang can answer whether those bytes
// are the term a caller matches.
//
// SPDX-License-Identifier: Apache-2.0

#include "seethrough/command.h"

#include <stdio.h>
#include <string.h>

static int rec_open(void *c, const char *d, char *e, size_t n) { (void)c; (void)d; (void)e; (void)n; return 0; }
static void rec_close(void *c) { (void)c; }

static int rec_decompose(void *ctx, const st_request_t *req, st_result_t *out) {
	(void)ctx;
	(void)req;
	out->layers = 7;
	out->ms = 359000;
	snprintf(out->sidecar, sizeof(out->sidecar), "res.psd.json");
	return 0;
}

static void emit(const char *dir, const char *name, st_state_t *st, const char *line) {
	unsigned char reply[4096];
	int stop = 0;
	const size_t n = st_ask(st, line, reply, sizeof(reply), &stop);

	char path[512];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "cannot write %s\n", path);
		return;
	}
	fwrite(reply, 1, n, f);
	fclose(f);
}

int main(int argc, char **argv) {
	const char *dir = argc > 1 ? argv[1] : "/tmp/replies-cpp";

	st_state_t live;
	live.engine.open = rec_open;
	live.engine.decompose = rec_decompose;
	live.engine.close = rec_close;
	live.engine.ctx = NULL;
	live.out_root = "/runpod-volume/see-through/out";
	live.opened = 1;

	st_state_t dead = live;
	dead.opened = 0;

	emit(dir, "res.cbor", &live, "decompose /in.png --res 512 --steps 30");
	emit(dir, "no_engine.cbor", &dead, "decompose /in.png --res 1280 --steps 30");
	emit(dir, "ok.cbor", &live, "decompose /in.png --res 1280 --steps 30");
	emit(dir, "verb.cbor", &live, "render /in.png");
	printf("wrote 4 replies to %s\n", dir);
	return 0;
}
