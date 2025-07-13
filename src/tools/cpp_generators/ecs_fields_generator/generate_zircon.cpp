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
        
        std::string debug_macro = "// " + std::to_string(field_index) + " = " + field_name +
            "\n#define ZIRCON_DEF_EDITOR_" + class_name + "_FIELD_" + field_name + " " + debug_value;
        output_debug.push_back(debug_macro);
        
        std::string release_macro = "// " + std::to_string(field_index) + " = " + field_name +
            "\n#define ZIRCON_DEF_GAME_" + class_name + "_FIELD_" + field_name + " " + release_value;
        output_release.push_back(release_macro);
        
        helper_map[class_name].push_back({debug_value, release_value});
        unit_test_release_map[class_name].push_back(
            "ZIRCON_DEF_GAME_" + class_name + "_FIELD_" + field_name
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
                
                helper_str += "field.str == std::string_view{" + pair.second + "}";
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

int main(int argc, char* argv[]) {
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0] << " --src <source_dir> --kotek_src <source_dir> --output <output_file>\n";
        return 1;
    }

    fs::path src_dir(argv[2]);
    fs::path header_dir(argv[4]);
    fs::path output_file(argv[6]);

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