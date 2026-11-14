"""Attach linker files after the platform has created the actual ELF node."""
from pathlib import Path

Import("env")

program = env.get("PIOMAINPROG")
if program:
    root = Path(env.subst("$PROJECT_DIR"))
    env.Depends(program, [env.File(str(path)) for path in sorted((root / "ld").glob("*.ld"))])
