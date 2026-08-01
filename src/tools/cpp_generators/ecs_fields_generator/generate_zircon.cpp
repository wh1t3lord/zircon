#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <clang-c/Index.h>
#include <openssl/md5.h>

namespace fs = std::filesystem;

constexpr int RELEASE_FIELD_LENGTH = 3;
const std::string CHARSET = "0123456789abcdefghijklmnopqrstuvwxyz";

struct Pair {
    std::string first;
    std::string second;
};

class FieldEncoder {
public:
    static std::string encode(uint32_t num) {
        std::string base36;
        while (num) {
            uint32_t rem = num % 36;
            num = num / 36;
            base36 = CHARSET[rem] + base36;
        }
        return std::string(RELEASE_FIELD_LENGTH - base36.length(), '0') + base36;
    }
};

uint32_t get_field_id(const std::string& field_name) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(field_name.c_str()), field_name.size(), digest);
    
    uint32_t numeric_hash = 0;
    for (int i = 0; i < 4; ++i) {
        numeric_hash = (numeric_hash << 8) | digest[i];
    }
    
    uint32_t modulus = 1;
    for (int i = 0; i < RELEASE_FIELD_LENGTH; ++i) {
        modulus *= 36;
    }
    return numeric_hash % modulus;
}

class HeaderGenerator {
private:
    fs::path src_dir;
    fs::path src_kotek_dir;
    std::set<fs::path> target_files;
    std::vector<std::string> output_debug;
    std::vector<std::string> output_release;
    std::map<std::string, std::vector<Pair>> helper_map;
    std::map<std::string, std::vector<std::string>> unit_test_release_map;
    std::vector<std::string> unit_test_str;
    std::string helper_str;
    std::set<std::string> repeated;

    void find_user_headers() {
        std::vector<std::string> extensions = {
            "*component*.h", "*component*.hpp", "*component*.H"
        };
        
        for (const auto& ext : extensions) {
            for (const auto& entry : fs::recursive_directory_iterator(src_dir)) {
                if (fs::is_regular_file(entry) && 
                    (entry.path().filename().string().find("component") != std::string::npos) &&
                    (entry.path().extension() == ".h" || 
                     entry.path().extension() == ".hpp" || 
                     entry.path().extension() == ".H")) {
                    target_files.insert(fs::canonical(entry.path()));
                }
            }
        }
        
        if (target_files.empty()) {
            throw std::runtime_error("No source files found in " + src_dir.string());
        }
    }

    bool is_user_file(const CXCursor& cursor) {
        CXFile file;
        unsigned line, column, offset;
        clang_getFileLocation(clang_getCursorLocation(cursor), &file, &line, &column, &offset);
        if (!file) return false;
        
        CXString cx_filename = clang_getFileName(file);
        std::string filename = clang_getCString(cx_filename);
        clang_disposeString(cx_filename);
        
        if (filename.empty()) return false;
        fs::path file_path = fs::canonical(filename);
        return target_files.find(file_path) != target_files.end();
    }

    std::string get_qualified_name(const CXCursor& cursor) {
        std::vector<std::string> names;
        CXCursor current = cursor;
        
        while (!clang_Cursor_isNull(current)) {
            CXCursorKind kind = clang_getCursorKind(current);
            if (kind == CXCursor_Namespace || kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
                CXString cx_spelling = clang_getCursorSpelling(current);
                std::string spelling = clang_getCString(cx_spelling);
                clang_disposeString(cx_spelling);
                names.push_back(spelling);
            }
            current = clang_getCursorSemanticParent(current);
        }
        
        std::reverse(names.begin(), names.end());
        std::string qualified_name;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) qualified_name += "::";
            qualified_name += names[i];
        }
        return qualified_name;
    }

    void process_class(const CXCursor& class_cursor) {
        std::string class_name = get_qualified_name(class_cursor);
        CXString cx_clean_class = clang_getCursorSpelling(class_cursor);
        std::string clean_class = clang_getCString(cx_clean_class);
        clang_disposeString(cx_clean_class);
        
        if (repeated.find(clean_class) != repeated.end()) return;
        repeated.insert(clean_class);
        
        unit_test_release_map[class_name] = std::vector<std::string>();
        output_debug.push_back("// " + class_name);
        output_release.push_back("// " + class_name);
        helper_map[class_name] = std::vector<Pair>();

        static int current_field_index = 0;
        current_field_index = 0;
        clang_visitChildren(class_cursor, [](CXCursor cursor, CXCursor parent, CXClientData client_data) {
            auto self = reinterpret_cast<HeaderGenerator*>(client_data);
            if (clang_getCursorKind(cursor) == CXCursor_FieldDecl) {
                self->add_field_macro(self->get_qualified_name(parent), cursor, current_field_index);
                current_field_index++;
            }
            return CXChildVisit_Continue;
        }, this);
        
        output_debug.push_back("// " + class_name + "\n");
        output_release.push_back("// " + class_name + "\n");
    }

    void add_field_macro(const std::string& class_name, const CXCursor& field_cursor, int field_index) {
        CXString cx_field_name = clang_getCursorSpelling(field_cursor);
        std::string field_name = clang_getCString(cx_field_name);
        clang_disposeString(cx_field_name);
        
        std::string debug_value = "\"" + field_name + "\"";
        uint32_t numeric_id = get_field_id(field_name);
        std::string release_value = "\"" + FieldEncoder::encode(numeric_id) + "\"";
        
        std::string to_upper_class_name = class_name;
        std::string to_upper_field_name = field_name;

        std::transform(to_upper_class_name.begin(), to_upper_class_name.end(), to_upper_class_name.begin(),
                   [](unsigned char c) { return std::toupper(c); });


        std::transform(to_upper_field_name.begin(), to_upper_field_name.end(), to_upper_field_name.begin(),
                   [](unsigned char c) { return std::toupper(c); });

        std::string debug_macro = "// " + std::to_string(field_index) + " = " + field_name +
            "\n#define ZIRCON_DEF_EDITOR_" + to_upper_class_name + "_FIELD_" + to_upper_field_name + " " + debug_value;
        output_debug.push_back(debug_macro);
        
        std::string release_macro = "// " + std::to_string(field_index) + " = " + field_name +
            "\n#define ZIRCON_DEF_GAME_" + to_upper_class_name + "_FIELD_" + to_upper_field_name + " " + release_value;
        output_release.push_back(release_macro);
        
        helper_map[class_name].push_back({debug_value, release_value});
        unit_test_release_map[class_name].push_back(
            "ZIRCON_DEF_GAME_" + to_upper_class_name + "_FIELD_" + to_upper_field_name
        );
    }

    void generate_unit_test() {
        for (const auto& [class_name, fields] : unit_test_release_map) {
            for (size_t i = 0; i < fields.size(); ++i) {
                for (size_t j = i + 1; j < fields.size(); ++j) {
                    std::string build_expression;
                    for (int k = 0; k < RELEASE_FIELD_LENGTH; ++k) {
                        build_expression += fields[i] + "[" + std::to_string(k) + "] != " +
                                            fields[j] + "[" + std::to_string(k) + "]";
                        if (k < RELEASE_FIELD_LENGTH - 1) {
                            build_expression += " || ";
                        }
                    }
                    unit_test_str.push_back(
                        "static_assert(" + build_expression + 
                        ", \"Field ID collision report to developers!\");"
                    );
                }
            }
        }
    }

    void generate_helper() {
        helper_str.clear();
        std::string template_header = 
            "template<kun_kotek templated_constexpr_string_t<" + 
            std::to_string(RELEASE_FIELD_LENGTH + 1) + "> field>";
        
        for (const auto& [class_name, fields] : helper_map) {
            if (fields.empty()) continue;
            
            helper_str += "\n/* " + class_name + "*/\n\n";
            helper_str += template_header + " inline constexpr const char* zircon_decode_encoded_field_for_" +
                          class_name + "(void) noexcept\n{\n";
            helper_str += "\tif (std::is_constant_evaluated()) \n\t{\n";
            
            std::string static_assert_str = "static_assert(";
            for (size_t i = 0; i < fields.size(); ++i) {
                const auto& pair = fields[i];
                if (i > 0) helper_str += "\t\telse if constexpr (";
                else helper_str += "\t\tif constexpr (";
                
                helper_str += "field == kun_kotek templated_constexpr_string_t{" + pair.second + "}";
                static_assert_str += "(field == kun_kotek templated_constexpr_string_t{" + pair.second + "})";
                
                helper_str += ") { return " + pair.first + "; }\n";
                if (i < fields.size() - 1) {
                    static_assert_str += " || ";
                }
            }
            static_assert_str += ", \"Unknown field for " + class_name + "\");";
            
            helper_str += "\t\telse { " + static_assert_str + 
                " return \"NOT_EXISTED_FIELD_FOR_" + class_name + "_OR_DIFFERET_VERSION_OR_KIND_OF_ENGINE\"; }\n";
            helper_str += "\t}\n\telse \n\t{\n";
            
            for (size_t i = 0; i < fields.size(); ++i) {
                const auto& pair = fields[i];
                if (i > 0) helper_str += "\t\telse if (";
                else helper_str += "\t\tif (";
                
                helper_str += "field == kun_kotek templated_constexpr_string_t{" + pair.second + "}";
                helper_str += ") { return " + pair.first + "; }\n";
            }
            
            helper_str += "\t\telse { KOTEK_ASSERT(false, \"failed to obtain field probably different versions of engine?\");"
                " return \"NOT_EXISTED_FIELD_FOR_" + class_name + "_OR_DIFFERET_VERSION_OR_KIND_OF_ENGINE\"; }\n";
            helper_str += "\t}\n}\n";
            helper_str += "\n/* " + class_name + "*/\n\n";
        }
    }

public:
    HeaderGenerator(const fs::path& src, const fs::path& kotek_src)
        : src_dir(fs::canonical(src)), src_kotek_dir(fs::canonical(kotek_src)) {
        find_user_headers();
    }

    void process_files() {
        CXIndex index = clang_createIndex(0, 0);
        std::string last_arg = ("-I" + src_dir.string());
        std::vector<const char*> args = {
            "-x", "c++", "-std=c++20", 
            "-D KOTEK_USE_SDK_IMGUI", "-D KOTEK_DEBUG",
            last_arg.c_str()
        };
        
        for (const auto& file_path : target_files) {
            CXTranslationUnit tu = clang_parseTranslationUnit(
                index,
                file_path.string().c_str(),
                args.data(), args.size(),
                nullptr, 0,
                CXTranslationUnit_None
            );
            
            if (!tu) {
                std::cerr << "Failed to parse: " << file_path << std::endl;
                continue;
            }
            
            CXCursor cursor = clang_getTranslationUnitCursor(tu);
            clang_visitChildren(
                cursor,
                [](CXCursor c, CXCursor parent, CXClientData client_data) {
                    HeaderGenerator* self = static_cast<HeaderGenerator*>(client_data);
                    if (self->is_user_file(c)) {
                        CXCursorKind kind = clang_getCursorKind(c);
                        if (kind == CXCursor_Namespace) {
                            clang_visitChildren(c, visit_namespace, client_data);
                        }
                        else if (kind == CXCursor_ClassDecl || 
                                 kind == CXCursor_StructDecl || 
                                 kind == CXCursor_ClassTemplate) {
                            self->process_class(c);
                        }
                    }
                    return CXChildVisit_Continue;
                },
                this
            );
            clang_disposeTranslationUnit(tu);
        }
        clang_disposeIndex(index);
    }

    static CXChildVisitResult visit_namespace(CXCursor cursor, CXCursor parent, CXClientData client_data) {
        HeaderGenerator* self = static_cast<HeaderGenerator*>(client_data);
        if (self->is_user_file(cursor)) {
            CXCursorKind kind = clang_getCursorKind(cursor);
            if (kind == CXCursor_ClassDecl || 
                kind == CXCursor_StructDecl || 
                kind == CXCursor_ClassTemplate) {
                self->process_class(cursor);
            }
        }
        return CXChildVisit_Recurse;
    }

    void generate_header(const fs::path& output_path) {
        process_files();
        generate_unit_test();
        generate_helper();
        
        std::time_t now = std::time(nullptr);
        std::tm* now_tm = std::localtime(&now);
        char date_buf[80];
        std::strftime(date_buf, sizeof(date_buf), "%m/%d/%Y, %H:%M:%S", now_tm);
        
        std::ofstream out(output_path);

        if (!out.good())
        {
            std::cerr << "FAILED TO CREATE FILE: " << output_path << std::endl;
            std::cerr << "TRY TO REQUEST ADMIN PRIVILEGES" << std::endl;
        }

        out << "/*\n"
            << " * author: wh1t3lord\n"
            << " * description: generated fields of each class for serialization\n"
            << " * date: " << date_buf << "\n"
            << " * ATTENTION: Auto-generated - DO NOT EDIT!\n"
            << "*/\n\n"
            << "#pragma once\n\n";
        
        for (const auto& line : output_debug) out << line << "\n";
        out << "\n";
        for (const auto& line : output_release) out << line << "\n";
        out << "\n";
        for (const auto& line : unit_test_str) out << line << "\n";
        out << helper_str;
    }
};

int generate_ecs_fields(int argc, const char* p_src, const char* p_header, const char* p_output)
{
    if (argc < 7) {
        std::cerr << "Usage: " << " --src <source_dir> --kotek_src <source_dir> --output <output_file>\n";
        return 1;
    }

    std::cout << "--src = " << p_src << std::endl;
    std::cout << "--kotek_src = " << p_header << std::endl;
    std::cout << "--output = " << p_output << std::endl;

    fs::path src_dir(p_src);
    fs::path header_dir(p_header);
    fs::path output_file(p_output);

    if (!fs::exists(src_dir))
    {
        std::cerr << "the following path doesn't exist: " << src_dir << std::endl;
        return 1;
    }

    if (!fs::exists(header_dir))
    {
        std::cerr << "the following path doesn't exist: " << header_dir << std::endl;
        return 1;
    }

    try {
        HeaderGenerator generator(src_dir, header_dir);
        generator.generate_header(output_file);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

// Helper: Convert snake_case or lower_with_underscores to PascalCase
std::string to_pascal_case(const std::string& input)
{
	std::string result;
	bool capitalize = true;
	for (char c : input)
	{
		if (c == '_')
		{
			capitalize = true;
		}
		else
		{
			result += capitalize ? std::toupper(c) : c;
			capitalize = false;
		}
	}
	return result;
}

int generate_sdk_fields(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cerr
			<< "Usage: --sdk_src <sdk_header_folder> --output <output_file>\n";
		return 1;
	}

	std::string sdk_src_dir = argv[4];
	std::string output_file = argv[6];

    std::cout << "sdk_src_dir = " << sdk_src_dir << std::endl;
    std::cout << "output_file = " << output_file << std::endl;

	if (!fs::exists(sdk_src_dir))
	{
		std::cerr << "Source directory does not exist: " << sdk_src_dir
				  << std::endl;
		return 1;
	}

	std::vector<fs::path> header_files;
	for (const auto& entry : fs::directory_iterator(sdk_src_dir))
	{
		if (entry.is_regular_file() &&
			(entry.path().extension() == ".h" ||
				entry.path().extension() == ".hpp"))
		{
			header_files.push_back(entry.path());
		}
	}

	std::vector<std::string> enum_fields;

	CXIndex index = clang_createIndex(0, 0);
	std::vector<const char*> args = {"-x", "c++", "-std=c++20"};

	for (const auto& file_path : header_files)
	{
		CXTranslationUnit tu =
			clang_parseTranslationUnit(index, file_path.string().c_str(),
				args.data(), args.size(), nullptr, 0, CXTranslationUnit_None);

		if (!tu)
		{
			std::cerr << "Failed to parse: " << file_path << std::endl;
			continue;
		}

		CXCursor cursor = clang_getTranslationUnitCursor(tu);
		clang_visitChildren(
			cursor,
			[](CXCursor c, CXCursor /*parent*/, CXClientData client_data)
			{
				auto* fields =
					static_cast<std::vector<std::string>*>(client_data);
				if (clang_getCursorKind(c) == CXCursor_ClassDecl)
				{
					CXString cx_class_name = clang_getCursorSpelling(c);
					std::string class_name = clang_getCString(cx_class_name);
					clang_disposeString(cx_class_name);

					const std::string prefix = "zircon_editor_ui_window_";
					if (class_name.find(prefix) == 0 &&
						class_name.size() > prefix.size())
					{
						std::string suffix = class_name.substr(prefix.size());
						std::string pascal = to_pascal_case(suffix);
						std::string enum_field = "kWindow_SDK_" + pascal;
						fields->push_back(enum_field);
					}
				}
				return CXChildVisit_Recurse;
			},
			&enum_fields);
		clang_disposeTranslationUnit(tu);
	}
	clang_disposeIndex(index);

	// Write to output file
	std::ofstream out(output_file);
	if (!out.good())
	{
		std::cerr << "FAILED TO CREATE FILE: " << output_file << std::endl;
		return 1;
	}

	out << "/*\n"
		<< " * Auto-generated enum for Zircon Editor UI Windows\n"
		<< " * Do not edit manually!\n"
		<< "*/\n\n"
		<< "#pragma once\n\n"
		<< "enum class eZirconWindowIDs : int {\n";

	for (const auto& field : enum_fields)
	{
		out << "    " << field << ",\n";
	}
	out << "    kTotalAmountOfEnum\n";
	out << "};\n\n";

    // Add Translate_ZirconWindowIDs function
	out << "inline const char* Translate_ZirconWindowIDs(eZirconWindowIDs "
	       "id)\n{\n"
		<< "    switch (id)\n    {\n";
	for (const auto& field : enum_fields)
	{
		out << "        case eZirconWindowIDs::" << field
			<< ": return \"" << field << "\";\n";
	}
	out << "        default: return \"kWindow_SDK_UNDEFINED_ENUM\";\n";
	out << "    }\n}\n";

	out.close();
	std::cout << "SDK window enum file generated: " << output_file << std::endl;
	return 0;
}

bool containsEditor(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr.find("editor") != std::string::npos;
}

bool hasZirconPrefix(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr.find("zircon_render_graph_pass") != std::string::npos;
}

std::string getCursorSpelling(CXCursor cursor) {
    CXString spelling = clang_getCursorSpelling(cursor);
    std::string result = clang_getCString(spelling);
    clang_disposeString(spelling);
    return result;
}

std::string getFullyQualifiedName(CXCursor cursor) {
    std::string qualifiedName;
    CXCursor current = cursor;
    
    // Get the fully qualified name by traversing parent cursors
    while (clang_isInvalid(clang_getCursorKind(current)) == 0) {
        if (clang_getCursorKind(current) == CXCursor_TranslationUnit) {
            break;
        }
        
        CXString spelling = clang_getCursorSpelling(current);
        std::string part = clang_getCString(spelling);
        clang_disposeString(spelling);
        
        if (!part.empty()) {
            if (qualifiedName.empty()) {
                qualifiedName = part;
            } else {
                qualifiedName = part + "::" + qualifiedName;
            }
        }
        
        current = clang_getCursorSemanticParent(current);
    }
    
    return qualifiedName;
}

CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    auto* classNames = static_cast<std::set<std::string>*>(client_data);
    CXCursorKind kind = clang_getCursorKind(cursor);
    
    if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
        // only classes DEFINED in the parsed header itself: declarations
        // pulled in from included headers (e.g. the pass base classes)
        // belong to their own files, otherwise the emitted factory
        // references classes that live outside the scanned folder
        if (!clang_Location_isFromMainFile(clang_getCursorLocation(cursor))) {
            return CXChildVisit_Continue;
        }

        // forward declarations have no body to instantiate
        if (!clang_isCursorDefinition(cursor)) {
            return CXChildVisit_Continue;
        }

        // the factory does `new <pass>()`; abstract classes cannot be
        // instantiated
        if (clang_CXXRecord_isAbstract(cursor)) {
            return CXChildVisit_Continue;
        }

        std::string className = getCursorSpelling(cursor);
        std::string qualifiedName = getFullyQualifiedName(cursor);
        
        if (!className.empty() && hasZirconPrefix(className)) {
            classNames->insert(qualifiedName);
        }
    }
    
    return CXChildVisit_Recurse;
}

void parseFile(const std::string& filePath, std::set<std::string>& classNames) {
    CXIndex index = clang_createIndex(0, 0);
    const char* args[] = {"-x", "c++", "-std=c++17"};
    CXTranslationUnit tu = clang_parseTranslationUnit(index, filePath.c_str(), args, 3, nullptr, 0, CXTranslationUnit_None);
    
    if (!tu) {
        std::cerr << "Failed to parse translation unit: " << filePath << std::endl;
        clang_disposeIndex(index);
        return;
    }
    
    CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(rootCursor, visitor, &classNames);
    
    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
}

std::string toEnumValue(const std::string& qualifiedName) {
    std::string enumValue = qualifiedName;
    
    // Replace "::" with "_" to make it a valid enum value
    size_t pos = 0;
    while ((pos = enumValue.find("::", pos)) != std::string::npos) {
        enumValue.replace(pos, 2, "_");
        pos += 1;
    }
    
    return "k" + enumValue;
}

std::string extractBackendFromPath(const std::string& path) {
    // Extract backend from path like "${CMAKE_SOURCE_DIR}/src/render/bgfx/passes/no_streaming"
    size_t renderPos = path.find("/render/");
    if (renderPos == std::string::npos) {
        return "bgfx"; // Default to bgfx if not found
    }
    
    size_t backendStart = renderPos + 8; // Length of "/render/"
    size_t backendEnd = path.find('/', backendStart);
    
    if (backendEnd == std::string::npos) {
        return path.substr(backendStart);
    }
    
    return path.substr(backendStart, backendEnd - backendStart);
}

void generateEnumFile(const std::string& filePath, const std::string& enumName, const std::set<std::string>& passes) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }
    
    std::string guard = enumName;
    std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);
    guard += "_H";
    
    file << "#ifndef " << guard << "\n";
    file << "#define " << guard << "\n\n";
    file << "enum class " << enumName << " : kun_kotek kun_ktk enum_base_t {\n";
    for (const auto& pass : passes) {
        file << "    " << toEnumValue(pass) << ",\n";
    }
    file << "};\n\n";
    file << "#endif // " << guard << "\n";
}

void generateFactoryHeader(const std::string& folderPath, 
                          const std::set<std::string>& gamePasses, 
                          const std::set<std::string>& editorPasses,
                          const std::string& backend,
                          const std::set<std::string>& passHeaders) {
    std::string factoryPath = folderPath + "/zircon_render_pass_factory.h";
    std::ofstream file(factoryPath);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << factoryPath << std::endl;
        return;
    }
    
    std::string guard = "ZIRCON_RENDER_PASS_FACTORY_H";
    // namespace segments compose as kun_kotek + kun_render +
    // kun_render_<backend> (each macro carries its own trailing ::,
    // e.g. Kotek::Render::Bgfx::)
    std::string returnType = "kun_kotek kun_render kun_render_" + backend + " ktkRenderGraphSimplifiedRenderPass*";
    
    file << "#ifndef " << guard << "\n";
    file << "#define " << guard << "\n\n";
    
    file << "#include \"zircon_render_game_passes_enum.h\"\n";
    file << "#include \"zircon_render_editor_passes_enum.h\"\n";
    // the concrete passes this factory instantiates — included here so a
    // consumer only needs this one header
    for (const auto& header : passHeaders) {
        file << "#include \"" << header << "\"\n";
    }
    file << "#include <cstring>\n\n";
    
    // Constexpr function to get class name from enum
    file << "    inline constexpr const char* convert_render_pass_to_string(eZirconRenderGamePasses pass) {\n";
    file << "        switch (pass) {\n";
    for (const auto& pass : gamePasses) {
        file << "            case eZirconRenderGamePasses::" << toEnumValue(pass) << ": return \"" << pass << "\";\n";
    }
    file << "            default: return \"\";\n";
    file << "        }\n";
    file << "    }\n\n";
    
    file << "    inline constexpr const char* convert_render_pass_to_string(eZirconRenderEditorPasses pass) {\n";
    file << "        switch (pass) {\n";
    for (const auto& pass : editorPasses) {
        file << "            case eZirconRenderEditorPasses::" << toEnumValue(pass) << ": return \"" << pass << "\";\n";
    }
    file << "            default: return \"\";\n";
    file << "        }\n";
    file << "    }\n";

    file << "class zircon_render_pass_factory {\n";
    file << "public:\n";
    
    // Game passes create method
    file << "    static " << returnType << " create(eZirconRenderGamePasses pass) {\n";
    file << "        switch (pass) {\n";
    for (const auto& pass : gamePasses) {
        file << "            case eZirconRenderGamePasses::" << toEnumValue(pass) << ": return new " << pass << "();\n";
    }
    file << "            default: return nullptr;\n";
    file << "        }\n";
    file << "    }\n\n";
    
    // Editor passes create method
    file << "    static " << returnType << " create(eZirconRenderEditorPasses pass) {\n";
    file << "        switch (pass) {\n";
    for (const auto& pass : editorPasses) {
        file << "            case eZirconRenderEditorPasses::" << toEnumValue(pass) << ": return new " << pass << "();\n";
    }
    file << "            default: return nullptr;\n";
    file << "        }\n";
    file << "    }\n\n";
    
    // String-based create method
    file << "    static " << returnType << " create(const char* class_name) {\n";
    file << "        // Game passes\n";
    for (const auto& pass : gamePasses) {
        file << "        if (std::strcmp(class_name, \"" << pass << "\") == 0) {\n";
        file << "            return new " << pass << "();\n";
        file << "        }\n";
    }
    file << "        \n        // Editor passes\n";
    for (const auto& pass : editorPasses) {
        file << "        if (std::strcmp(class_name, \"" << pass << "\") == 0) {\n";
        file << "            return new " << pass << "();\n";
        file << "        }\n";
    }
    file << "        \n        return nullptr;\n";
    file << "    }\n\n";
    
    file << "};\n\n";
    file << "#endif // " << guard << "\n";
}

void generateRenderPassEnums(const std::string& folderPath) {
    std::set<std::string> gamePasses;
    std::set<std::string> editorPasses;
    // headers that contributed at least one pass class — the factory
    // includes them so it stays self-contained for any consumer
    std::set<std::string> passHeaders;
    
    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".h") {
            std::set<std::string> classNames;
            parseFile(entry.path().string(), classNames);
            
            for (const auto& className : classNames) {
                if (containsEditor(className)) {
                    editorPasses.insert(className);
                } else {
                    gamePasses.insert(className);
                }
            }

            if (!classNames.empty()) {
                passHeaders.insert(entry.path().filename().string());
            }
        }
    }
    
    generateEnumFile(folderPath + "/zircon_render_game_passes_enum.h", "eZirconRenderGamePasses", gamePasses);
    generateEnumFile(folderPath + "/zircon_render_editor_passes_enum.h", "eZirconRenderEditorPasses", editorPasses);
    
    std::string backend = extractBackendFromPath(folderPath);
    generateFactoryHeader(folderPath, gamePasses, editorPasses, backend, passHeaders);
}

int main(int argc, char* argv[])
{
    int what_to_generate = -1;

    for (int i = 0; i < argc; ++i)
    {
        std::cout << "argv=" << argv[i] << " | strcmp=" << strcmp(argv[i], "--type") << std::endl;

        if (!strcmp(argv[i], "--type"))
        {
            std::cout << "Found type!" << std::endl;

            if (i + 1 < argc)
            {
                std::cout << "type = " << argv[i+1] << std::endl;
                int type = atoi(argv[i+1]);
                what_to_generate = type;
                break;
            }
        }
    }

    std::cout << "what_to_generate: " << what_to_generate << std::endl;

    switch (what_to_generate)
    {
        case 0:
        {
            std::cout << "Generating ecs fields!" << std::endl;

            int result_ecs = generate_ecs_fields(argc, argv[4], argv[6], argv[8]);

            if (result_ecs != 0)
            {
                std::cout << "failed to generate file for ecs" << std::endl;
                return -1;
            }

            break;
        }
        case 1:
        {
            std::cout << "Generating sdk fields!" << std::endl;

            int result_sdk = generate_sdk_fields(argc, argv);

            if (result_sdk != 0)
            {
                std::cout << "failed to generate file for sdk" << std::endl;
                return -2;
            }

            break;
        }
        case 2:
        {
            std::cout << "Generating renderer passes enums!" << std::endl;

            generateRenderPassEnums(argv[3]);

            break;
        }
        default:
        {
            std::cout << "Generating ecs fields! (default)" << std::endl;

            int result_ecs = generate_ecs_fields(argc, argv[2], argv[4], argv[6]);

            if (result_ecs != 0)
            {
                std::cout << "failed to generate file for ecs" << std::endl;
                return -1;
            }

            break;
        }
    }

    std::cout << "File was generated successfully!" << std::endl;

    return 0;
}