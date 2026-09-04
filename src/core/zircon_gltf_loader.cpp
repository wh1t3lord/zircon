#include "zircon_gltf_loader.h"

#include <kotek.core.api/include/kotek_api.h>
#include <kotek.core.containers.json/include/kotek_core_containers_json.h>

#include <cstring>

namespace
{
	// ------------------------------------------------------------------
	// json helpers — the backend-portable subset only (compiles against
	// the boost backend AND KOTEK_JSON_LIBRARY=KOTEK_OWN): is_*/as_*,
	// object::find + (*it).value(), array size/operator[], the
	// parse(string_view, error_code&, storage_ptr) overload
	// ------------------------------------------------------------------

	const kotek::json::value* gltf_json_find(
		const kotek::json::object& object, const char* p_key) noexcept
	{
		auto it = object.find(p_key);

		if (it == object.end())
			return nullptr;

		return &(*it).value();
	}

	// strict unsigned: doubles are rejected (indices/sizes must be
	// integers), range-checked before the narrowing cast
	bool gltf_json_to_u32(
		const kotek::json::value& value, kotek::uint32_t& out) noexcept
	{
		if (value.is_uint64())
		{
			const std::uint64_t data = value.as_uint64();

			if (data > 0xffffffffull)
				return false;

			out = static_cast<kotek::uint32_t>(data);
			return true;
		}

		if (value.is_int64())
		{
			const std::int64_t data = value.as_int64();

			if (data < 0 || data > 0xffffffffll)
				return false;

			out = static_cast<kotek::uint32_t>(data);
			return true;
		}

		return false;
	}

	bool gltf_json_to_f64(
		const kotek::json::value& value, double& out) noexcept
	{
		if (value.is_double())
		{
			out = value.as_double();
			return true;
		}

		if (value.is_int64())
		{
			out = static_cast<double>(value.as_int64());
			return true;
		}

		if (value.is_uint64())
		{
			out = static_cast<double>(value.as_uint64());
			return true;
		}

		return false;
	}

	void gltf_set_error(
		zircon_gltf_error_t& out_error, const char* p_message) noexcept
	{
		out_error.assign(p_message);
	}

	// ------------------------------------------------------------------
	// matrix helpers — the engine's model-matrix convention, identical
	// to bx (bx/inline/math.inl): row-major storage, translation at
	// [12..14], rotation laid out like bx::mtxFromQuaternion, multiply
	// like bx::mtxMul (a applied first), points transformed like
	// bx::mul(Vec3, mtx)
	// ------------------------------------------------------------------

	void gltf_mat4_identity(float* p_matrix) noexcept
	{
		for (int element = 0; element < 16; ++element)
			p_matrix[element] = 0.0f;

		p_matrix[0] = 1.0f;
		p_matrix[5] = 1.0f;
		p_matrix[10] = 1.0f;
		p_matrix[15] = 1.0f;
	}

	// out = a x b: out[row*4+col] = sum_k a[row*4+k] * b[k*4+col]
	void gltf_mat4_multiply(
		const float* p_a, const float* p_b, float* p_out) noexcept
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

	void gltf_mat4_transform_point(
		const float* p_matrix, const float* p_point,
		float* p_out) noexcept
	{
		const float x = p_point[0];
		const float y = p_point[1];
		const float z = p_point[2];

		p_out[0] = x * p_matrix[0] + y * p_matrix[4] +
			z * p_matrix[8] + p_matrix[12];
		p_out[1] = x * p_matrix[1] + y * p_matrix[5] +
			z * p_matrix[9] + p_matrix[13];
		p_out[2] = x * p_matrix[2] + y * p_matrix[6] +
			z * p_matrix[10] + p_matrix[14];
	}

	// glTF TRS compose in the engine convention: rotation stored like
	// bx::mtxFromQuaternion, scale folds into the rotation rows (scale
	// applies first), translation at [12..14]
	void gltf_mat4_from_trs(const float* p_translation,
		const float* p_quaternion, const float* p_scale,
		float* p_out) noexcept
	{
		const float qx = p_quaternion[0];
		const float qy = p_quaternion[1];
		const float qz = p_quaternion[2];
		const float qw = p_quaternion[3];

		const float x2 = qx + qx;
		const float y2 = qy + qy;
		const float z2 = qz + qz;
		const float x2x = x2 * qx;
		const float x2y = x2 * qy;
		const float x2z = x2 * qz;
		const float x2w = x2 * qw;
		const float y2y = y2 * qy;
		const float y2z = y2 * qz;
		const float y2w = y2 * qw;
		const float z2z = z2 * qz;
		const float z2w = z2 * qw;

		p_out[0] = (1.0f - (y2y + z2z)) * p_scale[0];
		p_out[1] = (x2y - z2w) * p_scale[0];
		p_out[2] = (x2z + y2w) * p_scale[0];
		p_out[3] = 0.0f;

		p_out[4] = (x2y + z2w) * p_scale[1];
		p_out[5] = (1.0f - (x2x + z2z)) * p_scale[1];
		p_out[6] = (y2z - x2w) * p_scale[1];
		p_out[7] = 0.0f;

		p_out[8] = (x2z - y2w) * p_scale[2];
		p_out[9] = (y2z + x2w) * p_scale[2];
		p_out[10] = (1.0f - (x2x + y2y)) * p_scale[2];
		p_out[11] = 0.0f;

		p_out[12] = p_translation[0];
		p_out[13] = p_translation[1];
		p_out[14] = p_translation[2];
		p_out[15] = 1.0f;
	}

	// ------------------------------------------------------------------
	// glTF payload plumbing
	// ------------------------------------------------------------------

	constexpr kotek::uint32_t _kGltfGlbMagic = 0x46546c67;  // "glTF"
	constexpr kotek::uint32_t _kGltfGlbChunkJson = 0x4e4f534a; // "JSON"
	constexpr kotek::uint32_t _kGltfGlbChunkBin = 0x004e4942;  // "BIN\0"

	constexpr kotek::uint32_t _kGltfComponentFloat = 5126;
	constexpr kotek::uint32_t _kGltfComponentU16 = 5123;
	constexpr kotek::uint32_t _kGltfComponentU32 = 5125;

	struct gltf_buffer_view_t
	{
		const kotek::uint8_t* p_data;
		kotek::uint32_t size;
	};

	struct gltf_buffers_t
	{
		gltf_buffer_view_t entries[zircon_DEF_GLTF_MAX_BUFFER_COUNT];
		kotek::uint32_t count;
	};

	// one accessor resolved to its first element with every range
	// already validated against the real buffer sizes
	struct gltf_accessor_info_t
	{
		const kotek::uint8_t* p_data;
		kotek::uint32_t stride;
		kotek::uint32_t count;
		kotek::uint32_t component_type;
		kotek::uint8_t component_count;
	};

	kotek::uint32_t gltf_accessor_element_size(
		kotek::uint32_t component_type,
		kotek::uint8_t component_count) noexcept
	{
		kotek::uint32_t component_size = 0;

		switch (component_type)
		{
		case _kGltfComponentFloat:
		case _kGltfComponentU32:
		{
			component_size = 4;
			break;
		}
		case _kGltfComponentU16:
		{
			component_size = 2;
			break;
		}
		default:
		{
			return 0;
		}
		}

		return component_size * component_count;
	}

	struct gltf_build_context_t
	{
		const kotek::json::value* p_accessors;    // json arrays
		const kotek::json::value* p_buffer_views; // (nullptr when the key
		const kotek::json::value* p_meshes;       //  is absent)
		const kotek::json::value* p_materials;
		const gltf_buffers_t* p_buffers;
		zircon_gltf_mesh_t* p_mesh;
		// log-once flags ("unsupported: <feature>" is reported one time
		// per file, not per occurrence)
		bool m_logged_attribute;
		bool m_logged_mode;
		bool m_logged_texture;
		bool m_logged_normalized;
		bool m_logged_component_type;
	};

	void gltf_log_unsupported(const char* p_feature) noexcept
	{
		KOTEK_MESSAGE_WARNING(
			"[gltf] unsupported: {} — continuing with base geometry",
			p_feature);
	}

	// resolves accessors[accessor_index]: schema checks + every
	// accessor/bufferView range validated against the real buffer sizes
	// (all arithmetic in 64-bit so corrupt 32-bit fields can't wrap)
	eZirconGltfLoadStatus gltf_resolve_accessor(
		gltf_build_context_t& context,
		kotek::uint32_t accessor_index,
		kotek::uint8_t expected_component_count,
		gltf_accessor_info_t& out_info,
		zircon_gltf_error_t& out_error) noexcept
	{
		if (context.p_accessors == nullptr ||
			context.p_accessors->is_array() == false)
		{
			gltf_set_error(out_error, "missing 'accessors' array");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::array& accessors =
			context.p_accessors->as_array();

		if (accessor_index >= accessors.size())
		{
			gltf_set_error(out_error, "accessor index out of range");
			return eZirconGltfLoadStatus::kError_AccessorOutOfRange;
		}

		const kotek::json::value& accessor = accessors[accessor_index];

		if (accessor.is_object() == false)
		{
			gltf_set_error(out_error, "accessor is not an object");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::object& accessor_object = accessor.as_object();

		kotek::uint32_t count = 0;
		kotek::uint32_t component_type = 0;
		kotek::uint32_t byte_offset = 0;

		const kotek::json::value* p_count =
			gltf_json_find(accessor_object, "count");
		const kotek::json::value* p_component_type =
			gltf_json_find(accessor_object, "componentType");
		const kotek::json::value* p_type =
			gltf_json_find(accessor_object, "type");
		const kotek::json::value* p_byte_offset =
			gltf_json_find(accessor_object, "byteOffset");
		const kotek::json::value* p_buffer_view =
			gltf_json_find(accessor_object, "bufferView");
		const kotek::json::value* p_normalized =
			gltf_json_find(accessor_object, "normalized");
		const kotek::json::value* p_sparse =
			gltf_json_find(accessor_object, "sparse");

		if (p_count == nullptr || p_component_type == nullptr ||
			p_type == nullptr ||
			gltf_json_to_u32(*p_count, count) == false ||
			gltf_json_to_u32(*p_component_type, component_type) ==
				false ||
			p_type->is_string() == false)
		{
			gltf_set_error(out_error,
				"accessor misses count/componentType/type");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (p_sparse)
		{
			gltf_set_error(
				out_error, "sparse accessors are not supported");
			return eZirconGltfLoadStatus::kError_Unsupported;
		}

		if (p_normalized && p_normalized->is_bool() &&
			p_normalized->as_bool())
		{
			if (context.m_logged_normalized == false)
			{
				gltf_log_unsupported("normalized accessors");
				context.m_logged_normalized = true;
			}
		}

		kotek::uint8_t component_count = 0;

		const kotek::json::string& type_string = p_type->as_string();

		if (type_string.size() == 6 &&
			std::memcmp(type_string.data(), "SCALAR", 6) == 0)
		{
			component_count = 1;
		}
		else if (
			type_string.size() == 4 &&
			std::memcmp(type_string.data(), "VEC2", 4) == 0)
		{
			component_count = 2;
		}
		else if (
			type_string.size() == 4 &&
			std::memcmp(type_string.data(), "VEC3", 4) == 0)
		{
			component_count = 3;
		}
		else if (
			type_string.size() == 4 &&
			std::memcmp(type_string.data(), "VEC4", 4) == 0)
		{
			component_count = 4;
		}
		else
		{
			gltf_set_error(
				out_error, "accessor type is outside the lite scope");
			return eZirconGltfLoadStatus::kError_Unsupported;
		}

		if (component_count != expected_component_count)
		{
			gltf_set_error(out_error,
				"accessor vector width mismatches its usage");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::uint32_t element_size =
			gltf_accessor_element_size(component_type, component_count);

		if (element_size == 0)
		{
			if (context.m_logged_component_type == false)
			{
				gltf_log_unsupported(
					"accessor component type (only "
					"float32/uint16/uint32 are decoded)");
				context.m_logged_component_type = true;
			}

			gltf_set_error(out_error,
				"accessor component type is outside the lite scope");
			return eZirconGltfLoadStatus::kError_Unsupported;
		}

		if (p_byte_offset &&
			gltf_json_to_u32(*p_byte_offset, byte_offset) == false)
		{
			gltf_set_error(out_error, "accessor byteOffset is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (count == 0)
		{
			out_info.p_data = nullptr;
			out_info.stride = element_size;
			out_info.count = 0;
			out_info.component_type = component_type;
			out_info.component_count = component_count;
			return eZirconGltfLoadStatus::kSuccess;
		}

		// an accessor without a bufferView is a zero-initialized
		// placeholder (used with sparse overrides) — out of the lite
		// scope
		if (p_buffer_view == nullptr)
		{
			gltf_set_error(out_error,
				"accessors without a bufferView are not supported");
			return eZirconGltfLoadStatus::kError_Unsupported;
		}

		kotek::uint32_t buffer_view_index = 0;

		if (gltf_json_to_u32(*p_buffer_view, buffer_view_index) == false)
		{
			gltf_set_error(out_error, "accessor bufferView is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (context.p_buffer_views == nullptr ||
			context.p_buffer_views->is_array() == false)
		{
			gltf_set_error(out_error, "missing 'bufferViews' array");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::array& buffer_views =
			context.p_buffer_views->as_array();

		if (buffer_view_index >= buffer_views.size())
		{
			gltf_set_error(out_error, "bufferView index out of range");
			return eZirconGltfLoadStatus::kError_AccessorOutOfRange;
		}

		const kotek::json::value& buffer_view =
			buffer_views[buffer_view_index];

		if (buffer_view.is_object() == false)
		{
			gltf_set_error(out_error, "bufferView is not an object");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::object& buffer_view_object =
			buffer_view.as_object();

		kotek::uint32_t buffer_index = 0;
		kotek::uint32_t view_byte_offset = 0;
		kotek::uint32_t view_byte_length = 0;
		kotek::uint32_t byte_stride = 0;

		const kotek::json::value* p_buffer_index =
			gltf_json_find(buffer_view_object, "buffer");
		const kotek::json::value* p_view_offset =
			gltf_json_find(buffer_view_object, "byteOffset");
		const kotek::json::value* p_view_length =
			gltf_json_find(buffer_view_object, "byteLength");
		const kotek::json::value* p_byte_stride =
			gltf_json_find(buffer_view_object, "byteStride");

		if (p_buffer_index == nullptr || p_view_length == nullptr ||
			gltf_json_to_u32(*p_buffer_index, buffer_index) == false ||
			gltf_json_to_u32(*p_view_length, view_byte_length) == false)
		{
			gltf_set_error(out_error,
				"bufferView misses buffer/byteLength");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (p_view_offset &&
			gltf_json_to_u32(*p_view_offset, view_byte_offset) == false)
		{
			gltf_set_error(
				out_error, "bufferView byteOffset is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (p_byte_stride &&
			gltf_json_to_u32(*p_byte_stride, byte_stride) == false)
		{
			gltf_set_error(
				out_error, "bufferView byteStride is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (buffer_index >= context.p_buffers->count)
		{
			gltf_set_error(
				out_error, "bufferView references a missing buffer");
			return eZirconGltfLoadStatus::kError_BufferMissing;
		}

		// tightly packed when no stride is given
		const kotek::uint32_t stride =
			byte_stride ? byte_stride : element_size;

		if (stride < element_size)
		{
			gltf_set_error(out_error,
				"bufferView byteStride is smaller than the element");
			return eZirconGltfLoadStatus::kError_AccessorOutOfRange;
		}

		const gltf_buffer_view_t& buffer =
			context.p_buffers->entries[buffer_index];

		const std::uint64_t view_begin = view_byte_offset;
		const std::uint64_t view_end =
			view_begin + static_cast<std::uint64_t>(view_byte_length);

		if (view_end > buffer.size)
		{
			gltf_set_error(out_error,
				"bufferView range exceeds its buffer");
			return eZirconGltfLoadStatus::kError_AccessorOutOfRange;
		}

		const std::uint64_t accessor_begin =
			view_begin + static_cast<std::uint64_t>(byte_offset);
		const std::uint64_t accessor_span =
			static_cast<std::uint64_t>(count - 1) * stride +
			element_size;

		if (accessor_begin + accessor_span > view_end)
		{
			gltf_set_error(out_error,
				"accessor range exceeds its bufferView");
			return eZirconGltfLoadStatus::kError_AccessorOutOfRange;
		}

		out_info.p_data = buffer.p_data +
			static_cast<kotek::size_t>(accessor_begin);
		out_info.stride = stride;
		out_info.count = count;
		out_info.component_type = component_type;
		out_info.component_count = component_count;

		return eZirconGltfLoadStatus::kSuccess;
	}

	float gltf_read_f32(const kotek::uint8_t* p_data) noexcept
	{
		float result;
		std::memcpy(&result, p_data, sizeof(result));
		return result;
	}

	kotek::uint32_t gltf_read_index(
		const kotek::uint8_t* p_data,
		kotek::uint32_t component_type) noexcept
	{
		if (component_type == _kGltfComponentU16)
		{
			kotek::uint16_t value;
			std::memcpy(&value, p_data, sizeof(value));
			return value;
		}

		kotek::uint32_t value;
		std::memcpy(&value, p_data, sizeof(value));
		return value;
	}

	void gltf_expand_aabb(zircon_gltf_mesh_t& mesh,
		bool& is_first_vertex, const float* p_point) noexcept
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			if (is_first_vertex ||
				p_point[axis] < mesh.m_aabb_min[axis])
			{
				mesh.m_aabb_min[axis] = p_point[axis];
			}

			if (is_first_vertex ||
				p_point[axis] > mesh.m_aabb_max[axis])
			{
				mesh.m_aabb_max[axis] = p_point[axis];
			}
		}

		is_first_vertex = false;
	}

	// decodes one primitive of one node: appends vertices + indices and
	// one submesh range carrying the node's flattened world transform
	eZirconGltfLoadStatus gltf_decode_primitive(
		gltf_build_context_t& context,
		const kotek::json::object& primitive,
		const float* p_world_matrix, bool& io_is_first_vertex,
		zircon_gltf_error_t& out_error) noexcept
	{
		// mode: only TRIANGLES (4, the default) is in the lite scope
		const kotek::json::value* p_mode =
			gltf_json_find(primitive, "mode");

		if (p_mode)
		{
			kotek::uint32_t mode = 0;

			if (gltf_json_to_u32(*p_mode, mode) == false)
			{
				gltf_set_error(out_error, "primitive mode is invalid");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			if (mode != 4)
			{
				if (context.m_logged_mode == false)
				{
					gltf_log_unsupported(
						"primitive mode (only TRIANGLES)");
					context.m_logged_mode = true;
				}

				return eZirconGltfLoadStatus::kSuccess;
			}
		}

		const kotek::json::value* p_attributes =
			gltf_json_find(primitive, "attributes");

		if (p_attributes == nullptr || p_attributes->is_object() == false)
		{
			gltf_set_error(
				out_error, "primitive misses the attributes object");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::object& attributes = p_attributes->as_object();

		const kotek::json::value* p_position =
			gltf_json_find(attributes, "POSITION");
		const kotek::json::value* p_normal =
			gltf_json_find(attributes, "NORMAL");
		const kotek::json::value* p_texcoord =
			gltf_json_find(attributes, "TEXCOORD_0");

		// a primitive without positions carries no geometry at all —
		// skip it (exporters emit those for material-only slots)
		if (p_position == nullptr)
		{
			if (context.m_logged_attribute == false)
			{
				gltf_log_unsupported("primitive without POSITION");
				context.m_logged_attribute = true;
			}

			return eZirconGltfLoadStatus::kSuccess;
		}

		kotek::uint32_t position_accessor = 0;
		kotek::uint32_t normal_accessor = 0;
		kotek::uint32_t texcoord_accessor = 0;

		if (gltf_json_to_u32(*p_position, position_accessor) == false)
		{
			gltf_set_error(out_error, "POSITION accessor is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (p_normal &&
			gltf_json_to_u32(*p_normal, normal_accessor) == false)
		{
			gltf_set_error(out_error, "NORMAL accessor is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (p_texcoord &&
			gltf_json_to_u32(*p_texcoord, texcoord_accessor) == false)
		{
			gltf_set_error(
				out_error, "TEXCOORD_0 accessor is invalid");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		gltf_accessor_info_t positions{};

		eZirconGltfLoadStatus status =
			gltf_resolve_accessor(context, position_accessor, 3,
				positions, out_error);

		if (status != eZirconGltfLoadStatus::kSuccess)
			return status;

		if (positions.component_type != _kGltfComponentFloat)
		{
			if (context.m_logged_component_type == false)
			{
				gltf_log_unsupported(
					"non-float32 POSITION (quantization)");
				context.m_logged_component_type = true;
			}

			return eZirconGltfLoadStatus::kSuccess;
		}

		gltf_accessor_info_t normals{};

		if (p_normal)
		{
			status = gltf_resolve_accessor(context, normal_accessor, 3,
				normals, out_error);

			if (status != eZirconGltfLoadStatus::kSuccess)
				return status;

			if (normals.count != positions.count ||
				normals.component_type != _kGltfComponentFloat)
			{
				gltf_set_error(out_error,
					"NORMAL accessor mismatches POSITION");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}
		}

		gltf_accessor_info_t texcoords{};

		if (p_texcoord)
		{
			status = gltf_resolve_accessor(context, texcoord_accessor,
				2, texcoords, out_error);

			if (status != eZirconGltfLoadStatus::kSuccess)
				return status;

			if (texcoords.count != positions.count ||
				texcoords.component_type != _kGltfComponentFloat)
			{
				gltf_set_error(out_error,
					"TEXCOORD_0 accessor mismatches POSITION");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}
		}

		if (positions.count == 0)
			return eZirconGltfLoadStatus::kSuccess;

		// capacity checks happen before anything is appended, so a
		// half-decoded primitive never lands in the mesh
		if (positions.count >
				zircon_DEF_GLTF_MAX_VERTEX_COUNT ||
			context.p_mesh->m_vertices.size() + positions.count >
				zircon_DEF_GLTF_MAX_VERTEX_COUNT)
		{
			gltf_set_error(out_error,
				"vertex count exceeds zircon_DEF_GLTF_MAX_VERTEX_COUNT");
			return eZirconGltfLoadStatus::kError_CapacityExceeded;
		}

		if (context.p_mesh->m_submeshes.size() >=
			zircon_DEF_GLTF_MAX_SUBMESH_COUNT)
		{
			gltf_set_error(out_error,
				"submesh count exceeds "
				"zircon_DEF_GLTF_MAX_SUBMESH_COUNT");
			return eZirconGltfLoadStatus::kError_CapacityExceeded;
		}

		// the index source: an indices accessor (uint16/uint32) or a
		// generated 0..count-1 sequence for non-indexed primitives
		gltf_accessor_info_t indices{};
		bool has_indices = false;

		const kotek::json::value* p_indices =
			gltf_json_find(primitive, "indices");

		if (p_indices)
		{
			kotek::uint32_t indices_accessor = 0;

			if (gltf_json_to_u32(*p_indices, indices_accessor) == false)
			{
				gltf_set_error(
					out_error, "indices accessor is invalid");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			status = gltf_resolve_accessor(context, indices_accessor, 1,
				indices, out_error);

			if (status != eZirconGltfLoadStatus::kSuccess)
				return status;

			if (indices.component_type != _kGltfComponentU16 &&
				indices.component_type != _kGltfComponentU32)
			{
				if (context.m_logged_component_type == false)
				{
					gltf_log_unsupported(
						"index component type (only "
						"uint16/uint32)");
					context.m_logged_component_type = true;
				}

				return eZirconGltfLoadStatus::kSuccess;
			}

			has_indices = true;
		}

		const kotek::uint32_t index_count =
			has_indices ? indices.count : positions.count;

		if (index_count > zircon_DEF_GLTF_MAX_INDEX_COUNT ||
			context.p_mesh->m_indices.size() + index_count >
				zircon_DEF_GLTF_MAX_INDEX_COUNT)
		{
			gltf_set_error(out_error,
				"index count exceeds zircon_DEF_GLTF_MAX_INDEX_COUNT");
			return eZirconGltfLoadStatus::kError_CapacityExceeded;
		}

		const kotek::uint32_t base_vertex = static_cast<
			kotek::uint32_t>(context.p_mesh->m_vertices.size());
		const kotek::uint32_t index_offset = static_cast<
			kotek::uint32_t>(context.p_mesh->m_indices.size());

		// vertices (zero-filled where an attribute is absent — the
		// mesh flags report it)
		for (kotek::uint32_t vertex_index = 0;
			 vertex_index < positions.count; ++vertex_index)
		{
			zircon_gltf_vertex_t vertex{};

			const kotek::uint8_t* p_position_element =
				positions.p_data +
				static_cast<kotek::size_t>(vertex_index) *
					positions.stride;

			vertex.m_position[0] = gltf_read_f32(p_position_element);
			vertex.m_position[1] =
				gltf_read_f32(p_position_element + 4);
			vertex.m_position[2] =
				gltf_read_f32(p_position_element + 8);

			if (p_normal)
			{
				const kotek::uint8_t* p_normal_element =
					normals.p_data +
					static_cast<kotek::size_t>(vertex_index) *
						normals.stride;

				vertex.m_normal[0] =
					gltf_read_f32(p_normal_element);
				vertex.m_normal[1] =
					gltf_read_f32(p_normal_element + 4);
				vertex.m_normal[2] =
					gltf_read_f32(p_normal_element + 8);
			}

			if (p_texcoord)
			{
				const kotek::uint8_t* p_texcoord_element =
					texcoords.p_data +
					static_cast<kotek::size_t>(vertex_index) *
						texcoords.stride;

				vertex.m_texcoord[0] =
					gltf_read_f32(p_texcoord_element);
				vertex.m_texcoord[1] =
					gltf_read_f32(p_texcoord_element + 4);
			}

			float world_position[3];

			gltf_mat4_transform_point(
				p_world_matrix, vertex.m_position, world_position);

			gltf_expand_aabb(*context.p_mesh, io_is_first_vertex,
				world_position);

			context.p_mesh->m_vertices.push_back(vertex);
		}

		if (p_normal == nullptr)
			context.p_mesh->m_has_normals = false;
		if (p_texcoord == nullptr)
			context.p_mesh->m_has_texcoords = false;

		// indices (validated against the primitive's vertex range —
		// corrupt indices are an error, never an OOB draw)
		for (kotek::uint32_t index_index = 0; index_index < index_count;
			 ++index_index)
		{
			kotek::uint32_t local_index = 0;

			if (has_indices)
			{
				local_index = gltf_read_index(
					indices.p_data +
						static_cast<kotek::size_t>(index_index) *
							indices.stride,
					indices.component_type);
			}
			else
			{
				local_index = index_index;
			}

			if (local_index >= positions.count)
			{
				gltf_set_error(out_error,
					"index points past the primitive's vertices");
				return eZirconGltfLoadStatus::
					kError_AccessorOutOfRange;
			}

			context.p_mesh->m_indices.push_back(
				base_vertex + local_index);
		}

		// the submesh range: world transform + the material note
		// (baseColorFactor / texture presence — nothing beyond that is
		// decoded until a material system exists)
		zircon_gltf_submesh_t submesh{};

		std::memcpy(submesh.m_world_matrix, p_world_matrix,
			sizeof(submesh.m_world_matrix));

		submesh.m_index_offset = index_offset;
		submesh.m_index_count = index_count;
		submesh.m_base_color_factor[0] = 1.0f;
		submesh.m_base_color_factor[1] = 1.0f;
		submesh.m_base_color_factor[2] = 1.0f;
		submesh.m_base_color_factor[3] = 1.0f;
		submesh.m_has_base_color_texture = false;

		const kotek::json::value* p_material =
			gltf_json_find(primitive, "material");

		if (p_material && context.p_materials &&
			context.p_materials->is_array())
		{
			kotek::uint32_t material_index = 0;

			if (gltf_json_to_u32(*p_material, material_index) &&
				material_index < context.p_materials->as_array().size())
			{
				const kotek::json::value& material =
					context.p_materials->as_array()[material_index];

				if (material.is_object())
				{
					const kotek::json::value* p_pbr = gltf_json_find(
						material.as_object(), "pbrMetallicRoughness");

					if (p_pbr && p_pbr->is_object())
					{
						const kotek::json::value* p_factor =
							gltf_json_find(p_pbr->as_object(),
								"baseColorFactor");

						if (p_factor && p_factor->is_array() &&
							p_factor->as_array().size() == 4)
						{
							for (int channel = 0; channel < 4;
								 ++channel)
							{
								double component = 1.0;

								if (gltf_json_to_f64(
										p_factor->as_array()[channel],
										component))
								{
									submesh.m_base_color_factor
										[channel] = static_cast<float>(
										component);
								}
							}
						}

						const kotek::json::value* p_texture =
							gltf_json_find(p_pbr->as_object(),
								"baseColorTexture");

						if (p_texture)
						{
							submesh.m_has_base_color_texture = true;

							if (context.m_logged_texture == false)
							{
								gltf_log_unsupported(
									"textures (noted on the submesh, "
									"not sampled)");
								context.m_logged_texture = true;
							}
						}
					}
				}
			}
		}

		context.p_mesh->m_submeshes.push_back(submesh);

		return eZirconGltfLoadStatus::kSuccess;
	}

	// walks one node of the hierarchy: flattens the parent's world
	// transform with the node's local TRS/matrix and decodes the node's
	// mesh primitives under it
	eZirconGltfLoadStatus gltf_walk_node(gltf_build_context_t& context,
		const kotek::json::array& nodes, kotek::uint32_t node_index,
		const float* p_parent_world, kotek::uint32_t depth,
		bool& io_is_first_vertex,
		zircon_gltf_error_t& out_error) noexcept
	{
		// a cycle or runaway graph trips the depth cap (a valid gltf
		// tree never nests deeper than its node count)
		if (depth > zircon_DEF_GLTF_MAX_NODE_COUNT)
		{
			gltf_set_error(out_error,
				"node graph is cyclic or too deep");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (node_index >= nodes.size())
		{
			gltf_set_error(out_error, "node index out of range");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::value& node = nodes[node_index];

		if (node.is_object() == false)
		{
			gltf_set_error(out_error, "node is not an object");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::object& node_object = node.as_object();

		float local_matrix[16];

		const kotek::json::value* p_matrix =
			gltf_json_find(node_object, "matrix");

		if (p_matrix)
		{
			if (p_matrix->is_array() == false ||
				p_matrix->as_array().size() != 16)
			{
				gltf_set_error(out_error,
					"node matrix must have 16 elements");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			// glTF stores node.matrix column-major — transpose into
			// the engine's row-major model convention
			for (int row = 0; row < 4; ++row)
			{
				for (int column = 0; column < 4; ++column)
				{
					double element = 0.0;

					if (gltf_json_to_f64(
							p_matrix->as_array()[column * 4 + row],
							element) == false)
					{
						gltf_set_error(out_error,
							"node matrix holds a non-number");
						return eZirconGltfLoadStatus::
							kError_JsonMalformed;
					}

					local_matrix[row * 4 + column] =
						static_cast<float>(element);
				}
			}
		}
		else
		{
			float translation[3] = {0.0f, 0.0f, 0.0f};
			float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			float scale[3] = {1.0f, 1.0f, 1.0f};

			const kotek::json::value* p_translation =
				gltf_json_find(node_object, "translation");
			const kotek::json::value* p_rotation =
				gltf_json_find(node_object, "rotation");
			const kotek::json::value* p_scale =
				gltf_json_find(node_object, "scale");

			if (p_translation)
			{
				if (p_translation->is_array() == false ||
					p_translation->as_array().size() != 3)
				{
					gltf_set_error(out_error,
						"node translation must have 3 elements");
					return eZirconGltfLoadStatus::
						kError_JsonMalformed;
				}

				for (int axis = 0; axis < 3; ++axis)
				{
					double element = 0.0;

					if (gltf_json_to_f64(
							p_translation->as_array()[axis],
							element) == false)
					{
						gltf_set_error(out_error,
							"node translation holds a non-number");
						return eZirconGltfLoadStatus::
							kError_JsonMalformed;
					}

					translation[axis] =
						static_cast<float>(element);
				}
			}

			if (p_rotation)
			{
				if (p_rotation->is_array() == false ||
					p_rotation->as_array().size() != 4)
				{
					gltf_set_error(out_error,
						"node rotation must have 4 elements");
					return eZirconGltfLoadStatus::
						kError_JsonMalformed;
				}

				for (int component = 0; component < 4; ++component)
				{
					double element = 0.0;

					if (gltf_json_to_f64(
							p_rotation->as_array()[component],
							element) == false)
					{
						gltf_set_error(out_error,
							"node rotation holds a non-number");
						return eZirconGltfLoadStatus::
							kError_JsonMalformed;
					}

					rotation[component] =
						static_cast<float>(element);
				}
			}

			if (p_scale)
			{
				if (p_scale->is_array() == false ||
					p_scale->as_array().size() != 3)
				{
					gltf_set_error(out_error,
						"node scale must have 3 elements");
					return eZirconGltfLoadStatus::
						kError_JsonMalformed;
				}

				for (int axis = 0; axis < 3; ++axis)
				{
					double element = 0.0;

					if (gltf_json_to_f64(
							p_scale->as_array()[axis],
							element) == false)
					{
						gltf_set_error(out_error,
							"node scale holds a non-number");
						return eZirconGltfLoadStatus::
							kError_JsonMalformed;
					}

					scale[axis] = static_cast<float>(element);
				}
			}

			gltf_mat4_from_trs(
				translation, rotation, scale, local_matrix);
		}

		// row-vector composition: the node's local transform applies
		// first, then the parent's (bx::mtxMul order)
		float world_matrix[16];

		gltf_mat4_multiply(local_matrix, p_parent_world, world_matrix);

		const kotek::json::value* p_mesh_index =
			gltf_json_find(node_object, "mesh");

		if (p_mesh_index)
		{
			kotek::uint32_t mesh_index = 0;

			if (gltf_json_to_u32(*p_mesh_index, mesh_index) == false)
			{
				gltf_set_error(out_error, "node mesh is invalid");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			if (context.p_meshes == nullptr ||
				context.p_meshes->is_array() == false)
			{
				gltf_set_error(out_error, "missing 'meshes' array");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::array& meshes =
				context.p_meshes->as_array();

			if (mesh_index >= meshes.size())
			{
				gltf_set_error(out_error, "mesh index out of range");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::value& mesh = meshes[mesh_index];

			if (mesh.is_object() == false)
			{
				gltf_set_error(out_error, "mesh is not an object");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::value* p_primitives =
				gltf_json_find(mesh.as_object(), "primitives");

			if (p_primitives == nullptr ||
				p_primitives->is_array() == false)
			{
				gltf_set_error(
					out_error, "mesh misses the primitives array");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::array& primitives =
				p_primitives->as_array();

			for (kotek::uint32_t primitive_index = 0;
				 primitive_index < primitives.size(); ++primitive_index)
			{
				const kotek::json::value& primitive =
					primitives[primitive_index];

				if (primitive.is_object() == false)
				{
					gltf_set_error(
						out_error, "primitive is not an object");
					return eZirconGltfLoadStatus::
						kError_JsonMalformed;
				}

				eZirconGltfLoadStatus status = gltf_decode_primitive(
					context, primitive.as_object(), world_matrix,
					io_is_first_vertex, out_error);

				if (status != eZirconGltfLoadStatus::kSuccess)
					return status;
			}
		}

		const kotek::json::value* p_children =
			gltf_json_find(node_object, "children");

		if (p_children)
		{
			if (p_children->is_array() == false)
			{
				gltf_set_error(
					out_error, "node children must be an array");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::array& children = p_children->as_array();

			for (kotek::uint32_t child_slot = 0;
				 child_slot < children.size(); ++child_slot)
			{
				kotek::uint32_t child_index = 0;

				if (gltf_json_to_u32(
						children[child_slot], child_index) == false)
				{
					gltf_set_error(
						out_error, "child node index is invalid");
					return eZirconGltfLoadStatus::
						kError_JsonMalformed;
				}

				eZirconGltfLoadStatus status = gltf_walk_node(context,
					nodes, child_index, world_matrix, depth + 1,
					io_is_first_vertex, out_error);

				if (status != eZirconGltfLoadStatus::kSuccess)
					return status;
			}
		}

		return eZirconGltfLoadStatus::kSuccess;
	}

	// walks the default scene's root nodes (or, when no scenes array
	// exists, every node no other node claims as a child)
	eZirconGltfLoadStatus gltf_build_mesh_from_document(
		const kotek::json::object& root, const gltf_buffers_t& buffers,
		zircon_gltf_mesh_t& out_mesh,
		zircon_gltf_error_t& out_error) noexcept
	{
		const kotek::json::value* p_nodes =
			gltf_json_find(root, "nodes");

		if (p_nodes && p_nodes->is_array() == false)
		{
			gltf_set_error(out_error, "'nodes' must be an array");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (p_nodes == nullptr || p_nodes->as_array().empty())
		{
			// meshes without nodes are not drawable — a valid,
			// empty result
			return eZirconGltfLoadStatus::kSuccess;
		}

		const kotek::json::array& nodes = p_nodes->as_array();

		if (nodes.size() > zircon_DEF_GLTF_MAX_NODE_COUNT)
		{
			gltf_set_error(out_error,
				"node count exceeds zircon_DEF_GLTF_MAX_NODE_COUNT");
			return eZirconGltfLoadStatus::kError_CapacityExceeded;
		}

		gltf_build_context_t context{};

		context.p_accessors = gltf_json_find(root, "accessors");
		context.p_buffer_views = gltf_json_find(root, "bufferViews");
		context.p_meshes = gltf_json_find(root, "meshes");
		context.p_materials = gltf_json_find(root, "materials");
		context.p_buffers = &buffers;
		context.p_mesh = &out_mesh;
		context.m_logged_attribute = false;
		context.m_logged_mode = false;
		context.m_logged_texture = false;
		context.m_logged_normalized = false;
		context.m_logged_component_type = false;

		float identity[16];
		gltf_mat4_identity(identity);

		bool is_first_vertex = true;

		const kotek::json::value* p_scenes =
			gltf_json_find(root, "scenes");

		if (p_scenes && p_scenes->is_array() &&
			p_scenes->as_array().empty() == false)
		{
			kotek::uint32_t scene_index = 0;

			const kotek::json::value* p_scene_index =
				gltf_json_find(root, "scene");

			if (p_scene_index &&
				gltf_json_to_u32(*p_scene_index, scene_index) == false)
			{
				gltf_set_error(out_error, "'scene' is invalid");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::array& scenes = p_scenes->as_array();

			if (scene_index >= scenes.size())
			{
				gltf_set_error(
					out_error, "default scene index out of range");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::value& scene = scenes[scene_index];

			if (scene.is_object() == false)
			{
				gltf_set_error(out_error, "scene is not an object");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::value* p_root_nodes =
				gltf_json_find(scene.as_object(), "nodes");

			if (p_root_nodes)
			{
				if (p_root_nodes->is_array() == false)
				{
					gltf_set_error(out_error,
						"scene nodes must be an array");
					return eZirconGltfLoadStatus::
						kError_JsonMalformed;
				}

				const kotek::json::array& root_nodes =
					p_root_nodes->as_array();

				for (kotek::uint32_t root_slot = 0;
					 root_slot < root_nodes.size(); ++root_slot)
				{
					kotek::uint32_t root_index = 0;

					if (gltf_json_to_u32(root_nodes[root_slot],
							root_index) == false)
					{
						gltf_set_error(out_error,
							"scene root node index is invalid");
						return eZirconGltfLoadStatus::
							kError_JsonMalformed;
					}

					eZirconGltfLoadStatus status = gltf_walk_node(
						context, nodes, root_index, identity, 0,
						is_first_vertex, out_error);

					if (status != eZirconGltfLoadStatus::kSuccess)
						return status;
				}
			}
		}
		else
		{
			// no scenes array: roots are the nodes no other node
			// references as a child
			bool referenced[zircon_DEF_GLTF_MAX_NODE_COUNT] = {};

			for (kotek::uint32_t node_index = 0;
				 node_index < nodes.size(); ++node_index)
			{
				const kotek::json::value& node = nodes[node_index];

				if (node.is_object() == false)
					continue;

				const kotek::json::value* p_children = gltf_json_find(
					node.as_object(), "children");

				if (p_children == nullptr ||
					p_children->is_array() == false)
				{
					continue;
				}

				for (const auto& child : p_children->as_array())
				{
					kotek::uint32_t child_index = 0;

					if (gltf_json_to_u32(child, child_index) &&
						child_index < zircon_DEF_GLTF_MAX_NODE_COUNT)
					{
						referenced[child_index] = true;
					}
				}
			}

			for (kotek::uint32_t node_index = 0;
				 node_index < nodes.size(); ++node_index)
			{
				if (referenced[node_index])
					continue;

				eZirconGltfLoadStatus status =
					gltf_walk_node(context, nodes, node_index,
						identity, 0, is_first_vertex, out_error);

				if (status != eZirconGltfLoadStatus::kSuccess)
					return status;
			}
		}

		return eZirconGltfLoadStatus::kSuccess;
	}

	// logs the out-of-scope top-level features once per file and rejects
	// the one extension class that makes the payload undecodable
	eZirconGltfLoadStatus gltf_check_features(
		const kotek::json::object& root,
		zircon_gltf_error_t& out_error) noexcept
	{
		if (gltf_json_find(root, "skins"))
			gltf_log_unsupported("skins");
		if (gltf_json_find(root, "animations"))
			gltf_log_unsupported("animations");
		if (gltf_json_find(root, "cameras"))
			gltf_log_unsupported("cameras");
		if (gltf_json_find(root, "images") ||
			gltf_json_find(root, "textures") ||
			gltf_json_find(root, "samplers"))
		{
			gltf_log_unsupported("images/textures");
		}

		const kotek::json::value* p_extensions_required =
			gltf_json_find(root, "extensionsRequired");

		if (p_extensions_required && p_extensions_required->is_array())
		{
			for (const auto& extension :
				p_extensions_required->as_array())
			{
				if (extension.is_string() == false)
					continue;

				const kotek::json::string& name =
					extension.as_string();

				// draco replaces the accessor payload — the base
				// geometry is unreadable, this is a hard error
				constexpr const char* _kDraco =
					"KHR_draco_mesh_compression";

				if (name.size() == std::strlen(_kDraco) &&
					std::memcmp(name.data(), _kDraco,
						name.size()) == 0)
				{
					gltf_set_error(out_error,
						"KHR_draco_mesh_compression is not supported");
					return eZirconGltfLoadStatus::kError_Unsupported;
				}

				KOTEK_MESSAGE_WARNING(
					"[gltf] unsupported: required extension '{}' — "
					"continuing with base geometry",
					name.data());
			}
		}

		return eZirconGltfLoadStatus::kSuccess;
	}

	eZirconGltfLoadStatus gltf_check_asset_version(
		const kotek::json::object& root,
		zircon_gltf_error_t& out_error) noexcept
	{
		const kotek::json::value* p_asset =
			gltf_json_find(root, "asset");

		if (p_asset == nullptr || p_asset->is_object() == false)
		{
			gltf_set_error(out_error, "missing 'asset' object");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::value* p_version =
			gltf_json_find(p_asset->as_object(), "version");

		if (p_version == nullptr || p_version->is_string() == false)
		{
			gltf_set_error(out_error, "missing asset version");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::string& version = p_version->as_string();

		// glTF 2.x only (the lite scope); "2.0" is the common spelling
		if (version.size() < 3 || version.data()[0] != '2' ||
			version.data()[1] != '.')
		{
			gltf_set_error(out_error, "not a glTF 2.x asset");
			return eZirconGltfLoadStatus::kError_UnsupportedVersion;
		}

		return eZirconGltfLoadStatus::kSuccess;
	}

	// parses the json text into a DOM (backend-agnostic) and hands the
	// root object to the mesh builder; the scratch resource feeds the
	// DOM and grows by bounded allocation when the document outgrows it
	eZirconGltfLoadStatus gltf_parse_and_build(const char* p_json_text,
		kotek::size_t json_size, const gltf_buffers_t& buffers,
		zircon_gltf_mesh_t& out_mesh,
		zircon_gltf_error_t& out_error) noexcept
	{
		if (json_size > zircon_DEF_GLTF_JSON_CHUNK_MAX_SIZE)
		{
			gltf_set_error(out_error,
				"json chunk exceeds zircon_DEF_GLTF_JSON_CHUNK_MAX_SIZE");
			return eZirconGltfLoadStatus::kError_CapacityExceeded;
		}

		unsigned char dom_scratch[zircon_DEF_GLTF_JSON_DOM_SCRATCH_SIZE];

		kotek::json::monotonic_resource resource(dom_scratch);

		kotek::json::error_code parse_error;

		kotek::json::value document = kotek::json::parse(
			kotek::json::string_view(p_json_text, json_size),
			parse_error, kotek::json::storage_ptr(&resource));

		if (parse_error)
		{
			gltf_set_error(out_error, "json parse failed");
			KOTEK_MESSAGE_WARNING(
				"[gltf] json parse failed: {}", parse_error.message());
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		if (document.is_object() == false)
		{
			gltf_set_error(
				out_error, "the gltf document is not an object");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::object& root = document.as_object();

		eZirconGltfLoadStatus status =
			gltf_check_asset_version(root, out_error);

		if (status != eZirconGltfLoadStatus::kSuccess)
			return status;

		status = gltf_check_features(root, out_error);

		if (status != eZirconGltfLoadStatus::kSuccess)
			return status;

		return gltf_build_mesh_from_document(
			root, buffers, out_mesh, out_error);
	}

	kotek::uint32_t gltf_read_u32(const kotek::uint8_t* p_data) noexcept
	{
		kotek::uint32_t value;
		std::memcpy(&value, p_data, sizeof(value));
		return value;
	}

	// splits a .glb container into its JSON and BIN chunks (every
	// declared length checked against the real size — truncated
	// containers are an error, never an overread)
	eZirconGltfLoadStatus gltf_split_glb(const kotek::uint8_t* p_data,
		kotek::size_t data_size, const kotek::uint8_t*& p_out_json,
		kotek::uint32_t& out_json_size,
		const kotek::uint8_t*& p_out_bin, kotek::uint32_t& out_bin_size,
		zircon_gltf_error_t& out_error) noexcept
	{
		p_out_json = nullptr;
		out_json_size = 0;
		p_out_bin = nullptr;
		out_bin_size = 0;

		if (data_size < 12)
		{
			gltf_set_error(out_error, "glb is smaller than its header");
			return eZirconGltfLoadStatus::kError_BadContainer;
		}

		if (gltf_read_u32(p_data) != _kGltfGlbMagic)
		{
			gltf_set_error(out_error, "bad glb magic");
			return eZirconGltfLoadStatus::kError_BadContainer;
		}

		if (gltf_read_u32(p_data + 4) != 2)
		{
			gltf_set_error(out_error, "glb container version is not 2");
			return eZirconGltfLoadStatus::kError_UnsupportedVersion;
		}

		const kotek::uint32_t declared_length =
			gltf_read_u32(p_data + 8);

		if (declared_length > data_size)
		{
			gltf_set_error(out_error,
				"glb declares more bytes than it holds (truncated)");
			return eZirconGltfLoadStatus::kError_BadContainer;
		}

		// chunks walk the declared span; trailing bytes past it are
		// ignored (tolerant of padded files)
		std::uint64_t cursor = 12;

		while (cursor + 8 <= declared_length)
		{
			const kotek::uint32_t chunk_length = gltf_read_u32(
				p_data + static_cast<kotek::size_t>(cursor));
			const kotek::uint32_t chunk_type = gltf_read_u32(
				p_data + static_cast<kotek::size_t>(cursor) + 4);

			if (static_cast<std::uint64_t>(chunk_length) >
				declared_length - (cursor + 8))
			{
				gltf_set_error(out_error,
					"glb chunk length exceeds the container");
				return eZirconGltfLoadStatus::kError_BadContainer;
			}

			const kotek::uint8_t* p_chunk_data =
				p_data + static_cast<kotek::size_t>(cursor) + 8;

			if (chunk_type == _kGltfGlbChunkJson && p_out_json == nullptr)
			{
				p_out_json = p_chunk_data;
				out_json_size = chunk_length;
			}
			else if (
				chunk_type == _kGltfGlbChunkBin && p_out_bin == nullptr)
			{
				p_out_bin = p_chunk_data;
				out_bin_size = chunk_length;
			}

			// chunk payloads are 4-byte aligned (padded), the length
			// field does not count the padding
			cursor += 8 +
				((static_cast<std::uint64_t>(chunk_length) + 3) & ~3ull);
		}

		if (p_out_json == nullptr)
		{
			gltf_set_error(out_error, "glb holds no JSON chunk");
			return eZirconGltfLoadStatus::kError_BadContainer;
		}

		return eZirconGltfLoadStatus::kSuccess;
	}

	// the shared .glb tail: buffers[0] is the BIN chunk (a uri on it
	// needs a file context the memory path doesn't have)
	eZirconGltfLoadStatus gltf_load_glb_bytes(const kotek::uint8_t* p_data,
		kotek::size_t data_size, zircon_gltf_mesh_t& out_mesh,
		zircon_gltf_error_t& out_error) noexcept
	{
		const kotek::uint8_t* p_json = nullptr;
		const kotek::uint8_t* p_bin = nullptr;
		kotek::uint32_t json_size = 0;
		kotek::uint32_t bin_size = 0;

		eZirconGltfLoadStatus status = gltf_split_glb(p_data, data_size,
			p_json, json_size, p_bin, bin_size, out_error);

		if (status != eZirconGltfLoadStatus::kSuccess)
			return status;

		gltf_buffers_t buffers{};
		buffers.count = 0;

		if (p_bin)
		{
			buffers.entries[0].p_data = p_bin;
			buffers.entries[0].size = bin_size;
			buffers.count = 1;
		}

		return gltf_parse_and_build(
			reinterpret_cast<const char*>(p_json), json_size, buffers,
			out_mesh, out_error);
	}

	bool gltf_has_extension(
		const kotek::static_path_t& path, const char* p_extension,
		kotek::size_t extension_length) noexcept
	{
		const char* p_text = path.c_str();
		const kotek::size_t text_length = std::strlen(p_text);

		if (text_length < extension_length)
			return false;

		const char* p_suffix = p_text + (text_length - extension_length);

		for (kotek::size_t index = 0; index < extension_length; ++index)
		{
			char left = p_suffix[index];
			char right = p_extension[index];

			if (left >= 'A' && left <= 'Z')
				left = static_cast<char>(left - 'A' + 'a');

			if (left != right)
				return false;
		}

		return true;
	}

	// reads a file through the kotek filesystem into caller storage.
	// Get_FileSize(path) answers the size query through the interface
	// (false = the file is absent — graceful since kotek B0, no assert
	// anywhere on the read path), so the capacity check happens before
	// the read and there is no existence/size TOCTOU pair
	eZirconGltfLoadStatus gltf_read_file(
		kotek::core::ktkIFileSystem* p_filesystem,
		const kotek::static_path_t& path_to_file,
		kotek::uint8_t* p_file_buffer,
		kotek::size_t file_buffer_capacity, kotek::size_t& io_used,
		zircon_gltf_error_t& out_error) noexcept
	{
		kotek::size_t file_size = 0;

		if (p_filesystem->Get_FileSize(path_to_file, file_size) == false)
		{
			gltf_set_error(out_error, "file does not exist");
			return eZirconGltfLoadStatus::kError_FileRead;
		}

		if (file_size == 0)
		{
			gltf_set_error(out_error, "file is empty or unreadable");
			return eZirconGltfLoadStatus::kError_FileRead;
		}

		if (file_size > file_buffer_capacity - io_used)
		{
			gltf_set_error(out_error,
				"file exceeds the caller's file buffer");
			return eZirconGltfLoadStatus::kError_FileRead;
		}

		kotek::uint8_t* p_target = p_file_buffer + io_used;
		kotek::size_t target_capacity = file_buffer_capacity - io_used;

		if (p_filesystem->Read_File(
				path_to_file, p_target, target_capacity) == false)
		{
			gltf_set_error(out_error, "file read failed");
			return eZirconGltfLoadStatus::kError_FileRead;
		}

		io_used += static_cast<kotek::size_t>(file_size);

		return eZirconGltfLoadStatus::kSuccess;
	}
} // namespace

eZirconGltfLoadStatus zircon_gltf_load_from_memory(const void* p_glb_data,
	kotek::size_t data_size, zircon_gltf_mesh_t& out_mesh,
	zircon_gltf_error_t& out_error) noexcept
{
	out_mesh.m_vertices.clear();
	out_mesh.m_indices.clear();
	out_mesh.m_submeshes.clear();
	out_mesh.m_aabb_min[0] = 0.0f;
	out_mesh.m_aabb_min[1] = 0.0f;
	out_mesh.m_aabb_min[2] = 0.0f;
	out_mesh.m_aabb_max[0] = 0.0f;
	out_mesh.m_aabb_max[1] = 0.0f;
	out_mesh.m_aabb_max[2] = 0.0f;
	out_mesh.m_has_normals = true;
	out_mesh.m_has_texcoords = true;
	out_error.clear();

	if (p_glb_data == nullptr || data_size == 0)
	{
		gltf_set_error(out_error, "invalid arguments");
		return eZirconGltfLoadStatus::kError_InvalidArguments;
	}

	return gltf_load_glb_bytes(
		static_cast<const kotek::uint8_t*>(p_glb_data), data_size,
		out_mesh, out_error);
}

eZirconGltfLoadStatus zircon_gltf_load_from_file(
	kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& path_to_file,
	kotek::uint8_t* p_file_buffer, kotek::size_t file_buffer_capacity,
	zircon_gltf_mesh_t& out_mesh,
	zircon_gltf_error_t& out_error) noexcept
{
	out_mesh.m_vertices.clear();
	out_mesh.m_indices.clear();
	out_mesh.m_submeshes.clear();
	out_mesh.m_aabb_min[0] = 0.0f;
	out_mesh.m_aabb_min[1] = 0.0f;
	out_mesh.m_aabb_min[2] = 0.0f;
	out_mesh.m_aabb_max[0] = 0.0f;
	out_mesh.m_aabb_max[1] = 0.0f;
	out_mesh.m_aabb_max[2] = 0.0f;
	out_mesh.m_has_normals = true;
	out_mesh.m_has_texcoords = true;
	out_error.clear();

	if (p_filesystem == nullptr || p_file_buffer == nullptr ||
		file_buffer_capacity == 0 || path_to_file.empty())
	{
		gltf_set_error(out_error, "invalid arguments");
		return eZirconGltfLoadStatus::kError_InvalidArguments;
	}

	kotek::size_t used = 0;

	eZirconGltfLoadStatus status =
		gltf_read_file(p_filesystem, path_to_file, p_file_buffer,
			file_buffer_capacity, used, out_error);

	if (status != eZirconGltfLoadStatus::kSuccess)
		return status;

	if (gltf_has_extension(path_to_file, ".glb", 4))
	{
		return gltf_load_glb_bytes(
			p_file_buffer, used, out_mesh, out_error);
	}

	if (gltf_has_extension(path_to_file, ".gltf", 5) == false)
	{
		gltf_set_error(out_error,
			"not a .glb/.gltf file");
		return eZirconGltfLoadStatus::kError_Unsupported;
	}

	// .gltf: the whole file is the json text; buffers resolve as
	// external files packed into the remaining file-buffer space (the
	// DOM owns its strings, the text is not needed after the parse)
	const char* p_json_text =
		reinterpret_cast<const char*>(p_file_buffer);
	const kotek::size_t json_size = used;

	if (json_size > zircon_DEF_GLTF_JSON_CHUNK_MAX_SIZE)
	{
		gltf_set_error(out_error,
			"json chunk exceeds zircon_DEF_GLTF_JSON_CHUNK_MAX_SIZE");
		return eZirconGltfLoadStatus::kError_CapacityExceeded;
	}

	unsigned char dom_scratch[zircon_DEF_GLTF_JSON_DOM_SCRATCH_SIZE];

	kotek::json::monotonic_resource resource(dom_scratch);

	kotek::json::error_code parse_error;

	kotek::json::value document = kotek::json::parse(
		kotek::json::string_view(p_json_text, json_size), parse_error,
		kotek::json::storage_ptr(&resource));

	if (parse_error)
	{
		gltf_set_error(out_error, "json parse failed");
		KOTEK_MESSAGE_WARNING(
			"[gltf] json parse failed: {}", parse_error.message());
		return eZirconGltfLoadStatus::kError_JsonMalformed;
	}

	if (document.is_object() == false)
	{
		gltf_set_error(
			out_error, "the gltf document is not an object");
		return eZirconGltfLoadStatus::kError_JsonMalformed;
	}

	const kotek::json::object& root = document.as_object();

	status = gltf_check_asset_version(root, out_error);

	if (status != eZirconGltfLoadStatus::kSuccess)
		return status;

	status = gltf_check_features(root, out_error);

	if (status != eZirconGltfLoadStatus::kSuccess)
		return status;

	// resolve the buffers array: every entry must be an external file
	// (uri-less buffers belong to the .glb path); data-URIs are out of
	// the lite scope
	gltf_buffers_t buffers{};
	buffers.count = 0;

	const kotek::json::value* p_buffers =
		gltf_json_find(root, "buffers");

	if (p_buffers)
	{
		if (p_buffers->is_array() == false)
		{
			gltf_set_error(out_error, "'buffers' must be an array");
			return eZirconGltfLoadStatus::kError_JsonMalformed;
		}

		const kotek::json::array& buffer_list = p_buffers->as_array();

		if (buffer_list.size() > zircon_DEF_GLTF_MAX_BUFFER_COUNT)
		{
			gltf_set_error(out_error,
				"buffer count exceeds zircon_DEF_GLTF_MAX_BUFFER_COUNT");
			return eZirconGltfLoadStatus::kError_CapacityExceeded;
		}

		for (kotek::uint32_t buffer_index = 0;
			 buffer_index < buffer_list.size(); ++buffer_index)
		{
			const kotek::json::value& buffer = buffer_list[buffer_index];

			if (buffer.is_object() == false)
			{
				gltf_set_error(out_error, "buffer is not an object");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			const kotek::json::object& buffer_object =
				buffer.as_object();

			kotek::uint32_t declared_byte_length = 0;

			const kotek::json::value* p_byte_length =
				gltf_json_find(buffer_object, "byteLength");
			const kotek::json::value* p_uri =
				gltf_json_find(buffer_object, "uri");

			if (p_byte_length == nullptr ||
				gltf_json_to_u32(
					*p_byte_length, declared_byte_length) == false)
			{
				gltf_set_error(
					out_error, "buffer misses byteLength");
				return eZirconGltfLoadStatus::kError_JsonMalformed;
			}

			if (p_uri == nullptr || p_uri->is_string() == false)
			{
				gltf_set_error(out_error,
					"a .gltf buffer without an uri has no data");
				return eZirconGltfLoadStatus::kError_BufferMissing;
			}

			const kotek::json::string& uri = p_uri->as_string();

			constexpr const char* _kDataUriPrefix = "data:";

			if (uri.size() >= std::strlen(_kDataUriPrefix) &&
				std::memcmp(uri.data(), _kDataUriPrefix,
					std::strlen(_kDataUriPrefix)) == 0)
			{
				gltf_set_error(out_error,
					"base64 data-URIs are not supported");
				KOTEK_MESSAGE_WARNING(
					"[gltf] unsupported: base64 data-URI buffer");
				return eZirconGltfLoadStatus::kError_Unsupported;
			}

			if (uri.size() == 0 || uri.data()[0] == '/' ||
				uri.data()[0] == '\\' ||
				(uri.size() > 1 && uri.data()[1] == ':'))
			{
				gltf_set_error(
					out_error, "buffer uri must be a relative file");
				return eZirconGltfLoadStatus::kError_BufferMissing;
			}

			kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
				uri_string;

			if (uri.size() > uri_string.capacity())
			{
				gltf_set_error(out_error, "buffer uri is too long");
				return eZirconGltfLoadStatus::kError_BufferMissing;
			}

			uri_string.assign(uri.data(), uri.size());

			kotek::static_path_t buffer_path =
				path_to_file.parent_path();

			buffer_path /= uri_string.c_str();

			const kotek::size_t buffer_offset = used;

			status = gltf_read_file(p_filesystem, buffer_path,
				p_file_buffer, file_buffer_capacity, used, out_error);

			if (status != eZirconGltfLoadStatus::kSuccess)
				return status;

			const kotek::size_t read_size = used - buffer_offset;

			if (declared_byte_length > read_size)
			{
				gltf_set_error(out_error,
					"buffer declares more bytes than its file holds");
				return eZirconGltfLoadStatus::kError_BufferMissing;
			}

			buffers.entries[buffer_index].p_data =
				p_file_buffer + buffer_offset;
			buffers.entries[buffer_index].size = static_cast<
				kotek::uint32_t>(read_size);
			buffers.count = buffer_index + 1;
		}
	}

	return gltf_build_mesh_from_document(
		root, buffers, out_mesh, out_error);
}
