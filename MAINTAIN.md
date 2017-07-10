# Filenames

- .c: C
- .cpp: C++
- .fs: (bgfx) fragment shader
- .h: C data-structures and APIs
- .hpp: C++ data-structures and APIs
- .ipp: C++ inlines
- .s: (bgfx) shader utilities
- .vs: (bgfx) vertex shader
- varying.def.sc: (bgfx) shared definitions between vertex/fragment shaders

- <platform>_unit_: compilation units

# Naming conventions

symbol suffixes:
- _i: indices
- _n: sizes, count
- _p: predicate
- 2: 2 dimension

# Headers

We generally try not to have headers (.h/.hpp) include other headers.
