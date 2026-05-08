#!/bin/bash
# build-deb.sh - Build DEB package for pqCheck

set -e

VERSION="1.0.0"
MAINTAINER="pqCheck Team <team@example.com>"
ARCH="amd64"
STAGEDIR=$(mktemp -d)

echo "📦 Building DEB package for pqCheck..."
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
    useradd -r -s /usr/sbin/nologin -d /var/log/pqcheck pqcheck
fi

# Fix permissions
chown pqcheck:pqcheck /var/log/pqcheck
chmod 750 /var/log/pqcheck

# Enable systemd service (optional)
systemctl daemon-reload
echo "[✓] pqCheck installed. To enable daemon mode:"
echo "    sudo systemctl enable pqcheck"
echo "    sudo systemctl start pqcheck"
echo "    Or for immediate testing:"
echo "    pqCheck --gen-test -o /tmp/test.pcap"
EOF
chmod 755 "$STAGEDIR/usr/share/pqcheck/postinst.sh"

# Build DEB package
if command -v fpm &> /dev/null; then
    fpm -s dir \
        -t deb \
        -n pqcheck \
        -v "$VERSION" \
        -a "$ARCH" \
        -m "$MAINTAINER" \
        --url "https://github.com/aditya-resql/pqcheck" \
        --description "PostgreSQL SQL Injection Detection Sensor" \
        --license "MIT" \
        -d libpcap0.8 \
        -d libncurses6 \
        -d libpq5 \
        --deb-recommends postgresql-client \
        --after-install "$STAGEDIR/usr/share/pqcheck/postinst.sh" \
        -C "$STAGEDIR" \
        usr var etc
    
    echo ""
    echo "✅ DEB package created: pqcheck_${VERSION}_${ARCH}.deb"
    echo ""
    echo "Installation:"
    echo "  sudo dpkg -i pqcheck_${VERSION}_${ARCH}.deb"
    echo "  sudo systemctl start pqcheck"
else
    echo "❌ fpm not found. Install with:"
    echo "   sudo apt-get install -y ruby-dev"
    echo "   sudo gem install fpm"
    exit 1
fi

# Cleanup
rm -rf "$STAGEDIR"
