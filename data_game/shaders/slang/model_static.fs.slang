// model_static.fs.slang — unlit static geometry, fragment stage.
// Conventions: see zircon_core.slang. No uniforms: the color arrives via
// the v_color0 varying (semantically the imgui fs shape — v_color0 in,
// SV_Target out).

import zircon_core;

[shader("fragment")]
float4 fs_main(ZirconVsToFs_model_static input) : SV_Target
{
	return input.v_color0;
}
