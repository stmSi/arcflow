#!/usr/bin/env python3
"""Prepare verified native downloads and the browser site for GitHub Releases."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--check-tag", action="store_true")
    parser.add_argument("--artifacts", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent
    version = re.search(r"project\(arcflow\s+VERSION\s+([0-9.]+)",
                        (source / "CMakeLists.txt").read_text(), re.I).group(1)
    if args.tag != "v" + version:
        raise ValueError(f"Tag must match the project version: v{version}")
    if args.check_tag:
        return
    if args.artifacts is None or args.output is None:
        parser.error("--artifacts and --output are required")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    args.output.mkdir(parents=True, exist_ok=True)
    for platform in ("linux", "windows"):
        root = args.artifacts / f"arcflow-{platform}-release"
        report = json.loads((root / "arcflow-production-readiness.json").read_text())
        if (not report["automated_acceptance"] or report["version"] != version or
                report["revision"] != revision[:12]):
            raise ValueError(f"{platform} artifact is not an accepted build of this commit")
        packages = sorted(root.glob("Arcflow-*"))
        if len(packages) != 4:
            raise ValueError(f"Expected two {platform} packages and their checksums")
        for package in packages:
            if package.suffix != ".sha256":
                expected = Path(str(package) + ".sha256").read_text().split()[0]
                if hashlib.sha256(package.read_bytes()).hexdigest() != expected:
                    raise ValueError(f"Checksum mismatch: {package.name}")
            shutil.copy2(package, args.output / package.name)
    web = args.output / f"Arcflow-{version}-Web.zip"
    with tempfile.TemporaryDirectory() as directory:
        site = Path(directory)
        with tarfile.open(args.artifacts / "github-pages" / "artifact.tar") as archive:
            archive.extractall(site, filter="data")
        for name in ("index.html", "index.js", "index.wasm", "index.data",
                     "player.html", "player.js", "player.wasm", "player.data", "favicon.svg"):
            if not (site / name).is_file():
                raise ValueError(f"Browser artifact is missing {name}")
        pdf = args.output / f"Arcflow-{version}-Tutorials.pdf"
        shutil.copy2(site / "learn" / "Arcflow-Tutorials.pdf", pdf)
        Path(str(pdf) + ".sha256").write_text(
            hashlib.sha256(pdf.read_bytes()).hexdigest() + "  " + pdf.name + "\n")
        with zipfile.ZipFile(web, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for file in sorted(site.rglob("*")):
                if file.is_file():
                    archive.write(file, file.relative_to(site))
    Path(str(web) + ".sha256").write_text(
        hashlib.sha256(web.read_bytes()).hexdigest() + "  " + web.name + "\n")
    (args.output / "notes.md").write_text(
        "Linux and Windows editor, player, compiler, and runtime; browser editor and player.\n\n"
        "Downloads include portable archives, a Windows installer, and a browser site ZIP, "
        "a tutorial PDF, and SHA-256 checksums.\n\n"
        "[Open in browser](https://stmsi.github.io/arcflow/)\n")
    print(f"Prepared {len(list(args.output.glob('Arcflow-*')))} release files for {args.tag}")


if __name__ == "__main__":
    main()
