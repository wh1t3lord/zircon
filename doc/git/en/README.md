supported languages: ru

## Overview

- Solution architecture
- Building requirements


## Solution architecture

### Engine architecture



### Game architecture



## Building requirements

- Python that will support pip and libraries from Python dependecies section (I personally used 3.13 version, but I need to choose real minimal version for it);
- C++20 compatiable compiler;
- CMake 3.19.3 version;


### Python dependencies

In order to successfully build the generated files for some projects this project uses following dependencies:

- pip install libclang;
- pip install llvm-installer;

Otherwise you won't get generated files and thus can't build solution of zircon successfully. 