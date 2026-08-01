#include "zircon_passlib.h"

// the generated registry tables + create-by-name factory (PRE_BUILD
// codegen over no_streaming/, never edit by hand)
#include "no_streaming/zircon_render_pass_factory.h"

extern "C"
{
	unsigned zircon_passlib_get_count(void)
	{
		return zircon_render_game_passes_registry_count +
			zircon_render_editor_passes_registry_count;
	}

	const char* zircon_passlib_get_name(unsigned index)
	{
		// game passes first, editor passes after (the union documented in
		// the header); both tables are generated string-literal arrays
		if (index < zircon_render_game_passes_registry_count)
		{
			return zircon_render_game_passes_registry[index];
		}

		index -= zircon_render_game_passes_registry_count;

		if (index < zircon_render_editor_passes_registry_count)
		{
			return zircon_render_editor_passes_registry[index];
		}

		return nullptr;
	}

	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
	zircon_passlib_create(const char* p_pass_name)
	{
		KOTEK_ASSERT(p_pass_name && p_pass_name[0] != '\0',
			"pass a valid registered pass class name");

		if (!p_pass_name)
		{
			return nullptr;
		}

		return zircon_render_pass_factory::create(p_pass_name);
	}

	void zircon_passlib_destroy(
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass)
	{
		if (p_pass)
		{
			// mirrors ktkRenderGraphSimplified::Shutdown's per-pass
			// sequence, but inside the library that allocated the pass
			p_pass->OnDestroyResources();
			delete p_pass;
		}
	}
}
