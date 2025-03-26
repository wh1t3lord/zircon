import sys
import argparse
import platform
from pathlib import Path
import hashlib  # For hashing field names
import clang.cindex
from clang.cindex import Config, Index, CursorKind, TranslationUnit, TokenKind
import datetime

class FieldEncoder:
    CHARSET = '0123456789abcdefghijklmnopqrstuvwxyz'
    
    @classmethod
    def encode(cls, num: int, length: int = 3) -> str:
        """Encode an integer into a fixed-length base36 string."""
        base36 = ""
        while num:
            num, rem = divmod(num, 36)
            base36 = cls.CHARSET[rem] + base36
        # Ensure the result is of fixed length, padding with '0's if necessary.
        return base36.rjust(length, '0')[-length:]

def get_field_id(field_name: str, length: int = 3) -> int:
    """
    Generate a numeric id from the field name using MD5 hash.
    The result is reduced modulo 36**length to ensure it fits in the fixed length.
    """
    h = hashlib.md5(field_name.encode('utf-8')).hexdigest()
    # Convert the hex digest to an integer
    numeric_hash = int(h, 16)
    # Restrict the range to ensure it fits in the desired number of base36 digits
    return numeric_hash % (36 ** length)

class HeaderGenerator:
    def __init__(self, src_dir, src_kotek_dir):
        self.build_type = "none"
        self.src_dir = Path(src_dir).resolve()
        self.src_kotek_dir = Path(src_kotek_dir).resolve()
        self.target_files = set()
        self.target_files_kotek = set()
        self.output_debug = []
        self.output_release = []
        self.unit_test_release_map = {}
        self.unit_test_str = []
        self.repeated = []
        self.release_field_length = 3
        self.configure_libclang()
        self.find_user_headers()
        # self.process_files_kotek()
        self.process_files()

    def configure_libclang(self):
        """Configure libclang library path"""
        try:
            if Config.library_path:
                return
            sys.exit("libclang not found! Install LLVM and set path manually")
        except Exception as e:
            sys.exit(f"libclang configuration failed: {str(e)}")

    def find_user_headers(self):
        """Collect all source files in the target directory"""
        extensions = ['*component*.h', '*component*.hpp', '*component*.H']
        for ext in extensions:
            self.target_files.update(self.src_dir.rglob(ext))
            self.target_files.update(self.src_dir.rglob(ext.upper()))
        
        if not self.target_files:
            sys.exit(f"No source files found in {self.src_dir}")

    def is_user_file(self, cursor):
        """Check if node comes from user's source files"""
        if not cursor.location.file:
            return False
        try:
            file_path = Path(cursor.location.file.name).resolve()
            return file_path in self.target_files or "kotek_" in file_path.name
        except:
            return False

    def get_qualified_name(self, cursor):
        """Get fully qualified class name"""
        names = []
        current = cursor
        while current:
            if current.kind in [CursorKind.NAMESPACE, CursorKind.CLASS_DECL]:
                names.append(current.spelling)
            current = current.semantic_parent
        return '::'.join(reversed(names)) if names else None

    def process_files(self):
        """Process all collected source files"""
        index = Index.create()
        for file_path in self.target_files:
            args = ['-x', 'c++', '-std=c++20', '-D KOTEK_USE_SDK_IMGUI', '-D KOTEK_DEBUG', f'-I{self.src_dir}']
            try:
                tu = index.parse(str(file_path), args, options=TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
                self.process_translation_unit(tu.cursor, tu)
            except Exception as e:
                sys.exit(f"Failed to parse {file_path}: {str(e)}")

    def process_translation_unit(self, cursor, tu):
        """Recursively process AST nodes"""
        for child in cursor.get_children():
            if not self.is_user_file(child):
                continue
            if child.kind == CursorKind.NAMESPACE:
                self.process_translation_unit(child, tu)
            elif child.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL, CursorKind.CLASS_TEMPLATE]:
                if child.is_definition():
                    self.process_class(child)

    def process_class(self, class_cursor):
        """Process class fields"""
        class_name = self.get_qualified_name(class_cursor)
        clean_class = class_cursor.spelling
        
        if clean_class in self.repeated:
            return
        self.repeated.append(clean_class)
        
        current_field_index = 0
        self.unit_test_release_map[class_name] = []
        self.output_debug.append(f"// {class_name}")
        self.output_release.append(f"// {class_name}")
        for child in class_cursor.get_children():
            if child.kind == CursorKind.FIELD_DECL:
                self.add_field_macro(clean_class, child, current_field_index)
                current_field_index += 1
        self.output_debug.append(f"// {class_name}\n")
        self.output_release.append(f"// {class_name}\n")

    def add_field_macro(self, class_name, field_cursor, field_index):
        """Generate macro for a class field"""
        field_name = field_cursor.spelling
        debug_value = f'"{field_name}"'
        
        # Use the hash-based approach to generate a stable id
        numeric_id = get_field_id(field_name, length=self.release_field_length)
        release_value = f'"{FieldEncoder.encode(numeric_id, length=self.release_field_length)}"'
        
        self.output_debug.append(
            f"#define ZIRCON_DEF_{class_name.upper()}_FIELD_{field_name.upper()} {debug_value} // {field_index}"
        )
        self.output_release.append(
            f"#define ZIRCON_DEF_{class_name.upper()}_FIELD_{field_name.upper()} {release_value} // {field_index}"
        )

        self.unit_test_release_map[class_name].append(
            f"ZIRCON_DEF_{class_name.upper()}_FIELD_{field_name.upper()}"
        )

    def generate_unit_test(self):
        for class_name, fields in self.unit_test_release_map.items():
            #print(class_name, fields)
            for field in fields:
                for other_field in fields:
                    if field != other_field:
                        build_expression = ""
                        for i in range(self.release_field_length):
                            build_expression += f"{field}[{i}] != {other_field}[{i}] "
                            if i < self.release_field_length - 1:
                                build_expression += "|| "

                        self.unit_test_str.append(f"static_assert({build_expression}, \"Field ID collision report to developers!\")")


    def generate_header(self, output_path):
        """Write generated macros to file"""
        now = datetime.datetime.now()
        self.generate_unit_test()

        content = [
            f"/*",
            "\tauthor: wh1t3lord",
            "\tdescription: generated fields of each class for serialization and deserialization using generate_zircon.py",
            "\t\t\t\t comments for each preprocessor shows the order how libclang parsed the class and its fields",
            "\tdate: " + now.strftime("%m/%d/%Y, %H:%M:%S"),
            "\tATTENTION: Auto-generated field definitions - DO NOT EDIT!",
            f"*/",
            "#pragma once",
            "#ifdef KOTEK_DEBUG"
        ] + self.output_debug + [
            "#else",
        ] + self.output_release + self.unit_test_str + [
            "#endif"
        ]
        
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text('\n'.join(content))

def main():
    parser = argparse.ArgumentParser(description='Generate ZIRCON macros from C++ classes')
    parser.add_argument('--src', required=True, help='Source directory with headers')
    parser.add_argument('--kotek_src', required=True, help='Source directory with headers')
    parser.add_argument('--output', required=True, help='Output header file path')
    
    args = parser.parse_args()
    
    generator = HeaderGenerator(args.src, args.kotek_src)
    generator.generate_header(Path(args.output))

if __name__ == "__main__":
    main()
