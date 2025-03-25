import sys
import argparse
import platform
from pathlib import Path
import clang.cindex
from clang.cindex import Config, Index, CursorKind, TranslationUnit, TokenKind

class FieldEncoder:
    CHARSET = '0123456789abcdefghijklmnopqrstuvwxyz'
    
    @classmethod
    def encode(cls, idx):
        return f"{cls.CHARSET[idx//36]}{cls.CHARSET[idx%36]}"

class HeaderGenerator:
    def __init__(self, src_dir, src_kotek_dir):
        self.build_type = "none"
        self.src_dir = Path(src_dir).resolve()
        self.src_kotek_dir = Path(src_kotek_dir).resolve()
        self.target_files = set()
        self.target_files_kotek = set()
        self.output = []
        self.output_debug = []
        self.output_release = []
        self.repeated = []
        
        self.configure_libclang()
        self.find_user_headers()
      #  self.process_files_kotek()
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

      #  extensions_kotek = ['*.h', '*.hpp', '*.H', '*.HPP', '*.hxx', '*.HXX']
      #  for ext in extensions_kotek:
      #      self.target_files_kotek.update(self.src_kotek_dir.rglob(ext))
        
        if not self.target_files:
            sys.exit(f"No source files found in {self.src_dir}")

    #    if not self.target_files_kotek:
     #       sys.exit(f"No source files found in {self.src_kotek_dir}")

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

    def process_files_kotek(self):
        index = Index.create()
        for file_path in self.target_files_kotek:
            args = ['-x', 'c++', '-std=c++20', f'-I{self.src_dir}']
            try:
                print('parsed file:', file_path)
                tu = index.parse(str(file_path), args, options=TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
                self.process_translation_unit(tu.cursor, tu)
            except Exception as e:
                sys.exit(f"Failed to parse {file_path}: {str(e)}")

        

        

    def process_files(self):
        """Process all collected source filollama run deepseek-coder:33bes"""
        index = Index.create()
        for file_path in self.target_files:
            args = ['-x', 'c++', '-std=c++20', f'-I{self.src_dir}']
            try:
              #  print('parsed file:', file_path)
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
                self.process_translation_unit(child)
            elif child.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL,CursorKind.CLASS_TEMPLATE]:
                if child.is_definition():
                    self.process_class(child)
                

    def process_class(self, class_cursor):
        """Process class fields"""
        class_name = self.get_qualified_name(class_cursor)
        clean_class = class_cursor.spelling
        field_idx = 0
       # print(f"Processing class: {clean_class}")
        
        if clean_class not in self.repeated:
            self.repeated.append(clean_class)
        else:
            return
        
        if "actor" in clean_class:
            for token in class_cursor.get_tokens():
                if token.kind == TokenKind.IDENTIFIER:
                    print(token.spelling, token.cursor.kind)

            for child in class_cursor.get_children():
                print(child.spelling, child.kind)

        for child in class_cursor.get_children():
           # print(child.spelling, child.kind)
            if child.kind == CursorKind.FIELD_DECL:
             #   print(child.spelling, clean_class)
                self.add_field_macro(clean_class, child, field_idx)
                field_idx += 1

    def add_field_macro(self, class_name, field_cursor, idx):
        """Generate macro for a class field"""
        field_name = field_cursor.spelling
        field_type = field_cursor.type.spelling
        
        debug_value = f'"{field_name}"'
        release_value = f'"{FieldEncoder.encode(idx)}"'
        
        self.output_debug.append(
            f"#define ZIRCON_DEF_{class_name.upper()}_FIELD_{field_name.upper()} {debug_value} \n"
        )
        self.output_release.append(
                        f"#define ZIRCON_DEF_{class_name.upper()}_FIELD_{field_name.upper()} {release_value} \n"
        )

    def generate_header(self, output_path):
        """Write generated macros to file"""
        content = [
            "#pragma once",
            "// Auto-generated field definitions - DO NOT EDIT",
            "#ifdef KOTEK_DEBUG"
        ] + self.output_debug + [
            "#else",
        ] + self.output_release + [
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