# pqCheck Automated Release & Packaging Workflows

**Status:** ✅ Fully automated with GitHub Actions

This document explains how pqCheck uses GitHub Actions to automatically build, test, and release packages.

---

## Quick Start: Creating a Release

### Option 1: Production Release (Recommended)
```bash
# 1. Create an annotated tag
git tag -a v1.1.0 -m "pqCheck 1.1.0 release"

# 2. Push the tag
git push origin v1.1.0

# 3. GitHub Actions automatically:
#    - Runs full test suite
#    - Generates changelog
#    - Builds DEB package
#    - Builds RPM package
#    - Creates GitHub release with all artifacts
```

### Option 2: Pre-Release (Testing)
```bash
# 1. Create a branch
git checkout -b release/v1.1.0-rc1

# 2. Push to GitHub
git push origin release/v1.1.0-rc1

# 3. GitHub Actions automatically:
#    - Builds pre-release binaries
#    - Creates draft release
#    - Generates changelog for testing
```

### Option 3: Manual Trigger
```bash
# Push to main branch with workflow_dispatch enabled
# Or trigger manually from GitHub Actions web interface
```

---

## Workflow Architecture

### 1. **CI Workflow** (`ci.yml`)
Runs on every push and pull request.

**Triggers:**
- Push to `main` or `master` branch
- Any pull request
- Manual dispatch (`workflow_dispatch`)

**Jobs:**
- ✅ `build-and-test`
  - Compiles default and TUI variants
  - Runs 25 unit tests
  - Runs security audit tests
  
- ✅ `postgres-audit`
  - Spins up PostgreSQL 16 service
  - Tests libpq correlation functionality
  - Validates SQL inspection

**Duration:** ~3-5 minutes

**Status Badge:**
```markdown
[![CI Status](https://github.com/YOUR_ORG/pqCheck/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_ORG/pqCheck/actions)
```

---

### 2. **Pre-Release Workflow** (`prerelease.yml`)
Runs on release branch creation.

**Triggers:**
- Push to branch matching `release/**`
- Manual dispatch

**Jobs:**
- 📝 `changelog`
  - Compares current branch to latest tag
  - Generates changelog markdown
  - Uploads as artifact
  
- 📦 `source-archive`
  - Creates `pqCheck-source.tar.gz`
  - Excludes `.git`, `pqCheck` binary, `dist/`
  
- 🔨 `packages`
  - Builds DEB package
  - Builds RPM package
  - Uploads as artifacts

**Output Files:**
- `pqCheck-source.tar.gz`
- `pqcheck_1.0.0_amd64.deb`
- `pqcheck-1.0.0-1.x86_64.rpm`
- `CHANGELOG.md`

**Duration:** ~5-7 minutes

**Typical Workflow:**
```bash
git checkout -b release/v1.1.0
# Make changes, bump version
git commit -m "Bump to v1.1.0"
git push origin release/v1.1.0
# GitHub Actions builds and creates draft release
```

---

### 3. **Release Workflow** (`release.yml`)
Runs when a git tag is pushed.

**Triggers:**
- Push of tag matching `v*` pattern (e.g., `v1.0.0`)

**Jobs:**
- 📝 `changelog`
  - Compares to previous tag
  - Generates detailed changelog
  
- 📦 `source-archive`
  - Creates reproducible source tarball
  
- 🔨 `packages`
  - Matrix build: DEB + RPM
  - Each format tested independently
  
- 🚀 `publish-release`
  - Downloads all artifacts
  - Creates GitHub release
  - Uploads all files to release
  - Uses generated changelog as release notes

**Output: Published GitHub Release**
- Release name: `pqCheck v1.0.0`
- Release notes from git log
- 3 downloadable artifacts:
  - Source tarball
  - DEB package
  - RPM package

**Duration:** ~8-10 minutes (total)

---

## Workflow Files Reference

### Build Dependencies (auto-installed)
```yaml
build-essential    # GCC, Make
libpcap-dev        # Packet capture
libpq-dev          # PostgreSQL client
libncurses-dev     # Terminal UI
python3            # Test scripts
ruby + fpm         # Package manager
postgresql-client  # pg_isready, psql
```

### Build Flags Used
```bash
make -B                    # Clean build
WITH_LIBPQ=1              # Include PostgreSQL support
WITH_TUI=1                # Include terminal UI
```

### Packaging Scripts
- `packaging/build-deb.sh` — Creates DEB package
- `packaging/build-rpm.sh` — Creates RPM package

---

## How to Trigger Each Workflow

### Trigger CI (every push)
```bash
git commit -m "Fix bug"
git push origin main
# CI runs automatically
```

### Trigger Pre-Release
```bash
git checkout -b release/v1.1.0
git push origin release/v1.1.0
# Pre-Release workflow runs
# Creates draft release with packages
```

### Trigger Production Release
```bash
git tag -a v1.1.0 -m "v1.1.0: New features"
git push origin v1.1.0
# Release workflow runs
# Creates published release with full artifacts
```

### Trigger Manually
**From GitHub Web:**
1. Go to **Actions** tab
2. Select workflow
3. Click **Run workflow**
4. Choose branch/inputs

**From CLI:**
```bash
gh workflow run ci.yml
gh workflow run prerelease.yml -b main
gh workflow run release.yml
```

---

## Release Checklist

### Before Publishing a Release

- [ ] All tests pass locally: `make test`
- [ ] Commit all changes: `git commit -m "..."`
- [ ] Push to main: `git push origin main`
- [ ] Wait for CI to pass (~5 min)
- [ ] Verify no warnings in build
- [ ] Check all 25 tests passed

### Creating the Release

- [ ] Create annotated tag: `git tag -a v1.1.0 -m "v1.1.0: Description"`
- [ ] Push tag: `git push origin v1.1.0`
- [ ] GitHub Actions triggers automatically
- [ ] Review release draft (~10 min)
- [ ] Verify all 3 artifacts present
- [ ] Review auto-generated changelog
- [ ] Publish release

### Post-Release

- [ ] Download packages to verify signatures
- [ ] Test DEB: `sudo dpkg -i pqcheck_1.0.0_amd64.deb`
- [ ] Test RPM: `sudo rpm -i pqcheck-1.0.0-1.x86_64.rpm`
- [ ] Verify binary installed: `pqCheck --version`
- [ ] Announce release (optional)

---

## Workflow Status & Monitoring

### View Workflow Status
1. Go to repository
2. Click **Actions** tab
3. View workflows by:
   - Status (passing, failing)
   - Trigger (PR, push, tag)
   - Branch
   - Run time

### Workflow Logs
**To debug a failed run:**
1. Click workflow run
2. Click job name
3. Expand failed step
4. View full build log

### Set Status Badge in README
```markdown
[![CI Status](https://github.com/ORG/pqCheck/actions/workflows/ci.yml/badge.svg)](...)
[![Release](https://img.shields.io/github/v/release/ORG/pqCheck)](...)
```

---

## Environment Variables & Secrets

### Auto-Available in Workflows
```bash
GITHUB_TOKEN              # Used for release creation
GITHUB_REF                # Branch/tag being pushed
GITHUB_REF_NAME           # Just the ref name (e.g., v1.0.0)
GITHUB_SHA                # Commit SHA
```

### Optional Secrets (for future use)
To add secrets for publishing to registries:
1. Go to **Settings** → **Secrets and variables**
2. Click **New repository secret**
3. Add secrets like:
   - `DOCKER_USERNAME` / `DOCKER_PASSWORD` (for Docker Hub)
   - `PYPI_TOKEN` (for PyPI)
   - `GPG_KEY` (for signed releases)

---

## Troubleshooting

### Release Failed - "libpq-fe.h: No such file or directory"
**Solution:** Already fixed in Makefile. Uses correct pg_config flags.

### Release Failed - "fpm command not found"
**Solution:** Workflow installs fpm automatically. Check Ubuntu version.

### DEB/RPM Not in Release Assets
1. Check artifact upload step in logs
2. Verify `package_glob` pattern matches actual file
3. Manually re-run workflow

### Changelog is Empty
**Normal for first release.** For subsequent releases:
- Ensure commits have clear messages
- Tag comparison works on 2+ tags

### Build Hangs on PostgreSQL Tests
**Check:**
- PostgreSQL service health endpoint (should be ready in 30s)
- Logs show "pg_isready" waiting
- May need to increase timeout

---

## Advanced: Custom Workflows

### To add Docker build:
```yaml
# .github/workflows/docker.yml
jobs:
  build-docker:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: docker/build-push-action@v5
        with:
          dockerfile: Dockerfile
          push: true
          tags: myorg/pqcheck:latest
```

### To publish to PyPI:
```yaml
# Add to release.yml
- name: Publish to PyPI
  run: |
    pip install twine
    twine upload dist/pqcheck*.whl
  env:
    TWINE_USERNAME: __token__
    TWINE_PASSWORD: ${{ secrets.PYPI_TOKEN }}
```

### To sign releases with GPG:
```yaml
- name: Sign artifacts
  run: |
    gpg --import <<< "${{ secrets.GPG_PRIVATE_KEY }}"
    gpg --armor --sign dist/pqcheck-*.tar.gz
```

---

## Workflow Performance

| Workflow | Duration | Cost | Trigger |
|----------|----------|------|---------|
| CI (build+test) | 3-5 min | Free | Every push |
| Pre-Release | 5-7 min | Free | Release branch |
| Release | 8-10 min | Free | Git tag `v*` |

**Note:** GitHub Actions provides 2,000 free minutes/month for private repos.

---

## Summary

✅ **What's automated:**
- Compile on every push
- Run 25 unit tests automatically
- Test PostgreSQL integration
- Build DEB package
- Build RPM package
- Create GitHub release
- Upload artifacts
- Generate changelog

✅ **How to release:**
```bash
git tag -a v1.1.0 -m "Release v1.1.0"
git push origin v1.1.0
# Wait 10 minutes → GitHub Release ready!
```

✅ **No manual steps needed** for packaging or release creation.

---

## Quick Links

- **Workflows:** `.github/workflows/`
- **Packaging Scripts:** `packaging/`
- **Build Configuration:** `Makefile`
- **CI Badge:** See README.md
- **Releases:** GitHub Releases tab

---

**Next:** [Read BUILD.md](BUILD.md) for manual build instructions.
