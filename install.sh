#!/bin/bash
# install.sh - Easy one-command pqCheck installation

set -e

VERSION="1.0.0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OS=$(uname -s)
DISTRO=""

# Detect Linux distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO="${ID:-unknown}"
fi

echo "╔════════════════════════════════════════════════════════╗"
echo "║              pqCheck Installation Script              ║"
echo "║                     v$VERSION                        ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Check OS
if [ "$OS" != "Linux" ]; then
    echo "❌ Unsupported operating system: $OS"
    echo "   pqCheck requires Linux. For macOS/Windows, use WSL2."
    exit 1
fi

echo "📋 Detected System:"
echo "   OS: $OS"
echo "   Distro: ${DISTRO:-unknown}"
echo ""

# Show installation options
echo "🔨 Installation Options:"
echo ""
echo "  1) Local user installation (~/.local) - No privileges needed"
echo "  2) System-wide installation - Requires sudo"
echo "  3) Build DEB package - For Debian/Ubuntu systems"
echo "  4) Build RPM package - For RHEL/CentOS/Fedora systems"
echo ""
read -p "Choose option (1-4): " option

case $option in
    1)
        echo ""
        echo "📦 Installing to ~/.local..."
        cd "$SCRIPT_DIR"
        make clean
        make WITH_TUI=1
        make install
        
        # Add to PATH if needed
        if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
            echo ""
            echo "⚠️  ~/.local/bin is not in your PATH"
            echo "Add this to your ~/.bashrc or ~/.zshrc:"
            echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
            echo ""
        fi
        
        echo "✅ Installation complete!"
        echo ""
        echo "Next steps:"
        echo "  pqCheck --gen-test -o test.pcap"
        echo "  pqCheck -A test.pcap -r test.pcap -o alerts.jsonl"
        echo "  pqCheck --tui -r test.pcap"
        ;;
        
    2)
        echo ""
        echo "📦 Installing system-wide to /usr/local..."
        cd "$SCRIPT_DIR"
        make clean
        make WITH_TUI=1 WITH_LIBPQ=1
        sudo make install PREFIX=/usr/local
        
        echo ""
        echo "✅ System-wide installation complete!"
        echo ""
        echo "Enable daemon mode (optional):"
        echo "  sudo cp packaging/pqcheck.service /etc/systemd/system/"
        echo "  sudo systemctl daemon-reload"
        echo "  sudo systemctl enable pqcheck"
        echo "  sudo systemctl start pqcheck"
        ;;
        
    3)
        echo ""
        echo "📦 Building DEB package..."
        
        # Check for fpm
        if ! command -v fpm &> /dev/null; then
            echo "⚠️  fpm not found. Installing..."
            case "$DISTRO" in
                ubuntu|debian)
                    sudo apt-get update
                    sudo apt-get install -y ruby-dev
                    sudo gem install fpm
                    ;;
                *)
                    echo "❌ Unsupported distro for automatic fpm install"
                    echo "Install manually: sudo apt-get install ruby-dev && sudo gem install fpm"
                    exit 1
                    ;;
            esac
        fi
        
        cd "$SCRIPT_DIR"
        bash packaging/build-deb.sh
        
        echo ""
        echo "✅ Installation ready!"
        echo "Install with: sudo dpkg -i pqcheck_${VERSION}_amd64.deb"
        ;;
        
    4)
        echo ""
        echo "📦 Building RPM package..."
        
        # Check for fpm
        if ! command -v fpm &> /dev/null; then
            echo "⚠️  fpm not found. Installing..."
            case "$DISTRO" in
                rhel|centos|fedora)
                    sudo dnf install -y ruby-devel
                    sudo gem install fpm
                    ;;
                *)
                    echo "❌ Unsupported distro for automatic fpm install"
                    echo "Install manually: sudo dnf install ruby-devel && sudo gem install fpm"
                    exit 1
                    ;;
            esac
        fi
        
        cd "$SCRIPT_DIR"
        bash packaging/build-rpm.sh
        
        echo ""
        echo "✅ Package build ready!"
        echo "Install with: sudo dnf install pqcheck-${VERSION}-1.x86_64.rpm"
        ;;
        
    *)
        echo "❌ Invalid option"
        exit 1
        ;;
esac

echo ""
echo "📖 Documentation:"
echo "  Quick Start: docs/QUICK-REF.md"
echo "  Full Guide: docs/quickstart.md"
echo "  CLI Help: pqCheck -h"
echo ""
