#include "zircon_render_graph_pass_editor_model_static.h"
#include <kotek.render.gl/include/kotek_render_resource_manager.h>
#include <kotek.render.gl/include/kotek_render_geometry_manager.h>
#include <kotek.render.gl/include/kotek_render_shader_manager.h>

#include "../../../../ecs/zircon_factory.h"
#include "../../../../world/zircon_world.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"

namespace no_streaming
{
	zircon_render_graph_pass_editor_model_static_gles3::
		zircon_render_graph_pass_editor_model_static_gles3() :
		zircon_render_graph_pass_editor(), m_p_manager_render_resource{}
	{
	}

	zircon_render_graph_pass_editor_model_static_gles3::
		~zircon_render_graph_pass_editor_model_static_gles3()
	{
	}

	/*
	void zircon_render_graph_pass_editor_model_static_gles3::OnSetupInput(
	    kn_kotek::kn_render::kn_render_gl::ktkRenderGraphSimplifiedStorageInput&
	        storage,
	    kn_kotek::kn_core::ktkIRenderDevice* p_device,
	    kn_kotek::kn_core::ktkFileSystem* p_file_system)
	{
	    storage.Add_Shader(this->Get_Name(),
	        kn_kotek::kn_render::kn_render_gl::eShaderType::kShaderType_Vertex,
	        kn_kotek::kn_render::kn_render_gl::ktkRenderGraphShaderTextInfo(
	            kn_kotek::kn_render::kn_render_gl::eShaderLoadingDataType::
	                kShaderLoadingDataType_FilePathString,
	            KOTEK_TEXTU("simple_instancing"),
	            KOTEK_TEXTU("simple_geometry.vert")));

	    storage.Add_Shader(this->Get_Name(),
	        kn_kotek::kn_render::kn_render_gl::eShaderType::kShaderType_Fragment,
	        kn_kotek::kn_render::kn_render_gl::ktkRenderGraphShaderTextInfo(
	            kn_kotek::kn_render::kn_render_gl::eShaderLoadingDataType::
	                kShaderLoadingDataType_FilePathString,
	            KOTEK_TEXTU("simple_instancing"),
	            KOTEK_TEXTU("simple_geometry.frag")));

	    // TODO: make field for settings about instance count
	    kn_kotek::ktk::size_t memory_ssbo_instance_matricies =
	        sizeof(kn_kotek::ktk::math::mat4x4f_t) * 10000;

	    storage.Add_Buffer(this->Get_Name(),
	        KOTEK_TEXTU("buffer_for_instance_matricies_in_vertex_shader"),
	        {GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW, "InstancedMatriciesData",
	1, memory_ssbo_instance_matricies, sizeof(kn_kotek::ktk::math::mat4x4f_t),
	this->Get_Name(), "buffer_for_instance_matricies_in_vertex_shader"});

	    kn_kotek::ktk::size_t memory_camera_data =
	        sizeof(kn_kotek::ktk::math::mat4x4f_t) * 2;

	    storage.Add_Buffer(this->Get_Name(),
	        KOTEK_TEXTU("buffer_camera_data_in_vertex_shader"),
	        {GL_UNIFORM_BUFFER, GL_STATIC_DRAW, "CameraData", 0,
	memory_camera_data, sizeof(kn_kotek::ktk::math::mat4x4f_t),
	this->Get_Name(), "buffer_camera_data_in_vertex_shader"});

	    if (!this->m_p_factory)
	    {
	        zircon_game_manager* p_game_manager =
	            dynamic_cast<zircon_game_manager*>(
	                this->m_p_manager_main->GetGameManager());

	        if (p_game_manager)
	        {
	            this->m_p_factory = p_game_manager->get_factory_game();
	            KOTEK_ASSERT(this->m_p_factory,
	                "you must initialize factory before rendering!");
	        }
	    }

	    this->m_p_manager_render_resource = dynamic_cast<
	        kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager*>(
	        this->m_p_manager_resource);
	}
	*/

	void zircon_render_graph_pass_editor_model_static_gles3::OnCreateResources(
		kotek::core::ktkMainManager* p_manager_main,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		this->m_p_manager_render_resource =
			dynamic_cast<kotek::render::gl::ktkRenderResourceManager*>(
				p_manager_resource);

		KOTEK_ASSERT(this->m_p_manager_render_resource,
			"must be a valid resource manager!");

		this->m_p_manager_render_geometry =
			this->m_p_manager_render_resource->Get_ManagerGeometry();

		KOTEK_ASSERT(this->m_p_manager_render_geometry,
			"must be a valid geometry manager!");

		this->m_p_manager_render_shader =
			this->m_p_manager_render_resource->Get_ManagerShader();

		KOTEK_ASSERT(
			this->m_p_manager_render_shader, "must be a valid shader manager!");

		KOTEK_ASSERT(this->m_p_manager_session_editor,
			"must be a valid session editor manager!");

		zircon_session_editor* p_session =
			this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor->get_current_session_id());

		KOTEK_ASSERT(p_session, "must be valid!");

		if (!p_session)
		{
			KOTEK_MESSAGE_WARNING("can't obtain session by id: {}",
				this->m_p_manager_session_editor->get_current_session_id());
			return;
		}

		zircon_world* p_world = p_session->get_world();

		KOTEK_ASSERT(p_world, "session editor_{}#{} must have a world",
			p_session->get_session_name(), p_session->get_id());

		if (!p_world)
		{
			KOTEK_MESSAGE_WARNING("session editor_{}#{} doesn't have world!",
				p_session->get_session_name(), p_session->get_id());
			return;
		}

		zircon_factory* p_factory = p_world->get_factory();

		KOTEK_ASSERT(p_factory, "world doesn't have factory!");

		if (!p_factory)
		{
			KOTEK_MESSAGE_WARNING("world doesn't have a factory!");
			return;
		}

		if (this->m_p_manager_render_shader)
		{
			kotek::static_cstring_t<1024> stack_buffer;
			auto shader_vertex = this->m_p_manager_render_shader->Create_Shader(
				stack_buffer, "gles3/simple_geometry.vert");
			auto shader_pixel = this->m_p_manager_render_shader->Create_Shader(
				stack_buffer, "gles3/simple_geometry.frag");

			this->m_shaders_geometry_color_only =
				this->m_p_manager_render_shader->Create_Program(
					shader_vertex, shader_pixel);

			this->m_p_manager_render_shader->Destroy_Shader(shader_vertex);
			this->m_p_manager_render_shader->Destroy_Shader(shader_pixel);

			// todo: provide struct for easy updating, for now it is hardcoded
			this->m_shader_buffer_camera =
				this->m_p_manager_render_shader->Create_Buffer(
					sizeof(kotek::ktk::math::mat4x4f_t) * 2, GL_UNIFORM_BUFFER,
					GL_STATIC_DRAW, 0
#ifdef KOTEK_DEBUG
					,
					"CameraData"
#endif
				);

			// todo: think about instancing data and how much objects we can
			// draw... todo: also think about hanlding amount of objects to draw
			this->m_shader_buffer_instancing_data =
				this->m_p_manager_render_shader->Create_Buffer(
					sizeof(kn_kotek::ktk::math::mat4x4f_t) * 120,
					GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW, 1
#ifdef KOTEK_DEBUG
					,
					"InstancedMatriciesData"
#endif
				);
		}
	}

	void
	zircon_render_graph_pass_editor_model_static_gles3::OnDestroyResources()
	{
		KOTEK_ASSERT(this->m_p_manager_render_shader,
			"you must register shader manager!");

		if (this->m_p_manager_render_shader)
		{
			this->m_p_manager_render_shader->Destroy_Program(
				this->m_shaders_geometry_color_only);
			this->m_p_manager_render_shader->Destroy_Buffer(
				this->m_shader_buffer_camera);
			this->m_p_manager_render_shader->Destroy_Buffer(
				this->m_shader_buffer_instancing_data);
		}
	}

	void zircon_render_graph_pass_editor_model_static_gles3::OnUpdate(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
		this->update_sdk_camera();
		this->update_instances();
	}

	void zircon_render_graph_pass_editor_model_static_gles3::OnRender(
		const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass)
	{
		//	kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager*
		//p_manager = 		dynamic_cast<
		//			kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager*>(
		//			this->m_p_manager_resource);

		//	const auto& info_buffer_instance =
		//	node.Get_Buffer("buffer_for_instance_matricies_in_vertex_shader");

		//	const auto& info_buffer_camera =
		//		node.Get_Buffer("buffer_camera_data_in_vertex_shader");

		//	auto program = node.Get_Program(KOTEK_TEXTU("simple_instancing"));

		//	this->render_sdk_camera(info_buffer_camera);
		//	this->render_instances(info_buffer_instance, program);

		this->render_instances();
	}

	void zircon_render_graph_pass_editor_model_static_gles3::update_sdk_camera()
	{
		KOTEK_ASSERT(this->m_p_manager_main,
			"must be initialized at that time main manager");
		KOTEK_ASSERT(this->m_p_manager_session_editor,
			"Did you call OnRegisterManagers because session manager editor is "
		    "not "
			"initialized!");

		zircon_session_editor* p_session =
			this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor->get_current_session_id());

		KOTEK_ASSERT(p_session, "must be valid!");

		if (!p_session)
		{
			KOTEK_MESSAGE_WARNING("can't obtain session by id: {}",
				this->m_p_manager_session_editor->get_current_session_id());
			return;
		}

		zircon_world* p_world = p_session->get_world();

		KOTEK_ASSERT(p_world, "session editor_{}#{} must have a world",
			p_session->get_session_name(), p_session->get_id());

		if (!p_world)
		{
			KOTEK_MESSAGE_WARNING("session editor_{}#{} doesn't have world!",
				p_session->get_session_name(), p_session->get_id());
			return;
		}

		zircon_factory* p_factory = p_world->get_factory();

		KOTEK_ASSERT(p_factory, "world doesn't have factory!");

		if (!p_factory)
		{
			KOTEK_MESSAGE_WARNING("world doesn't have a factory!");
			return;
		}

		zircon_ecs_context_t* p_context = p_world->get_ecs_context();

		// pico has no view<>, so scan entities for the (single)
		// sdk_camera component owner
		kotek::entity_t entities
			[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
		kotek::uint32_t entities_count =
			p_factory->get_all_entities(
				p_context,
				p_world->get_entity_count_max_limit(),
				entities, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
			);

		kotek::entity_t id = kotek::ktk::kInvalidECSEntity;

		for (kotek::uint32_t entity_index = 0;
		     entity_index < entities_count; ++entity_index)
		{
			if (p_factory->has_component(
					p_context, entities[entity_index],
					eZirconComponentType::kzircon_component_sdk_camera
				))
			{
				id = entities[entity_index];
				break;
			}
		}

		if (ecs_is_invalid_entity(id) == false)
		{
			auto* p_component_camera = static_cast<
				zircon_component_sdk_camera*>(
				p_factory->get_component_by_enum(
					p_context, id,
					eZirconComponentType::kzircon_component_sdk_camera
				)
			);

			KOTEK_ASSERT(
				p_component_camera, "must be valid component"
			);

			const auto& component_camera = *p_component_camera;

			auto buffer_object_type =
				this->m_shader_buffer_camera.Get_BufferObjectType();
			auto handle_id = this->m_shader_buffer_camera.Get_Buffer();
			auto binding_point_index =
				this->m_shader_buffer_camera.Get_BindingPointIndex();

			glBindBuffer(buffer_object_type, handle_id);
			KOTEK_GL_ASSERT();

			const kotek::ktk::math::matrix4x4f& projection =
				component_camera.get_camera().get_projection();

			glBufferSubData(buffer_object_type, 0,
				sizeof(kotek::ktk::math::mat4x4f_t),
				kotek::ktk::math::value_ptr(projection));
			KOTEK_GL_ASSERT();

			const kotek::ktk::math::matrix4x4f& view =
				component_camera.get_camera().get_view();

			glBufferSubData(buffer_object_type,
				sizeof(kotek::ktk::math::mat4x4f_t),
				sizeof(kotek::ktk::math::mat4x4f_t),
				kotek::ktk::math::value_ptr(view));
			KOTEK_GL_ASSERT();

			glBindBufferBase(
				buffer_object_type, binding_point_index, handle_id);
			KOTEK_GL_ASSERT();

			glBindBuffer(buffer_object_type, 0);
			KOTEK_GL_ASSERT();
		}
	}

	void zircon_render_graph_pass_editor_model_static_gles3::update_instances()
	{
		KOTEK_ASSERT(this->m_p_manager_main,
			"must be initialized at that time main manager");
		KOTEK_ASSERT(this->m_p_manager_session_editor,
			"Did you call OnRegisterManagers because sessino editor manager is "
		    "not "
			"initialized!");

		zircon_session_editor* p_session =
			this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor->get_current_session_id());

		KOTEK_ASSERT(p_session, "must be valid!");

		if (!p_session)
		{
			KOTEK_MESSAGE_WARNING("can't obtain session by id: {}",
				this->m_p_manager_session_editor->get_current_session_id());
			return;
		}

		zircon_world* p_world = p_session->get_world();

		KOTEK_ASSERT(p_world, "session editor_{}#{} must have a world",
			p_session->get_session_name(), p_session->get_id());

		if (!p_world)
		{
			KOTEK_MESSAGE_WARNING("session editor_{}#{} doesn't have world!",
				p_session->get_session_name(), p_session->get_id());
			return;
		}

		zircon_factory* p_factory = p_world->get_factory();

		KOTEK_ASSERT(p_factory, "world doesn't have factory!");

		if (!p_factory)
		{
			KOTEK_MESSAGE_WARNING("world doesn't have a factory!");
			return;
		}

		if (p_factory)
		{
			zircon_ecs_context_t* p_context =
				p_world->get_ecs_context();

			// pico has no view<>, so scan entities and filter by
			// geometry+transform components
			kotek::entity_t entities
				[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
			kotek::uint32_t entities_count =
				p_factory->get_all_entities(
					p_context,
					p_world->get_entity_count_max_limit(),
					entities, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
				);

			auto buffer_object_type =
				this->m_shader_buffer_instancing_data.Get_BufferObjectType();
			auto buffer_handle_id =
				this->m_shader_buffer_instancing_data.Get_Buffer();
			auto buffer_binding_point_index =
				this->m_shader_buffer_instancing_data.Get_BindingPointIndex();

			kotek::ktk::math::mat4x4f_t mat(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);

			for (kotek::uint32_t entity_index = 0;
			     entity_index < entities_count; ++entity_index)
			{
				if (p_factory->has_component(
						p_context, entities[entity_index],
						eZirconComponentType::kzircon_component_geometry
					) == false ||
					p_factory->has_component(
						p_context, entities[entity_index],
						eZirconComponentType::kzircon_component_transform
					) == false)
				{
					continue;
				}

				glBindBuffer(buffer_object_type, buffer_handle_id);
				KOTEK_GL_ASSERT();

				glBufferSubData(buffer_object_type, 0,
					sizeof(kotek::ktk::math::mat4x4f_t),
					kotek::ktk::math::value_ptr(mat));
				KOTEK_GL_ASSERT();

				glBindBufferBase(buffer_object_type, buffer_binding_point_index,
					buffer_handle_id);
				KOTEK_GL_ASSERT();

				glBindBuffer(buffer_object_type, 0);
				KOTEK_GL_ASSERT();
			}
		}
	}

	void zircon_render_graph_pass_editor_model_static_gles3::render_instances()
	{
		if (this->m_p_manager_render_geometry)
		{
			kotek::ktk::uint32_t commands_count =
				this->m_p_manager_render_geometry
					->Get_CurrentIndirectCommandsInUse();
			if (commands_count > 0)
			{
				glBindBuffer(this->m_p_manager_render_geometry
								 ->Get_Buffer_DrawIndirectCommands()
								 ->Get_Target(),
					this->m_p_manager_render_geometry
						->Get_Buffer_DrawIndirectCommands()
						->Get_Handle());
				KOTEK_GL_ASSERT();

				auto buffer_object_type = this->m_shader_buffer_instancing_data
											  .Get_BufferObjectType();
				auto buffer_instancing_handle_id =
					this->m_shader_buffer_instancing_data.Get_Buffer();
				auto buffer_point_index = this->m_shader_buffer_instancing_data
											  .Get_BindingPointIndex();

				glBindBuffer(buffer_object_type, buffer_instancing_handle_id);
				KOTEK_GL_ASSERT();

				glBindBufferBase(buffer_object_type, buffer_point_index,
					buffer_instancing_handle_id);
				KOTEK_GL_ASSERT();

				glBindVertexArray(this->m_p_manager_render_geometry->Get_VAO());
				KOTEK_GL_ASSERT();

				glUseProgram(this->m_shaders_geometry_color_only);
				KOTEK_GL_ASSERT();

				glMultiDrawElementsIndirect(
					GL_TRIANGLES, GL_UNSIGNED_INT, 0, commands_count, 0);
				KOTEK_GL_ASSERT();

				glBindVertexArray(0);
				KOTEK_GL_ASSERT();

				glBindBuffer(buffer_object_type, 0);
				KOTEK_GL_ASSERT();

				glBindBuffer(this->m_p_manager_render_geometry
								 ->Get_Buffer_DrawIndirectCommands()
								 ->Get_Target(),
					0);
				KOTEK_GL_ASSERT();
			}
		}

		/*
		if (this->m_p_manager_render_geometry)
		{
		    bool is_empty = this->m_p_manager_render_geometry
		                        ->Get_IndirectCommands_PredefinedModels()
		                        .empty();

		    if (is_empty == false)
		    {
		        glBindBuffer(this->m_p_manager_render_geometry
		                         ->Get_Buffer_DrawIndirectCommands()
		                         ->Get_Target(),
		            this->m_p_manager_render_geometry
		                ->Get_Buffer_DrawIndirectCommands()
		                ->Get_Handles()[0]);
		        KOTEK_GL_ASSERT();

		        auto buffer_object_type =
		            this->m_shader_buffer_instancing_data.Get_BufferObjectType();
		        auto buffer_instancing_handle_id =
		            this->m_shader_buffer_instancing_data.Get_Buffer();
		        auto buffer_point_index =
		            this->m_shader_buffer_instancing_data.Get_BindingPointIndex();

		        glBindBuffer(buffer_object_type, buffer_instancing_handle_id);
		        KOTEK_GL_ASSERT();

		        glBindBufferBase(buffer_object_type, buffer_point_index,
		            buffer_instancing_handle_id);
		        KOTEK_GL_ASSERT();

		        glBindVertexArray(this->m_p_manager_render_geometry->Get_VAO());
		        KOTEK_GL_ASSERT();

		        glUseProgram(this->m_shaders_geometry_color_only);
		        KOTEK_GL_ASSERT();

		        glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, 0);
		        KOTEK_GL_ASSERT();
		    }
		}
		*/
	}
}