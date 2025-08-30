#!/bin/bash
# build.sh - Main build script with options

set -e

# Default build mode
BUILD_MODE="dev"
BUILD_TYPE="Debug"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --deployment|--prod)
            BUILD_MODE="deployment"
            BUILD_TYPE="Release"
            shift
            ;;
        --dev|--development)
            BUILD_MODE="dev"
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --help|-h)
            echo "LVGL Dashboard Build Script"
            echo "Usage: $0 [options]"
            echo ""
            echo "Build Modes:"
            echo "  --dev, --development    Development build (windowed, default)"
            echo "  --deployment, --prod    Deployment build (fullscreen)"
            echo ""
            echo "Build Types:"
            echo "  --debug                 Debug build (default for dev)"
            echo "  --release               Release build (default for deployment)"
            echo ""
            echo "Examples:"
            echo "  $0                      Development build (windowed)"
            echo "  $0 --deployment         Deployment build (fullscreen)"
            echo "  $0 --dev --release      Development build with release optimizations"
            exit 0
            ;;
        *)
            echo "Unknown option $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Set deployment build type to Release by default
if [[ "$BUILD_MODE" == "deployment" && "$BUILD_TYPE" == "Debug" ]]; then
    BUILD_TYPE="Release"
fi

echo "Building LVGL Dashboard..."
echo "Build Mode: $BUILD_MODE"
echo "Build Type: $BUILD_TYPE"
echo ""

# Create build directory
mkdir -p build
cd build

# Configure CMake based on build mode
if [[ "$BUILD_MODE" == "deployment" ]]; then
    echo "Configuring for DEPLOYMENT (fullscreen)..."
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DDEPLOYMENT_BUILD=ON ..
else
    echo "Configuring for DEVELOPMENT (windowed)..."
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DDEPLOYMENT_BUILD=OFF ..
fi

echo ""
echo "Compiling project..."
make -j$(nproc)

echo ""
echo "Build complete!"

# Show the appropriate executable name
if [[ "$BUILD_MODE" == "deployment" ]]; then
    echo "Deployment executable: ./build/LVGLDashboard_deployment"
    echo ""
    echo "To run in fullscreen:"
    echo "  ./build/LVGLDashboard_deployment"
else
    echo "Development executable: ./build/LVGLDashboard_dev"
    echo ""
    echo "To run in development mode:"
    echo "  ./build/LVGLDashboard_dev"
fi

echo ""
echo "Build configuration summary:"
echo "  Mode: $BUILD_MODE"
echo "  Type: $BUILD_TYPE"
echo "  Display: $([ "$BUILD_MODE" == "deployment" ] && echo "Fullscreen" || echo "Windowed (1024x600)")"