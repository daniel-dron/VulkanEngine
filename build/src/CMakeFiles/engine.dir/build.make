# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.30

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /snap/cmake/1403/bin/cmake

# The command to remove a file.
RM = /snap/cmake/1403/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/danield/personal/vkquide

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/danield/personal/vkquide/build

# Include any dependencies generated for this target.
include src/CMakeFiles/engine.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/CMakeFiles/engine.dir/compiler_depend.make

# Include the progress variables for this target.
include src/CMakeFiles/engine.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/engine.dir/flags.make

src/CMakeFiles/engine.dir/cmake_pch.hxx.gch: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/cmake_pch.hxx.gch: src/CMakeFiles/engine.dir/cmake_pch.hxx.cxx
src/CMakeFiles/engine.dir/cmake_pch.hxx.gch: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/cmake_pch.hxx.gch: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/CMakeFiles/engine.dir/cmake_pch.hxx.gch"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -x c++-header -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/cmake_pch.hxx.gch -MF CMakeFiles/engine.dir/cmake_pch.hxx.gch.d -o CMakeFiles/engine.dir/cmake_pch.hxx.gch -c /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx.cxx

src/CMakeFiles/engine.dir/cmake_pch.hxx.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/cmake_pch.hxx.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -x c++-header -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx.cxx > CMakeFiles/engine.dir/cmake_pch.hxx.i

src/CMakeFiles/engine.dir/cmake_pch.hxx.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/cmake_pch.hxx.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -x c++-header -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx.cxx -o CMakeFiles/engine.dir/cmake_pch.hxx.s

src/CMakeFiles/engine.dir/main.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/main.cpp.o: /home/danield/personal/vkquide/src/main.cpp
src/CMakeFiles/engine.dir/main.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/main.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/main.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/CMakeFiles/engine.dir/main.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/main.cpp.o -MF CMakeFiles/engine.dir/main.cpp.o.d -o CMakeFiles/engine.dir/main.cpp.o -c /home/danield/personal/vkquide/src/main.cpp

src/CMakeFiles/engine.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/main.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/main.cpp > CMakeFiles/engine.dir/main.cpp.i

src/CMakeFiles/engine.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/main.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/main.cpp -o CMakeFiles/engine.dir/main.cpp.s

src/CMakeFiles/engine.dir/vk_initializers.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/vk_initializers.cpp.o: /home/danield/personal/vkquide/src/vk_initializers.cpp
src/CMakeFiles/engine.dir/vk_initializers.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/vk_initializers.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/vk_initializers.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object src/CMakeFiles/engine.dir/vk_initializers.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/vk_initializers.cpp.o -MF CMakeFiles/engine.dir/vk_initializers.cpp.o.d -o CMakeFiles/engine.dir/vk_initializers.cpp.o -c /home/danield/personal/vkquide/src/vk_initializers.cpp

src/CMakeFiles/engine.dir/vk_initializers.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/vk_initializers.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/vk_initializers.cpp > CMakeFiles/engine.dir/vk_initializers.cpp.i

src/CMakeFiles/engine.dir/vk_initializers.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/vk_initializers.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/vk_initializers.cpp -o CMakeFiles/engine.dir/vk_initializers.cpp.s

src/CMakeFiles/engine.dir/vk_images.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/vk_images.cpp.o: /home/danield/personal/vkquide/src/vk_images.cpp
src/CMakeFiles/engine.dir/vk_images.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/vk_images.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/vk_images.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object src/CMakeFiles/engine.dir/vk_images.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/vk_images.cpp.o -MF CMakeFiles/engine.dir/vk_images.cpp.o.d -o CMakeFiles/engine.dir/vk_images.cpp.o -c /home/danield/personal/vkquide/src/vk_images.cpp

src/CMakeFiles/engine.dir/vk_images.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/vk_images.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/vk_images.cpp > CMakeFiles/engine.dir/vk_images.cpp.i

src/CMakeFiles/engine.dir/vk_images.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/vk_images.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/vk_images.cpp -o CMakeFiles/engine.dir/vk_images.cpp.s

src/CMakeFiles/engine.dir/vk_descriptors.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/vk_descriptors.cpp.o: /home/danield/personal/vkquide/src/vk_descriptors.cpp
src/CMakeFiles/engine.dir/vk_descriptors.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/vk_descriptors.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/vk_descriptors.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object src/CMakeFiles/engine.dir/vk_descriptors.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/vk_descriptors.cpp.o -MF CMakeFiles/engine.dir/vk_descriptors.cpp.o.d -o CMakeFiles/engine.dir/vk_descriptors.cpp.o -c /home/danield/personal/vkquide/src/vk_descriptors.cpp

src/CMakeFiles/engine.dir/vk_descriptors.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/vk_descriptors.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/vk_descriptors.cpp > CMakeFiles/engine.dir/vk_descriptors.cpp.i

src/CMakeFiles/engine.dir/vk_descriptors.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/vk_descriptors.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/vk_descriptors.cpp -o CMakeFiles/engine.dir/vk_descriptors.cpp.s

src/CMakeFiles/engine.dir/vk_pipelines.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/vk_pipelines.cpp.o: /home/danield/personal/vkquide/src/vk_pipelines.cpp
src/CMakeFiles/engine.dir/vk_pipelines.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/vk_pipelines.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/vk_pipelines.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object src/CMakeFiles/engine.dir/vk_pipelines.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/vk_pipelines.cpp.o -MF CMakeFiles/engine.dir/vk_pipelines.cpp.o.d -o CMakeFiles/engine.dir/vk_pipelines.cpp.o -c /home/danield/personal/vkquide/src/vk_pipelines.cpp

src/CMakeFiles/engine.dir/vk_pipelines.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/vk_pipelines.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/vk_pipelines.cpp > CMakeFiles/engine.dir/vk_pipelines.cpp.i

src/CMakeFiles/engine.dir/vk_pipelines.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/vk_pipelines.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/vk_pipelines.cpp -o CMakeFiles/engine.dir/vk_pipelines.cpp.s

src/CMakeFiles/engine.dir/vk_engine.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/vk_engine.cpp.o: /home/danield/personal/vkquide/src/vk_engine.cpp
src/CMakeFiles/engine.dir/vk_engine.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/vk_engine.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/vk_engine.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Building CXX object src/CMakeFiles/engine.dir/vk_engine.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/vk_engine.cpp.o -MF CMakeFiles/engine.dir/vk_engine.cpp.o.d -o CMakeFiles/engine.dir/vk_engine.cpp.o -c /home/danield/personal/vkquide/src/vk_engine.cpp

src/CMakeFiles/engine.dir/vk_engine.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/vk_engine.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/vk_engine.cpp > CMakeFiles/engine.dir/vk_engine.cpp.i

src/CMakeFiles/engine.dir/vk_engine.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/vk_engine.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/vk_engine.cpp -o CMakeFiles/engine.dir/vk_engine.cpp.s

src/CMakeFiles/engine.dir/vk_loader.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/vk_loader.cpp.o: /home/danield/personal/vkquide/src/vk_loader.cpp
src/CMakeFiles/engine.dir/vk_loader.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/vk_loader.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/vk_loader.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "Building CXX object src/CMakeFiles/engine.dir/vk_loader.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/vk_loader.cpp.o -MF CMakeFiles/engine.dir/vk_loader.cpp.o.d -o CMakeFiles/engine.dir/vk_loader.cpp.o -c /home/danield/personal/vkquide/src/vk_loader.cpp

src/CMakeFiles/engine.dir/vk_loader.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/vk_loader.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/vk_loader.cpp > CMakeFiles/engine.dir/vk_loader.cpp.i

src/CMakeFiles/engine.dir/vk_loader.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/vk_loader.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/vk_loader.cpp -o CMakeFiles/engine.dir/vk_loader.cpp.s

src/CMakeFiles/engine.dir/camera.cpp.o: src/CMakeFiles/engine.dir/flags.make
src/CMakeFiles/engine.dir/camera.cpp.o: /home/danield/personal/vkquide/src/camera.cpp
src/CMakeFiles/engine.dir/camera.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx
src/CMakeFiles/engine.dir/camera.cpp.o: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
src/CMakeFiles/engine.dir/camera.cpp.o: src/CMakeFiles/engine.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Building CXX object src/CMakeFiles/engine.dir/camera.cpp.o"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -MD -MT src/CMakeFiles/engine.dir/camera.cpp.o -MF CMakeFiles/engine.dir/camera.cpp.o.d -o CMakeFiles/engine.dir/camera.cpp.o -c /home/danield/personal/vkquide/src/camera.cpp

src/CMakeFiles/engine.dir/camera.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/engine.dir/camera.cpp.i"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -E /home/danield/personal/vkquide/src/camera.cpp > CMakeFiles/engine.dir/camera.cpp.i

src/CMakeFiles/engine.dir/camera.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/engine.dir/camera.cpp.s"
	cd /home/danield/personal/vkquide/build/src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -Winvalid-pch -include /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/cmake_pch.hxx -S /home/danield/personal/vkquide/src/camera.cpp -o CMakeFiles/engine.dir/camera.cpp.s

# Object files for target engine
engine_OBJECTS = \
"CMakeFiles/engine.dir/main.cpp.o" \
"CMakeFiles/engine.dir/vk_initializers.cpp.o" \
"CMakeFiles/engine.dir/vk_images.cpp.o" \
"CMakeFiles/engine.dir/vk_descriptors.cpp.o" \
"CMakeFiles/engine.dir/vk_pipelines.cpp.o" \
"CMakeFiles/engine.dir/vk_engine.cpp.o" \
"CMakeFiles/engine.dir/vk_loader.cpp.o" \
"CMakeFiles/engine.dir/camera.cpp.o"

# External object files for target engine
engine_EXTERNAL_OBJECTS =

/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/cmake_pch.hxx.gch
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/main.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/vk_initializers.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/vk_images.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/vk_descriptors.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/vk_pipelines.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/vk_engine.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/vk_loader.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/camera.cpp.o
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/build.make
/home/danield/personal/vkquide/bin/engine: /usr/lib/x86_64-linux-gnu/libvulkan.so
/home/danield/personal/vkquide/bin/engine: third_party/fmt/libfmtd.a
/home/danield/personal/vkquide/bin/engine: third_party/libvkbootstrap.a
/home/danield/personal/vkquide/bin/engine: third_party/libimgui.a
/home/danield/personal/vkquide/bin/engine: third_party/fastgltf/libfastgltf.a
/home/danield/personal/vkquide/bin/engine: /usr/lib/x86_64-linux-gnu/libvulkan.so
/home/danield/personal/vkquide/bin/engine: third_party/SDL/libSDL2-2.0d.so.0.2800.4
/home/danield/personal/vkquide/bin/engine: third_party/fastgltf/libfastgltf_simdjson.a
/home/danield/personal/vkquide/bin/engine: src/CMakeFiles/engine.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/danield/personal/vkquide/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_10) "Linking CXX executable /home/danield/personal/vkquide/bin/engine"
	cd /home/danield/personal/vkquide/build/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/engine.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/engine.dir/build: /home/danield/personal/vkquide/bin/engine
.PHONY : src/CMakeFiles/engine.dir/build

src/CMakeFiles/engine.dir/clean:
	cd /home/danield/personal/vkquide/build/src && $(CMAKE_COMMAND) -P CMakeFiles/engine.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/engine.dir/clean

src/CMakeFiles/engine.dir/depend:
	cd /home/danield/personal/vkquide/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/danield/personal/vkquide /home/danield/personal/vkquide/src /home/danield/personal/vkquide/build /home/danield/personal/vkquide/build/src /home/danield/personal/vkquide/build/src/CMakeFiles/engine.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : src/CMakeFiles/engine.dir/depend
