#include "zircon_render_graph_pass_editor_grid.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include "../../../../ecs/zircon_factory.h"
#include "../../../../ecs/zircon_component_sdk_camera.h"
#include "../../../../world/zircon_world.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"

#include <cmath>

namespace no_streaming
{
	zircon_render_graph_pass_editor_grid_bgfx::
		zircon_render_graph_pass_editor_grid_bgfx(void) :
		zircon_render_graph_pass_editor_bgfx(),
		m_program{BGFX_INVALID_HANDLE},
		m_uniform_camera_pos{BGFX_INVALID_HANDLE},
		m_is_vertex_id_supported{false},
		m_is_warned_about_missing_program{false}
	{
	}

	zircon_render_graph_pass_editor_grid_bgfx::
		~zircon_render_graph_pass_editor_grid_bgfx(void)
	{
	}

	void zircon_render_graph_pass_editor_grid_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_manager_main, "must be valid!");

		this->m_p_manager_main = p_manager_main;
		this->m_p_manager_resource = p_manager_resource;

		// the fullscreen triangle is vertex-id generated (no vertex
		// buffer) — bgfx gates that pattern behind BGFX_CAPS_VERTEX_ID
		const bgfx::Caps* p_caps = bgfx::getCaps();

		this->m_is_vertex_id_supported =
			p_caps && (p_caps->supported & BGFX_CAPS_VERTEX_ID) != 0;

		if (this->m_is_vertex_id_supported == false)
		{
			KOTEK_MESSAGE_WARNING(
				"[editor_grid] the renderer '{}' lacks "
				"BGFX_CAPS_VERTEX_ID — the grid pass stays inert",
				p_caps ? bgfx::getRendererName(p_caps->rendererType)
					   : "unknown");
		}

		// the only pass-written uniform (u_invViewProj is a bgfx
		// predefined name and must NOT be created — the renderer fills
		// it from the view transform); the name mirrors the fragment
		// shader's cbuffer and the cmake uniform table
		this->m_uniform_camera_pos =
			bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);

		KOTEK_ASSERT(bgfx::isValid(this->m_uniform_camera_pos),
			"failed to create the u_cameraPos uniform!");

		// the blob names the Slang pipeline produces from
		// data_game/shaders/slang/grid.<stage>.slang; until they exist
		// the pass stays inert (one-time warning)
		bgfx::ShaderHandle shader_vertex =
			this->load_shader_blob("grid.vs.bin");
		bgfx::ShaderHandle shader_fragment =
			this->load_shader_blob("grid.fs.bin");

		if (bgfx::isValid(shader_vertex) && bgfx::isValid(shader_fragment))
		{
			this->m_program =
				bgfx::createProgram(shader_vertex, shader_fragment, true);

			KOTEK_ASSERT(bgfx::isValid(this->m_program),
				"failed to link the editor_grid program!");
		}
		else
		{
			if (bgfx::isValid(shader_vertex))
				bgfx::destroy(shader_vertex);
			if (bgfx::isValid(shader_fragment))
				bgfx::destroy(shader_fragment);

			if (this->m_is_warned_about_missing_program == false)
			{
				KOTEK_MESSAGE_WARNING(
					"[editor_grid] compiled shader blobs are absent "
					"under data_user/shader_cache/bgfx/ (the Slang "
					"pipeline has not produced them yet) — the pass "
					"stays inert");

				this->m_is_warned_about_missing_program = true;
			}
		}
	}

	void zircon_render_graph_pass_editor_grid_bgfx::OnDestroyResources()
	{
		if (bgfx::isValid(this->m_program))
		{
			bgfx::destroy(this->m_program);
			this->m_program = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_uniform_camera_pos))
		{
			bgfx::destroy(this->m_uniform_camera_pos);
			this->m_uniform_camera_pos = BGFX_INVALID_HANDLE;
		}

		this->m_is_vertex_id_supported = false;
		this->m_is_warned_about_missing_program = false;
	}

	void zircon_render_graph_pass_editor_grid_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
	}

	void zircon_render_graph_pass_editor_grid_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		if (bgfx::isValid(this->m_program) == false ||
			this->m_is_vertex_id_supported == false)
		{
			return;
		}

		const bgfx::ViewId pass_id =
			static_cast<bgfx::ViewId>(my_id_in_queue);

		bgfx::setViewName(pass_id, "EditorGrid");

		// the present pass (earlier slot) clears the color; this pass
		// owns the depth clear so its depth test reads a deterministic
		// buffer
		bgfx::setViewRect(pass_id, 0, 0, bgfx::BackbufferRatio::Equal);
		bgfx::setViewClear(pass_id, BGFX_CLEAR_DEPTH);

		float view[16];
		float projection[16];

		if (this->resolve_editor_camera(view, projection) == false)
		{
			float aspect_ratio = 4.0f / 3.0f;

			if (this->m_p_manager_main &&
				this->m_p_manager_main->Get_WindowManager())
			{
				int window_width = this->m_p_manager_main
									   ->Get_WindowManager()
									   ->ActiveWindow_GetWidth();
				int window_height = this->m_p_manager_main
										->Get_WindowManager()
										->ActiveWindow_GetHeight();

				if (window_width > 0 && window_height > 0)
				{
					aspect_ratio =
						static_cast<float>(window_width) /
						static_cast<float>(window_height);
				}
			}

			build_default_orbit_view_projection(view, projection,
				aspect_ratio, bgfx::getCaps()->homogeneousDepth);
		}

		// fills the predefined u_invViewProj of the fragment stage
		bgfx::setViewTransform(pass_id, view, projection);

		float camera_position[3];
		compute_camera_position(view, camera_position);

		const float uniform_camera_pos[4] = {camera_position[0],
			camera_position[1], camera_position[2], 1.0f};

		bgfx::setUniform(this->m_uniform_camera_pos, uniform_camera_pos);

		// alpha-blended under the scene geometry the later passes draw:
		// depth-tested against this pass's clear, but no depth write (a
		// following opaque pass always wins the pixel); the clip-space z
		// of the fullscreen triangle is 0, which passes LESS against the
		// cleared 1.0 in both depth conventions
		constexpr kotek::uint64_t _kState = BGFX_STATE_WRITE_RGB |
			BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
			BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
				BGFX_STATE_BLEND_INV_SRC_ALPHA);

		bgfx::setVertexCount(3);
		bgfx::setState(_kState);
		bgfx::submit(pass_id, this->m_program);
	}

	void zircon_render_graph_pass_editor_grid_bgfx::
		build_default_orbit_view_projection(float* p_out_view,
			float* p_out_projection, float aspect_ratio,
			bool homogeneous_depth) noexcept
	{
		KOTEK_ASSERT(p_out_view, "must be valid storage");
		KOTEK_ASSERT(p_out_projection, "must be valid storage");
		KOTEK_ASSERT(aspect_ratio > 0.0f, "must be positive");

		if (p_out_view == nullptr || p_out_projection == nullptr ||
			aspect_ratio <= 0.0f)
		{
			return;
		}

		// TODO(zircon): editor camera controller — until one drives the
		// editor camera, a world without an sdk_camera component renders
		// the grid through this fixed orbit (mirrors the game
		// model_static pass's default)
		bx::mtxLookAt(p_out_view, bx::Vec3(4.0f, 3.0f, 4.0f),
			bx::Vec3(0.0f, 0.0f, 0.0f), bx::Vec3(0.0f, 1.0f, 0.0f));

		bx::mtxProj(p_out_projection, 60.0f, aspect_ratio, 0.1f, 100.0f,
			homogeneous_depth);
	}

	void zircon_render_graph_pass_editor_grid_bgfx::
		compose_inverse_view_projection(const float* p_view,
			const float* p_projection,
			float* p_out_inverse_view_projection) noexcept
	{
		KOTEK_ASSERT(p_view, "must be valid");
		KOTEK_ASSERT(p_projection, "must be valid");
		KOTEK_ASSERT(p_out_inverse_view_projection,
			"must be valid storage");

		if (p_view == nullptr || p_projection == nullptr ||
			p_out_inverse_view_projection == nullptr)
		{
			return;
		}

		float view_projection[16];

		// the bx order: the view applies first — identical to how the
		// renderer composes m_viewProj for the predefined uniforms
		// (renderer.h: bx::float4x4_mul(view, proj))
		bx::mtxMul(view_projection, p_view, p_projection);
		bx::mtxInverse(p_out_inverse_view_projection, view_projection);
	}

	void zircon_render_graph_pass_editor_grid_bgfx::compute_camera_position(
		const float* p_view, float* p_out_position) noexcept
	{
		KOTEK_ASSERT(p_view, "must be valid");
		KOTEK_ASSERT(p_out_position, "must be valid storage");

		if (p_view == nullptr || p_out_position == nullptr)
			return;

		// column-major layout: column j row i sits at [j*4+i]; a rigid
		// view inverts to rotation R^T with translation -(R^T * t), and
		// R^T's rows are R's columns
		p_out_position[0] =
			-(p_view[0] * p_view[12] + p_view[1] * p_view[13] +
				p_view[2] * p_view[14]);
		p_out_position[1] =
			-(p_view[4] * p_view[12] + p_view[5] * p_view[13] +
				p_view[6] * p_view[14]);
		p_out_position[2] =
			-(p_view[8] * p_view[12] + p_view[9] * p_view[13] +
				p_view[10] * p_view[14]);
	}

	bool zircon_render_graph_pass_editor_grid_bgfx::compute_world_ray(
		const float* p_inverse_view_projection, float ndc_x, float ndc_y,
		float* p_out_point_near, float* p_out_point_far) noexcept
	{
		KOTEK_ASSERT(p_inverse_view_projection, "must be valid");
		KOTEK_ASSERT(p_out_point_near, "must be valid storage");
		KOTEK_ASSERT(p_out_point_far, "must be valid storage");

		if (p_inverse_view_projection == nullptr ||
			p_out_point_near == nullptr || p_out_point_far == nullptr)
		{
			return false;
		}

		// world = invVP * clip (the column-vector reading of the
		// column-major storage — the shader's mul(u_invViewProj, clip));
		// the two depths (-1 near, +1 far) stay on the same view ray in
		// any depth convention
		const float depths[2] = {-1.0f, 1.0f};
		float* outputs[2] = {p_out_point_near, p_out_point_far};

		for (int point_index = 0; point_index < 2; ++point_index)
		{
			const float clip_z = depths[point_index];
			float* p_out = outputs[point_index];

			const float w = p_inverse_view_projection[3] * ndc_x +
				p_inverse_view_projection[7] * ndc_y +
				p_inverse_view_projection[11] * clip_z +
				p_inverse_view_projection[15];

			if (std::fabs(w) < 1e-8f)
				return false;

			for (int component = 0; component < 3; ++component)
			{
				p_out[component] =
					(p_inverse_view_projection[component] * ndc_x +
						p_inverse_view_projection[4 + component] *
							ndc_y +
						p_inverse_view_projection[8 + component] *
							clip_z +
						p_inverse_view_projection[12 + component]) /
					w;
			}
		}

		return true;
	}

	bool zircon_render_graph_pass_editor_grid_bgfx::intersect_xz_plane(
		const float* p_ray_origin, const float* p_ray_direction,
		float* p_out_hit) noexcept
	{
		KOTEK_ASSERT(p_ray_origin, "must be valid");
		KOTEK_ASSERT(p_ray_direction, "must be valid");
		KOTEK_ASSERT(p_out_hit, "must be valid storage");

		if (p_ray_origin == nullptr || p_ray_direction == nullptr ||
			p_out_hit == nullptr)
		{
			return false;
		}

		// the shader's tests, one to one: parallel ray, then plane
		// behind the camera
		if (std::fabs(p_ray_direction[1]) < 1e-6f)
			return false;

		const float ray_length = -p_ray_origin[1] / p_ray_direction[1];

		if (ray_length <= 0.0f)
			return false;

		p_out_hit[0] = p_ray_origin[0] + p_ray_direction[0] * ray_length;
		p_out_hit[1] = 0.0f;
		p_out_hit[2] = p_ray_origin[2] + p_ray_direction[2] * ray_length;

		return true;
	}

	bool zircon_render_graph_pass_editor_grid_bgfx::resolve_editor_camera(
		float* p_out_view, float* p_out_projection) noexcept
	{
		KOTEK_ASSERT(p_out_view, "must be valid storage");
		KOTEK_ASSERT(p_out_projection, "must be valid storage");

		if (p_out_view == nullptr || p_out_projection == nullptr)
			return false;

		zircon_session_editor* p_session = nullptr;

		if (this->m_p_manager_session_editor)
		{
			// the same lookup the editor imgui pass does: the current id
			// is pushed through the console at session creation, so it
			// is set long before the graph's first frame
			p_session = this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor
					->get_current_session_id());
		}

		zircon_world* p_world =
			p_session ? p_session->get_world() : nullptr;

		if (p_world == nullptr || p_world->is_initialized() == false)
			return false;

		zircon_factory* p_factory = p_world->get_factory();
		zircon_ecs_context_t* p_context = p_world->get_ecs_context();

		if (p_factory == nullptr || p_context == nullptr)
			return false;

		kotek::entity_t entity_ids
			[zircon_DEF_RENDER_PASS_EDITOR_GRID_MAX_ENTITY_SCAN_COUNT];

		kotek::uint32_t entity_count = p_factory->get_all_entities(
			p_context, p_world->get_entity_count_max_limit(), entity_ids,
			zircon_DEF_RENDER_PASS_EDITOR_GRID_MAX_ENTITY_SCAN_COUNT);

		for (kotek::uint32_t entity_index = 0;
			 entity_index < entity_count; ++entity_index)
		{
			if (p_factory->has_component(p_context,
					entity_ids[entity_index],
					eZirconComponentType::
						kzircon_component_sdk_camera) == false)
			{
				continue;
			}

			zircon_component_sdk_camera* p_sdk_camera =
				static_cast<zircon_component_sdk_camera*>(
					p_factory->get_component_by_enum(p_context,
						entity_ids[entity_index],
						eZirconComponentType::
							kzircon_component_sdk_camera));

			if (p_sdk_camera == nullptr)
				continue;

			const zircon_component_camera& camera =
				p_sdk_camera->get_camera();

			const float* p_camera_view =
				kotek::math::value_ptr(camera.get_view());
			const float* p_camera_projection =
				kotek::math::value_ptr(camera.get_projection());

			for (int element_index = 0; element_index < 16;
				 ++element_index)
			{
				p_out_view[element_index] =
					p_camera_view[element_index];
				p_out_projection[element_index] =
					p_camera_projection[element_index];
			}

			return true;
		}

		return false;
	}

	bgfx::ShaderHandle
	zircon_render_graph_pass_editor_grid_bgfx::load_shader_blob(
		const char* p_shader_file_name) noexcept
	{
		bgfx::ShaderHandle result = BGFX_INVALID_HANDLE;

		KOTEK_ASSERT(p_shader_file_name, "must be valid");
		KOTEK_ASSERT(this->m_p_manager_main,
			"OnCreateResources must run before loading shaders");

		if (p_shader_file_name == nullptr ||
			this->m_p_manager_main == nullptr)
		{
			return result;
		}

		kotek::core::ktkIFileSystem* p_filesystem =
			this->m_p_manager_main->GetFileSystem();

		KOTEK_ASSERT(p_filesystem, "must be valid");

		if (p_filesystem == nullptr)
			return result;

		// the backend-dialect directory of the active renderer (same
		// route as the game model_static pass): one blob set per backend
		// under shader_cache/bgfx/
		const char* p_dialect_directory = nullptr;

		switch (bgfx::getRendererType())
		{
		case bgfx::RendererType::Direct3D11:
		{
			p_dialect_directory = "dx11";
			break;
		}
		case bgfx::RendererType::Direct3D12:
		{
			p_dialect_directory = "dx12";
			break;
		}
		case bgfx::RendererType::OpenGLES:
		{
			p_dialect_directory = "essl";
			break;
		}
		case bgfx::RendererType::OpenGL:
		{
			p_dialect_directory = "glsl";
			break;
		}
		case bgfx::RendererType::Vulkan:
		{
			p_dialect_directory = "vulkan";
			break;
		}
		default:
		{
			break;
		}
		}

		if (p_dialect_directory == nullptr)
		{
			KOTEK_MESSAGE_WARNING(
				"[editor_grid] no shader-blob dialect directory for "
				"renderer '{}' — shader '{}' not loaded",
				bgfx::getRendererName(bgfx::getRendererType()),
				p_shader_file_name);

			return result;
		}

		kotek::static_path_t shader_path;

		p_filesystem->Make_Path(shader_path,
			kotek::core::eFolderIndex::kFolderIndex_DataUser_ShaderCache);

		shader_path /= "bgfx";
		shader_path /= p_dialect_directory;
		shader_path /= p_shader_file_name;

		// an absent blob is an expected pre-pipeline state, not an error
		// (the native read path asserts on a missing file, hence the
		// existence check first)
		if (p_filesystem->Is_Exists(shader_path) == false)
			return result;

		kotek::uint8_t
			buffer[zircon_DEF_RENDER_PASS_EDITOR_GRID_SHADER_BIN_MAX_SIZE];

		kotek::uint8_t* p_buffer = buffer;
		kotek::size_t buffer_length = sizeof(buffer);

		bool read_status =
			p_filesystem->Read_File(shader_path, p_buffer, buffer_length);

		if (read_status && buffer_length)
		{
			// bgfx::copy hands bgfx its own copy, so the filesystem's
			// buffer is not referenced past this call
			result = bgfx::createShader(bgfx::copy(p_buffer,
				static_cast<uint32_t>(buffer_length)));

			KOTEK_ASSERT(bgfx::isValid(result),
				"shader blob '{}' failed to create — corrupt or "
				"wrong-dialect data?",
				p_shader_file_name);
		}
		else
		{
			KOTEK_MESSAGE_WARNING(
				"[editor_grid] failed to read shader blob '{}'",
				p_shader_file_name);
		}

		return result;
	}
} // namespace no_streaming
