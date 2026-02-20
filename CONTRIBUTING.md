# Contributing to Poor House Dub v2

Thank you for your interest in contributing! This document provides guidelines and instructions for contributing to the project.

## Getting Started

### Prerequisites
- Git
- A code editor (VS Code, Cursor, etc.)

### Setup

1. **Fork the repository**
   ```bash
   git clone https://github.com/parkredding/poor-house-dub-v2.git
   cd poor-house-dub-v2
   ```

## Development Workflow

### 1. Create a Branch

Always create a feature branch for your work:

```bash
# For new features
git checkout -b feature/your-feature-name

# For bug fixes
git checkout -b fix/bug-description
```

### 2. Make Changes

- Write clean, readable code
- Follow existing code style
- Add comments for complex logic
- Update documentation if needed

### 3. Test Your Changes

- Open `index.html` in a browser and verify the synth works correctly
- Test on Raspberry Pi hardware if possible
- Check the browser console for errors

### 4. Commit Your Changes

Use clear, descriptive commit messages:

```bash
git add .
git commit -m "Add feature: description of what you did

- Bullet point 1
- Bullet point 2
- Bullet point 3"
```

**Good commit messages:**
- ✅ "Fix delay feedback causing audio blow-up"
- ✅ "Add tape wobble effect to delay"
- ✅ "Update LFO depth range to 0-200%"

**Bad commit messages:**
- ❌ "fix bug"
- ❌ "update"
- ❌ "wip"

### 5. Push and Create PR

```bash
git push -u origin your-branch-name
```

Then create a Pull Request on GitHub with:
- **Clear title** describing the change
- **Description** explaining what and why
- **Screenshots** if UI changes are involved

## Pull Request Guidelines

### PR Title Format
- `feat: Add new feature description`
- `fix: Fix bug description`
- `docs: Update documentation`
- `refactor: Refactor component name`

### PR Description Template
```markdown
## What does this PR do?
Brief description of changes

## Why is this needed?
Explain the problem or motivation

## How was it tested?
- [ ] Manually tested in browser
- [ ] Tested on Raspberry Pi hardware

## Screenshots (if applicable)
Add screenshots or recordings

## Checklist
- [ ] Code follows project style
- [ ] Documentation updated
- [ ] No console errors
```

### PR Review Process

1. **Code review** by maintainer
   - Code quality check
   - Architecture review

2. **Approval and merge**
   - Squash and merge preferred
   - Delete branch after merge

## Code Style Guidelines

### JavaScript
- Use clear variable names
- Prefer `const` over `let`, avoid `var`
- Add comments for complex logic
- Keep functions small and focused

### Audio Code
- Always clean up audio nodes
- Use proper gain staging
- Clamp values to prevent blow-ups
- Document signal routing in comments

## Areas for Contribution

### High Priority
- 🐛 **Bug fixes** - Check GitHub Issues
- 📚 **Documentation** - Improve clarity
- ♿ **Accessibility** - ARIA labels, keyboard nav

### Medium Priority
- ✨ **New effects** - Chorus, phaser, etc.
- 🎛️ **UI improvements** - Better controls
- 🎵 **Presets** - Save/load sound presets
- 📱 **Mobile optimization** - Touch improvements

### Low Priority
- 🎨 **Themes** - Dark/light modes
- 🌐 **i18n** - Internationalization
- 🔌 **MIDI support** - MIDI input
- 📊 **Analytics** - Usage tracking

## Getting Help

- 💬 **Questions**: Open a GitHub Discussion
- 🐛 **Bugs**: Open a GitHub Issue
- 💡 **Feature Ideas**: Open a GitHub Issue with `enhancement` label
- 📧 **Private matters**: Contact maintainer directly

## Code of Conduct

- Be respectful and inclusive
- Provide constructive feedback
- Focus on the code, not the person
- Help others learn and grow

## Recognition

Contributors will be:
- Listed in README.md
- Mentioned in release notes
- Credited in commit history

Thank you for contributing! 🎵🎉
