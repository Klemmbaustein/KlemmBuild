# KlemmBuild C++ build system.

A C++ build system for Windows MSVC and Linux Clang/GCC.

## How to install:

On Linux:
1.	Clone the repository with submodules
    ```bash
    git clone --recurse-submodules https://github.com/Klemmbaustein/KlemmBuild.git
    ```
2.	Build and install using make:
    ```bash
    make && sudo make install
    ```

## How to use:

Makefiles use the json format, but get preprocessed with the C Preprocessor before they are parsed.

A simple makefile looks like this:

```json
{
  "projects": {
    "name": "example",
    "outputPath": "bin/",
    "sources": [ "src/*.cpp" ]
  }
}
```

Here, a program called "example" will be built from all source files in the directory `src/` with the extension `.cpp`.
The executable will be placed in the `bin/` directory.

```json
{
  "projects": {
#if MSVC_WINDOWS
    "name": "example_windows",
#else
    "name": "example_linux",
#endif
    "outputPath": "bin/",
    "sources": [ "src/*.cpp" ]
  }
}
```

Here, the program will be called "example_windows" if compiled for Windows, otherwise it will be called "example_linux".
