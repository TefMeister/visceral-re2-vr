// rainhold.h -- the title rain's request no longer crashes inside the game's own effect creation (2026-10-17, Fable,
// x64dbg live on the home PC).
//
// What the game does [verified-live 2026-10-17, hardware write-watch]: EffectManager.createEffectInternal (0x140f7fd90)
// keeps its one-entry list of effect GameObjects in a GameObject[] taken from the calling thread's SCRATCH PAGE (array
// allocator 0x141f79930: temporary objects, 4000-byte pages on the via.clr thread context). Inside the same call, the
// first managed-object allocation (0x141f5d750) runs the context's scratch CLEANUP (0x141f790b0) when the context's
// allocation-nesting counter at [ctx+0x78] drops back to 0: the page is rewound to its base and zeroed, the new object
// lands on top of the list, and the loop then reads junk (length 0xNN0000, entry 1 = UTF-16 "ffec" -- every crash dump
// since 2026-10-04). Our request met this while the title was still loading; the game's own (Story open) does not.
//
// Lever check (PROTOCOL section 11): the cleanup is decided inside the allocation, in the middle of the call; a hold on
// the counter taken in the pre-hook of ObjectEffectManager.requestEffectInternal (the call's entry) runs BEFORE that
// decision, so this fixes rather than annotates. The counter goes back to what it was in the post-hook. Proof of
// effect: the first calls log "counter N -> N+1 (held)" and the Lua logs attempt 1 accepted; a "no hold" line means
// argv[0] was not this thread's context and the old refusals will still show.
#pragma once

namespace vn::rainhold {
void install();   // the hook (once, from the game thread)
} // namespace vn::rainhold
