from __future__ import annotations

import argparse
import code
import sys
from pathlib import Path

import infomap
from . import Infomap, MultilayerNode, Options
from ._summary import summary_data as _summary_data


def _format_value(value):
    if isinstance(value, float):
        return f"{value:.12g}"
    return str(value)


def _print_summary(data):
    for key, value in data.items():
        print(f"{key}: {_format_value(value)}")


def _make_banner(network_file=None):
    lines = [
        "Infomap shell",
        "Imported: infomap, Infomap, Options, MultilayerNode",
        "Created: im = Infomap()",
        "Helpers: summary(), options()",
    ]
    if network_file is not None:
        lines.append(f"Loaded: {network_file}")
    return "\n".join(lines)


def create_namespace(network_file=None):
    im = Infomap()
    if network_file is not None:
        im.read_file(str(network_file))

    namespace = {
        "infomap": infomap,
        "Infomap": Infomap,
        "Options": Options,
        "MultilayerNode": MultilayerNode,
        "im": im,
    }

    def summary(obj=None):
        target = namespace["im"] if obj is None else obj
        data = target.summary() if hasattr(target, "summary") else _summary_data(target)
        _print_summary(data)
        return data

    def options():
        help(Options)
        return Options

    namespace["summary"] = summary
    namespace["options"] = options
    return namespace


def launch_shell(namespace, banner, *, use_ipython=True):
    if use_ipython:
        try:
            from IPython import start_ipython  # pyright: ignore[reportMissingImports]  # optional dep, no stubs
        except ImportError:
            pass
        else:
            print(banner)
            start_ipython(argv=[], user_ns=namespace)
            return

    code.interact(banner=banner, local=namespace)


def _parser():
    parser = argparse.ArgumentParser(
        prog="infomap-shell",
        description="Start an interactive Python shell with Infomap preloaded.",
    )
    parser.add_argument(
        "--no-ipython",
        action="store_true",
        help="Use the standard Python REPL even when IPython is installed.",
    )
    parser.add_argument(
        "network_file",
        nargs="?",
        help="Network file to load into the default im object.",
    )
    return parser


def main(argv=None, *, launcher=launch_shell):
    args = _parser().parse_args(argv)
    network_file = None

    if args.network_file is not None:
        network_file = Path(args.network_file)
        if not network_file.is_file():
            print(f"Error: No such file: {network_file}", file=sys.stderr)
            return 1

    namespace = create_namespace(network_file)
    banner = _make_banner(network_file)
    launcher(namespace, banner, use_ipython=not args.no_ipython)
    return 0


if __name__ == "__main__":
    sys.exit(main())
