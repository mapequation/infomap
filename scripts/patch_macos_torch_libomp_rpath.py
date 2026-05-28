#!/usr/bin/env python3
import base64
import csv
import hashlib
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
from shutil import which


TORCH_LIBOMP_RPATH = "@loader_path/../torch/lib"
VENDORED_LIBOMP_RPATH = "@loader_path/.dylibs"
LIBOMP_LOAD_NAME = "@rpath/libomp.dylib"
REQUIRED_TOOLS = ("otool", "install_name_tool", "codesign")


def run(*args):
    subprocess.run(args, check=True)


def output(*args):
    return subprocess.check_output(args, text=True)


def current_libomp_dependency(binary):
    for line in output("otool", "-L", str(binary)).splitlines()[1:]:
        dependency = line.strip().split(" ", 1)[0]
        if dependency.endswith("/libomp.dylib") or dependency == LIBOMP_LOAD_NAME:
            return dependency
    return ""


def current_rpaths(binary):
    rpaths = []
    lines = output("otool", "-l", str(binary)).splitlines()
    for index, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            for rpath_line in lines[index + 1 : index + 4]:
                stripped = rpath_line.strip()
                if stripped.startswith("path "):
                    rpaths.append(stripped.split(" ", 2)[1])
                    break
    return rpaths


def add_rpath(binary, rpath):
    if rpath not in current_rpaths(binary):
        run("install_name_tool", "-add_rpath", rpath, str(binary))


def patch_extension(binary):
    dependency = current_libomp_dependency(binary)
    if not dependency:
        raise RuntimeError(f"{binary} does not link to libomp.dylib")

    if dependency != LIBOMP_LOAD_NAME:
        run("install_name_tool", "-change", dependency, LIBOMP_LOAD_NAME, str(binary))

    add_rpath(binary, TORCH_LIBOMP_RPATH)
    add_rpath(binary, VENDORED_LIBOMP_RPATH)
    run("codesign", "--force", "--sign", "-", str(binary))


def digest(path):
    data = path.read_bytes()
    value = base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=")
    return f"sha256={value.decode()}", str(len(data))


def rewrite_record(root):
    record_paths = list(root.glob("*.dist-info/RECORD"))
    if len(record_paths) != 1:
        raise RuntimeError(f"expected one RECORD file, found {len(record_paths)}")

    record = record_paths[0]
    rows = []
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        relpath = path.relative_to(root).as_posix()
        if path == record:
            rows.append((relpath, "", ""))
        else:
            file_hash, size = digest(path)
            rows.append((relpath, file_hash, size))

    with record.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerows(rows)


def repack_wheel(root, wheel):
    tmp_wheel = wheel.with_suffix(".tmp.whl")
    if tmp_wheel.exists():
        tmp_wheel.unlink()

    with zipfile.ZipFile(tmp_wheel, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(p for p in root.rglob("*") if p.is_file()):
            archive.write(path, path.relative_to(root).as_posix())

    tmp_wheel.replace(wheel)


def patch_wheel(wheel):
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "wheel"
        root.mkdir()
        with zipfile.ZipFile(wheel) as archive:
            archive.extractall(root)

        extensions = list((root / "infomap").glob("_infomap*.so"))
        if len(extensions) != 1:
            raise RuntimeError(
                f"expected one infomap extension, found {len(extensions)}"
            )

        patch_extension(extensions[0])
        rewrite_record(root)
        repack_wheel(root, wheel)


def main(argv):
    if len(argv) < 2:
        raise SystemExit("usage: patch_macos_torch_libomp_rpath.py WHEEL [...]")

    if sys.platform != "darwin":
        raise SystemExit("patch_macos_torch_libomp_rpath.py only runs on macOS")

    missing_tools = [tool for tool in REQUIRED_TOOLS if which(tool) is None]
    if missing_tools:
        tools = ", ".join(missing_tools)
        raise SystemExit(f"missing required macOS tool(s): {tools}")

    for arg in argv[1:]:
        wheel = Path(arg)
        if not wheel.exists():
            raise FileNotFoundError(wheel)
        patch_wheel(wheel)
        print(f"Patched {wheel}")


if __name__ == "__main__":
    main(sys.argv)
