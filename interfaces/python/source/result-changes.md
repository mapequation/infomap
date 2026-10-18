# Result changes

Some fixes change what Infomap reports on purpose. Use this list to tell an
intended change from a regression when you compare two versions, for example
to rerun a published analysis or to check test outputs.

The same rows are in
[`RESULT_CHANGES.csv`](https://github.com/mapequation/infomap/blob/master/RESULT_CHANGES.csv)
at the root of the repository, for scripts to read. The
[CHANGELOG](https://github.com/mapequation/infomap/blob/master/CHANGELOG.md)
lists every change. This page lists only the ones that change results.

## Columns

- **version**: the first release with the change, or `unreleased`.
- **flags**: the command-line flags, by long name, that a run must use for the
  row to apply. Python and R take the same options with underscores. An empty
  field means the row does not depend on a flag: the description says which
  input or API call it applies to.
- **changes**: one or more of these words, separated by spaces:
  - `codelength`: the reported codelength.
  - `partition`: the modules found.
  - `module-ids`: module numbering or node order, with the same partition.
  - `levels`: the number of levels in the tree.
  - `output`: other output, such as file names, header lines or JSON fields.
  - `warning`: a new warning.
  - `error`: the run is refused where it gave a result before.
  - `runs`: the run gives a result where it was refused or crashed before.
- **issue** and **pr**: GitHub issue and pull request numbers, separated by
  spaces.

## Changes

```{csv-table}
:file: ../../../RESULT_CHANGES.csv
:header-rows: 1
:widths: 8 18 14 48 6 6
```
