// me_stub.S — Media Engine reset-vector stub.
//
// Lineage: melib package, (c) mrbrown, as shipped in DaedalusX64
// Source/SysPSP/PRX/MediaEngine/me_stub.S (GPL-2.0, licence-compatible with
// this project).  DaedalusX64 1.1.8 shipped ME-offloaded audio as
// "completely stable" on this exact stub — that is the prior art this
// project's MEDIAENGINE-FINDINGS.md chose over table-driven alternatives.
//
// One deliberate change from the original: the I/D cache invalidation
// loops cover 16 KiB, not 8 KiB.  Sources disagree on the ME's cache size
// (mrbrown-era docs say 8K+8K, PSPTEK-derived docs say 16K+16K);
// invalidating 16 KiB is correct for either (index ops wrap on the smaller
// cache and merely repeat), while invalidating 8 KiB of a 16 KiB cache
// would leave half the lines stale after reset.
//
// What it does, in order: enable the ME bus clocks, zero the cache tag
// registers, index-invalidate I$ and D$, clear CAUSE, open the memory
// protection registers (0xBC000000..2C := -1 — this is what lets the ME
// see all of main RAM), set STATUS (CU1 on), then load the entry point
// and argument the kernel PRX parked at 0xBFC00600/604 and jump.

#include <regdef.h>

#define STATUS	$12
#define CAUSE	$13
#define CONFIG	$16
#define TAGLO	$28
#define TAGHI	$29
#define IXIST	0x01
#define DXIST	0x11

	.global me_stub
	.global me_stub_end
	.set noreorder
	.set noat

me_stub:
	li		k0, 0xbc100000
	li		t0, 7
	sw		t0, 80(k0)
	li		t0, 2
	sw		t0, 64(k0)
	mtc0	zero, TAGLO
	mtc0	zero, TAGHI
	li		k1, 16384
a:
	addi	k1, k1, -64
	bne		k1, zero, a
	cache	IXIST, 0(k1)
	li		k1, 16384
b:
	addi	k1, k1, -64
	bne		k1, zero, b
	cache	DXIST, 0(k1)
	mtc0	zero, CAUSE

	li		t0, 0xbc00002c
	li		t1, 0xbc000000
	li		t2, -1
c:
	sw		t2, 0(t1)
	bne		t1, t0, c
	addiu	t1, t1, 4

	li		k0, 0x20000000
	mtc0	k0, STATUS
	sync
	li		t0, 0xbfc00000
	lw		a0, 0x604(t0)
	lw		k0, 0x600(t0)
	li		sp, 0x80200000
	jr		k0
	nop
me_stub_end:
