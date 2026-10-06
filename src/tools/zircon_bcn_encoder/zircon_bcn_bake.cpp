// zircon_bcn_bake.cpp — the BCn texture bake host tool (task Z24 phase
// B4): raw RGBA8 (.zraw — OUR minimal intake container, see
// src/core/zircon_texture_bcn.h) in, a baked .bcn texture out (BCn
// blocks + the mip chain, encoded OFFLINE through the vendored
// single-file encoders — the plan's "encode at pack/bake time, the
// runtime never transcodes"). The tool writes the entry-layout file
// <root>/textures/<scene>/<name>.bcn; PACKING it into a .kpack is
// zircon_kpacker's job (the two tools compose; the runtime reads both
// shapes through the same filesystem dispatcher).
//
// Host tool: std is allowed (AGENTS.md §2.1 exemption); the embedded
// discipline is kept anyway (bounded reads, no exceptions). The bake
// driver + the format are compiled from the ENGINE sources
// (../../core/zircon_texture_bcn.cpp — the zircon_kpacker pattern:
// writer, reader, tool and tests can never drift on the format); the
// vendored encoders compile as regular sources here.
//
// exit codes: 0 ok, 1 operational failure, 2 usage error

// FIRST: the lowercase-alias surface for the zircon headers below (the
// engine gets it from the PCH; a standalone tool gets it from here —
// see the file's banner)
#include "zircon_tool_alias_prologue.h"

#include "zircon_bcn_encoder_adapter.h"

// the decode seam's implementation (the adapter declares it; the tool
// carries it so the adapter's decode path links if a future tool
// command wants a verification leg — the encoder-only tools pay for it
// in neither size nor run time at these file sizes)
#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

#include <kotek.core.log/include/kotek_log.h>

#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG
	#include <spdlog/sinks/stdout_sinks.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// the logger shim (the zircon_kpacker pattern verbatim): the tool IS the
// log module for the kotek/zircon objects it compiles — the bake driver
// reports through KOTEK_MESSAGE_*, which resolve the logger through
// these module-slot functions; the branch matches the engine tree's log
// backend (the driver's ZIRCON_KOTEK_LOG_LIBRARY)
// ---------------------------------------------------------------------------
KOTEK_BEGIN_NAMESPACE_KOTEK

namespace
{
#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG
	spdlog::logger* g_p_bcn_bake_logger = nullptr;
#elif defined(KOTEK_USE_LOG_LIBRARY_CUSTOM)
	Core::ktkCustomLogger* g_p_bcn_bake_logger = nullptr;
#endif
}

#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG

spdlog::logger* Get_LoggerMain()
{
	return g_p_bcn_bake_logger;
}

spdlog::logger* Get_LoggerMsvcOutput()
{
	return nullptr;
}

void Set_LoggerMain(void* p_logger)
{
	g_p_bcn_bake_logger = static_cast<spdlog::logger*>(p_logger);
}

void Set_LoggerMsvcOutput(void*)
{
}

#elif defined(KOTEK_USE_LOG_LIBRARY_CUSTOM)

Core::ktkCustomLogger* Get_LoggerMain()
{
	return g_p_bcn_bake_logger;
}

Core::ktkCustomLogger* Get_LoggerMsvcOutput()
{
	return nullptr;
}

void Set_LoggerMain(void* p_logger)
{
	g_p_bcn_bake_logger =
		static_cast<Core::ktkCustomLogger*>(p_logger);
}

void Set_LoggerMsvcOutput(void*)
{
}

#endif

KOTEK_END_NAMESPACE_KOTEK

namespace
{
	struct tool_args_t
	{
		bool m_synthesize_boot_checker = false;
		std::string m_input_zraw;
		// the content root the textures/<scene>/<name>.bcn layout is
		// written under — data_game by default (the folder doctrine: game
		// assets live under data_game/; the render loader resolves
		// content-root-relative names through kFolderIndex_DataGame)
		std::string m_root = "data_game";
		std::string m_scene;
		std::string m_name;
		std::string m_class_name = "albedo";
		std::string m_format_name; // empty = the class default
		std::string m_quality_name = "default";
		std::string m_mips = "full";
	};

	void tool_print_help()
	{
		fprintf(stdout,
			"zircon_bcn_bake — the BCn texture bake host tool (task Z24 "
			"B4)\n"
			"\n"
			"bake one .zraw image into a .bcn texture (BCn blocks + the "
			"mip chain, encoded offline; the runtime uploads the blocks "
			"AS-IS — hardware decodes in the sampler):\n"
			"  zircon_bcn_bake --in <file.zraw> --scene <scene> --name "
			"<tex> [--root data_game]\n"
			"      [--class albedo|normal|ao|mask|hdr] [--format "
			"bc1|bc3|bc5|bc7|bc6h]\n"
			"      [--quality fast|default|high] [--mips full|N]\n"
			"\n"
			"synthesize + bake the shipped boot probe (the documented "
			"regeneration command for\n"
			"data_game/textures/boot/boot_checker.bcn):\n"
			"  zircon_bcn_bake --synthesize_boot_checker [--root "
			"data_game] [--quality fast|default|high]\n"
			"\n"
			"class -> default format (the plan's mapping; --format "
			"overrides): albedo=bc7, normal=bc5, ao=bc7, mask=bc1,\n"
			"  hdr=bc6h (ENCODE DEFERRED this phase — no license-clean "
			"single-file BC6H encoder exists)\n"
			"\n"
			"input: .zraw = the house intake container (16-byte header "
			"'ZRAW01' + width + height, then RGBA8) — no PNG/TGA\n"
			"  dependency this phase; real-format intake is the recorded "
			"later task\n"
			"output: <root>/textures/<scene>/<name>.bcn (the folder "
			"doctrine — game assets live under data_game/, the default "
			"root; the render loader resolves content-root-relative "
			"names through kFolderIndex_DataGame. PACKING into a .kpack "
			"is zircon_kpacker's job — the tools compose)\n"
			"mips: full = the chain down to 1x1 (the runtime REQUIRES "
			"the full chain today); N caps it (pack-side experiments "
			"only)\n"
			"\n"
			"exit codes: 0 ok, 1 operational failure, 2 usage error\n");
	}

	bool tool_parse_class(const std::string& name,
		eZirconTextureBcnClass& out_class)
	{
		if (name == "albedo")
			out_class = eZirconTextureBcnClass::kAlbedo;
		else if (name == "normal")
			out_class = eZirconTextureBcnClass::kNormal;
		else if (name == "ao")
			out_class = eZirconTextureBcnClass::kAO;
		else if (name == "mask")
			out_class = eZirconTextureBcnClass::kMask;
		else if (name == "hdr")
			out_class = eZirconTextureBcnClass::kHDR;
		else
			return false;
		return true;
	}

	bool tool_parse_format(const std::string& name,
		eZirconTextureBcnFormat& out_format)
	{
		if (name == "bc1")
			out_format = eZirconTextureBcnFormat::kBC1;
		else if (name == "bc3")
			out_format = eZirconTextureBcnFormat::kBC3;
		else if (name == "bc5")
			out_format = eZirconTextureBcnFormat::kBC5;
		else if (name == "bc7")
			out_format = eZirconTextureBcnFormat::kBC7;
		else if (name == "bc6h")
			out_format = eZirconTextureBcnFormat::kBC6H;
		else
			return false;
		return true;
	}

	bool tool_parse_quality(const std::string& name,
		eZirconBcnQuality& out_quality)
	{
		if (name == "fast")
			out_quality = eZirconBcnQuality::kFast;
		else if (name == "default")
			out_quality = eZirconBcnQuality::kDefault;
		else if (name == "high")
			out_quality = eZirconBcnQuality::kHigh;
		else
			return false;
		return true;
	}

	bool tool_parse_args(int argc, char** argv, tool_args_t& args,
		std::string& out_error)
	{
		for (int i = 1; i < argc; ++i)
		{
			const std::string arg = argv[i];

			auto take_value = [&](std::string& out) -> bool
			{
				if (i + 1 >= argc)
				{
					out_error = "missing a value after '" + arg + "'";
					return false;
				}
				out = argv[++i];
				return true;
			};

			if (arg == "--synthesize_boot_checker")
				args.m_synthesize_boot_checker = true;
			else if (arg == "--in")
			{
				if (take_value(args.m_input_zraw) == false)
					return false;
			}
			else if (arg == "--root")
			{
				if (take_value(args.m_root) == false)
					return false;
			}
			else if (arg == "--scene")
			{
				if (take_value(args.m_scene) == false)
					return false;
			}
			else if (arg == "--name")
			{
				if (take_value(args.m_name) == false)
					return false;
			}
			else if (arg == "--class")
			{
				if (take_value(args.m_class_name) == false)
					return false;
			}
			else if (arg == "--format")
			{
				if (take_value(args.m_format_name) == false)
					return false;
			}
			else if (arg == "--quality")
			{
				if (take_value(args.m_quality_name) == false)
					return false;
			}
			else if (arg == "--mips")
			{
				if (take_value(args.m_mips) == false)
					return false;
			}
			else
			{
				out_error = "unknown argument '" + arg + "'";
				return false;
			}
		}

		if (args.m_synthesize_boot_checker)
		{
			if (args.m_input_zraw.empty() == false)
			{
				out_error =
					"--synthesize_boot_checker and --in are exclusive";
				return false;
			}

			args.m_scene = "boot";
			args.m_name = "boot_checker";
			args.m_class_name = "albedo";
			return true;
		}

		if (args.m_input_zraw.empty())
		{
			out_error = "nothing to bake (need --in <file.zraw> or "
						"--synthesize_boot_checker)";
			return false;
		}

		if (args.m_scene.empty() || args.m_name.empty())
		{
			out_error = "--in needs --scene and --name (the entry path "
						"textures/<scene>/<name>.bcn)";
			return false;
		}

		return true;
	}

	// the shared bake leg: pixels -> .bcn bytes -> the entry-layout file
	// under <root>/textures/<scene>/
	int tool_bake_and_write(const tool_args_t& args,
		const std::vector<uint8_t>& rgba_pixels, uint32_t width,
		uint32_t height)
	{
		eZirconTextureBcnClass content_class;
		if (tool_parse_class(args.m_class_name, content_class) == false)
		{
			fprintf(stderr, "bcn_bake: unknown class '%s'\n",
				args.m_class_name.c_str());
			return 2;
		}

		eZirconTextureBcnFormat format =
			zircon_texture_bcn_default_format_for_class(content_class);

		if (args.m_format_name.empty() == false &&
			tool_parse_format(args.m_format_name, format) == false)
		{
			fprintf(stderr, "bcn_bake: unknown format '%s'\n",
				args.m_format_name.c_str());
			return 2;
		}

		eZirconBcnQuality quality;
		if (tool_parse_quality(args.m_quality_name, quality) == false)
		{
			fprintf(stderr, "bcn_bake: unknown quality '%s'\n",
				args.m_quality_name.c_str());
			return 2;
		}

		uint32_t mip_count = 0;
		if (args.m_mips != "full")
		{
			const int parsed = atoi(args.m_mips.c_str());
			if (parsed < 1)
			{
				fprintf(stderr, "bcn_bake: --mips must be 'full' or a "
								"positive count\n");
				return 2;
			}
			mip_count = static_cast<uint32_t>(parsed);
		}

		zircon_bcn_encoder_context_t encoder_context;
		zircon_bcn_encoder_initialize(encoder_context, quality);

		const uint32_t required = zircon_texture_bcn_total_file_bytes(
			width, height, format,
			mip_count == 0
				? zircon_texture_bcn_full_mip_count(width, height)
				: mip_count);

		std::vector<uint8_t> file_bytes(required);

		size_t file_size = 0;
		const eZirconTextureBcnStatus status = zircon_texture_bcn_build(
			rgba_pixels.data(), width, height, format, content_class,
			mip_count, static_cast<uint8_t>(quality),
			&zircon_bcn_encoder_encode_level, &encoder_context,
			file_bytes.data(), file_bytes.size(), file_size);

		if (status != eZirconTextureBcnStatus::kSuccess)
		{
			fprintf(stderr,
				"bcn_bake: the bake failed with status %d (see the log "
				"above)\n",
				static_cast<int>(status));
			return 1;
		}

		file_bytes.resize(file_size);

		const std::filesystem::path output_directory =
			std::filesystem::path(args.m_root) / "textures" /
			args.m_scene;

		std::error_code ec;
		std::filesystem::create_directories(output_directory, ec);

		const std::filesystem::path output_path =
			output_directory / (args.m_name + ".bcn");

		std::ofstream output(
			output_path, std::ios::binary | std::ios::trunc);

		if (output.is_open() == false)
		{
			fprintf(stderr, "bcn_bake: cannot open '%s' for writing\n",
				output_path.string().c_str());
			return 1;
		}

		output.write(reinterpret_cast<const char*>(file_bytes.data()),
			static_cast<std::streamsize>(file_bytes.size()));
		output.close();

		if (output.good() == false)
		{
			fprintf(stderr, "bcn_bake: the write to '%s' failed\n",
				output_path.string().c_str());
			return 1;
		}

		fprintf(stdout,
			"bcn_bake: wrote '%s' (%u x %u, %u bytes)\n",
			output_path.string().c_str(), width, height,
			static_cast<uint32_t>(file_bytes.size()));

		return 0;
	}

	int tool_run(const tool_args_t& args)
	{
		if (args.m_synthesize_boot_checker)
		{
			// the recipe's single home is zircon.core (the fixture, the
			// tests and this tool can never drift)
			std::vector<uint8_t> pixels(
				ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE *
				ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE * 4);

			zircon_texture_bcn_make_boot_checker_rgba(pixels.data());

			return tool_bake_and_write(args, pixels,
				ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE,
				ZIRCON_DEF_TEXTURE_BCN_BOOT_CHECKER_SIZE);
		}

		// the .zraw intake (bounded — the format cap)
		std::ifstream input(
			args.m_input_zraw, std::ios::binary | std::ios::ate);

		if (input.is_open() == false)
		{
			fprintf(stderr, "bcn_bake: cannot open '%s'\n",
				args.m_input_zraw.c_str());
			return 1;
		}

		const std::streamoff input_size = input.tellg();

		if (input_size <= 0 || static_cast<uint64_t>(input_size) >
				ZIRCON_DEF_TEXTURE_BCN_ZRAW_MAX_BYTES)
		{
			fprintf(stderr,
				"bcn_bake: '%s' is empty or past the zraw cap\n",
				args.m_input_zraw.c_str());
			return 1;
		}

		std::vector<uint8_t> zraw_bytes(
			static_cast<size_t>(input_size));

		input.seekg(0);
		input.read(reinterpret_cast<char*>(zraw_bytes.data()),
			input_size);

		if (input.good() == false)
		{
			fprintf(stderr, "bcn_bake: the read of '%s' failed\n",
				args.m_input_zraw.c_str());
			return 1;
		}

		uint32_t width = 0, height = 0, pixel_offset = 0;

		const eZirconTextureBcnStatus parse_status =
			zircon_texture_bcn_zraw_parse(zraw_bytes.data(),
				zraw_bytes.size(), width, height, pixel_offset);

		if (parse_status != eZirconTextureBcnStatus::kSuccess)
		{
			fprintf(stderr,
				"bcn_bake: '%s' is not a valid .zraw (status %d)\n",
				args.m_input_zraw.c_str(),
				static_cast<int>(parse_status));
			return 1;
		}

		const uint8_t* p_pixels = zraw_bytes.data() + pixel_offset;
		std::vector<uint8_t> pixels(p_pixels,
			p_pixels + static_cast<size_t>(width) * height * 4);

		return tool_bake_and_write(args, pixels, width, height);
	}
} // namespace

int main(int argc, char** argv)
{
	// the console logger for the bake driver's diagnostics (the shim at
	// the top; the branch matches the engine tree's log backend)
#ifdef KOTEK_USE_LOG_LIBRARY_SPDLOG
	auto p_bcn_bake_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
	spdlog::logger bcn_bake_logger("bcn_bake", p_bcn_bake_sink);
	kun_kotek Set_LoggerMain(&bcn_bake_logger);
#elif defined(KOTEK_USE_LOG_LIBRARY_CUSTOM)
	kun_kotek kun_core ktkCustomLogger tool_logger(nullptr, false);
	kun_kotek Set_LoggerMain(&tool_logger);
#endif

	int exit_code = 2;

	if (argc >= 2 &&
		(strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
	{
		tool_print_help();
		exit_code = 0;
	}
	else
	{
		tool_args_t args;
		std::string error;

		if (tool_parse_args(argc, argv, args, error) == false)
		{
			fprintf(stderr, "bcn_bake: %s (try --help)\n", error.c_str());
		}
		else
		{
			exit_code = tool_run(args);
		}
	}

	kun_kotek Set_LoggerMain(nullptr);
	return exit_code;
}
