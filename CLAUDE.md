# btop++ Development Guide

## GitLab Repository

### Repository Information
- **URL**: https://gitlab.berger.sx/mac/btop-plusplus
- **Group**: MAC
- **Project Name**: BTOP++
- **Path**: mac/btop-plusplus

### Git Remotes
| Remote | URL | Purpose |
|--------|-----|---------|
| `gitlab` | https://gitlab.berger.sx/mac/btop-plusplus.git | Primary development |
| `origin` | https://github.com/emanuelez/btop.git | Upstream fork |
| `emanuelez` | https://github.com/emanuelez/btop.git | Upstream fork |

### Branches
- `main` - Main development branch
- `gpu-apple-silicon-nosudo` - Apple Silicon GPU support (no sudo required)

## GitLab CLI (glab) Setup

### Authentication
```bash
# Login to GitLab instance
glab auth login --hostname gitlab.berger.sx --token YOUR_PAT_TOKEN

# Verify authentication
glab auth status --hostname gitlab.berger.sx
```

### Common Commands
```bash
# List issues
GITLAB_HOST=gitlab.berger.sx glab issue list

# Create issue
GITLAB_HOST=gitlab.berger.sx glab issue create --title "Issue title" --description "Description"

# List merge requests
GITLAB_HOST=gitlab.berger.sx glab mr list

# Create merge request
GITLAB_HOST=gitlab.berger.sx glab mr create --source-branch feature-branch --title "MR title"

# View project
GITLAB_HOST=gitlab.berger.sx glab repo view mac/btop-plusplus
```

### Push/Pull with Credentials
For authenticated operations, use the `gitlab` remote:
```bash
# Push changes
git push gitlab branch-name

# Pull changes
git pull gitlab branch-name

# Push all branches
git push gitlab --all

# Push tags
git push gitlab --tags
```

### Git Credential Helper (Optional)
To avoid entering credentials repeatedly:
```bash
# Store credentials in macOS Keychain
git config --global credential.helper osxkeychain

# Or use the glab credential helper
git config --global credential.https://gitlab.berger.sx.helper "glab auth git-credential"
```

## Project Structure

See Serena memories for detailed project information:
- `project_overview.md` - Architecture and purpose
- `suggested_commands.md` - Build commands
- `code_style.md` - Style conventions
- `task_completion.md` - Pre-commit checklist

## Quick Reference

### Build
```bash
make                    # Standard build
make DEBUG=true         # Debug build
make clean && make      # Clean rebuild
```

### Run
```bash
./bin/btop              # Run btop
./bin/btop --help       # Show options
```

### Git Workflow
```bash
# Ensure on correct branch
git checkout gpu-apple-silicon-nosudo

# Make changes, then:
git add -A
git commit -m "Description of changes"
git push gitlab gpu-apple-silicon-nosudo
```

## CI/CD Pipeline

### Pipeline Overview
The project uses GitLab CI/CD (`.gitlab-ci.yml`) with the following stages:

| Stage | Jobs | Description |
|-------|------|-------------|
| **build** | `build:macos-arm64`, `build:linux-x86_64` | Compile for target platforms |
| **test** | `test:macos-smoke`, `test:linux-smoke` | Basic smoke tests |
| **package** | `package:tarball` | Create release archives |
| **release** | `release:gitlab` | Create GitLab releases (tags only) |

### Build Matrix

| Job | Platform | GPU Support | Trigger |
|-----|----------|-------------|---------|
| `build:macos-arm64` | macOS ARM64 | Apple Silicon | Auto |
| `build:macos-arm64-debug` | macOS ARM64 | Apple Silicon | Manual |
| `build:linux-x86_64` | Linux x86_64 | No | Auto |
| `build:linux-x86_64-static` | Linux x86_64 (static) | No | Manual/Tags |
| `build:linux-alpine` | Alpine/musl | No | Manual/Tags |
| `build:nightly` | macOS ARM64 | Apple Silicon | Scheduled/Manual |

### Runner Requirements

#### macOS Runner (required for Apple Silicon builds)
```yaml
tags:
  - macos
  - arm64
```
Register a self-hosted runner on an Apple Silicon Mac:
```bash
# Download and install GitLab Runner
brew install gitlab-runner

# Register runner
gitlab-runner register \
  --url https://gitlab.berger.sx \
  --registration-token YOUR_TOKEN \
  --executor shell \
  --tag-list "macos,arm64" \
  --description "macOS ARM64 Runner"

# Start runner
gitlab-runner start
```

#### Linux Runner (Docker)
```yaml
tags:
  - linux
  - docker
```
Uses Docker executor with Ubuntu/Alpine images.

### Pipeline Triggers

| Event | Builds Triggered |
|-------|------------------|
| Push to any branch | macOS ARM64, Linux x86_64, smoke tests |
| Push tag | All builds + packaging + release |
| Scheduled pipeline | Nightly build |
| Manual trigger | Any job can be triggered manually |

### Artifacts

Build artifacts are stored for:
- **Regular builds**: 30 days
- **Debug builds**: 7 days
- **Nightly builds**: 7 days
- **Release tarballs**: 90 days

### Creating a Release
```bash
# Tag the release
git tag -a v1.5.0 -m "Release v1.5.0"
git push gitlab v1.5.0

# Pipeline will automatically:
# 1. Build all platforms
# 2. Run tests
# 3. Create tarball
# 4. Create GitLab release with artifacts
```

### Manual Jobs
Some jobs require manual triggering in the GitLab UI:
- `build:macos-arm64-debug` - Debug build
- `build:linux-x86_64-static` - Static Linux build
- `build:linux-alpine` - Alpine/musl build
- `build:nightly` - Full clean rebuild
- `lint:cppcheck` - Static code analysis

### Pipeline URLs
- **Pipelines**: https://gitlab.berger.sx/mac/btop-plusplus/-/pipelines
- **Jobs**: https://gitlab.berger.sx/mac/btop-plusplus/-/jobs
- **Releases**: https://gitlab.berger.sx/mac/btop-plusplus/-/releases
