#!/bin/bash
# build-rpm.sh - Build RPM package for pqCheck

set -e

VERSION="1.0.0"
ARCH="x86_64"
STAGEDIR=$(mktemp -d)

echo "📦 Building RPM package for pqCheck..."
echo "   Version: $VERSION"
echo "   Stage: $STAGEDIR"

# Build with all features
make clean
make WITH_TUI=1 WITH_LIBPQ=1

# Install to staging directory
mkdir -p "$STAGEDIR"
make install DESTDIR="$STAGEDIR" PREFIX=/usr

# Create systemd service
mkdir -p "$STAGEDIR/etc/systemd/system"
cp packaging/pqcheck.service "$STAGEDIR/etc/systemd/system/"

# Create log directory
mkdir -p "$STAGEDIR/var/log/pqcheck"

# Create pqcheck user helper
mkdir -p "$STAGEDIR/usr/share/pqcheck"
cat > "$STAGEDIR/usr/share/pqcheck/postinst.sh" << 'EOF'
#!/bin/bash
# Create pqcheck user and group
if ! id pqcheck &>/dev/null; then
    echo "[+] Creating pqcheck user..."
    useradd -r -s /sbin/nologin -d /var/log/pqcheck pqcheck
fi

# Fix permissions
chown pqcheck:pqcheck /var/log/pqcheck
chmod 750 /var/log/pqcheck

# Enable systemd service (optional)
systemctl daemon-reload
echo "[✓] pqCheck installed. To enable daemon mode:"
echo "    sudo systemctl enable pqcheck"
echo "    sudo systemctl start pqcheck"
EOF
chmod 755 "$STAGEDIR/usr/share/pqcheck/postinst.sh"

# Build RPM package
if command -v fpm &> /dev/null; then
    fpm -s dir \
        -t rpm \
        -n pqcheck \
        -v "$VERSION" \
        -a "$ARCH" \
        -m "pqCheck Team <team@example.com>" \
        --url "https://github.com/aditya-resql/pqcheck" \
        --description "PostgreSQL SQL Injection Detection Sensor" \
        --license MIT \
        -d libpcap \
        -d ncurses-libs \
        -d libpq \
        --rpm-recommends postgresql \
        --after-install "$STAGEDIR/usr/share/pqcheck/postinst.sh" \
        -C "$STAGEDIR" \
        usr var etc
    
    echo ""
    echo "✅ RPM package created: pqcheck-${VERSION}-1.${ARCH}.rpm"
    echo ""
    echo "Installation:"
    echo "  sudo dnf install pqcheck-${VERSION}-1.${ARCH}.rpm"
    echo "  sudo systemctl start pqcheck"
else
    echo "❌ fpm not found. Install with:"
    echo "   # On CentOS/RHEL:"
    echo "   sudo dnf install -y git ruby-devel"
    echo "   sudo gem install fpm"
    echo ""
    echo "   # On Fedora:"
    echo "   sudo dnf install -y ruby-devel"
    echo "   sudo gem install fpm"
    exit 1
fi

# Cleanup
rm -rf "$STAGEDIR"
