#include "zircon_render_graph_pass_model_static.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include "../../../../ecs/zircon_factory.h"
#include "../../../../game/session/zircon_session_game.h"
#include "../../../../game/session/zircon_session_game_manager.h"
#include "../../../../world/zircon_world.h"

#include <cstring>

namespace
{
	// the fallback cube fixture data: 6 faces x 4 vertices, unit cube
	// centered at the origin (AABB [-1,-1,-1] .. [1,1,1]), per-face
	// normal + ABGR color so faces read apart in the unlit pass
	constexpr zircon_model_static_vertex_t
		_kCubeVertices
			[no_streaming::zircon_render_graph_pass_model_static_bgfx::
					kCubeVertexCount] = {
				// +X (red)
				{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, 0xff0000ff},
				{{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, 0xff0000ff},
				{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 0xff0000ff},
				{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 0xff0000ff},
				// -X (dark red)
				{{-1.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, 0xff0000aa},
				{{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, 0xff0000aa},
				{{-1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, 0xff0000aa},
				{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, 0xff0000aa},
				// +Y (green)
				{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, 0xff00ff00},
				{{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, 0xff00ff00},
				{{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, 0xff00ff00},
				{{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, 0xff00ff00},
				// -Y (dark green)
				{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, 0xff00aa00},
				{{-1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 0xff00aa00},
				{{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 0xff00aa00},
				{{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, 0xff00aa00},
				// +Z (blue)
				{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 0xffff0000},
				{{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 0xffff0000},
				{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 0xffff0000},
				{{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, 0xffff0000},
				// -Z (dark blue)
				{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, 0xffaa0000},
				{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, 0xffaa0000},
				{{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, 0xffaa0000},
				{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, 0xffaa0000},
		};

	// the upload staging (the file scratch buffer) holds the converted
	// vertex array followed by the 16-bit index array — prove it fits
	// for the loader's maximum output
	static_assert(
		zircon_DEF_GLTF_MAX_VERTEX_COUNT *
					sizeof(zircon_model_static_vertex_t) +
				zircon_DEF_GLTF_MAX_INDEX_COUNT *
					sizeof(kotek::uint16_t) <=
			zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_FILE_MAX_SIZE,
		"the mesh file scratch must hold the converted upload staging");
	// the index upload is 16-bit, so the vertex cap must fit it
	static_assert(zircon_DEF_GLTF_MAX_VERTEX_COUNT <= 65536,
		"16-bit indices can't address more vertices");
} // namespace

namespace no_streaming
{
	zircon_render_graph_pass_model_static_bgfx::
		zircon_render_graph_pass_model_static_bgfx(void) :
		zircon_render_graph_pass_bgfx(),
		m_vertex_buffer{BGFX_INVALID_HANDLE},
		m_index_buffer{BGFX_INVALID_HANDLE},
		m_program{BGFX_INVALID_HANDLE},
		m_is_warned_about_missing_program{false},
		m_is_warned_about_mesh_cache_full{false},
		m_last_submitted_draw_count{0xffffffffu}
	{
		for (auto& slot : this->m_mesh_slots)
		{
			slot.m_vertex_buffer = BGFX_INVALID_HANDLE;
			slot.m_index_buffer = BGFX_INVALID_HANDLE;
			slot.m_is_failed = false;
		}
	}

	zircon_render_graph_pass_model_static_bgfx::
		~zircon_render_graph_pass_model_static_bgfx(void)
	{
	}

	void zircon_render_graph_pass_model_static_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_manager_main, "must be valid!");

		this->m_p_manager_main = p_manager_main;
		this->m_p_manager_resource = p_manager_resource;

		this->m_layout.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();

		zircon_model_static_vertex_t vertices[kCubeVertexCount];
		kotek::uint16_t indices[kCubeIndexCount];

		build_cube_mesh(vertices, indices);

		this->m_vertex_buffer = bgfx::createVertexBuffer(
			bgfx::copy(vertices, sizeof(vertices)), this->m_layout);
		this->m_index_buffer = bgfx::createIndexBuffer(
			bgfx::copy(indices, sizeof(indices)));

		KOTEK_ASSERT(bgfx::isValid(this->m_vertex_buffer),
			"failed to create the fallback cube vertex buffer!");
		KOTEK_ASSERT(bgfx::isValid(this->m_index_buffer),
			"failed to create the fallback cube index buffer!");

		// the blob names the shader pipeline (Slang, see AGENTS.md §5a)
		// produces from data_game/shaders/slang/model_static.<stage>.slang;
		// until they exist the pass stays inert (one-time warning)
		bgfx::ShaderHandle shader_vertex =
			this->load_shader_blob("model_static.vs.bin");
		bgfx::ShaderHandle shader_fragment =
			this->load_shader_blob("model_static.fs.bin");

		if (bgfx::isValid(shader_vertex) && bgfx::isValid(shader_fragment))
		{
			this->m_program = bgfx::createProgram(
				shader_vertex, shader_fragment, true);

			KOTEK_ASSERT(bgfx::isValid(this->m_program),
				"failed to link the model_static program!");
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
					"[model_static] compiled shader blobs are absent "
					"under data_user/shader_cache/bgfx/ (the Slang "
					"pipeline has not produced them yet) — the pass "
					"stays inert");

				this->m_is_warned_about_missing_program = true;
			}
		}
	}

	void zircon_render_graph_pass_model_static_bgfx::OnDestroyResources()
	{
		if (bgfx::isValid(this->m_program))
		{
			bgfx::destroy(this->m_program);
			this->m_program = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_index_buffer))
		{
			bgfx::destroy(this->m_index_buffer);
			this->m_index_buffer = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_vertex_buffer))
		{
			bgfx::destroy(this->m_vertex_buffer);
			this->m_vertex_buffer = BGFX_INVALID_HANDLE;
		}

		for (auto& slot : this->m_mesh_slots)
		{
			if (bgfx::isValid(slot.m_index_buffer))
			{
				bgfx::destroy(slot.m_index_buffer);
				slot.m_index_buffer = BGFX_INVALID_HANDLE;
			}

			if (bgfx::isValid(slot.m_vertex_buffer))
			{
				bgfx::destroy(slot.m_vertex_buffer);
				slot.m_vertex_buffer = BGFX_INVALID_HANDLE;
			}

			slot.m_name.clear();
			slot.m_submeshes.clear();
			slot.m_is_failed = false;
		}

		this->m_last_submitted_draw_count = 0xffffffffu;
	}

	void zircon_render_graph_pass_model_static_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
	}

	void zircon_render_graph_pass_model_static_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		kotek::uint32_t submitted_draw_count = 0;

		if (bgfx::isValid(this->m_program))
		{
			zircon_session_game* p_session = nullptr;

			if (this->m_p_manager_session_game)
			{
				// scan the (single-slot) session pool directly:
				// get_current_session_id is only driven through the
				// console-command path and get_session asserts on an
				// unknown id, so probing the current id first is not
				// safe; the game graph only ever draws while its
				// session exists
				for (kotek::uint8_t session_id = 0;
					 session_id <
					 ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT;
					 ++session_id)
				{
					p_session =
						this->m_p_manager_session_game->get_session(
							session_id);

					if (p_session)
						break;
				}
			}

			zircon_world* p_world =
				p_session ? p_session->get_world() : nullptr;

			if (p_world && p_world->is_initialized())
			{
				zircon_factory* p_factory = p_world->get_factory();
				zircon_ecs_context_t* p_context =
					p_world->get_ecs_context();

				if (p_factory && p_context)
				{
					const kotek::uint32_t draw_item_count =
						collect_draw_items(p_factory, p_context,
							p_world->get_entity_count_max_limit(),
							this->m_draw_items,
							zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

					if (draw_item_count)
					{
						bgfx::ViewId pass_id =
							static_cast<bgfx::ViewId>(my_id_in_queue);

						// the present pass (earlier slot) clears the
						// color; this pass owns the depth clear and the
						// 3D draws
						bgfx::setViewRect(pass_id, 0, 0,
							bgfx::BackbufferRatio::Equal);
						bgfx::setViewClear(pass_id, BGFX_CLEAR_DEPTH);

						float view[16];
						float projection[16];
						bool is_camera_driven = false;

						// view/projection from the first enabled camera
						// component when one exists
						kotek::entity_t entity_ids
							[zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT];

						kotek::uint32_t entity_count =
							p_factory->get_all_entities(p_context,
								p_world->get_entity_count_max_limit(),
								entity_ids,
								zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

						for (kotek::uint32_t entity_index = 0;
							 entity_index < entity_count; ++entity_index)
						{
							if (p_factory->has_component(p_context,
									entity_ids[entity_index],
									eZirconComponentType::
										kzircon_component_camera) ==
								false)
							{
								continue;
							}

							zircon_component_camera* p_camera =
								static_cast<zircon_component_camera*>(
									p_factory->get_component_by_enum(
										p_context,
										entity_ids[entity_index],
										eZirconComponentType::
											kzircon_component_camera));

							if (p_camera == nullptr ||
								p_camera->is_enabled() == false)
							{
								continue;
							}

							const float* p_view = kotek::math::value_ptr(
								p_camera->get_view());
							const float* p_projection =
								kotek::math::value_ptr(
									p_camera->get_projection());

							for (int element_index = 0; element_index < 16;
								 ++element_index)
							{
								view[element_index] =
									p_view[element_index];
								projection[element_index] =
									p_projection[element_index];
							}

							is_camera_driven = true;
							break;
						}

						if (is_camera_driven == false)
						{
							// sane default: look at the origin from an
							// elevated diagonal, 60-degree fov
							bx::mtxLookAt(view, bx::Vec3(4.0f, 3.0f, 4.0f),
								bx::Vec3(0.0f, 0.0f, 0.0f),
								bx::Vec3(0.0f, 1.0f, 0.0f));

							float aspect_ratio = 4.0f / 3.0f;

							if (this->m_p_manager_main &&
								this->m_p_manager_main
									->Get_WindowManager())
							{
								int window_width =
									this->m_p_manager_main
										->Get_WindowManager()
										->ActiveWindow_GetWidth();
								int window_height =
									this->m_p_manager_main
										->Get_WindowManager()
										->ActiveWindow_GetHeight();

								if (window_width > 0 && window_height > 0)
								{
									aspect_ratio =
										static_cast<float>(window_width) /
										static_cast<float>(window_height);
								}
							}

							bx::mtxProj(projection, 60.0f, aspect_ratio,
								0.1f, 100.0f,
								bgfx::getCaps()->homogeneousDepth);
						}

						bgfx::setViewTransform(pass_id, view, projection);

						for (kotek::uint32_t draw_index = 0;
							 draw_index < draw_item_count; ++draw_index)
						{
							const zircon_render_pass_model_static_draw_item_t&
								item = this->m_draw_items[draw_index];

							// no culling: the fallback cube reads
							// double-sided, the unlit pass does not care
							// about winding yet
							constexpr kotek::uint64_t _kState =
								BGFX_STATE_WRITE_RGB |
								BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
								BGFX_STATE_DEPTH_TEST_LESS;

							if (item.m_mesh_name.empty())
							{
								bgfx::setTransform(item.m_model_matrix);

								bgfx::setVertexBuffer(
									0, this->m_vertex_buffer);
								bgfx::setIndexBuffer(this->m_index_buffer);

								bgfx::setState(_kState);

								bgfx::submit(pass_id, this->m_program);
								++submitted_draw_count;
							}
							else
							{
								const zircon_render_pass_model_static_mesh_slot_t*
									p_slot = this->resolve_mesh(
										item.m_mesh_name.c_str());

								if (p_slot == nullptr ||
									bgfx::isValid(
										p_slot->m_vertex_buffer) == false)
								{
									continue;
								}

								for (const auto& submesh :
									p_slot->m_submeshes)
								{
									// the node's flattened world
									// transform applies first, then the
									// entity's model matrix
									float composed_model[16];

									multiply_model_matrices(
										submesh.m_world_matrix,
										item.m_model_matrix,
										composed_model);

									bgfx::setTransform(composed_model);

									bgfx::setVertexBuffer(
										0, p_slot->m_vertex_buffer);
									bgfx::setIndexBuffer(
										p_slot->m_index_buffer,
										submesh.m_index_offset,
										submesh.m_index_count);

									bgfx::setState(_kState);

									bgfx::submit(pass_id, this->m_program);
									++submitted_draw_count;
								}
							}
						}
					}
				}
			}
		}

		// rate-limited submission trace (logs on count changes only) — a
		// headless boot log shows the pass's submission counts
		if (submitted_draw_count != this->m_last_submitted_draw_count)
		{
			KOTEK_MESSAGE_TRACE(
				"[model_static] submitted {} draws", submitted_draw_count);

			this->m_last_submitted_draw_count = submitted_draw_count;
		}
	}

	void zircon_render_graph_pass_model_static_bgfx::build_cube_mesh(
		zircon_model_static_vertex_t* p_vertices,
		kotek::uint16_t* p_indices) noexcept
	{
		KOTEK_ASSERT(p_vertices, "must be valid storage");
		KOTEK_ASSERT(p_indices, "must be valid storage");

		if (p_vertices == nullptr || p_indices == nullptr)
			return;

		for (kotek::uint8_t vertex_index = 0;
			 vertex_index < kCubeVertexCount; ++vertex_index)
		{
			p_vertices[vertex_index] = _kCubeVertices[vertex_index];
		}

		// two triangles per face quad
		for (kotek::uint8_t face_index = 0; face_index < 6; ++face_index)
		{
			const kotek::uint16_t base =
				static_cast<kotek::uint16_t>(face_index * 4);
			const kotek::uint8_t index_offset =
				static_cast<kotek::uint8_t>(face_index * 6);

			p_indices[index_offset + 0] = base;
			p_indices[index_offset + 1] = base + 1;
			p_indices[index_offset + 2] = base + 2;
			p_indices[index_offset + 3] = base;
			p_indices[index_offset + 4] = base + 2;
			p_indices[index_offset + 5] = base + 3;
		}
	}

	kotek::uint32_t
	zircon_render_graph_pass_model_static_bgfx::collect_draw_items(
		zircon_factory* p_factory, zircon_ecs_context_t* p_context,
		kotek::uint32_t entity_count_max_limit,
		zircon_render_pass_model_static_draw_item_t* p_out_items,
		kotek::uint32_t out_items_capacity) noexcept
	{
		KOTEK_ASSERT(p_factory, "must be valid");
		KOTEK_ASSERT(p_context, "must be valid");
		KOTEK_ASSERT(p_out_items, "must be valid storage");
		KOTEK_ASSERT(out_items_capacity, "must be non-zero");

		if (p_factory == nullptr || p_context == nullptr ||
			p_out_items == nullptr || out_items_capacity == 0)
		{
			return 0;
		}

		kotek::entity_t entity_ids
			[zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT];

		kotek::uint32_t entity_count = p_factory->get_all_entities(
			p_context, entity_count_max_limit, entity_ids,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

		kotek::uint32_t written_count = 0;

		for (kotek::uint32_t entity_index = 0;
			 entity_index < entity_count &&
			 written_count < out_items_capacity;
			 ++entity_index)
		{
			const kotek::entity_t& entity = entity_ids[entity_index];

			if (p_factory->has_component(p_context, entity,
					eZirconComponentType::kzircon_component_geometry) ==
					false ||
				p_factory->has_component(p_context, entity,
					eZirconComponentType::kzircon_component_transform) ==
					false)
			{
				continue;
			}

			zircon_component_geometry* p_geometry =
				static_cast<zircon_component_geometry*>(
					p_factory->get_component_by_enum(p_context, entity,
						eZirconComponentType::
							kzircon_component_geometry));

			zircon_component_transform* p_transform =
				static_cast<zircon_component_transform*>(
					p_factory->get_component_by_enum(p_context, entity,
						eZirconComponentType::
							kzircon_component_transform));

			if (p_geometry == nullptr || p_transform == nullptr)
				continue;

			if (p_geometry->is_enabled() == false ||
				p_geometry->is_visible() == false ||
				p_transform->is_enabled() == false)
			{
				continue;
			}

			// the mesh source: a mesh name routes the entity through
			// the pass's glTF cache (the file resolves at draw time);
			// without one the kBox primitive is the fallback cube and
			// every other geometry kind is skipped silently
			const bool has_mesh_name =
				p_geometry->get_mesh_name()[0] != '\0';

			if (has_mesh_name == false &&
				p_geometry->get_geometry_type() !=
					kotek::core::eStaticGeometryType::kBox)
			{
				continue;
			}

			const kotek::math::vec3f_t& position =
				p_transform->get_position();
			const kotek::math::quatf_t& rotation =
				p_transform->get_rotation();
			const kotek::math::vec3f_t& scale =
				p_transform->get_scale();

			// model = rotation (bx, column-major, zero translation)
			// with the scale folded into the rotation columns (scale
			// applies first) and the translation written at [12..14] —
			// the 3-argument bx::mtxFromQuaternion overload is NOT used:
			// it stores a negated rotated translation (view-matrix
			// helper), not a model matrix
			float rotation_translation[16];

			bx::mtxFromQuaternion(rotation_translation,
				bx::Quaternion(rotation.x(), rotation.y(), rotation.z(),
					rotation.w()));

			const float scales[3] = {scale.x(), scale.y(), scale.z()};

			float* p_model =
				p_out_items[written_count].m_model_matrix;

			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					float element =
						rotation_translation[column * 4 + row];

					if (column < 3 && row < 3)
						element *= scales[column];

					p_model[column * 4 + row] = element;
				}
			}

			p_model[12] = position.x();
			p_model[13] = position.y();
			p_model[14] = position.z();

			p_out_items[written_count].m_mesh_name.assign(
				p_geometry->get_mesh_name());

			++written_count;
		}

		return written_count;
	}

	const zircon_render_pass_model_static_mesh_slot_t*
	zircon_render_graph_pass_model_static_bgfx::resolve_mesh(
		const char* p_mesh_name) noexcept
	{
		KOTEK_ASSERT(p_mesh_name, "must be valid");

		if (p_mesh_name == nullptr || p_mesh_name[0] == '\0')
			return nullptr;

		// cache hit (failed loads stay recorded so they don't retry per
		// frame)
		for (const auto& slot : this->m_mesh_slots)
		{
			if (slot.m_name.empty())
				break;

			if (slot.m_name == p_mesh_name)
				return slot.m_is_failed ? nullptr : &slot;
		}

		zircon_render_pass_model_static_mesh_slot_t* p_free_slot =
			nullptr;

		for (auto& slot : this->m_mesh_slots)
		{
			if (slot.m_name.empty())
			{
				p_free_slot = &slot;
				break;
			}
		}

		if (p_free_slot == nullptr)
		{
			if (this->m_is_warned_about_mesh_cache_full == false)
			{
				KOTEK_MESSAGE_WARNING(
					"[model_static] the mesh cache is full ({} slots, "
					"'{}' not loaded) — raise "
					"zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_CACHE_COUNT",
					zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_CACHE_COUNT,
					p_mesh_name);

				this->m_is_warned_about_mesh_cache_full = true;
			}

			return nullptr;
		}

		p_free_slot->m_name.assign(p_mesh_name);

		kotek::core::ktkIFileSystem* p_filesystem =
			this->m_p_manager_main
				? this->m_p_manager_main->GetFileSystem()
				: nullptr;

		if (p_filesystem == nullptr)
		{
			KOTEK_MESSAGE_WARNING(
				"[model_static] no filesystem — mesh '{}' not loaded",
				p_mesh_name);

			p_free_slot->m_is_failed = true;
			return nullptr;
		}

		kotek::static_path_t mesh_path;

		p_filesystem->Make_Path(mesh_path,
			kotek::core::eFolderIndex::kFolderIndex_DataGame_Models);

		mesh_path /= p_mesh_name;

		zircon_gltf_error_t load_error;

		eZirconGltfLoadStatus load_status = zircon_gltf_load_from_file(
			p_filesystem, mesh_path, this->m_mesh_file_buffer,
			sizeof(this->m_mesh_file_buffer), this->m_mesh_scratch,
			load_error);

		if (load_status != eZirconGltfLoadStatus::kSuccess)
		{
			KOTEK_MESSAGE_WARNING(
				"[model_static] mesh '{}' failed to load (status {}): "
				"{}",
				p_mesh_name, static_cast<int>(load_status),
				load_error.c_str());

			p_free_slot->m_is_failed = true;
			return nullptr;
		}

		// an empty (but valid) model draws nothing and needs no buffers
		if (this->m_mesh_scratch.m_vertices.empty() ||
			this->m_mesh_scratch.m_indices.empty() ||
			this->m_mesh_scratch.m_submeshes.empty())
		{
			KOTEK_MESSAGE_WARNING(
				"[model_static] mesh '{}' holds no drawable geometry",
				p_mesh_name);

			return p_free_slot;
		}

		// upload staging: the decoded mesh is repacked into the pass's
		// vertex format (the loader's texcoords are dropped until a
		// textured technique exists) inside the file scratch — its
		// content was fully decoded, so overwriting it is safe; the
		// 16-bit index array follows the vertex array (the static_assert
		// at the top proves the pair always fits)
		zircon_model_static_vertex_t* p_staging_vertices =
			reinterpret_cast<zircon_model_static_vertex_t*>(
				this->m_mesh_file_buffer);

		const kotek::uint32_t vertex_count = static_cast<
			kotek::uint32_t>(this->m_mesh_scratch.m_vertices.size());
		const kotek::uint32_t index_count = static_cast<
			kotek::uint32_t>(this->m_mesh_scratch.m_indices.size());

		for (kotek::uint32_t vertex_index = 0;
			 vertex_index < vertex_count; ++vertex_index)
		{
			const zircon_gltf_vertex_t& source =
				this->m_mesh_scratch.m_vertices[vertex_index];

			zircon_model_static_vertex_t& target =
				p_staging_vertices[vertex_index];

			target.m_position[0] = source.m_position[0];
			target.m_position[1] = source.m_position[1];
			target.m_position[2] = source.m_position[2];
			target.m_normal[0] = source.m_normal[0];
			target.m_normal[1] = source.m_normal[1];
			target.m_normal[2] = source.m_normal[2];
			// opaque white until materials drive the color (P2g)
			target.m_color_abgr = 0xffffffffu;
		}

		kotek::uint16_t* p_staging_indices =
			reinterpret_cast<kotek::uint16_t*>(
				this->m_mesh_file_buffer +
				vertex_count * sizeof(zircon_model_static_vertex_t));

		for (kotek::uint32_t index_index = 0; index_index < index_count;
			 ++index_index)
		{
			// the loader caps vertices below 65536, so the narrowing
			// can't truncate
			p_staging_indices[index_index] = static_cast<
				kotek::uint16_t>(
				this->m_mesh_scratch.m_indices[index_index]);
		}

		p_free_slot->m_vertex_buffer = bgfx::createVertexBuffer(
			bgfx::copy(p_staging_vertices,
				vertex_count * sizeof(zircon_model_static_vertex_t)),
			this->m_layout);

		p_free_slot->m_index_buffer = bgfx::createIndexBuffer(
			bgfx::copy(p_staging_indices,
				index_count * sizeof(kotek::uint16_t)));

		if (bgfx::isValid(p_free_slot->m_vertex_buffer) == false ||
			bgfx::isValid(p_free_slot->m_index_buffer) == false)
		{
			KOTEK_MESSAGE_WARNING(
				"[model_static] mesh '{}' failed to upload its buffers",
				p_mesh_name);

			if (bgfx::isValid(p_free_slot->m_vertex_buffer))
			{
				bgfx::destroy(p_free_slot->m_vertex_buffer);
				p_free_slot->m_vertex_buffer = BGFX_INVALID_HANDLE;
			}

			if (bgfx::isValid(p_free_slot->m_index_buffer))
			{
				bgfx::destroy(p_free_slot->m_index_buffer);
				p_free_slot->m_index_buffer = BGFX_INVALID_HANDLE;
			}

			p_free_slot->m_is_failed = true;
			return nullptr;
		}

		for (const auto& submesh : this->m_mesh_scratch.m_submeshes)
		{
			zircon_render_pass_model_static_mesh_slot_t::submesh_t
				target{};

			std::memcpy(target.m_world_matrix, submesh.m_world_matrix,
				sizeof(target.m_world_matrix));

			target.m_index_offset = submesh.m_index_offset;
			target.m_index_count = submesh.m_index_count;

			p_free_slot->m_submeshes.push_back(target);
		}

		return p_free_slot;
	}

	void zircon_render_graph_pass_model_static_bgfx::
		multiply_model_matrices(const float* p_a, const float* p_b,
			float* p_out) noexcept
	{
		float result[16];

		for (int row = 0; row < 4; ++row)
		{
			for (int column = 0; column < 4; ++column)
			{
				float sum = 0.0f;

				for (int step = 0; step < 4; ++step)
				{
					sum += p_a[row * 4 + step] * p_b[step * 4 + column];
				}

				result[row * 4 + column] = sum;
			}
		}

		std::memcpy(p_out, result, sizeof(result));
	}

	bgfx::ShaderHandle
	zircon_render_graph_pass_model_static_bgfx::load_shader_blob(
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

		// the backend-dialect directory of the active renderer — the
		// shader pipeline emits one blob set per backend under
		// shader_cache/bgfx/ (vocabulary per AGENTS.md §5a: vulkan, dx11,
		// dx12, ...)
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
				"[model_static] no shader-blob dialect directory for "
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
			buffer[zircon_DEF_RENDER_PASS_MODEL_STATIC_SHADER_BIN_MAX_SIZE];

		kotek::uint8_t* p_buffer = buffer;
		kotek::size_t buffer_length = sizeof(buffer);

		bool read_status =
			p_filesystem->Read_File(shader_path, p_buffer, buffer_length);

		if (read_status && buffer_length)
		{
			// bgfx::copy hands bgfx its own copy, so the filesystem's
			// buffer (or its internal cache for oversized reads) is not
			// referenced past this call
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
				"[model_static] failed to read shader blob '{}'",
				p_shader_file_name);
		}

		return result;
	}
} // namespace no_streaming
