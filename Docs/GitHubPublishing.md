# Public browser and Windows release

- Play: https://ATritle.github.io/Forbidden-Code-Survivor/
- Repository: https://github.com/ATritle/Forbidden-Code-Survivor
- Windows release: https://github.com/ATritle/Forbidden-Code-Survivor/releases/tag/v0.2.0
- Installer: https://github.com/ATritle/Forbidden-Code-Survivor/releases/download/v0.2.0/ForbiddenCode-Survivor-Setup.exe

The public repository contains the browser game's complete static files, shared tuning, native C++ source/configuration, authoring tools and documentation. Large native `Content/`, `ContentSource/` and original `SFX/` folders have not been uploaded in this publishing pass. Consequently a fresh clone runs the browser game, but rebuilding the Unreal edition also needs those local asset folders. The Windows installer includes the cooked assets needed to play.

Pages uses `.github/workflows/deploy-browser.yml`. Push relevant changes to `main` to validate and deploy `Web/`, or run the workflow manually from GitHub Actions. Node tests and a comparison with regenerated UE balance snapshots must pass before deployment.

The first publication was prepared in the separate local checkout `Artifacts/GitHubPublish`, preserving the original development workspace. Use that Git checkout for future commits, or clone the public repository elsewhere. Copy updated browser files and any changed shared/native definitions into the checkout, regenerate tuning with `Scripts/export_browser_rules.py`, test, commit and push. Do not run `git add .` in the original asset workspace until native LFS/storage has been planned.

The Windows download button in `Web/index.html` points to the pinned v0.2.0 release asset. Update that URL and size when publishing a new Windows version. Keep installers attached to releases, not in Git history or Pages artifacts. Build instructions and feature parity limits are in `TwoEditions.md`.
