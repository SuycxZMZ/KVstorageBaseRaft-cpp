# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

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
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/suycx/Code/KVstorageBaseRaft-cpp

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/suycx/Code/KVstorageBaseRaft-cpp/build

# Include any dependencies generated for this target.
include example/raftCoreExample/CMakeFiles/callerMain.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include example/raftCoreExample/CMakeFiles/callerMain.dir/compiler_depend.make

# Include the progress variables for this target.
include example/raftCoreExample/CMakeFiles/callerMain.dir/progress.make

# Include the compile flags for this target's objects.
include example/raftCoreExample/CMakeFiles/callerMain.dir/flags.make

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/flags.make
example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o: /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/clerk.cpp
example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/suycx/Code/KVstorageBaseRaft-cpp/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o -MF CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o.d -o CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o -c /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/clerk.cpp

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.i"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/clerk.cpp > CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.i

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.s"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/clerk.cpp -o CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.s

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/flags.make
example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o: /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/raftServerRpcUtil.cpp
example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/suycx/Code/KVstorageBaseRaft-cpp/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o -MF CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o.d -o CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o -c /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/raftServerRpcUtil.cpp

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.i"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/raftServerRpcUtil.cpp > CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.i

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.s"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/suycx/Code/KVstorageBaseRaft-cpp/src/raftClerk/raftServerRpcUtil.cpp -o CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.s

example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/flags.make
example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.o: /home/suycx/Code/KVstorageBaseRaft-cpp/example/raftCoreExample/caller.cpp
example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/suycx/Code/KVstorageBaseRaft-cpp/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.o"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.o -MF CMakeFiles/callerMain.dir/caller.cpp.o.d -o CMakeFiles/callerMain.dir/caller.cpp.o -c /home/suycx/Code/KVstorageBaseRaft-cpp/example/raftCoreExample/caller.cpp

example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/callerMain.dir/caller.cpp.i"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/suycx/Code/KVstorageBaseRaft-cpp/example/raftCoreExample/caller.cpp > CMakeFiles/callerMain.dir/caller.cpp.i

example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/callerMain.dir/caller.cpp.s"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/suycx/Code/KVstorageBaseRaft-cpp/example/raftCoreExample/caller.cpp -o CMakeFiles/callerMain.dir/caller.cpp.s

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/flags.make
example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o: /home/suycx/Code/KVstorageBaseRaft-cpp/src/common/util.cpp
example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o: example/raftCoreExample/CMakeFiles/callerMain.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/suycx/Code/KVstorageBaseRaft-cpp/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o -MF CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o.d -o CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o -c /home/suycx/Code/KVstorageBaseRaft-cpp/src/common/util.cpp

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.i"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/suycx/Code/KVstorageBaseRaft-cpp/src/common/util.cpp > CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.i

example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.s"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/suycx/Code/KVstorageBaseRaft-cpp/src/common/util.cpp -o CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.s

# Object files for target callerMain
callerMain_OBJECTS = \
"CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o" \
"CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o" \
"CMakeFiles/callerMain.dir/caller.cpp.o" \
"CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o"

# External object files for target callerMain
callerMain_EXTERNAL_OBJECTS =

/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/clerk.cpp.o
/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/raftClerk/raftServerRpcUtil.cpp.o
/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: example/raftCoreExample/CMakeFiles/callerMain.dir/caller.cpp.o
/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: example/raftCoreExample/CMakeFiles/callerMain.dir/__/__/src/common/util.cpp.o
/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: example/raftCoreExample/CMakeFiles/callerMain.dir/build.make
/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: /home/suycx/Code/KVstorageBaseRaft-cpp/lib/libskip_list_on_raft.a
/home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain: example/raftCoreExample/CMakeFiles/callerMain.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/suycx/Code/KVstorageBaseRaft-cpp/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking CXX executable /home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain"
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/callerMain.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
example/raftCoreExample/CMakeFiles/callerMain.dir/build: /home/suycx/Code/KVstorageBaseRaft-cpp/bin/callerMain
.PHONY : example/raftCoreExample/CMakeFiles/callerMain.dir/build

example/raftCoreExample/CMakeFiles/callerMain.dir/clean:
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample && $(CMAKE_COMMAND) -P CMakeFiles/callerMain.dir/cmake_clean.cmake
.PHONY : example/raftCoreExample/CMakeFiles/callerMain.dir/clean

example/raftCoreExample/CMakeFiles/callerMain.dir/depend:
	cd /home/suycx/Code/KVstorageBaseRaft-cpp/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/suycx/Code/KVstorageBaseRaft-cpp /home/suycx/Code/KVstorageBaseRaft-cpp/example/raftCoreExample /home/suycx/Code/KVstorageBaseRaft-cpp/build /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample /home/suycx/Code/KVstorageBaseRaft-cpp/build/example/raftCoreExample/CMakeFiles/callerMain.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : example/raftCoreExample/CMakeFiles/callerMain.dir/depend

