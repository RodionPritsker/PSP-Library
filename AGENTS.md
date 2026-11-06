# PSP Library development rules

- Prioritize stable behavior on a real PSP. Keep PPSSPP and physical-PSP claims evidence-based; never imply runtime validation that was not performed.
- Preserve working subsystems and keep changes narrowly scoped to the requested task.
- Check the current GitHub Actions workflow and relevant logs/artifacts for build or release work.
- Do not change the PSPLIB database format or public APIs without an explicit task requiring that compatibility change.
- Preserve licenses and notices for borrowed code, fonts, images, and other components.
- Avoid duplicated code, speculative abstractions, and extra architectural layers; extend the existing design when it is sufficient.
- Treat `.agents/skills/` as project-local Codex guidance. Keep general policy here and specialized build, UI, and regression procedures in their respective skill files.
