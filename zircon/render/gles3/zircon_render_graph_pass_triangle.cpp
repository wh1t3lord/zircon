#include "zircon_render_graph_pass_triangle.h"
#include <kotek.render.gl/include/kotek_render_graph_simplified_node.h>

zircon_render_graph_pass_triangle_gles3::
	zircon_render_graph_pass_triangle_gles3(void) :
	m_vertex_buffer_object{},
	m_vertex_array_object{}, m_p_main_manager{}, m_vertices{-0.5f, -0.5f, 0.0f,
													 0.5f, -0.5f, 0.0f, 0.0f,
													 0.5f, 0.0f}
{
}

zircon_render_graph_pass_triangle_gles3::
	~zircon_render_graph_pass_triangle_gles3(void)
{
	glDeleteVertexArrays(1, &this->m_vertex_array_object);
	glDeleteBuffers(1, &this->m_vertex_buffer_object);
}

void zircon_render_graph_pass_triangle_gles3::OnSetupInput(
	Kotek::Render::gl::ktkRenderGraphSimplifiedStorageInput& storage,
	Kotek::Core::ktkIRenderDevice* p_device,
	Kotek::Core::ktkFileSystem* p_file_system)
{
	storage.Add_Shader(this->Get_Name(),
		Kotek::Render::gl::eShaderType::kShaderType_Vertex,
		Kotek::Render::gl::ktkRenderGraphShaderTextInfo(
			Kotek::Render::gl::eShaderLoadingDataType::
				kShaderLoadingDataType_SourceCode_TextString,
			KOTEK_TEXTU("triangle"),
			KOTEK_TEXTU("#version 310 es \n precision highp float; \n  layout (location = 0) in "
					"vec3 inPos; void main() { gl_Position = "
					"vec4(inPos.x, inPos.y, inPos.z, 1.0f); }")));

	storage.Add_Shader(this->Get_Name(),
		Kotek::Render::gl::eShaderType::kShaderType_Fragment,
		Kotek::Render::gl::ktkRenderGraphShaderTextInfo(
			Kotek::Render::gl::eShaderLoadingDataType::
				kShaderLoadingDataType_SourceCode_TextString,
			KOTEK_TEXTU("triangle"),
			KOTEK_TEXTU("#version 310 es \n precision highp float; \n out vec4 FragColor; "
					"void main() { FragColor = vec4(0.0f, 0.5f, "
					"1.0f, 1.0f); }")));
}

void zircon_render_graph_pass_triangle_gles3::OnSetupOutput(
	Kotek::Render::gl::ktkRenderGraphSimplifiedStorageOutput& storage,
	Kotek::Core::ktkIRenderDevice* p_device)
{
}

void zircon_render_graph_pass_triangle_gles3::OnCreatedResources(void)
{
	glGenVertexArrays(1, &this->m_vertex_array_object);
	glGenBuffers(1, &this->m_vertex_buffer_object);

	glBindVertexArray(this->m_vertex_array_object);

	glBindBuffer(GL_ARRAY_BUFFER, this->m_vertex_buffer_object);
	glBufferData(GL_ARRAY_BUFFER, sizeof(this->m_vertices), this->m_vertices,
		GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

void zircon_render_graph_pass_triangle_gles3::OnUpdate() {}

void zircon_render_graph_pass_triangle_gles3::OnRender(
	const Kotek::Render::gl::ktkRenderGraphSimplifiedNode& node)
{
	glUseProgram(node.Get_Program(this->Get_Name()));
	glBindVertexArray(this->m_vertex_array_object);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}
