// What an interactor's reply is made of.
//
// CBOR, not JSON text. A reply is a batch of entity rows and scalars, and RFC 8949 packs those
// in a fraction of the bytes while staying self-describing, which is what a control path needs
// and what a bitpacked struct would give up. The hot path is bitpacked and is not this.
//
// It is here rather than in any one interactor for the reason the harness holds the limits:
// left alone every interactor would grow its own writer, and the copies would drift. A reply
// that decodes differently depending on which interactor sent it is not a contract.
//
// Framing is deliberately absent. A length prefix belongs to whichever transport needs one —
// a byte stream does, a WebTransport stream does not, because its FIN is the boundary. A
// writer that always prefixed would put a stream's framing inside a datagram.
//
// SPDX-License-Identifier: Apache-2.0
#ifndef WEFT_CBOR_H
#define WEFT_CBOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned char *p;
	size_t n, cap;
	int over; // the buffer ran out, and every write since has been dropped
} weft_cbor_t;

static inline weft_cbor_t weft_cbor_to(unsigned char *buf, size_t cap) {
	weft_cbor_t c = {buf, 0, cap, 0};
	return c;
}

// A truncated batch decodes as a short one and the reader cannot tell the difference, so
// `over` is checked rather than the byte count. Nothing here reports failure any other way.
static inline int weft_cbor_over(const weft_cbor_t *c) { return c->over; }

void weft_cbor_raw(weft_cbor_t *c, unsigned char b);
void weft_cbor_head(weft_cbor_t *c, int major, uint64_t v);
void weft_cbor_int(weft_cbor_t *c, int64_t v);
void weft_cbor_text(weft_cbor_t *c, const char *s);
void weft_cbor_map(weft_cbor_t *c, uint64_t pairs);
void weft_cbor_array(weft_cbor_t *c, uint64_t items);
void weft_cbor_bool(weft_cbor_t *c, int b);
void weft_cbor_break(weft_cbor_t *c);
void weft_cbor_kv_int(weft_cbor_t *c, const char *k, int64_t v);
void weft_cbor_kv_text(weft_cbor_t *c, const char *k, const char *v);
void weft_cbor_kv_bool(weft_cbor_t *c, const char *k, int v);

// ── An Elixir term ────────────────────────────────────────────────────────────
//
// RFD 0124 carries a reply as a term a BEAM caller matches on, in CBOR rather than in a second
// encoding. The mapping is closed: an atom is tag 39, a tuple is an array, a map is a map, a
// binary is a text string. A reply carries no lists, so an array is always a tuple.
//
// Tag 39 is IANA's "identifier", which is what Erlang atoms and Ruby symbols already travel
// under. It is registered rather than invented, so a decoder that knows CBOR needs nothing
// from us to read it.

#define WEFT_CBOR_TAG_IDENTIFIER 39

// An atom. Usable as a map key as well as a value: a reply's keys are atoms, so a caller
// matches %{layers: n} instead of reaching into a map with binary keys.
void weft_cbor_atom(weft_cbor_t *c, const char *name);

// `{:error, :reason}` whole.
void weft_cbor_error(weft_cbor_t *c, const char *reason);

// `{:error, {:reason, %{...}}}` up to and including the map header. The caller writes the
// `pairs` key/value pairs itself, each key an atom.
void weft_cbor_error_detail(weft_cbor_t *c, const char *reason, uint64_t pairs);

// `{:ok, %{...}}` up to and including the map header, on the same terms.
void weft_cbor_ok_map(weft_cbor_t *c, uint64_t pairs);

#ifdef __cplusplus
}
#endif

#endif
