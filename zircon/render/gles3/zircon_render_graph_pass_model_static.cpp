#include "zircon_render_graph_pass_model_static.h"
#include <kotek.render.gl/include/kotek_render_graph_simplified_node.h>
#include <kotek.render.gl/include/kotek_render_resource_manager.h>
#include <kotek.render.gl/include/kotek_render_geometry_manager.h>

#include "../../ecs/components/zircon_factory.h"
#include "../../game/zircon_game_manager.h"

zircon_render_graph_pass_model_static_gles3::
	zircon_render_graph_pass_model_static_gles3() :
	m_p_factory{},
	m_p_manager_render_resource{}
{
}

zircon_render_graph_pass_model_static_gles3::
	~zircon_render_graph_pass_model_static_gles3()
{
}

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
	kn_kotek::kn_ktk::size_t memory_ssbo_instance_matricies =
		sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t) * 10000;

	storage.Add_Buffer(this->Get_Name(),
		KOTEK_TEXTU("buffer_for_instance_matricies_in_vertex_shader"),
		{GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW, "InstancedMatriciesData", 1,
			memory_ssbo_instance_matricies,
			sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t), this->Get_Name(),
			"buffer_for_instance_matricies_in_vertex_shader"});

	kn_kotek::kn_ktk::size_t memory_camera_data =
		sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t) * 2;

	storage.Add_Buffer(this->Get_Name(),
		KOTEK_TEXTU("buffer_camera_data_in_vertex_shader"),
		{GL_UNIFORM_BUFFER, GL_STATIC_DRAW, "CameraData", 0, memory_camera_data,
			sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t), this->Get_Name(),
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

void zircon_render_graph_pass_model_static_gles3::OnSetupOutput(
	kn_kotek::kn_render::kn_render_gl::ktkRenderGraphSimplifiedStorageOutput&
		storage,
	kn_kotek::kn_core::ktkIRenderDevice* p_device)
{
}

void zircon_render_graph_pass_model_static_gles3::OnCreatedResources(void) {}

void zircon_render_graph_pass_model_static_gles3::OnUpdate() {}

void zircon_render_graph_pass_model_static_gles3::OnRender(
	const kn_kotek::kn_render::kn_render_gl::ktkRenderGraphSimplifiedNode& node)
{
	kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager* p_manager =
		dynamic_cast<
			kn_kotek::kn_render::kn_render_gl::ktkRenderResourceManager*>(
			this->m_p_manager_resource);

	const auto& info_buffer_instance =
		node.Get_Buffer("buffer_for_instance_matricies_in_vertex_shader");

	const auto& info_buffer_camera =
		node.Get_Buffer("buffer_camera_data_in_vertex_shader");

	auto program = node.Get_Program(KOTEK_TEXTU("simple_instancing"));

	this->render_sdk_camera(info_buffer_camera);
	this->render_instances(info_buffer_instance, program);
}

void zircon_render_graph_pass_model_static_gles3::render_sdk_camera(
	const kn_kotek::kn_render::kn_render_gl::ktkBufferModule& buffer_camera)
{
	auto& registry = this->m_p_factory->GetRegistry();
	auto entities = registry.view<zircon_component_sdk_camera>();

	if (!entities.empty())
	{
		auto id = entities[0];

		const auto& component_camera =
			this->m_p_factory->GetComponent<zircon_component_sdk_camera>(
				id);

		glBindBuffer(
			buffer_camera.Get_BufferObjectType(), buffer_camera.Get_Buffer());
		KOTEK_GL_ASSERT();

		glBufferSubData(buffer_camera.Get_BufferObjectType(), 0,
			sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t),
			kn_kotek::kn_ktk::kn_math::value_ptr(
				component_camera.get_camera().get_projection()));
		KOTEK_GL_ASSERT();

		glBufferSubData(buffer_camera.Get_BufferObjectType(),
			sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t),
			sizeof(kn_kotek::kn_ktk::kn_math::mat4x4f_t),
			kn_kotek::kn_ktk::kn_math::value_ptr(
				component_camera.get_camera().get_view()));
		KOTEK_GL_ASSERT();

		glBindBufferBase(buffer_camera.Get_BufferObjectType(),
			buffer_camera.Get_BindingPointIndex(), buffer_camera.Get_Buffer());
		KOTEK_GL_ASSERT();

		glBindBuffer(buffer_camera.Get_BufferObjectType(), 0);
		KOTEK_GL_ASSERT();
	}
}

void zircon_render_graph_pass_model_static_gles3::render_instances(
	const kn_kotek::kn_render::kn_render_gl::ktkBufferModule& buffer_instances,
	GLuint program_id)
{
	auto* p_manager_geometry =
		this->m_p_manager_render_resource->Get_ManagerGeometry();

	if (p_manager_geometry)
	{
		bool is_empty =
			p_manager_geometry->Get_IndirectCommands_PredefinedModels().empty();

		if (is_empty == false)
		{
			glBindBuffer(p_manager_geometry->Get_Buffer_DrawIndirectCommands()
							 ->Get_Target(),
				p_manager_geometry->Get_Buffer_DrawIndirectCommands()
					->Get_Handles()[0]);
			KOTEK_GL_ASSERT();

			glBindBuffer(buffer_instances.Get_BufferObjectType(),
				buffer_instances.Get_Buffer());
			KOTEK_GL_ASSERT();

			glBindBufferBase(buffer_instances.Get_BufferObjectType(),
				buffer_instances.Get_BindingPointIndex(),
				buffer_instances.Get_Buffer());
			KOTEK_GL_ASSERT();

			glBindVertexArray(p_manager_geometry->Get_VAO());
			KOTEK_GL_ASSERT();

			glUseProgram(program_id);
			KOTEK_GL_ASSERT();

			glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, 0);
			KOTEK_GL_ASSERT();
		}
	}
}
