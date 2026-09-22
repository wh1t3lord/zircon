#include "zircon_render_graph_pass_editor_csg.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include "../../../../ecs/zircon_factory.h"
#include "../../../../ecs/zircon_component_sdk_camera.h"
#include "../../../../world/zircon_world.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"
#include "zircon_render_graph_pass_editor_grid.h"

#include <cmath>

namespace no_streaming
{
	zircon_render_graph_pass_editor_csg_bgfx::
		zircon_render_graph_pass_editor_csg_bgfx(void) :
		zircon_render_graph_pass_editor_bgfx(),
		m_layout{},
		m_vertex_buffer{BGFX_INVALID_HANDLE},
		m_index_buffer{BGFX_INVALID_HANDLE},
		m_program{BGFX_INVALID_HANDLE},
		m_uniform_light_dir{BGFX_INVALID_HANDLE},
		m_uniform_light_color{BGFX_INVALID_HANDLE},
		m_uniform_ambient{BGFX_INVALID_HANDLE},
		m_uniform_camera_pos{BGFX_INVALID_HANDLE},
		m_is_warned_about_missing_program{false},
		m_is_first_draw_logged{false},
		m_last_submitted_index_count{0}
	{
	}

	zircon_render_graph_pass_editor_csg_bgfx::
		~zircon_render_graph_pass_editor_csg_bgfx(void)
	{
	}

	void zircon_render_graph_pass_editor_csg_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_manager_main, "must be valid!");

		this->m_p_manager_main = p_manager_main;
		this->m_p_manager_resource = p_manager_resource;

		this->m_pool.initialize();

		// the geometry layout is model_static's (position + normal +
		// packed ABGR color — the shared forward-Phong contract)
		this->m_layout.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8,
				true)
			.end();

		// ONE dynamic vertex pool + ONE dynamic index pool for every
		// editor compound; the changed ranges are mirrored with
		// bgfx::update (the CPU shadows are the single source of
		// truth — the buffers are never read back)
		this->m_vertex_buffer = bgfx::createDynamicVertexBuffer(
			zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES,
			this->m_layout);

		this->m_index_buffer = bgfx::createDynamicIndexBuffer(
			zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_INDICES,
			BGFX_BUFFER_INDEX32);

		KOTEK_ASSERT(bgfx::isValid(this->m_vertex_buffer),
			"failed to create the editor_csg dynamic vertex pool!");
		KOTEK_ASSERT(bgfx::isValid(this->m_index_buffer),
			"failed to create the editor_csg dynamic index pool!");

		// the LightParams cbuffer members of the fragment stage (the
		// model_static fixed-scene-light contract — defines until
		// light components land)
		this->m_uniform_light_dir =
			bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
		this->m_uniform_light_color =
			bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
		this->m_uniform_ambient =
			bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
		this->m_uniform_camera_pos =
			bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);

		KOTEK_ASSERT(bgfx::isValid(this->m_uniform_light_dir) &&
				bgfx::isValid(this->m_uniform_light_color) &&
				bgfx::isValid(this->m_uniform_ambient) &&
				bgfx::isValid(this->m_uniform_camera_pos),
			"failed to create the editor_csg light uniforms!");

		// the blob names the Slang pipeline produces from
		// data_game/shaders/slang/editor_csg.<stage>.slang; until they
		// exist the pass stays inert (one-time warning)
		bgfx::ShaderHandle shader_vertex =
			this->load_shader_blob("editor_csg.vs.bin");
		bgfx::ShaderHandle shader_fragment =
			this->load_shader_blob("editor_csg.fs.bin");

		if (bgfx::isValid(shader_vertex) && bgfx::isValid(shader_fragment))
		{
			this->m_program =
				bgfx::createProgram(shader_vertex, shader_fragment, true);

			KOTEK_ASSERT(bgfx::isValid(this->m_program),
				"failed to link the editor_csg program!");

			KOTEK_MESSAGE(
				"[editor_csg] pass created: one dynamic pool "
				"({} vertices / {} indices), the shared phong "
				"contract",
				zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_VERTICES,
				zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_INDICES);
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
					"[editor_csg] compiled shader blobs are absent "
					"under data_user/shader_cache/bgfx/ (the Slang "
					"pipeline has not produced them yet) — the pass "
					"stays inert");

				this->m_is_warned_about_missing_program = true;
			}
		}
	}

	void zircon_render_graph_pass_editor_csg_bgfx::OnDestroyResources()
	{
		if (bgfx::isValid(this->m_program))
		{
			bgfx::destroy(this->m_program);
			this->m_program = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_uniform_light_dir))
		{
			bgfx::destroy(this->m_uniform_light_dir);
			this->m_uniform_light_dir = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_uniform_light_color))
		{
			bgfx::destroy(this->m_uniform_light_color);
			this->m_uniform_light_color = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_uniform_ambient))
		{
			bgfx::destroy(this->m_uniform_ambient);
			this->m_uniform_ambient = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_uniform_camera_pos))
		{
			bgfx::destroy(this->m_uniform_camera_pos);
			this->m_uniform_camera_pos = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_vertex_buffer))
		{
			bgfx::destroy(this->m_vertex_buffer);
			this->m_vertex_buffer = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_index_buffer))
		{
			bgfx::destroy(this->m_index_buffer);
			this->m_index_buffer = BGFX_INVALID_HANDLE;
		}

		this->m_pool.clear();
		this->m_is_warned_about_missing_program = false;
		this->m_is_first_draw_logged = false;
		this->m_last_submitted_index_count = 0;
	}

	void zircon_render_graph_pass_editor_csg_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		(void)p_previous_pass;
		(void)my_id_in_queue;

		if (bgfx::isValid(this->m_program) == false)
			return;

		this->merge_completed_rebuilds();
		this->reconcile_pool_with_world();
		this->upload_dirty_spans();
	}

	void zircon_render_graph_pass_editor_csg_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		(void)p_previous_pass;

		if (bgfx::isValid(this->m_program) == false)
			return;

		const kotek::uint32_t index_count =
			this->m_pool.get_index_high_water();

		// an empty pool is a no-op
		if (this->m_pool.get_live_compound_count() == 0 || index_count == 0)
			return;

		const bgfx::ViewId pass_id =
			static_cast<bgfx::ViewId>(my_id_in_queue);

		bgfx::setViewName(pass_id, "EditorCSG");

		bgfx::setViewRect(pass_id, 0, 0, bgfx::BackbufferRatio::Equal);

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

			zircon_render_graph_pass_editor_grid_bgfx::
				build_default_orbit_view_projection(view, projection,
					aspect_ratio, bgfx::getCaps()->homogeneousDepth);
		}

		bgfx::setViewTransform(pass_id, view, projection);

		// the fixed scene light (the model_static defines until light
		// components land) + the per-frame camera position for the
		// specular view vector
		float light_dir[4] = {
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_X,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_Y,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_Z,
			0.0f};

		const float light_dir_length = std::sqrt(light_dir[0] *
				light_dir[0] +
			light_dir[1] * light_dir[1] +
			light_dir[2] * light_dir[2]);

		KOTEK_ASSERT(light_dir_length > 0.0f,
			"the editor_csg light direction degenerated");

		if (light_dir_length > 0.0f)
		{
			light_dir[0] /= light_dir_length;
			light_dir[1] /= light_dir_length;
			light_dir[2] /= light_dir_length;
		}

		const float light_color[4] = {
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_R,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_G,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_B,
			1.0f};
		const float ambient[4] = {
			zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_R,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_G,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_B,
			1.0f};

		float camera_position[3];

		zircon_render_graph_pass_editor_grid_bgfx::
			compute_camera_position(view, camera_position);

		const float camera_pos[4] = {camera_position[0],
			camera_position[1], camera_position[2], 1.0f};

		bgfx::setUniform(this->m_uniform_light_dir, light_dir);
		bgfx::setUniform(this->m_uniform_light_color, light_color);
		bgfx::setUniform(this->m_uniform_ambient, ambient);
		bgfx::setUniform(this->m_uniform_camera_pos, camera_pos);

		// ONE submit over the pool's used range — the compounds are
		// ranges inside it, the holes read as degenerate triangles
		constexpr kotek::uint64_t _kState = BGFX_STATE_WRITE_RGB |
			BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
			BGFX_STATE_DEPTH_TEST_LESS;

		bgfx::setVertexBuffer(0, this->m_vertex_buffer, 0,
			this->m_pool.get_vertex_high_water());
		bgfx::setIndexBuffer(this->m_index_buffer, 0, index_count);
		bgfx::setState(_kState);
		bgfx::submit(pass_id, this->m_program);

		if (this->m_is_first_draw_logged == false)
		{
			KOTEK_MESSAGE(
				"[editor_csg] first pool draw: {} compounds, {} "
				"indices (one submit)",
				this->m_pool.get_live_compound_count(),
				index_count);

			this->m_is_first_draw_logged = true;
		}
		else if (index_count != this->m_last_submitted_index_count)
		{
			KOTEK_MESSAGE(
				"[editor_csg] pool draw changed: {} compounds, {} "
				"indices",
				this->m_pool.get_live_compound_count(),
				index_count);
		}

		this->m_last_submitted_index_count = index_count;
	}

	void zircon_render_graph_pass_editor_csg_bgfx::
		merge_completed_rebuilds(void) noexcept
	{
		zircon_session_editor* p_session = nullptr;

		if (this->m_p_manager_session_editor)
		{
			p_session = this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor
					->get_current_session_id());
		}

		if (p_session == nullptr)
			return;

		zircon_csg_editor_rebuild_scheduler* p_scheduler =
			p_session->get_csg_scheduler();

		if (p_scheduler == nullptr)
			return;

		zircon_csg_rebuild_result_view_t view{};

		while (p_scheduler->pop_completed(view))
		{
			using traits_t =
				zircon_csg_scalar_traits<zircon_csg_scalar_t>;

			if (view.m_status ==
					static_cast<kotek::uint32_t>(
						eZirconCsgEvaluationStatus::kSuccess) ||
				view.m_triangle_count)
			{
				// the scalar -> float conversion into the scratch
				// (the view aliases the scheduler's slot — the pool
				// upsert must see a stable copy)
				this->m_position_scratch.resize(
					view.m_position_count * 3);
				this->m_normal_scratch.resize(
					view.m_triangle_count * 3);

				for (kotek::uint32_t position_index = 0;
					 position_index < view.m_position_count;
					 ++position_index)
				{
					for (kotek::uint8_t axis = 0; axis < 3; ++axis)
					{
						this->m_position_scratch
							[position_index * 3 + axis] =
							static_cast<float>(
								traits_t::to_double(
									view.m_p_positions
										[position_index]
										.m[axis]));
					}
				}

				for (kotek::uint32_t triangle_index = 0;
					 triangle_index < view.m_triangle_count;
					 ++triangle_index)
				{
					for (kotek::uint8_t axis = 0; axis < 3; ++axis)
					{
						this->m_normal_scratch
							[triangle_index * 3 + axis] =
							static_cast<float>(
								traits_t::to_double(
									view.m_p_normals
										[triangle_index]
										.m[axis]));
					}
				}

				this->m_pool.upsert_compound_mesh(view.m_compound_id,
					this->m_position_scratch.data(),
					view.m_position_count,
					this->m_normal_scratch.data(),
					view.m_p_indices,
					view.m_triangle_count);
			}
			else
			{
				// a failed evaluation draws nothing (the empty-mesh
				// upsert removes the compound)
				this->m_pool.upsert_compound_mesh(view.m_compound_id,
					nullptr, 0, nullptr, nullptr, 0);
			}

			if (view.m_sliver_drop_count || view.m_capacity_drop_count)
			{
				KOTEK_MESSAGE_WARNING(
					"[editor_csg] compound {} rebuilt with "
					"degradations: {} slivers, {} capacity drops",
					static_cast<kotek::uint32_t>(view.m_compound_id),
					view.m_sliver_drop_count,
					view.m_capacity_drop_count);
			}

			p_scheduler->return_completed(view.m_compound_id);
		}
	}

	void zircon_render_graph_pass_editor_csg_bgfx::
		reconcile_pool_with_world(void) noexcept
	{
		zircon_session_editor* p_session = nullptr;

		if (this->m_p_manager_session_editor)
		{
			p_session = this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor
					->get_current_session_id());
		}

		zircon_world* p_world = p_session ? p_session->get_world() : nullptr;

		if (p_world == nullptr || p_world->get_factory() == nullptr ||
			p_world->get_ecs_context() == nullptr)
			return;

		zircon_factory* p_factory = p_world->get_factory();
		zircon_ecs_context_t* p_context = p_world->get_ecs_context();

		for (kotek::uint32_t slot = 0;
			 slot < this->m_pool.get_record_count(); ++slot)
		{
			if (this->m_pool.is_compound_live(slot) == false)
				continue;

			const kotek::entity_t compound_id{
				this->m_pool.get_compound_id(slot)};

			if (p_factory->is_valid_entity(p_context, compound_id) &&
				p_factory->has_component(p_context, compound_id,
					eZirconComponentType::kzircon_component_csg))
				continue;

			this->m_pool.remove_compound(compound_id.id);
		}
	}

	void zircon_render_graph_pass_editor_csg_bgfx::upload_dirty_spans(
		void) noexcept
	{
		zircon_csg_editor_pool_dirty_span_t spans
			[zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_DIRTY_SPANS];

		const kotek::uint32_t span_count = this->m_pool.take_dirty_spans(
			spans, zircon_DEF_RENDER_CSG_EDITOR_POOL_MAX_DIRTY_SPANS);

		for (kotek::uint32_t span_index = 0; span_index < span_count;
			 ++span_index)
		{
			const auto& span = spans[span_index];

			if (span.m_is_index_pool)
			{
				// bgfx::copy hands bgfx its own copy of the span —
				// the shadow is not referenced past this call
				bgfx::update(this->m_index_buffer, span.m_offset,
					bgfx::copy(
						this->m_pool.get_index_shadow() + span.m_offset,
						span.m_count * sizeof(kotek::uint32_t)));
			}
			else
			{
				bgfx::update(this->m_vertex_buffer, span.m_offset,
					bgfx::copy(
						this->m_pool.get_vertex_shadow() +
							span.m_offset,
						span.m_count *
							sizeof(zircon_model_static_vertex_t)));
			}
		}
	}

	bool zircon_render_graph_pass_editor_csg_bgfx::resolve_editor_camera(
		float* p_out_view, float* p_out_projection) noexcept
	{
		KOTEK_ASSERT(p_out_view, "must be valid storage");
		KOTEK_ASSERT(p_out_projection, "must be valid storage");

		if (p_out_view == nullptr || p_out_projection == nullptr)
			return false;

		zircon_session_editor* p_session = nullptr;

		if (this->m_p_manager_session_editor)
		{
			// the same lookup the grid/gizmo passes do
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
			[zircon_DEF_RENDER_PASS_EDITOR_CSG_MAX_ENTITY_SCAN_COUNT];

		kotek::uint32_t entity_count = p_factory->get_all_entities(
			p_context, p_world->get_entity_count_max_limit(), entity_ids,
			zircon_DEF_RENDER_PASS_EDITOR_CSG_MAX_ENTITY_SCAN_COUNT);

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

			// the camera component is USER data (scene content): a
			// default-constructed or corrupt camera (zero/NaN
			// matrices) must not poison the frame — skip it and let
			// the caller fall back to the default orbit
			if (zircon_render_graph_pass_editor_grid_bgfx::
					is_matrix_usable(p_camera_view) == false ||
				zircon_render_graph_pass_editor_grid_bgfx::
					is_matrix_usable(p_camera_projection) == false)
			{
				continue;
			}

			for (int element_index = 0; element_index < 16;
				 ++element_index)
			{
				p_out_view[element_index] = p_camera_view[element_index];
				p_out_projection[element_index] =
					p_camera_projection[element_index];
			}

			return true;
		}

		return false;
	}

	bgfx::ShaderHandle
	zircon_render_graph_pass_editor_csg_bgfx::load_shader_blob(
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
		// route as the grid pass): one blob set per backend under
		// shader_cache/bgfx/
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
				"[editor_csg] no shader-blob dialect directory for "
				"renderer '{}' — shader '{}' not loaded",
				bgfx::getRendererName(bgfx::getRendererType()),
				p_shader_file_name);

			return result;
		}

		kotek::static_path_t shader_path;

		kotek::core::path_for(p_filesystem,
			kotek::core::eFolderIndex::kFolderIndex_DataUser_ShaderCache,
			"bgfx", shader_path);

		shader_path /= p_dialect_directory;
		shader_path /= p_shader_file_name;

		// an absent blob is an expected pre-pipeline state, not an
		// error — the existence check keeps the (graceful since kotek
		// B0) missing-file read from logging its warning
		if (p_filesystem->Is_Exists(shader_path) == false)
			return result;

		kotek::uint8_t
			buffer[zircon_DEF_RENDER_PASS_EDITOR_CSG_SHADER_BIN_MAX_SIZE];

		kotek::size_t blob_size = 0;

		bool read_status = kotek::core::read_file(p_filesystem,
			shader_path, buffer, sizeof(buffer), blob_size);

		if (read_status && blob_size)
		{
			result = bgfx::createShader(bgfx::copy(buffer,
				static_cast<uint32_t>(blob_size)));

			KOTEK_ASSERT(bgfx::isValid(result),
				"shader blob '{}' failed to create — corrupt or "
				"wrong-dialect data?",
				p_shader_file_name);
		}
		else
		{
			KOTEK_MESSAGE_WARNING(
				"[editor_csg] failed to read shader blob '{}'",
				p_shader_file_name);
		}

		return result;
	}
} // namespace no_streaming
