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
