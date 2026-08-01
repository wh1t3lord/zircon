// grid.vs.slang — editor infinite grid, vertex stage. Conventions: see
// zircon_core.slang. No vertex buffer and no uniforms: the pass submits
// bgfx::setVertexCount(3) (the BGFX_CAPS_VERTEX_ID pattern) and this stage
// expands the fullscreen triangle from the vertex id, forwarding the NDC
// corner so the fragment stage can reconstruct the world ray per pixel.

import zircon_core;

[shader("vertex")]
ZirconVsToFs_grid vs_main(uint vertex_id : SV_VertexID)
{
	ZirconVsToFs_grid output;

	// the three corners (-1,-1) (3,-1) (-1,3) cover the viewport exactly
	// once; clip w = 1 keeps v_ndc interpolation exact
	float2 ndc =
		float2(float((vertex_id << 1u) & 2u), float(vertex_id & 2u)) * 2.0 -
		1.0;

	output.position = float4(ndc, 0.0, 1.0);
	output.v_ndc = ndc;

	return output;
}
