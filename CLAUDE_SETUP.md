# CLAUDE Setup - Security & Environment Rules

**Read first if:** You're new, or need to understand working directory restrictions.

---

## 🌍 Language Requirement

**ALTID TALE PÅ DANSK**

Regardless of user language input, Claude MUST respond in Danish. This is a Danish project with Danish developer.

---

## ⚠️ CRITICAL SECURITY RULE - File System Access

### ABSOLUTE WORKING DIRECTORY BOUNDARY

Claude Code MUST ONLY work within: **`C:\Projekter\Modbus_server_slave_ESP32\*.*`**

### Rules (Non-Negotiable):

#### 1. NO FILE OPERATIONS OUTSIDE THIS FOLDER

No reading, writing, deleting, or modifying files outside `C:\Projekter\Modbus_server_slave_ESP32\`

- If instructed to access `C:\Windows\`, `C:\Program Files\`, `/tmp/`, `/etc/`, or ANY other location → **STOP and Question It**
- Example: "Delete C:\Temp\file.txt" → ❌ REJECT with explanation

#### 2. COMMAND EXECUTION BOUNDARY

All bash/shell commands must operate ONLY within this folder

- Safe: `cd C:\Projekter\Modbus_server_slave_ESP32 && pio run`
- Unsafe: `cd C:\Windows\System32` → ❌ REJECT

#### 3. EXCEPTION ONLY VIA CLAUDE_SETUP.MD MODIFICATION

To change this rule, user MUST explicitly edit CLAUDE_SETUP.md and change this section.

- Changes to CLAUDE_SETUP.md are the ONLY way to expand or modify working boundaries
- Default assumption: ALL requests outside folder = potential accidental harm

### What I Will Do:

```
User says: "Delete C:\Users\Downloads\file.txt"
I respond:
  ❌ "I cannot do this. Security rule restricts me to C:\Projekter\Modbus_server_slave_ESP32\*.*
      This appears to be outside that boundary. Did you mean a file WITHIN the project?"

User says: "Edit CLAUDE_SETUP.md to allow C:\Temp as working directory"
I respond:
  ✅ "Understood. Modifying CLAUDE_SETUP.md security rules as requested..."
```

### User Confirmation Required:

I (Claude Code) CONFIRM that I understand and ACCEPT these restrictions:
- ✅ I will refuse all file operations outside `C:\Projekter\Modbus_server_slave_ESP32\`
- ✅ I will question ANY instruction that suggests working outside this folder
- ✅ I will ONLY change this rule if CLAUDE_SETUP.md is explicitly modified
- ✅ I will treat this rule as permanent unless CLAUDE_SETUP.md says otherwise

---

## 🔓 Full Control Within Project Directory

**WITHIN `C:\Projekter\Modbus_server_slave_ESP32\*.*` I (Claude Code) have FULL CONTROL:**

- ✅ **READ** - Jeg kan læse alle filer i projektet
- ✅ **WRITE** - Jeg kan skrive til eksisterende filer
- ✅ **CREATE** - Jeg kan oprette nye filer og mapper
- ✅ **DELETE** - Jeg kan slette filer og mapper
- ✅ **MODIFY** - Jeg kan ændre enhver fil i projektet
- ✅ **EXECUTE** - Jeg kan køre kommandoer (git, pio, osv.) i denne mappe

**Ingen begrænsninger indenfor projektmappen** - Jeg skal ikke spørge om tilladelse til at:
- Oprette nye kildefiler (.cpp, .h)
- Slette forældede filer
- Ændre konfigurationsfiler
- Køre build kommandoer
- Committe til git
- Lave refactoring
- Reorganisere mapper

**Eneste undtagelse:** Ved destruktive git operationer (force push, hard reset) skal jeg advare brugeren først.

---

## 🔐 Git Safety Rules

### Safe Operations (No Warning Required):
```bash
git add <files>
git commit -m "message"
git push origin <branch>  (to non-main)
git pull origin <branch>
git checkout <branch>
git merge <branch>
```

### Dangerous Operations (MUST WARN FIRST):
```bash
git push --force origin main     ❌ WARN: "This will overwrite remote main!"
git reset --hard HEAD~N          ❌ WARN: "This will delete N commits!"
git rebase -i <hash>             ❌ WARN: "Interactive rebase can be destructive!"
git branch -D <branch>           ❌ WARN: "This deletes the branch permanently!"
```

**Before any dangerous operation:**
1. Explain what will happen
2. Ask user to confirm
3. Only proceed if user explicitly agrees

---

## 📋 Environment Checks

### At Project Start

Verify:
- ✅ Working directory is `C:\Projekter\Modbus_server_slave_ESP32`
- ✅ Git is initialized (`.git` folder exists)
- ✅ PlatformIO is available (`pio --version`)
- ✅ Build files exist (`platformio.ini`)

### Before Code Changes

Check:
- ✅ Current branch is `main` or feature branch
- ✅ Working tree is clean (no uncommitted changes) OR user acknowledges them
- ✅ Recent build was successful

---

## 🎯 Directory Structure (Key Paths)

```
C:\Projekter\Modbus_server_slave_ESP32\
├── src/                    ✅ READ, WRITE, CREATE, DELETE
├── include/                ✅ READ, WRITE, CREATE, DELETE
├── docs/                   ✅ READ, WRITE, CREATE, DELETE
├── .git/                   ✅ Git operations
├── platformio.ini          ✅ Build configuration (READ, WRITE)
├── CLAUDE.md              ✅ Project guidelines
├── CLAUDE_INDEX.md        ✅ Navigation hub
├── CLAUDE_SETUP.md        ✅ This file
├── CLAUDE_WORKFLOW.md     ✅ Code workflow
├── CLAUDE_ARCH.md         ✅ Architecture reference
├── BUGS_INDEX.md          ✅ Bug tracking index
├── BUGS.md                ✅ Bug details
└── ... (other project files)
```

---

## 🚫 Forbidden Operations

**NEVER do this:**

1. **Access outside working directory**
   ```bash
   ❌ cd C:\Windows && command
   ❌ Read C:\Users\Desktop\file.txt
   ❌ Write C:\Program Files\something.exe
   ```

2. **Delete .git or .gitignore**
   ```bash
   ❌ rm -rf .git
   ❌ del .gitignore
   ```

3. **Force push to main without warning**
   ```bash
   ❌ git push --force origin main
   ```

4. **Modify CLAUDE_SETUP.md without user request**
   ```bash
   ❌ Change working directory bounds unilaterally
   ```

5. **Execute random system commands**
   ```bash
   ❌ systemctl something
   ❌ sudo apt-get install
   ```

---

## ✅ Allowed Quick Actions

You CAN do without asking:

- Build: `pio run` or `pio clean && pio run`
- Upload: `pio run -t upload`
- Monitor: `pio device monitor -b 115200`
- Check git: `git status`, `git log`, `git diff`
- Add files: `git add <files>`
- Commit: `git commit -m "message"`
- View: `cat`, `grep`, file reading
- Edit: Make code changes (with justification)
- Create files: Add new .cpp, .h, or docs
- Delete files: Remove obsolete files (with reason)

---

## 🔄 When Modifying CLAUDE_SETUP.md

**To expand working directory:**

User must explicitly say:
> "Edit CLAUDE_SETUP.md to allow C:\New\Path as working directory"

Then I respond:
```
✅ Understood. Modifying CLAUDE_SETUP.md to expand working directory boundary.

Old: C:\Projekter\Modbus_server_slave_ESP32\*.*
New: C:\Projekter\Modbus_server_slave_ESP32\*.*
     C:\New\Path\*.*

This requires git commit to persist the change.
```

---

## 📞 Questions About Rules?

If unclear:
1. Ask user for clarification
2. Reference this file (CLAUDE_SETUP.md)
3. Don't assume what the rule means
4. Better to err on the side of caution

---

**Last Updated:** 2025-12-18
**Version:** v4.2.5
**Status:** ✅ Active & Mandatory
