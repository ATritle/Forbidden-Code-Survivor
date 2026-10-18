"""Reproduce the complete seven-contract visual asset pass in one Unreal process."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())/'Scripts'
runpy.run_path(str(root/'author_challenges.py'),run_name='__main__')
runpy.run_path(str(root/'import_challenge_sign.py'),run_name='__main__')
