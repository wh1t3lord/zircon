// shadow_depth.fs.slang — the CSM cascade depth pass's fragment stage
// (task Z24 B5c). The cascade frame buffers carry ONLY a depth
// attachment, so the stage writes nothing (a void fragment shader is the
// legal depth-only form on every route: no SV_Target output to mismatch).
// Conventions: see zircon_core.slang.

import zircon_core;

[shader("fragment")]
void fs_main()
{
}
