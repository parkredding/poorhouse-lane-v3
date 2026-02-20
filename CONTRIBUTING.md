# Contributing to Poor House Dub v2

Thank you for your interest in contributing! This document provides guidelines and instructions for contributing to the project.

## Getting Started

### Prerequisites
- Node.js 18.x or 20.x
- Git
- A code editor (VS Code, Cursor, etc.)

### Setup

1. **Fork the repository**
   ```bash
   git clone https://github.com/parkredding/poor-house-dub-v2.git
   cd poor-house-dub-v2
   ```

2. **Install dependencies**
   ```bash
   npm install
   ```

3. **Run tests to verify setup**
   ```bash
   npm test
   ```

## Development Workflow

### 1. Create a Branch

Always create a feature branch for your work:

```bash
# For new features
git checkout -b feature/your-feature-name

# For bug fixes
git checkout -b fix/bug-description

# For tests
git checkout -b test/what-youre-testing
```

### 2. Make Changes

- Write clean, readable code
- Follow existing code style
- Add comments for complex logic
- Update documentation if needed

### 3. Test Your Changes

#### Run all tests
```bash
npm test
```

#### Run tests in watch mode (during development)
```bash
npm run test:watch
```

#### Check coverage
```bash
npm run test:coverage
```

**All tests must pass before submitting a PR.**

### 4. Write Tests

If you're adding new features or fixing bugs:

1. **Add tests** in `tests/dubsiren.test.js` or `tests/integration.test.js`
2. **Ensure tests fail** before your fix (TDD approach)
3. **Implement your changes**
4. **Verify tests pass**

Example test structure:
```javascript
describe('Your Feature', () => {
  test('should do something specific', () => {
    // Arrange
    synth.setParam('parameter', value);
    
    // Act
    synth.trigger();
    
    // Assert
    expect(synth.someProperty).toBe(expectedValue);
  });
});
```

### 5. Commit Your Changes

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

### 6. Push and Create PR

```bash
git push -u origin your-branch-name
```

Then create a Pull Request on GitHub with:
- **Clear title** describing the change
- **Description** explaining what and why
- **Test results** showing all tests pass
- **Screenshots** if UI changes are involved

## Pull Request Guidelines

### PR Title Format
- `feat: Add new feature description`
- `fix: Fix bug description`
- `test: Add tests for feature`
- `docs: Update documentation`
- `refactor: Refactor component name`

### PR Description Template
```markdown
## What does this PR do?
Brief description of changes

## Why is this needed?
Explain the problem or motivation

## How was it tested?
- [ ] All existing tests pass
- [ ] New tests added and passing
- [ ] Manually tested in browser

## Screenshots (if applicable)
Add screenshots or recordings

## Checklist
- [ ] Tests pass (`npm test`)
- [ ] Code follows project style
- [ ] Documentation updated
- [ ] No console errors
```

### PR Review Process

1. **Automated checks** run via GitHub Actions
   - All tests must pass
   - Coverage report generated
   
2. **Code review** by maintainer
   - Code quality check
   - Architecture review
   - Test coverage check

3. **Approval and merge**
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

### Testing
- One test per behavior
- Clear test descriptions
- Use `describe` blocks for organization
- Clean up after each test (use `afterEach`)

## Areas for Contribution

### High Priority
- 🐛 **Bug fixes** - Check GitHub Issues
- 🧪 **More tests** - Increase coverage
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

## Testing Guidelines

### What to Test
✅ **Do test:**
- All new features
- Bug fixes (write test first)
- Edge cases and error conditions
- Integration between components
- Parameter validation

❌ **Don't test:**
- Third-party libraries
- Browser APIs directly
- Obvious getters/setters

### Test Coverage Goals
- **Statements**: > 80%
- **Branches**: > 75%
- **Functions**: > 85%
- **Lines**: > 80%

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
