#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		// task Z3 P5: the passes library's C ABI under test + the
		// generated registry tables it unifies (PRE_BUILD codegen over
		// no_streaming/, never edit by hand)
		#include "../../render/bgfx/passes/zircon_passlib.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_pass_factory.h"

		#ifndef ZIRCON_DEF_UNIT_TEST_PASSLIB
			#define ZIRCON_DEF_UNIT_TEST_PASSLIB 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_PASSLIB == 1

// functional proofs for task Z3 P5 (the passes library's C ABI,
// zircon_passlib.{h,cpp}): the four entries are the ONLY symbols the
// executor resolves from the hot-swappable zircon.render.passes.bgfx.dll
// in the graphics-development configuration, and in the default static
// configuration they are plain internal links over the SAME generated
// registry — so their contract is pinned here once for both linkage
// shapes. The roundtrip needs no renderer: fresh passes carry only
// invalid bgfx handles, every OnDestroyResources override is
// bgfx::isValid-guarded, and the imgui pass's destructor tolerates a
// never-initialized object (the P3b destroy-before-create rule), so
// create -> destroy runs headless before bgfx ever initializes

TEST(Zircon_Game, PassLibRegistryMatchesGeneratedTables)
{
	// the unified enumeration is exactly the two generated tables
	// concatenated (game first, editor after — the header's contract)
	constexpr unsigned _k_expected_count =
		zircon_render_game_passes_registry_count +
		zircon_render_editor_passes_registry_count;

	EXPECT_EQ(zircon_passlib_get_count(), _k_expected_count);

	for (unsigned index = 0; index < _k_expected_count; ++index)
	{
		const char* p_name = zircon_passlib_get_name(index);

		// every in-range name is a non-empty string literal
		ASSERT_NE(p_name, nullptr);
		EXPECT_NE(p_name[0], '\0');

		// string-identical to the generated table entry at the union
		// position (order included — the Render Passes window and the
		// config validators index these tables directly)
		const char* p_expected =
			(index < zircon_render_game_passes_registry_count)
			? zircon_render_game_passes_registry[index]
			: zircon_render_editor_passes_registry
				  [index - zircon_render_game_passes_registry_count];

		EXPECT_STREQ(p_name, p_expected);

		// unique per index: a duplicated registration would shadow a
		// pass in create-by-name and silently drop it from the sets
		for (unsigned other_index = index + 1;
			 other_index < _k_expected_count; ++other_index)
		{
			const char* p_other_name = zircon_passlib_get_name(other_index);

			ASSERT_NE(p_other_name, nullptr);
			EXPECT_STRNE(p_name, p_other_name);
		}
	}

	// out-of-range reads are a clean nullptr, not garbage
	EXPECT_EQ(zircon_passlib_get_name(_k_expected_count), nullptr);
	EXPECT_EQ(zircon_passlib_get_name(_k_expected_count + 1), nullptr);
}

TEST(Zircon_Game, PassLibCreateDestroyRoundtrip)
{
	// every registered name must create a live pass and destroy it
	// inside the library (the cross-CRT law: allocated by the library's
	// CRT, freed by the same CRT) — a name that fails to create breaks
	// the config-driven pass sets AND the hot-reload recreate, so the
	// whole registry round-trips here, not a sample
	const unsigned count = zircon_passlib_get_count();

	ASSERT_GT(count, 0u);

	for (unsigned index = 0; index < count; ++index)
	{
		const char* p_name = zircon_passlib_get_name(index);

		ASSERT_NE(p_name, nullptr);

		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass =
			zircon_passlib_create(p_name);

		ASSERT_NE(p_pass, nullptr);

		// the created object IS a live
		// ktkRenderGraphSimplifiedRenderPass: the base-class virtual
		// dispatches into the (never-Set_Name) debug name storage,
		// which is never nullptr in KOTEK_DEBUG
		EXPECT_NE(p_pass->Get_Name(), nullptr);

		zircon_passlib_destroy(p_pass);
	}

	// the negative control: an unregistered name is a clean nullptr
	// (the loader error-logs and falls back to the static twin — never
	// a fabricated object)
	EXPECT_EQ(zircon_passlib_create("definitely.not.a.pass"), nullptr);

	// destroy(nullptr) is a tolerated no-op (the library guards)
	zircon_passlib_destroy(nullptr);
}

		#endif
	#endif
#endif
