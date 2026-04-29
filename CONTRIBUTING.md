# Contributing

Thanks for helping improve Infomap. The project is maintainer-led and accepts a
small number of external pull requests. This is intentional: Infomap's runtime
behavior is regression-sensitive, and changes can be difficult to validate
without deep project context.

Issues, discussions, bug reports, reproducible examples, documentation fixes,
and small maintenance suggestions are welcome. For code changes, please open an
issue or discussion first so maintainers can confirm the scope and validation
path before review work starts.

Pull requests are most likely to be reviewed when they are:

- discussed with maintainers before implementation
- small, focused, and easy to verify
- limited to documentation, tests, packaging, or clearly bounded fixes
- explicit about what was verified and what was not

Runtime, algorithmic, numerical, determinism, memory, release, and broad
cross-surface changes are usually handled directly by maintainers. If you are
unsure whether a change fits, start with an issue or discussion instead of a
pull request.

Before opening an agreed pull request, read:

- `BUILD.md` for local build and verification commands
- `ARCHITECTURE.md` for source-of-truth and ownership rules
- `AGENTS.md` for repo-local maintenance guidance and the verification matrix
- `RELEASING.md` before touching release or publishing behavior

Branch from `master`; do not push directly to `master`. Use
[Conventional Commits](https://www.conventionalcommits.org/) for commit
messages. Run the smallest sufficient verification from `AGENTS.md` before
requesting review.
