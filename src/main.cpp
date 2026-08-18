// The interactor: the harness command loop, with see-through behind it.
//
// No socket, no HTTP, no RunPod. What reaches this process is a command off the bus, and what
// leaves it is reply bytes -- `transport-runpod`'s `rp-worker-bus` is what turns a queue job
// into one of those, and swapping that for a different transport layer changes nothing here.
//
// The weights are read from the network volume, which is also why this binary starts before
// any job arrives: loading them is the expensive part, and a worker that loaded them per job
// would pay it on every one.
//
// SPDX-License-Identifier: Apache-2.0

#include "seethrough/command.h"

#include "weft/loop.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
	const char *weights = std::getenv("ST_WEIGHTS_DIR");
	if (!weights || !*weights) {
		weights = ST_WEIGHTS_DIR_DEFAULT;
	}
	const char *out_root = std::getenv("ST_OUT_DIR");
	if (!out_root || !*out_root) {
		out_root = "/runpod-volume/see-through/out";
	}

	st_state_t st;
	st.engine = st_engine_absent();
	st.out_root = out_root;
	st.opened = 0;

	char error[256] = { 0 };
	if (st.engine.open(st.engine.ctx, weights, error, sizeof(error)) == 0) {
		st.opened = 1;
	} else {
		// Not fatal, and that is the point. A worker that exits here takes its own explanation
		// with it: RunPod records a worker that died before its first job, and the job it
		// would have refused is retried against another worker that will die the same way.
		// Answering every command with this line puts the reason in the job result instead.
		std::fprintf(stderr, "see-through: %s\n", error);
	}

	const int rc = weft::run_command_loop(&st, st_ask);
	if (st.opened) {
		st.engine.close(st.engine.ctx);
	}
	return rc;
}
