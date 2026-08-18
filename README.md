# interactor-see-through-cpp

Single-image layer decomposition for anime characters, after
[See-through](https://doi.org/10.1145/3799902.3811209), as an interactor on the harness command
bus. The C++ half of a pair; `interactor-see-through-python` is the other, and they exist to
disagree.

## What it answers

One verb. `decompose <in-path> --res <px> --steps <n> [--out <dir>]` returns a CBOR map naming
where the layers were written, how many there are, and how long the engine says it took. Not
the pixels: the bus carries one value of at most 128 KiB and a layer set is larger than that,
so the artefacts stay on the volume and the reply says where they are.

## The gate

**A run below 1280px or 30 steps is refused, not served.** Low step counts make degenerate
layers — undifferentiated depth medians, soft detail — that look plausible in a viewer and say
nothing about output quality, and a 512px/8-step artefact that vanished at 1280/30 is what the
see-through project's MADR 0009 was written about. An interactor that answered a cheap request
with a cheap result would invite exactly that comparison. `proof/decompose.c` holds the gate,
and checks that a refused command never reaches the engine.

The timing in the reply is measured by the engine around its own work. A duration taken from
anywhere else has been wrong here before, confidently, in the direction of a defect that did
not exist.

## Where the weights are

On the network volume RunPod mounts at `/runpod-volume`, never in the image: an image carrying
them re-downloads gigabytes into every cold worker, where the volume is written once and read
by all. A worker that cannot find them answers every job with the path it looked in rather than
exiting — a worker that dies before its first job-take is recorded as one that was never asked,
and the job is retried against another that dies the same way.

`src/engine_absent.c` is what a build with no see-through linked gets, and it fails at open. It
is deliberately not a stub returning a plausible layer set: this pipeline is judged by
compositing its layers and looking at them, so a fabricated result is not caught by checking
that a result arrived, and a missing model must stay distinguishable from a bad one.
