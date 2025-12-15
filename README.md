# Matrix Automation Visualizer

A C++ desktop application built with **Dear ImGui**, **GLFW**, and **OpenGL**. This project includes a pre-configured build command for **MinGW-w64** on Windows.

---

## Prerequisites

Before building the project, make sure you have the following installed:

- **Operating System:** Windows
- **Compiler:** MinGW-w64 (`g++`)
  - Ensure `g++` is added to your system **PATH** environment variable
- **Git (optional):** Required only if you want to clone the repository

---

## Project Structure

Ensure your project directory (`gui testing`) is structured as follows:

```
gui testing/
├── imgui/                  # Dear ImGui library source files
├── glfw/                   # GLFW library headers and binaries
│   ├── include/
│   └── lib-mingw-w64/
└── main.cpp                # Application source code
```

---

## Installation & Setup

### Clone the Repository

```
git clone https://github.com/towtu/matrix-automation.git
```

### Navigate to the Project Directory

Open **PowerShell**, **Command Prompt**, or a **VS Code Terminal**, then navigate to the folder containing `main.cpp`:

```
cd "path/to/your/gui testing"
```

---

## Compilation

To build the executable (`visualizer.exe`), run the following **single-line** command. This statically links the required OpenGL, ImGui, and GLFW libraries:

```
g++ main.cpp imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp -I imgui -I imgui/backends -I glfw/include -L glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32 -limm32 -static-libgcc -static-libstdc++ "-Wl,-subsystem,console" -o visualizer.exe
```

### Console Window Note

- Keeping `"-Wl,-subsystem,console"` enables the console window (recommended for debugging).
- To hide the console window, remove this flag.

---

## Usage

After a successful build, run the application from the terminal:

```
visualizer.exe
```

---

## Troubleshooting

### `g++` is not recognized

- Verify MinGW-w64 is installed
- Ensure the `bin` directory of MinGW-w64 is added to your system **PATH**

### Missing file or include errors

- Confirm that the `imgui` and `glfw` folders are located in the **same directory** as `main.cpp`
- Verify the folder names and paths match the project structure exactly

---

## License

This project is provided as-is for educational and experimental purposes.

