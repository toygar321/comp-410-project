# Compiler
CPP = g++

# Compiler flags
CPP_FLAGS = -g -Wall -Wextra -std=c++17 

# Include directories
INCLUDES = -I./src/include -I./src

# Libraries
LIBS = -lGLEW -lGL -lglfw

# Name of the program
TARGET = a

# Source
SRCS = src/main.cpp \
       src/Application.cpp \
	   src/Camera.cpp \
	   src/SceneObject.cpp \
	   src/include/InitShader.cpp \
	   src/include/imgui.cpp \
	   src/include/imgui_draw.cpp \
	   src/include/imgui_impl_glfw.cpp \
	   src/include/imgui_impl_opengl3.cpp \
	   src/include/imgui_tables.cpp \
	   src/include/imgui_widgets.cpp 

OBJ_DIR = obj

#
OBJS = $(addprefix $(OBJ_DIR)/,$(SRCS:.cpp=.o))

# Default rule
all: $(TARGET)

# Rule to compile .cpp files to object files
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CPP) $(CPP_FLAGS) $(INCLUDES) -c $< -o $@

# Rule to link object files into the executable
$(TARGET): $(OBJS)
	$(CPP) $(CPP_FLAGS) $(OBJS) -o $(TARGET) $(LIBS)

# Clean rule
clean:
	rm -f $(TARGET)
	rm -rf $(OBJ_DIR)
