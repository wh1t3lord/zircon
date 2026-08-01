// zircon_shaderpack — wraps a compiled backend shader blob into bgfx's binary
// shader container (.bin), byte-compatible with what bgfx's own shaderc writes
// (container/bin version 11) and with what bgfx's renderer backends read.
//
// Slang is zircon's only user shading language; slangc emits the raw blobs
// (SPIR-V for bgfx-vulkan, DXIL for NRI-dx12). bgfx's shaderc was evaluated
// and rejected: it only parses its own $input/$output macro dialect, so the
// container is packed here instead. NRI consumes the raw DXIL directly — this
// tool exists for the bgfx container only.
//
// Container layout (all integers little-endian), cross-checked against bgfx
// 1.129.8940-496 (vcpkg pin, bgfx commit 7a70927d2af7ffcc5cef9c9e8f4c821e31fcb099):
//
//   u32 magic     'VSH'/'FSH'/'CSH' | bin version 11 (BX_MAKEFOURCC)
//                 writer: bgfx/tools/shaderc/shaderc.cpp:2209-2225
//   u32 hashIn    fragment: murmur2a of the SORTED input (varying) names;
//                 vertex: 0. Hash = bx::HashMurmur2A over the concatenated
//                 name bytes, seed 0 (shaderc.cpp:949-985 parseInOut;
//                 bx/src/hash.cpp:155-285 HashMurmur2APod; empty list => 0)
//   u32 hashOut   vertex: murmur2a of the sorted output names; fragment: 0
//   u16 uniformCount
//   uniform[uniformCount]                writer: shaderc_spirv.cpp:338-377
//     u8  nameLen, char name[nameLen]    (no NUL terminator)
//     u8  type | fragmentBit             bgfx UniformType (Sampler=0, End=1,
//                                       Vec4=2, Mat3=3, Mat4=4 —
//                                       bgfx/include/bgfx/bgfx.h:271-285),
//                                       OR'd with kUniformFragmentBit (0x10)
//                                       for fragment shaders
//                                       (bgfx/src/bgfx_p.h:1468-1476);
//                                       samplers also carry kUniformSamplerBit
//                                       (0x20)
//     u8  num       array element count (0 = not an array — d3d reflection
//                   convention, shaderc_hlsl.cpp:487)
//     u16 regIndex  cbuffer-backed uniforms: BYTE offset inside the stage
//                   constant buffer (shaderc_hlsl.cpp:488,
//                   shaderc_spirv.cpp:675-676). Samplers/storage: the SPIR-V
//                   binding instead (shaderc_spirv.cpp:805/811/842)
//     u16 regCount  16-byte registers occupied (vec4=1, mat3=3, mat4=4 per
//                   element, x array elements; shaderc_hlsl.cpp:489,
//                   shaderc_spirv.cpp:689-697)
//     u8  texComponent, u8 texDimension, u16 texFormat
//                   texture metadata for samplers/storage (0 for plain
//                   cbuffer uniforms)
//   u32 blobSize, u8 blob[blobSize]      verbatim backend blob (SPIR-V: starts
//                                        with 03 02 23 07)
//   u8  0                                 single NUL after the blob
//   u8  numAttrs, u16 attrId[numAttrs]   vertex input attributes in SPIR-V
//                                        location order (fragment: 0); ids
//                                        from bgfx/src/vertexlayout.cpp:167-191
//                                        (a_position=1 ... a_texcoord7=0x17),
//                                        0xffff for names bgfx doesn't know
//   u16 size      max(regIndex + regCount*16) over cbuffer-backed uniforms
//                 (base type > UniformType::End), 0 when none — the stage
//                 constant-buffer byte size (shaderc_spirv.cpp:353-356)
//
// Reader (authoritative): bgfx/src/renderer_vk.cpp ShaderVK::create
// (~5006-5297): magic/hashIn/hashOut (~5011-5042), uniform loop (~5077-5110),
// blobSize+blob+NUL (~5243-5247), numAttrs+ids (~5267-5280), size (~5293).
//
// bgfx-vulkan binding model (bin version >= 11, i.e. NOT m_oldBindingModel):
// vertex-stage cbuffer at SPIR-V set 0 binding 0, fragment-stage cbuffer at
// set 0 binding 1; textures at binding 2+N with their samplers at 2+16+N
// (shaderc_spirv.cpp:484-488 setShiftBinding; bgfx/src/shader_spirv.h:15-25).
// The Slang sources must declare the matching [[vk::binding(...)]] pairs.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	// -------------------------------------------------------------------
	// bx::HashMurmur2A reimplementation (seed 0), from bx/src/hash.cpp
	// (HashMurmur2APod, kMurmur2AMul = 0x5bd1e995) — the hash bgfx uses for
	// the $input/$output name sets. Names are sorted lexicographically and
	// fed one after another with no separators (shaderc.cpp:973-981).
	// -------------------------------------------------------------------
	class murmur2a
	{
	public:
		void add(const void* data, size_t len)
		{
			const uint8_t* p = static_cast<const uint8_t*>(data);
			m_size += static_cast<uint32_t>(len);

			// top up a pending tail first (bx mixTail: only when a tail is
			// pending or the whole input is shorter than one word)
			while (0 != len && (0 < m_count || 4 > len))
			{
				m_tail[m_count++] = *p++;
				--len;

				if (4 == m_count)
				{
					uint32_t kk = static_cast<uint32_t>(m_tail[0]) |
						(static_cast<uint32_t>(m_tail[1]) << 8) |
						(static_cast<uint32_t>(m_tail[2]) << 16) |
						(static_cast<uint32_t>(m_tail[3]) << 24);
					mix(kk);
					m_count = 0;
				}
			}

			while (4 <= len)
			{
				uint32_t kk = static_cast<uint32_t>(p[0]) |
					(static_cast<uint32_t>(p[1]) << 8) |
					(static_cast<uint32_t>(p[2]) << 16) |
					(static_cast<uint32_t>(p[3]) << 24);
				mix(kk);
				p += 4;
				len -= 4;
			}

			while (0 != len)
			{
				m_tail[m_count++] = *p++;
				--len;
			}
		}

		uint32_t end()
		{
			uint32_t kk = 0;
			switch (m_count)
			{
			case 3:
				kk |= static_cast<uint32_t>(m_tail[2]) << 16;
				[[fallthrough]];
			case 2:
				kk |= static_cast<uint32_t>(m_tail[1]) << 8;
				[[fallthrough]];
			case 1:
				kk |= static_cast<uint32_t>(m_tail[0]);
				[[fallthrough]];
			case 0:
				// bx mixes the (zero-padded) tail word even when empty
				mix(kk);
				break;
			}

			mix(m_size);

			m_hash ^= m_hash >> 13;
			m_hash *= k_mul;
			m_hash ^= m_hash >> 15;
			return m_hash;
		}

	private:
		void mix(uint32_t& k)
		{
			k *= k_mul;
			k ^= k >> 24;
			k *= k_mul;

			m_hash *= k_mul;
			m_hash ^= k;
		}

		static constexpr uint32_t k_mul = 0x5bd1e995;

		uint32_t m_hash = 0;
		uint32_t m_size = 0;
		uint8_t m_tail[4] = {};
		uint8_t m_count = 0;
	};

	// hash of a name list exactly as shaderc's parseInOut computes it:
	// sorted lexicographically, concatenated with no separators; empty => 0
	uint32_t hash_names(std::vector<std::string> names)
	{
		if (names.empty())
		{
			return 0;
		}

		std::sort(names.begin(), names.end());

		murmur2a hash;
		for (const std::string& name : names)
		{
			hash.add(name.data(), name.size());
		}
		return hash.end();
	}

	// -------------------------------------------------------------------
	// container constants
	// -------------------------------------------------------------------
	constexpr uint32_t k_magic_vsh = 0x0b485356; // 'VSH' + version 11
	constexpr uint32_t k_magic_fsh = 0x0b485346; // 'FSH' + version 11
	constexpr uint32_t k_magic_csh = 0x0b435348; // 'CSH' + version 11

	// bgfx UniformType::Enum (bgfx.h:276-282) + the type-byte flag bits
	// (bgfx_p.h:1468-1471)
	constexpr uint8_t k_uniform_type_sampler = 0;
	constexpr uint8_t k_uniform_type_vec4 = 2;
	constexpr uint8_t k_uniform_type_mat3 = 3;
	constexpr uint8_t k_uniform_type_mat4 = 4;
	constexpr uint8_t k_uniform_fragment_bit = 0x10;
	constexpr uint8_t k_uniform_sampler_bit = 0x20;
	constexpr uint8_t k_uniform_mask = 0xf0; // kUniformMask

	// bgfx attribute name -> id table (bgfx/src/vertexlayout.cpp:167-191,
	// names in shaderc_spirv.cpp:285-307)
	struct attrib_name_id
	{
		const char* name;
		uint16_t id;
	};

	constexpr attrib_name_id k_attribs[] = {
		{"a_position", 0x0001}, {"a_normal", 0x0002},	 {"a_tangent", 0x0003},
		{"a_bitangent", 0x0004}, {"a_color0", 0x0005},	 {"a_color1", 0x0006},
		{"a_color2", 0x0018},	 {"a_color3", 0x0019},	 {"a_indices", 0x000e},
		{"a_weight", 0x000f},	 {"a_texcoord0", 0x0010}, {"a_texcoord1", 0x0011},
		{"a_texcoord2", 0x0012}, {"a_texcoord3", 0x0013}, {"a_texcoord4", 0x0014},
		{"a_texcoord5", 0x0015}, {"a_texcoord6", 0x0016}, {"a_texcoord7", 0x0017},
	};

	uint16_t attrib_id_for(const std::string& name)
	{
		for (const attrib_name_id& entry : k_attribs)
		{
			if (name == entry.name)
			{
				return entry.id;
			}
		}
		return 0xffff; // unknown to bgfx — shaderc writes UINT16_MAX too
	}

	// -------------------------------------------------------------------
	// little-endian writers (explicit bytes — no host-endianness surprises)
	// -------------------------------------------------------------------
	void write_u8(std::vector<uint8_t>& out, uint8_t v) { out.push_back(v); }

	void write_u16(std::vector<uint8_t>& out, uint16_t v)
	{
		out.push_back(static_cast<uint8_t>(v & 0xff));
		out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
	}

	void write_u32(std::vector<uint8_t>& out, uint32_t v)
	{
		out.push_back(static_cast<uint8_t>(v & 0xff));
		out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
		out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
		out.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
	}

	// -------------------------------------------------------------------
	// little-endian reader cursor for --dump
	// -------------------------------------------------------------------
	struct cursor
	{
		const uint8_t* data;
		size_t size;
		size_t pos = 0;

		bool take_u8(uint8_t& v)
		{
			if (pos + 1 > size)
				return false;
			v = data[pos];
			++pos;
			return true;
		}

		bool take_u16(uint16_t& v)
		{
			if (pos + 2 > size)
				return false;
			v = static_cast<uint16_t>(data[pos] | (data[pos + 1] << 8));
			pos += 2;
			return true;
		}

		bool take_u32(uint32_t& v)
		{
			if (pos + 4 > size)
				return false;
			v = static_cast<uint32_t>(data[pos]) |
				(static_cast<uint32_t>(data[pos + 1]) << 8) |
				(static_cast<uint32_t>(data[pos + 2]) << 16) |
				(static_cast<uint32_t>(data[pos + 3]) << 24);
			pos += 4;
			return true;
		}

		bool take_bytes(size_t n)
		{
			if (pos + n > size)
				return false;
			pos += n;
			return true;
		}
	};

	// -------------------------------------------------------------------
	// uniform table entry (see the layout comment at the top)
	// -------------------------------------------------------------------
	struct uniform_entry
	{
		std::string name;
		uint8_t type = 0; // base type WITHOUT the fragment bit (added at write)
		uint8_t num = 0;
		uint16_t reg_index = 0;
		uint16_t reg_count = 0;
		uint8_t tex_component = 0;
		uint8_t tex_dimension = 0;
		uint16_t tex_format = 0;
	};

	bool parse_uniform_spec(const std::string& spec, uniform_entry& out)
	{
		// name:type:regIndex:regCount[:num[:texComponent:texDimension:texFormat]]
		std::vector<std::string> fields;
		size_t start = 0;
		for (;;)
		{
			const size_t colon = spec.find(':', start);
			if (colon == std::string::npos)
			{
				fields.push_back(spec.substr(start));
				break;
			}
			fields.push_back(spec.substr(start, colon - start));
			start = colon + 1;
		}

		if (fields.size() < 4)
		{
			std::fprintf(stderr,
				"[zircon_shaderpack]: bad --uniform spec '%s' (want "
				"name:type:regIndex:regCount[:num[...]])\n",
				spec.c_str());
			return false;
		}

		const std::string& type = fields[1];
		if (type == "sampler")
			out.type = k_uniform_type_sampler | k_uniform_sampler_bit;
		else if (type == "vec4")
			out.type = k_uniform_type_vec4;
		else if (type == "mat3")
			out.type = k_uniform_type_mat3;
		else if (type == "mat4")
			out.type = k_uniform_type_mat4;
		else
		{
			std::fprintf(stderr,
				"[zircon_shaderpack]: unknown uniform type '%s' in '%s' "
				"(sampler|vec4|mat3|mat4)\n",
				type.c_str(), spec.c_str());
			return false;
		}

		out.name = fields[0];
		out.reg_index = static_cast<uint16_t>(std::stoul(fields[2]));
		out.reg_count = static_cast<uint16_t>(std::stoul(fields[3]));
		if (fields.size() > 4)
			out.num = static_cast<uint8_t>(std::stoul(fields[4]));
		if (fields.size() > 5)
			out.tex_component = static_cast<uint8_t>(std::stoul(fields[5]));
		if (fields.size() > 6)
			out.tex_dimension = static_cast<uint8_t>(std::stoul(fields[6]));
		if (fields.size() > 7)
			out.tex_format = static_cast<uint16_t>(std::stoul(fields[7]));
		return true;
	}

	std::vector<std::string> split_names(const std::string& csv)
	{
		std::vector<std::string> names;
		size_t start = 0;
		for (;;)
		{
			const size_t comma = csv.find(',', start);
			std::string token = csv.substr(
				start, comma == std::string::npos ? comma : comma - start);
			// trim spaces (shaderc's parseInOut splits on ',' or ' ' and
			// trims — ',' is our canonical separator)
			const size_t first = token.find_first_not_of(' ');
			const size_t last = token.find_last_not_of(' ');
			if (first != std::string::npos)
			{
				names.push_back(token.substr(first, last - first + 1));
			}
			if (comma == std::string::npos)
			{
				break;
			}
			start = comma + 1;
		}
		return names;
	}

	bool read_file(const char* path, std::vector<uint8_t>& out)
	{
		FILE* f = std::fopen(path, "rb");
		if (nullptr == f)
		{
			std::fprintf(stderr, "[zircon_shaderpack]: cannot open '%s'\n", path);
			return false;
		}
		std::fseek(f, 0, SEEK_END);
		const long size = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		if (size < 0)
		{
			std::fclose(f);
			std::fprintf(stderr, "[zircon_shaderpack]: cannot size '%s'\n", path);
			return false;
		}
		out.resize(static_cast<size_t>(size));
		const size_t read = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
		std::fclose(f);
		if (read != out.size())
		{
			std::fprintf(stderr, "[zircon_shaderpack]: short read on '%s'\n", path);
			return false;
		}
		return true;
	}

	bool write_file(const char* path, const std::vector<uint8_t>& data)
	{
		FILE* f = std::fopen(path, "wb");
		if (nullptr == f)
		{
			std::fprintf(stderr, "[zircon_shaderpack]: cannot write '%s'\n", path);
			return false;
		}
		const size_t written =
			data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
		std::fclose(f);
		if (written != data.size())
		{
			std::fprintf(stderr, "[zircon_shaderpack]: short write on '%s'\n", path);
			return false;
		}
		return true;
	}

	void print_usage()
	{
		std::printf(
			"zircon_shaderpack — pack a compiled shader blob into bgfx's .bin\n"
			"container (version 11), byte-compatible with bgfx shaderc.\n"
			"\n"
			"pack:\n"
			"  zircon_shaderpack --type v|f --input <blob> --output <bin>\n"
			"      [--in-names a_position,a_color0] [--out-names v_color0]\n"
			"      [--uniform name:type:regIndex:regCount[:num[:texComp:texDim:texFmt]]]...\n"
			"        type: sampler|vec4|mat3|mat4; regIndex = cbuffer byte offset\n"
			"        (sampler: SPIR-V binding); regCount = 16-byte registers\n"
			"      vertex:   --in-names  = attributes (SPIR-V location order)\n"
			"                --out-names = varyings   (hashed)\n"
			"      fragment: --in-names  = varyings   (hashed)\n"
			"\n"
			"inspect:\n"
			"  zircon_shaderpack --dump <bin>\n"
			"  zircon_shaderpack --print-hash name0,name1,...\n");
	}

	int pack(const char* type_arg, const char* input_path, const char* output_path,
		const std::vector<std::string>& in_names,
		const std::vector<std::string>& out_names,
		const std::vector<uniform_entry>& uniforms)
	{
		const bool is_vertex = 0 == std::strcmp(type_arg, "v");
		const bool is_fragment = 0 == std::strcmp(type_arg, "f");

		std::vector<uint8_t> blob;
		if (!read_file(input_path, blob))
		{
			return 2;
		}

		// header (shaderc.cpp:2209-2225): fragment hashes its inputs, vertex
		// hashes its outputs, the other slot is 0
		const uint32_t hash_in = is_fragment ? hash_names(in_names) : 0;
		const uint32_t hash_out = is_vertex ? hash_names(out_names) : 0;

		std::vector<uint8_t> out;
		out.reserve(64 + blob.size());

		write_u32(out, is_vertex ? k_magic_vsh : k_magic_fsh);
		write_u32(out, hash_in);
		write_u32(out, hash_out);

		// uniform table (shaderc_spirv.cpp:338-377 writeUniformArray)
		write_u16(out, static_cast<uint16_t>(uniforms.size()));

		// trailing 'size': max(regIndex + regCount*16) over cbuffer-backed
		// uniforms — bgfx's stage constant-buffer byte size
		uint16_t cb_size = 0;
		for (const uniform_entry& un : uniforms)
		{
			write_u8(out, static_cast<uint8_t>(un.name.size()));
			out.insert(out.end(), un.name.begin(), un.name.end());
			write_u8(out,
				static_cast<uint8_t>(un.type |
					(is_fragment ? k_uniform_fragment_bit : 0)));
			write_u8(out, un.num);
			write_u16(out, un.reg_index);
			write_u16(out, un.reg_count);
			write_u8(out, un.tex_component);
			write_u8(out, un.tex_dimension);
			write_u16(out, un.tex_format);

			if ((un.type & ~k_uniform_mask) > 1) // > UniformType::End
			{
				const uint16_t span = static_cast<uint16_t>(
					un.reg_index + un.reg_count * 16);
				cb_size = cb_size < span ? span : cb_size;
			}
		}

		// blob + NUL (shaderc_spirv.cpp:861-865)
		write_u32(out, static_cast<uint32_t>(blob.size()));
		out.insert(out.end(), blob.begin(), blob.end());
		write_u8(out, 0);

		// attribute table (shaderc_spirv.cpp:867-881): vertex inputs in
		// SPIR-V location order; fragment shaders write zero attributes
		if (is_vertex)
		{
			write_u8(out, static_cast<uint8_t>(in_names.size()));
			for (const std::string& name : in_names)
			{
				const uint16_t id = attrib_id_for(name);
				if (0xffff == id)
				{
					std::fprintf(stderr,
						"[zircon_shaderpack]: warning: attribute '%s' is not a "
						"bgfx attribute, id 0xffff\n",
						name.c_str());
				}
				write_u16(out, id);
			}
		}
		else
		{
			write_u8(out, 0);
		}

		write_u16(out, cb_size);

		if (!write_file(output_path, out))
		{
			return 2;
		}

		std::printf("[zircon_shaderpack]: %s: %u bytes blob -> %u bytes .bin "
					"(hashIn 0x%08x, hashOut 0x%08x, uniforms %u, cb %u bytes)\n",
			output_path, static_cast<unsigned>(blob.size()),
			static_cast<unsigned>(out.size()), hash_in, hash_out,
			static_cast<unsigned>(uniforms.size()), cb_size);
		return 0;
	}

	int dump(const char* path)
	{
		std::vector<uint8_t> data;
		if (!read_file(path, data))
		{
			return 2;
		}

		cursor c{data.data(), data.size(), 0};

		uint32_t magic = 0, hash_in = 0, hash_out = 0;
		uint16_t uniform_count = 0;
		if (!c.take_u32(magic) || !c.take_u32(hash_in) || !c.take_u32(hash_out) ||
			!c.take_u16(uniform_count))
		{
			std::fprintf(stderr, "[zircon_shaderpack]: '%s' truncated header\n",
				path);
			return 3;
		}

		const char* type_name = "unknown";
		if (k_magic_vsh == magic)
			type_name = "vertex";
		else if (k_magic_fsh == magic)
			type_name = "fragment";
		else if (k_magic_csh == magic)
			type_name = "compute";
		else
		{
			std::fprintf(stderr,
				"[zircon_shaderpack]: '%s' bad magic 0x%08x\n", path, magic);
			return 3;
		}

		std::printf("magic       0x%08x (%s, bin version %u)\n", magic, type_name,
			(magic >> 24) & 0xff);
		std::printf("hashIn      0x%08x\n", hash_in);
		std::printf("hashOut     0x%08x\n", hash_out);
		std::printf("uniforms    %u\n", uniform_count);

		for (uint16_t ii = 0; ii < uniform_count; ++ii)
		{
			uint8_t name_len = 0;
			if (!c.take_u8(name_len) || c.pos + name_len > c.size)
			{
				std::fprintf(stderr, "[zircon_shaderpack]: truncated uniform\n");
				return 3;
			}
			std::string name(reinterpret_cast<const char*>(c.data + c.pos),
				name_len);
			c.pos += name_len;

			uint8_t type = 0, num = 0, tex_comp = 0, tex_dim = 0;
			uint16_t reg_index = 0, reg_count = 0, tex_fmt = 0;
			if (!c.take_u8(type) || !c.take_u8(num) || !c.take_u16(reg_index) ||
				!c.take_u16(reg_count) || !c.take_u8(tex_comp) ||
				!c.take_u8(tex_dim) || !c.take_u16(tex_fmt))
			{
				std::fprintf(stderr, "[zircon_shaderpack]: truncated uniform\n");
				return 3;
			}

			const char* base = "?";
			switch (type & ~k_uniform_mask)
			{
			case k_uniform_type_sampler:
				base = "sampler";
				break;
			case k_uniform_type_vec4:
				base = "vec4";
				break;
			case k_uniform_type_mat3:
				base = "mat3";
				break;
			case k_uniform_type_mat4:
				base = "mat4";
				break;
			}
			std::printf("  [%u] '%s' type %s%s%s num %u regIndex %u regCount "
						"%u tex(%u,%u,%u)\n",
				ii, name.c_str(), base,
				(type & k_uniform_fragment_bit) ? "|fragment" : "",
				(type & k_uniform_sampler_bit) ? "|samplerBit" : "", num,
				reg_index, reg_count, tex_comp, tex_dim, tex_fmt);
		}

		uint32_t blob_size = 0;
		if (!c.take_u32(blob_size) || c.pos + blob_size > c.size)
		{
			std::fprintf(stderr, "[zircon_shaderpack]: truncated blob\n");
			return 3;
		}
		std::printf("blob        %u bytes at offset 0x%zx (head %02x %02x %02x "
					"%02x)\n",
			blob_size, c.pos,
			blob_size > 0 ? c.data[c.pos] : 0, blob_size > 1 ? c.data[c.pos + 1] : 0,
			blob_size > 2 ? c.data[c.pos + 2] : 0,
			blob_size > 3 ? c.data[c.pos + 3] : 0);
		c.pos += blob_size;

		uint8_t nul = 0, num_attrs = 0;
		uint16_t cb_size = 0;
		if (!c.take_u8(nul) || !c.take_u8(num_attrs))
		{
			std::fprintf(stderr, "[zircon_shaderpack]: truncated tail\n");
			return 3;
		}
		std::printf("nul         %u\n", nul);
		std::printf("attributes  %u\n", num_attrs);
		for (uint8_t ii = 0; ii < num_attrs; ++ii)
		{
			uint16_t id = 0;
			if (!c.take_u16(id))
			{
				std::fprintf(stderr, "[zircon_shaderpack]: truncated attrs\n");
				return 3;
			}
			const char* name = "?";
			for (const attrib_name_id& entry : k_attribs)
			{
				if (entry.id == id)
				{
					name = entry.name;
					break;
				}
			}
			std::printf("  [%u] id 0x%04x (%s)\n", ii, id, name);
		}
		if (!c.take_u16(cb_size))
		{
			std::fprintf(stderr, "[zircon_shaderpack]: truncated size\n");
			return 3;
		}
		std::printf("cb size     %u\n", cb_size);
		std::printf("total       %zu bytes (parsed %zu%s)\n", c.size, c.pos,
			c.pos == c.size ? "" : " — TRAILING GARBAGE");
		return c.pos == c.size ? 0 : 3;
	}
} // namespace

int main(int argc, char** argv)
{
	const char* type = nullptr;
	const char* input = nullptr;
	const char* output = nullptr;
	const char* dump_path = nullptr;
	const char* hash_names_arg = nullptr;
	std::vector<std::string> in_names;
	std::vector<std::string> out_names;
	std::vector<uniform_entry> uniforms;

	for (int ii = 1; ii < argc; ++ii)
	{
		const char* arg = argv[ii];
		auto need_value = [&](const char* key) -> const char* {
			if (ii + 1 >= argc)
			{
				std::fprintf(stderr, "[zircon_shaderpack]: %s needs a value\n",
					key);
				return nullptr;
			}
			return argv[++ii];
		};

		if (0 == std::strcmp(arg, "--type"))
		{
			type = need_value(arg);
		}
		else if (0 == std::strcmp(arg, "--input"))
		{
			input = need_value(arg);
		}
		else if (0 == std::strcmp(arg, "--output"))
		{
			output = need_value(arg);
		}
		else if (0 == std::strcmp(arg, "--in-names"))
		{
			if (const char* v = need_value(arg))
				in_names = split_names(v);
		}
		else if (0 == std::strcmp(arg, "--out-names"))
		{
			if (const char* v = need_value(arg))
				out_names = split_names(v);
		}
		else if (0 == std::strcmp(arg, "--uniform"))
		{
			if (const char* v = need_value(arg))
			{
				uniform_entry un;
				if (!parse_uniform_spec(v, un))
				{
					return 1;
				}
				uniforms.push_back(un);
			}
		}
		else if (0 == std::strcmp(arg, "--dump"))
		{
			dump_path = need_value(arg);
		}
		else if (0 == std::strcmp(arg, "--print-hash"))
		{
			hash_names_arg = need_value(arg);
		}
		else if (0 == std::strcmp(arg, "--help") || 0 == std::strcmp(arg, "-h"))
		{
			print_usage();
			return 0;
		}
		else
		{
			std::fprintf(stderr, "[zircon_shaderpack]: unknown argument '%s'\n",
				arg);
			print_usage();
			return 1;
		}
	}

	if (nullptr != hash_names_arg)
	{
		std::printf("0x%08x\n", hash_names(split_names(hash_names_arg)));
		return 0;
	}

	if (nullptr != dump_path)
	{
		return dump(dump_path);
	}

	if (nullptr == type || nullptr == input || nullptr == output ||
		(0 != std::strcmp(type, "v") && 0 != std::strcmp(type, "f")))
	{
		print_usage();
		return 1;
	}

	if (0 == std::strcmp(type, "f") && !out_names.empty())
	{
		std::fprintf(stderr,
			"[zircon_shaderpack]: warning: fragment shaders hash no outputs; "
			"--out-names ignored\n");
	}

	return pack(type, input, output, in_names, out_names, uniforms);
}
