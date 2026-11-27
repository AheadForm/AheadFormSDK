"""Prepare the SDK-private Python environment; never install into system Python."""
import os
from pathlib import Path
import subprocess
import sys

import dds


def host_environment():
    # pip/venv run on the build host, even when the SDK targets another CPU.
    environment = os.environ.copy()
    for key in dds.KEYS + ("CPP", "LDSHARED", "BLDSHARED", "PYTHONHOME", "PYTHONPATH", "_PYTHON_HOST_PLATFORM",
                           "_PYTHON_SYSCONFIGDATA_NAME", "PIP_TARGET", "PIP_PREFIX", "PIP_USER"):
        environment.pop(key, None)
    return environment


def probe(python, code, environment):
    try:
        result = dds.run([python, "-I", "-c", code], env=environment,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return True, result.stdout.strip()
    except subprocess.CalledProcessError as error:
        return False, (error.stderr or error.stdout or "").strip()
    except OSError as error:
        return False, str(error)


def prepare(installer, args, config):
    if args.skip_python:
        return None
    if args.python_wheelhouse and not Path(args.python_wheelhouse).is_dir():
        raise RuntimeError("--python-wheelhouse must name an existing directory: " + args.python_wheelhouse)
    directory = dds.ROOT / "python_demo"
    venv = directory / ".venv"
    python = venv / "bin/python"
    environment = host_environment()
    print("Preparing private Python environment: " + str(venv), flush=True)

    # Retry an interrupted venv creation in place; never delete a customer's environment.
    pip_check = "import pip, sys; assert sys.prefix != sys.base_prefix"
    if not probe(python, pip_check, environment)[0]:
        ready, reason = probe(sys.executable, "import venv, ensurepip", environment)
        if not ready:
            installer.install(["python"], "Python venv/pip support is missing:\n" + reason)
            ready, reason = probe(sys.executable, "import venv, ensurepip", environment)
            if not ready:
                raise RuntimeError("Prepare venv/pip for " + sys.executable + ":\n" + reason)
        dds.run([sys.executable, "-m", "venv", str(venv)], env=environment)
    ready, reason = probe(python, pip_check, environment)
    if not ready:
        raise RuntimeError("The private Python environment is incomplete; rerun deployment:\n" + reason)

    if not config["cross"]:
        headers = ("from pathlib import Path; import sysconfig; "
                   "p = Path(sysconfig.get_path('include')) / 'Python.h'; "
                   "assert p.is_file(), 'Missing Python development header: ' + str(p)")
        ready, reason = probe(python, headers, environment)
        if not ready:
            installer.install(["python"], reason)
            ready, reason = probe(python, headers, environment)
            if not ready:
                raise RuntimeError("Install development headers matching this Python interpreter:\n" + reason)

    # Reuse the binding generator's version check instead of maintaining another version list.
    requirements_check = (
        "import sys; sys.path.insert(0, " + repr(str(directory)) + "); "
        "from build_bindings import cffi_version; cffi_version(); "
        "from cffi import FFI; FFI().cdef('int aheadform_dependency_probe(void);')")
    if not probe(python, requirements_check, environment)[0]:
        pip = [python, "-m", "pip", "--require-virtualenv", "install", "--disable-pip-version-check",
               "--prefix", str(venv), "--no-user", "-r", directory / "requirements.txt"]
        if args.python_wheelhouse:
            pip += ["--no-index", "--find-links", str(Path(args.python_wheelhouse).resolve())]
        try:
            dds.run(pip, env=environment)
        except subprocess.CalledProcessError:
            # On platforms without a CFFI wheel, pip compiles CFFI and needs host libffi headers.
            ffi = subprocess.run(["pkg-config", "--exists", "libffi"], env=environment,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if ffi.returncode:
                installer.install(["python"], "CFFI installation failed and host libffi development files are missing.")
                dds.run(pip, env=environment)
            else:
                raise RuntimeError("Cannot install Python requirements. Check the pip output, network or "
                                   "--python-wheelhouse; rerun deployment after preparing dependencies.")
    ready, reason = probe(python, requirements_check, environment)
    if not ready:
        raise RuntimeError("Python requirements validation failed:\n" + reason)
    print("Python requirements are ready; using " + str(python), flush=True)
    return python
