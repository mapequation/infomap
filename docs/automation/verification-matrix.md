# Verification Matrix

Default rule: run the smallest sufficient verification for the changed surface. Do not default to the broadest available build or test sequence.

| Changed surface | Minimum verification | Notes |
| --- | --- | --- |
| `README.rst`, `docs/automation/`, other docs-only text | No code verification required | If the change affects generated docs behavior or committed Sphinx output freshness, run `make test-docs` instead of editing generated output blindly. |
| `src/` C++ code | `make build-native` | Use this as the minimum binary build check. Add narrower reproduction checks if the issue provides them. |
| `interfaces/python/`, Python packaging, SWIG-facing Python surface | `make build-python` and `make test-python` | `make test-python` exercises pytest, doctest, and the Python examples. |
| `interfaces/js/` TypeScript or npm package surface | `npm ci` and `make build-js` | Use for package or TypeScript-only JS changes. |
| JS worker or Emscripten boundary | `npm ci` and `make test-js` | Use when the change touches worker generation, bundled worker files, or the C++ to JS boundary. |
| `.github/workflows/` CI changes | Validate the smallest related local command, then keep PR as `draft` | Do not claim full CI validation from local-only checks. |
| Release or publish workflows | No automatic release verification | Keep automation out of release execution; triage only unless a maintainer explicitly approves. |

## Additional Rules

- If the issue affects more than one major surface, choose the higher verification burden or escalate the issue risk.
- If a required tool such as `swig`, `gh`, `node`, or `em++` is unavailable, mark the issue `blocked` instead of skipping verification silently.
- If a narrow reproduction command exists in the issue, run that in addition to the baseline command for the surface.
- For `medium` risk issues, passing verification does not upgrade the PR from `draft` to `ready`.
