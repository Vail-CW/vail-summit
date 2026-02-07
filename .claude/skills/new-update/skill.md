---
name: new-update
description: Build firmware, create a GitHub release with changelog, and attach firmware binaries.
allowed-tools: Read, Grep, Glob, Bash, Edit, AskUserQuestion
user-invocable: true
---

# VAIL SUMMIT - New Update Release

Build the firmware, create a GitHub release with a user-friendly changelog, and attach all firmware binaries.

## Step 1: Ask for Version Number

**ALWAYS ask the user what version number to use before doing anything else.** Use AskUserQuestion to ask:
- "What version number should this release use? (current version is X.XX)"
- Show the current version from `src/core/config.h` FIRMWARE_VERSION for reference.

Do NOT proceed until the user provides a version number.

## Step 2: Get the Previous Release Tag

Run:
```
git tag --sort=-creatordate | head -1
```
This gives you the last release tag (e.g., `v0.50`). You'll use this to generate the changelog.

## Step 3: Update Version in Three Places

Update the version number and date in all required locations:

1. **`src/core/config.h`**: Update `FIRMWARE_VERSION` to the new version (e.g., `"0.51"`) and `FIRMWARE_DATE` to today's date (e.g., `"2026-02-06"`)
2. **`README.md`** (line 5): Update the version badge: `![Version](https://img.shields.io/badge/version-X.XX-blue)`

## Step 4: Compile Firmware (Clean Build)

**IMPORTANT:** Always use `--clean` to force a full rebuild. Without it, arduino-cli reuses cached object files that may contain the OLD version string, resulting in firmware that reports the wrong version on the boot screen.

Build using the short-path Arduino CLI method (required on Windows to avoid path length issues). Must use `powershell.exe -Command` since the shell is bash:

```bash
powershell.exe -Command "cd C:\acli; .\arduino-cli.exe compile --clean --config-file arduino-cli.yaml --fqbn 'esp32:esp32:adafruit_feather_esp32s3:CDCOnBoot=cdc,PartitionScheme=huge_app,PSRAM=enabled,FlashSize=4M,USBMode=hwcdc' --output-dir C:\vs\build --export-binaries C:\vs"
```

**If the build fails, stop and report the error to the user. Do not continue.**

After a successful build, verify the version string is correct in the compiled binary:
```bash
powershell.exe -Command "Select-String -Path 'C:\vs\build\vail-summit.ino.bin' -Pattern 'FIRMWARE_VERSION' -Encoding byte"
```
Or use: `strings C:\vs\build\vail-summit.ino.bin | grep -i "0.XX"` (substituting the expected version number) to confirm.

## Step 5: Prepare Firmware Files

The three firmware files needed for flashing are in the `build/` directory. They need to be copied/renamed for the release:

- `build/vail-summit.ino.bin` → attach as `vail-summit.bin`
- `build/vail-summit.ino.bootloader.bin` → attach as `bootloader.bin`
- `build/vail-summit.ino.partitions.bin` → attach as `partitions.bin`

Copy them to a temporary staging location with clean names (use `powershell.exe -Command` since the shell is bash):
```bash
powershell.exe -Command "Copy-Item 'C:\vs\build\vail-summit.ino.bin' 'C:\vs\build\vail-summit.bin'; Copy-Item 'C:\vs\build\vail-summit.ino.bootloader.bin' 'C:\vs\build\bootloader.bin'; Copy-Item 'C:\vs\build\vail-summit.ino.partitions.bin' 'C:\vs\build\partitions.bin'"
```

## Step 6: Generate Changelog

Get all commits since the last release tag:
```
git log <previous_tag>..HEAD --pretty=format:"%s" --reverse
```

Using these commit messages, write a **user-friendly changelog** following these rules:

- **Target audience: non-technical ham radio operators** who use the device. They don't know what refactoring, CI, or architecture means.
- **Group changes into categories**: "New Features", "Improvements", "Bug Fixes" (skip empty categories)
- **Use plain language**: Instead of "Refactor mode registry", say "Improved internal organization for better reliability"
- **Skip purely internal/developer changes** that have zero user impact (CI changes, code cleanup with no behavior change). If ALL commits are internal, write a brief note like "Internal improvements for reliability and performance."
- **Be specific about user-facing behavior**: "Fixed volume control not saving between reboots" is good. "Fixed bug in settings" is too vague.
- **Bold feature names** for readability
- **Keep it concise** — one line per change, no paragraphs

Use this format:
```markdown
## What's New in vX.XX

### New Features
- **Feature Name** - Brief description of what users can now do

### Improvements
- **Area** - What got better and why users should care

### Bug Fixes
- Fixed [specific user-visible problem]

---
**Full Changelog**: https://github.com/Vail-CW/vail-summit/compare/<previous_tag>...vX.XX
```

## Step 7: Commit Version Changes

Stage and commit the version updates:
```
git add src/core/config.h README.md
git commit -m "vX.XX: <brief description>"
```

## Step 8: Create Git Tag

Create an annotated tag:
```
git tag -a vX.XX -m "vX.XX: <brief description>"
```

## Step 9: Push to GitHub

Push the commit and tag:
```
git push && git push origin vX.XX
```

## Step 10: Create GitHub Release with Assets

Create the release using `gh` CLI. The release must be **published (not a draft)**.

```
gh release create vX.XX --title "vX.XX - <Title>" --notes "<changelog>" C:\vs\build\vail-summit.bin C:\vs\build\bootloader.bin C:\vs\build\partitions.bin
```

Use a HEREDOC for the notes to preserve formatting:
```powershell
gh release create vX.XX --title "vX.XX - <Title>" --notes "$(cat <<'EOF'
<changelog content here>
EOF
)" C:\vs\build\vail-summit.bin C:\vs\build\bootloader.bin C:\vs\build\partitions.bin
```

**IMPORTANT:** Do NOT use `--draft`. The release must be published immediately.

## Step 11: Verify

Run `gh release view vX.XX` to confirm:
- Release is published (not draft)
- All 3 firmware assets are attached (vail-summit.bin, bootloader.bin, partitions.bin)
- Changelog is present and readable

Report the release URL to the user.

## Step 12: Clean Up

Remove the temporary renamed copies:
```bash
powershell.exe -Command "Remove-Item 'C:\vs\build\vail-summit.bin'; Remove-Item 'C:\vs\build\bootloader.bin'; Remove-Item 'C:\vs\build\partitions.bin'"
```
