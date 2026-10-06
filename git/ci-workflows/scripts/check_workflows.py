"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  06/10/2026 by Tsukini

File Name:
##  check_workflows.py

File Description:
##  Check GitHub workflows: YAML syntax (actionlint when installed) and the user's conventions
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from sys import argv, exit, stderr

# Import that can be checked
try:
    from pathlib import Path # Used to find the workflows
    from shutil import which # Used to find actionlint
    import subprocess # Used to run actionlint
    import yaml # Used to parse the workflows
    import re # Used for the conventions
except ImportError as e:
    stderr.write(f"Import Error ({__file__}): {e}\n")
    exit(255)

# Check if the program is call and not imported
if __name__ != "__main__":
    stderr.write(f"The {__file__} can only be executed and not imported!\n")
    exit(255)

##### Const #####
NAME = re.compile(r"^.+ - .+|^.+ \((CI|CD|CI/CD)\)$") # Name - Kind [(CI|CD)] / Name (CI|CD) (as in the user's workflows)
IMAGES = re.compile(r"^ghcr\.io/tsukini22/(ci|unit-tests|package|git)(:|$)")

##### Tools #####
def check(path: Path) -> list[str]:
    problems = []
    try:
        workflow = yaml.safe_load(path.read_text(encoding="utf-8"))
    except yaml.YAMLError as e:
        return [f"YAML error: {e}"]
    if not isinstance(workflow, dict):
        return ["not a workflow (no mapping at the root)"]
    if not NAME.match(str(workflow.get("name", ""))):
        problems.append(f"name '{workflow.get('name')}' is not 'Name - Kind (CI|CD)' / 'Name (CI|CD)'")
    for job_id, job in (workflow.get("jobs") or {}).items():
        runs_on = str(job.get("runs-on", ""))
        if "vars.RUNNER" not in runs_on:
            problems.append(f"job {job_id}: runs-on '{runs_on}' (convention: ${{{{ vars.RUNNER }}}})")
        image = (job.get("container") or {}).get("image", "") if isinstance(job.get("container"), dict) else str(job.get("container") or "")
        if image and not IMAGES.match(image) and "slim" not in image and "alpine" not in image:
            problems.append(f"job {job_id}: image '{image}' (ghcr.io/tsukini22/* first, else a slim / alpine image)")
        for step in job.get("steps") or []:
            run = step.get("run", "")
            commands = [line for line in run.strip().splitlines() if line.strip() and not line.rstrip().endswith("\\") and not line.strip().startswith("#")]
            if len(commands) > 1 and "set -e" not in run and "set -uo" not in run:
                problems.append(f"job {job_id}, step '{step.get('name', '?')}': multi-line run without set -euo pipefail")
            if "run" in step and "name" not in step and "\n" in run.strip():
                problems.append(f"job {job_id}: multi-line step without name")
    return problems

##### Program #####
paths = [Path(p) for p in argv[1:]] or sorted(Path(".github/workflows").glob("*.y*ml"))
if not paths:
    stderr.write("Error: no workflow (give the files or run from the repository root)\n")
    exit(1)
status = 0
if which("actionlint"):
    status = subprocess.run(["actionlint", *map(str, paths)]).returncode
for path in paths:
    problems = check(path)
    for problem in problems:
        print(f"{path}: {problem}")
    status = status or (1 if problems else 0)
print("ok" if status == 0 else "problems found")
exit(status)
