import json


def create_package_meta(package_json, meta_file):
    with open(package_json, "r") as fp:
        pkg = json.load(fp)

    lines = (
        f'__name__ = "infomap"\n',
        f"__version__ = \"{pkg.get('version')}\"\n",
        f"__description__ = \"{pkg.get('description')}\"\n",
        f"__author__ = \"{pkg.get('author')['name']}\"\n",
        f"__email__ = \"{pkg.get('author')['email']}\"\n",
        f"__license__ = \"{pkg.get('license')}\"\n",
        f"__homepage__ = \"{pkg.get('homepage')}\"\n",
        f"__url__ = \"{pkg.get('python')['homepage']}\"\n",
        f"__repo__ = \"{pkg.get('repository')['url']}\"\n",
        f"__issues__ = \"{pkg.get('bugs')['url']}\"\n",
        f"__keywords__ = \"{' '.join([kw.replace(' ', '-') for kw in pkg.get('keywords')])}\"\n",
        f"__classifiers__ = {pkg.get('python')['classifiers']}\n",
    )

    with open(meta_file, "w") as fp:
        fp.writelines(lines)


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        raise RuntimeError("missing argument: outfile")

    create_package_meta("./package.json", sys.argv[1])
