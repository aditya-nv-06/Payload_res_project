# Automated Release & Packaging Guide

**Status:** ✅ Fully automated with GitHub Actions  
**Last Updated:** May 9, 2026

---

## 🚀 Quick Release Command

To create a production release:

```bash
git tag -a v1.1.0 -m "Release v1.1.0"
git push origin v1.1.0
# GitHub Actions runs automatically (8-10 minutes)
# Result: GitHub Release with 3 artifacts
```

That's it! GitHub Actions handles:
- ✅ Building from source
- ✅ Running all 25 tests
- ✅ Creating DEB package
- ✅ Creating RPM package
- ✅ Generating changelog
- ✅ Publishing release
- ✅ Uploading all artifacts

---

## What Gets Automated

### 1. **CI Workflow** (on every push)
- Compile with all variants
- Run 25 unit tests
- Test PostgreSQL integration
- **Time:** 3-5 minutes
- **Cost:** Free

### 2. **Pre-Release Workflow** (on release branches)
- Build packages
- Create draft release
- **Command:** `git push origin release/v1.1.0`
- **Time:** 5-7 minutes

### 3. **Production Release** (on version tags)
- Run full test suite
- Build DEB package
- Build RPM package
- Generate changelog
- Create GitHub release
- Upload artifacts
- **Command:** `git tag -a v1.1.0 -m "..."`
- **Time:** 8-10 minutes
- **Automatically publishes** to GitHub Releases

---

## Release Artifacts Generated

When you create a release with a tag, GitHub automatically generates:

1. **Source Archive**
   - Filename: `pqCheck-source.tar.gz`
   - Size: ~500 KB
   - Contains: Full source code (no binaries)

2. **DEB Package** (Debian/Ubuntu)
   - Filename: `pqcheck_1.0.0_amd64.deb`
   - Size: ~200 KB
   - Install: `sudo apt install ./pqcheck_*.deb`

3. **RPM Package** (RedHat/CentOS/Fedora)
   - Filename: `pqcheck-1.0.0-1.x86_64.rpm`
   - Size: ~200 KB
   - Install: `sudo rpm -i pqcheck-*.rpm`

4. **Changelog** (auto-generated)
   - Generated from git commit messages
   - Compares to previous release
   - Included in release notes

---

## Step-by-Step: Create Your First Release

### Step 1: Verify tests pass locally
```bash
cd /home/aditya/res_cli/Payload_res_project
make clean && make test
# Should show: Results: 25 passed, 0 failed
```

### Step 2: Ensure all changes are committed
```bash
git status
# Should show: nothing to commit, working tree clean
git log --oneline -5
# Verify your commits are there
```

### Step 3: Create annotated tag
```bash
git tag -a v1.1.0 -m "pqCheck v1.1.0

- Add new detection rules
- Improve PostgreSQL correlation
- Fix compiler warnings"
```

### Step 4: Push tag to GitHub
```bash
git push origin v1.1.0
# GitHub Actions automatically starts!
```

### Step 5: Monitor workflow (optional)
```bash
# Check GitHub Actions tab
# Or use GitHub CLI:
gh workflow view release.yml
gh run list -w release.yml
```

### Step 6: Release is ready!
- Go to **Releases** tab on GitHub
- Download DEB/RPM/Source tarball
- Share release link

---

## Automation Workflows

### File Locations
```
.github/workflows/
├── ci.yml              ← Runs on every push
├── prerelease.yml      ← Runs on release/* branches
├── release.yml         ← Runs on v* tags
└── audit.yml           ← Security audit workflow
```

### Workflow Triggers

| Workflow | Trigger | When | Duration |
|----------|---------|------|----------|
| **ci.yml** | Push to main/master | Every commit | 3-5 min |
| **prerelease.yml** | Push to release/* | Release branch | 5-7 min |
| **release.yml** | Tag matching v* | Version tag | 8-10 min |
| **audit.yml** | Manual dispatch | On request | 2-3 min |

---

## Version Numbering

pqCheck uses semantic versioning: `MAJOR.MINOR.PATCH`

**Examples:**
- `v1.0.0` — Initial release
- `v1.1.0` — New features (minor version bump)
- `v1.1.1` — Bug fix (patch version bump)
- `v2.0.0` — Breaking changes (major version bump)

**To release v1.1.0:**
```bash
git tag -a v1.1.0 -m "pqCheck v1.1.0"
git push origin v1.1.0
```

---

## Built Artifacts

### What Gets Built
In each release workflow, GitHub Actions builds:

**Variant 1: Default**
```bash
make -B                 # No special flags
```

**Variant 2: Full-Featured**
```bash
make -B WITH_LIBPQ=1 WITH_TUI=1    # All features
```

Both variants are tested, then packaged into DEB and RPM.

### Package Contents
After release, you can:

**Install DEB:**
```bash
sudo apt install pqcheck_1.0.0_amd64.deb
pqCheck --version
```

**Install RPM:**
```bash
sudo rpm -i pqcheck-1.0.0-1.x86_64.rpm
pqCheck --version
```

**Use from source:**
```bash
tar -xzf pqCheck-source.tar.gz
cd pqCheck-1.0.0
make install                # To ~/.local
sudo make install          # To /usr/local
```

---

## Status Badges (for README)

### CI Status
```markdown
[![CI](https://github.com/YOUR_ORG/pqCheck/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_ORG/pqCheck/actions)
```

### Latest Release
```markdown
[![Release](https://img.shields.io/github/v/release/YOUR_ORG/pqCheck)](https://github.com/YOUR_ORG/pqCheck/releases)
```

### Downloads
```markdown
[![DEB](https://img.shields.io/badge/download-deb-blue)](https://github.com/YOUR_ORG/pqCheck/releases)
[![RPM](https://img.shields.io/badge/download-rpm-red)](https://github.com/YOUR_ORG/pqCheck/releases)
```

---

## Troubleshooting Workflow Failures

### Build fails with "libpq-fe.h not found"
✅ Already fixed! Makefile uses correct pg_config flags.

### Tests fail in CI but pass locally
- Check: Different Ubuntu version on CI (18.04 vs 22.04)
- Check: Missing library dependencies
- Action: Run CI job locally or check logs

### Release artifacts not in release
- Check: Artifact names in workflow match actual files
- Check: Step succeeded (not skipped/failed)
- Action: Manually re-run workflow, or trigger manually

### Changelog is empty
- Normal for first release (no previous tag)
- Next release will compare v1.0.0 → v1.1.0
- Action: Create more releases for better changelog history

### Workflow running too long
- Normal: First run ~10 min, cached runs ~5 min
- Action: Parallel jobs speed it up (matrix builds)

---

## Manual Release (if automation fails)

If GitHub Actions fails, you can manually create a release:

### Build packages locally
```bash
cd /home/aditya/res_cli/Payload_res_project

# Clean build
make clean

# Build with all features
make WITH_LIBPQ=1 WITH_TUI=1

# Create packages
bash packaging/build-deb.sh
bash packaging/build-rpm.sh

# Create source archive
tar -czf pqCheck-source.tar.gz \
    --exclude=.git \
    --exclude=pqCheck \
    --exclude=.o \
    .

# Upload to GitHub manually
# Go to Releases → Draft new release → Upload files
```

---

## Continuous Delivery

### Current Setup: Automated
```
1. Developer commits code
   ↓
2. GitHub Actions CI runs (3 min)
   ↓
3. All tests pass?
   ├─ YES → Ready for release
   └─ NO  → Notifies developer
```

### Release: Fully Automated
```
1. Developer creates tag: v1.1.0
   ↓
2. GitHub Actions release.yml starts
   ├─ Builds DEB
   ├─ Builds RPM
   ├─ Generates changelog
   └─ Creates release
   ↓
3. GitHub Release published (8-10 min)
   ↓
4. Users download from Releases tab
```

---

## Advanced Topics

### Signing Releases (GPG)
```bash
git tag -s -a v1.1.0 -m "Signed release"  # Sign tag
git push origin v1.1.0
```

### Pre-releases
```bash
git tag -a v1.1.0-rc1 -m "Release candidate 1"
git push origin v1.1.0-rc1
# GitHub Actions builds, marks as pre-release
```

### Docker Builds
Can add additional workflow for Docker images.
See main [AUTOMATION.md](../../docs/AUTOMATION.md) for examples.

### Custom Distribution
Can extend workflows to publish to:
- Docker Hub
- PyPI (Python packages)
- Package repositories
- S3 buckets

---

## Next Steps

1. **Create First Release:**
   - `git tag -a v1.0.0 -m "Initial release"`
   - `git push origin v1.0.0`
   - Wait 10 minutes

2. **Monitor Release:**
   - Go to GitHub Actions
   - View release.yml workflow
   - Check for completion

3. **Download Artifacts:**
   - Go to Releases
   - Download DEB, RPM, or source
   - Test installation

4. **Announce Release:**
   - Share GitHub Release link
   - Post on social media / forums
   - Update documentation

---

## Reference

- **Full Automation Guide:** [docs/AUTOMATION.md](../../docs/AUTOMATION.md)
- **Build Guide:** [BUILD.md](../../BUILD.md)
- **Packaging Scripts:** `packaging/`
- **GitHub Actions Status:** GitHub Actions tab
- **Release History:** GitHub Releases tab

---

**Ready to release? Run:**
```bash
git tag -a v1.1.0 -m "pqCheck v1.1.0" && git push origin v1.1.0
```

Everything else is automatic! ✅
