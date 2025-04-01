#include "zircon_render_graph_pass_model_static.h"
#include <kotek.render.gl/include/kotek_render_resource_manager.h>
#include <kotek.render.gl/include/kotek_render_geometry_manager.h>
#include <kotek.render.gl/include/kotek_render_shader_manager.h>

#include "../../ecs/components/zircon_factory.h"
#include "../../engine/zircon_game_manager.h"

zircon_render_graph_pass_model_static_gles3::
	zircon_render_graph_pass_model_static_gles3(
		const kotek::static_u8string_view_t& render_pass_name) :
	kotek::render::gl::ktkRenderGraphSimplifiedRenderPass(
		render_pass_name.data()),
	m_p_factory{}, m_p_manager_render_resource{}
{
}

zircon_render_graph_pass_model_static_gles3::
	zircon_render_graph_pass_model_static_gles3() :
	m_p_factory{}, m_p_manager_render_resource{}
{
}

zircon_render_graph_pass_model_static_gles3::
	~zircon_render_graph_pass_model_static_gles3()
{
}

/*
void zircon_render_graph_pass_model_static_gles3::OnSetupInput(
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
        {GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW, "InstancedMatriciesData", 1,
            memory_ssbo_instance_matricies,
            sizeof(kn_kotek::ktk::math::mat4x4f_t), this->Get_Name(),
            "buffer_for_instance_matricies_in_vertex_shader"});

    kn_kotek::ktk::size_t memory_camera_data =
        sizeof(kn_kotek::ktk::math::mat4x4f_t) * 2;

    storage.Add_Buffer(this->Get_Name(),
        KOTEK_TEXTU("buffer_camera_data_in_vertex_shader"),
        {GL_UNIFORM_BUFFER, GL_STATIC_DRAW, "CameraData", 0, memory_camera_data,
            sizeof(kn_kotek::ktk::math::mat4x4f_t), this->Get_Name(),
            "buffer_camera_data_in_vertex_shader"});

    if (!this->m_p_factory)
    {
        zircon_manager_game* p_game_manager =
            dynamic_cast<zircon_manager_game*>(
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

void zircon_render_graph_pass_model_static_gles3::OnCreateResources(
	kotek::core::ktkMainManager* p_manager_main,
	kotek::core::ktkIRenderResourceManager* p_manager_resource)
{
	this->m_p_manager_render_resource =
		dynamic_cast<kotek::render::gl::ktkRenderResourceManager*>(
			p_manager_resource);

	KOTEK_ASSERT(
		this->m_p_manager_render_resource, "must be a valid resource manager!");

	this->m_p_manager_render_geometry =
		this->m_p_manager_render_resource->Get_ManagerGeometry();

	KOTEK_ASSERT(
		this->m_p_manager_render_geometry, "must be a valid geometry manager!");

	this->m_p_manager_render_shader =
		this->m_p_manager_render_resource->Get_ManagerShader();

	KOTEK_ASSERT(
		this->m_p_manager_render_shader, "must be a valid shader manager!");

	zircon_manager_game* p_manager_game =
		dynamic_cast<zircon_manager_game*>(p_manager_main->GetGameManager());

	KOTEK_ASSERT(p_manager_game, "must be valid!");

	this->m_p_factory = p_manager_game->get_factory_game();

	if (this->m_p_manager_render_shader)
	{
		kotek::static_cstring_t<512> stack_buffer;
		auto shader_vertex = this->m_p_manager_render_shader->Create_Shader(
			stack_buffer, "simple_geometry.vert");
		auto shader_pixel = this->m_p_manager_render_shader->Create_Shader(
			stack_buffer, "simple_geometry.frag");

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

		// todo: think about instancing data and how much objects we can draw...
		// todo: also think about hanlding amount of objects to draw
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

void zircon_render_graph_pass_model_static_gles3::OnDestroyResources()
{
	KOTEK_ASSERT(
		this->m_p_manager_render_shader, "you must register shader manager!");

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

void zircon_render_graph_pass_model_static_gles3::OnUpdate(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	this->update_sdk_camera();
	this->update_instances();
}

void zircon_render_graph_pass_model_static_gles3::OnRender(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	//	kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager* p_manager =
	//		dynamic_cast<
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

void zircon_render_graph_pass_model_static_gles3::update_sdk_camera()
{
	if (!this->m_p_factory)
		return;

	auto& registry = this->m_p_factory->GetRegistry();
	auto entities = registry.view<zircon_component_sdk_camera>();

	if (!entities.empty())
	{
		auto id = entities[0];

		const auto& component_camera =
			this->m_p_factory->GetComponent<zircon_component_sdk_camera>(id);

		auto buffer_object_type =
			this->m_shader_buffer_camera.Get_BufferObjectType();
		auto handle_id = this->m_shader_buffer_camera.Get_Buffer();
		auto binding_point_index =
			this->m_shader_buffer_camera.Get_BindingPointIndex();

		glBindBuffer(buffer_object_type, handle_id);
		KOTEK_GL_ASSERT();

		glBufferSubData(buffer_object_type, 0,
			sizeof(kotek::ktk::math::mat4x4f_t),
			kotek::ktk::math::value_ptr(
				component_camera.get_camera().get_projection()));
		KOTEK_GL_ASSERT();

		glBufferSubData(buffer_object_type, sizeof(kotek::ktk::math::mat4x4f_t),
			sizeof(kotek::ktk::math::mat4x4f_t),
			kotek::ktk::math::value_ptr(
				component_camera.get_camera().get_view()));
		KOTEK_GL_ASSERT();

		glBindBufferBase(buffer_object_type, binding_point_index, handle_id);
		KOTEK_GL_ASSERT();

		glBindBuffer(buffer_object_type, 0);
		KOTEK_GL_ASSERT();
	}
}

void zircon_render_graph_pass_model_static_gles3::update_instances()
{
	if (this->m_p_factory)
	{
		auto& registry = this->m_p_factory->GetRegistry();

		auto view =
			registry
				.view<zircon_component_geometry, zircon_component_transform>();

		auto buffer_object_type =
			this->m_shader_buffer_instancing_data.Get_BufferObjectType();
		auto buffer_handle_id =
			this->m_shader_buffer_instancing_data.Get_Buffer();
		auto buffer_binding_point_index =
			this->m_shader_buffer_instancing_data.Get_BindingPointIndex();

		kotek::ktk::math::mat4x4f_t mat;
		mat.Identity();
		mat[2][3] = -13.0f;

		for (auto&& [entity, geometry, transform] : view.each())
		{
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

void zircon_render_graph_pass_model_static_gles3::render_instances()
{
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
					->Get_Handle());
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
}
