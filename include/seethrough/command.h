// The command this interactor answers, and the one rule it refuses to bend.
//
// A command is a line, because that is what `contract-command` carries and what the RunPod
// job's `input` is turned into. There is one verb:
//
//     decompose <in-path> --res <px> --steps <n> [--out <dir>]
//
// SPDX-License-Identifier: Apache-2.0
#ifndef SEETHROUGH_COMMAND_H
#define SEETHROUGH_COMMAND_H

#include "seethrough/engine.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Production settings, and the reason this file has a gate in it at all.
//
// A run below either of these is refused rather than served. Low step counts produce
// degenerate layers -- undifferentiated depth medians, soft detail -- that look plausible in a
// viewer and say nothing about output quality, and a 512px/8-step artefact that vanished at
// 1280/30 is what MADR 0009 was written about. An interactor that answers a cheap request with
// a cheap result invites exactly that comparison, so it answers with a refusal instead.
#define ST_MIN_RES 1280
#define ST_MIN_STEPS 30

// Where the weights are. RunPod mounts the network volume here in every worker, and weights
// are cached on it rather than baked into the image: an image carrying them re-downloads
// several gigabytes into every cold worker, and the volume is written once and read by all of
// them. Overridden by ST_WEIGHTS_DIR for a machine that is not a RunPod worker.
#define ST_WEIGHTS_DIR_DEFAULT "/runpod-volume/see-through/hf"

// Parses one command line into a request. Returns 0 on success; on refusal writes the reason
// into `error` and returns non-zero. `line` is borrowed; `req`'s strings point into `store`,
// which the caller owns and must keep alive for as long as `req`.
int st_parse(const char *line, st_request_t *req, char *store, size_t store_cap, char *error,
		size_t error_cap);

// Answers one command: parse, gate, decompose, and write the CBOR reply. This is
// `weft_interactor_t::ask` with `ctx` an `st_state_t`.
typedef struct {
	st_engine_t engine;
	const char *out_root;
	int opened;
} st_state_t;

size_t st_ask(void *ctx, const char *command, unsigned char *reply, size_t cap, int *stop);

#ifdef __cplusplus
}
#endif

#endif
