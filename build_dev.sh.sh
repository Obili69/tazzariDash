#!/bin/bash
# build_dev.sh - Quick development build
echo "Building for development (windowed mode)..."
./build.sh --dev

#!/bin/bash
# build_deployment.sh - Quick deployment build  
echo "Building for deployment (fullscreen mode)..."
./build.sh --deployment

#!/bin/bash
# run_dev.sh - Run development build
echo "Starting development dashboard (windowed)..."
if [ -f "./build/LVGLDashboard_dev" ]; then
    ./build/LVGLDashboard_dev
else
    echo "Development build not found. Run ./build_dev.sh first."
fi

#!/bin/bash
# run_deployment.sh - Run deployment build
echo "Starting deployment dashboard (fullscreen)..."
if [ -f "./build/LVGLDashboard_deployment" ]; then
    ./build/LVGLDashboard_deployment
else
    echo "Deployment build not found. Run ./build_deployment.sh first."
fi

#!/bin/bash
# install_and_build.sh - Complete setup script
echo "=== LVGL Dashboard Complete Build Setup ==="
echo ""

# Check if installer was run
if [ ! -f "./install_dashboard_deps.sh" ]; then
    echo "Error: install_dashboard_deps.sh not found!"
    echo "Please run the installation script first."
    exit 1
fi

# Make all scripts executable
chmod +x *.sh

echo "Step 1: Installing dependencies..."
if [ ! -d "./lvgl" ]; then
    ./install_dashboard_deps.sh
    echo "Please logout and login to apply group changes, then run this script again."
    exit 0
fi

echo ""
echo "Step 2: Building development version..."
./build.sh --dev

echo ""
echo "Step 3: Building deployment version..."
./build.sh --deployment

echo ""
echo "=== Build Complete! ==="
echo ""
echo "Available executables:"
echo "  Development (windowed):  ./build/LVGLDashboard_dev"
echo "  Deployment (fullscreen): ./build/LVGLDashboard_deployment"
echo ""
echo "Quick run scripts:"
echo "  ./run_dev.sh        - Run development build"
echo "  ./run_deployment.sh - Run deployment build"
echo ""
echo "Quick build scripts:"
echo "  ./build_dev.sh       - Build development only"
echo "  ./build_deployment.sh - Build deployment only"