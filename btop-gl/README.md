# btop-gl - High-FPS OpenGL System Monitor

A high-performance OpenGL-based system monitor screensaver built on top of the original btop++ project. This version provides smooth, hardware-accelerated visualizations of system statistics perfect for use as a screensaver.

## Features

- **High-FPS Rendering**: Hardware-accelerated OpenGL graphics for smooth animations
- **Real-time System Monitoring**: CPU, Memory, Network, and Disk usage visualization
- **Screensaver Ready**: Designed for use as a screensaver with fullscreen mode
- **Cross-platform**: Built on existing btop++ collectors for maximum compatibility
- **Smooth Animations**: Animated graphs and transitions with configurable speeds
- **Modern Graphics**: Beautiful gradients, glow effects, and smooth lines

## Screenshots

*Coming soon...*

## Requirements

### Dependencies

- **OpenGL 3.3+** compatible graphics card
- **GLFW 3.x** for window management
- **GLEW** for OpenGL extension loading
- **CMake 3.12+** for building
- **C++20** compatible compiler (GCC 11+, Clang 13+, or MSVC 2019+)

### macOS

```bash
# Using Homebrew
brew install cmake glfw glew

# Using MacPorts
sudo port install cmake glfw glew
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install cmake libglfw3-dev libglew-dev build-essential
```

### Linux (Fedora/CentOS)

```bash
sudo dnf install cmake glfw-devel glew-devel gcc-c++
```

## Building

1. **Clone the repository** (if you haven't already):
```bash
git clone https://github.com/aristocratos/btop.git
cd btop
```

2. **Navigate to the btop-gl directory**:
```bash
cd btop-gl
```

3. **Create build directory and compile**:
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

4. **Install** (optional):
```bash
sudo make install
```

## Usage

### Basic Usage

```bash
# Run in windowed mode
./btop-gl

# Run in fullscreen mode
./btop-gl --fullscreen
./btop-gl -f

# Run as screensaver
./btop-gl --screensaver
./btop-gl -s
```

### Keyboard Controls

- **ESC** or **Q**: Quit the application
- **F11** or **F**: Toggle fullscreen mode

### Command Line Options

```
btop-gl [OPTIONS]

Options:
  -f, --fullscreen    Start in fullscreen mode
  -s, --screensaver   Run as screensaver (implies fullscreen)
  -h, --help          Show help message

Controls:
  ESC, Q              Quit
  F11, F              Toggle fullscreen
```

## macOS Screensaver Installation

### Automatic Installation (Recommended)

```bash
cd build
make install-screensaver
```

### Manual Installation

1. Build the screensaver bundle:
```bash
cd build
cmake ..
make BtopScreensaver
```

2. Install the screensaver:
```bash
cp -r Btop.saver ~/Library/Screen\ Savers/
```

3. Open **System Preferences** → **Desktop & Screen Saver** → **Screen Saver**
4. Select **Btop** from the list

## Configuration

The application uses the same system monitoring backend as btop++, so it will automatically detect and monitor your system's resources.

### Performance Tuning

- **VSync**: Enabled by default to prevent screen tearing
- **Multisampling**: 4x MSAA enabled for smooth lines
- **Update Rate**: Data is collected at 1-second intervals by default
- **Animation Speed**: Configurable in screensaver mode

## Architecture

btop-gl leverages the existing btop++ collectors for maximum compatibility and reliability:

- **Data Collection**: Uses original btop++ platform-specific collectors (`src/osx/`, `src/linux/`, etc.)
- **Rendering**: Modern OpenGL 3.3+ with GLSL shaders
- **Threading**: Separate data collection and rendering threads for smooth performance
- **Memory Management**: Efficient circular buffers for graph history

## File Structure

```
btop-gl/
├── CMakeLists.txt          # Build configuration
├── Info.plist.in           # macOS screensaver bundle info
├── README.md               # This file
├── include/                # Header files
│   ├── btop_glue.hpp       # Interface to original btop collectors
│   ├── btop_renderer.hpp   # OpenGL rendering engine
│   └── shader.hpp          # Shader utility class
├── src/                    # Source files
│   ├── btop_glue.cpp       # Data collection wrapper
│   ├── btop_renderer.cpp   # Rendering implementation
│   ├── main.cpp            # Application entry point
│   └── screensaver_mac.mm  # macOS screensaver wrapper
└── shaders/                # GLSL shader files
    ├── line.vert           # Line graph vertex shader
    ├── line.frag           # Line graph fragment shader
    ├── quad.vert           # Quad vertex shader
    └── quad.frag           # Quad fragment shader
```

## Troubleshooting

### Common Issues

1. **"Failed to initialize GLFW"**
   - Ensure you have a compatible graphics driver installed
   - Try running with software rendering: `LIBGL_ALWAYS_SOFTWARE=1 ./btop-gl`

2. **"ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ"**
   - Make sure you're running from the build directory or shaders are installed
   - Check that shader files exist in `./shaders/` or `/usr/local/share/btop-gl/shaders/`

3. **"Failed to initialize btop collector"**
   - Ensure you have permissions to read system information
   - On some systems, you might need to run with appropriate privileges

### Performance Issues

- **Low FPS**: Try disabling multisampling in the code or reducing update frequency
- **High CPU usage**: Reduce animation speed or graph history size
- **Memory usage**: The application maintains circular buffers for graph data

## Contributing

This project extends the original btop++ with OpenGL visualization. Contributions are welcome!

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project inherits the license from the original btop++ project (Apache License 2.0).

## Credits

- **btop++**: Original system monitoring application by [Aristocratos](https://github.com/aristocratos)
- **OpenGL**: Graphics rendering
- **GLFW**: Window and input management
- **GLEW**: OpenGL extension loading

## Related Projects

- [btop++](https://github.com/aristocratos/btop) - Original terminal-based system monitor
- [bpytop](https://github.com/aristocratos/bpytop) - Python version
- [bashtop](https://github.com/aristocratos/bashtop) - Bash version 