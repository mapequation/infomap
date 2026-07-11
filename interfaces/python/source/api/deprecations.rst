Deprecation policy
==================

.. currentmodule:: infomap

Infomap's Python API follows a predictable deprecation contract so you can rely
on it across releases. Nothing listed here is removed before 3.0; until then
the old spelling keeps working, and the reference docs mark each deprecated name
with the release it was deprecated in.

Signature tiers
---------------

The keyword arguments on :class:`Infomap` and :meth:`Infomap.run` are split into
two tiers:

- **Common tier** — the handful of options most runs touch (``seed``,
  ``num_trials``, ``two_level``, ``directed``, ``markov_time``). These stay on
  the :class:`Infomap` signatures.
- **Advanced tier** — the long tail of tuning and I/O options. From 2.15 these
  are ``.. deprecated::`` on the :class:`Infomap` signatures and leave them in
  3.0. Most are *tuning* options that stay available — only their entry point
  moves — so pass them through :class:`Options` instead::

      import infomap

      options = infomap.Options(regularized=True, flow_tolerance=1e-12)
      result = infomap.run("network.net", options=options)

  The same :class:`Options` object works with :meth:`Network.run`. A few
  advanced options migrate elsewhere rather than to :class:`Options`, and each
  keyword's ``.. deprecated::`` note states its own path: the file-output
  options (``no_file_output``, ``tree``, ``clu``, ``out_name``, …) move to the
  ``Result.write_*`` / ``Network.write_*`` methods; ``silent`` and
  ``verbosity_level`` give way to logging (see
  :doc:`/working-with-infomap/running-and-options`); ``threads`` is superseded
  by ``num_threads``; and ``print_config_fingerprint`` is a CLI-only diagnostic
  with no library replacement.

Passing an advanced-tier keyword to :class:`Infomap` or :meth:`Infomap.run`
emits a :class:`PendingDeprecationWarning` (silent by default; surface it with
``python -W`` or a logging filter). Routing the option through :class:`Options`
is the warning-free path, so existing code keeps running unchanged until 3.0.

Redesigned result access
-------------------------

The stateful accessors on :class:`Infomap` (``get_modules``, ``codelength``,
``num_top_modules``, and friends) are ``.. deprecated:: 2.14`` in favour of the
immutable :class:`Result` that :func:`run` and :meth:`Infomap.run` return. Read
scalars as properties (``result.codelength``) and collections as methods
(``result.modules()``, ``result.nodes()``, ``result.tree()``). See
:doc:`/working-with-infomap/results-and-iteration`.

Compatibility aliases
---------------------

- ``include_self_links`` is a deprecated alias kept for backward compatibility.
  Self-links are included by default; pass ``no_self_links=True`` to exclude
  them. Passing ``include_self_links`` explicitly emits a
  :class:`DeprecationWarning`.
- ``pretty`` is a deprecated no-op — it is accepted for backward compatibility
  but has no effect. Passing it explicitly emits a :class:`DeprecationWarning`.

Error base classes
------------------

Through 2.x, :class:`InfomapError` inherits :class:`RuntimeError` and
:class:`NotRunError` also keeps :class:`ValueError` in its MRO, so pre-taxonomy
``except RuntimeError`` / ``except ValueError`` code keeps working. These legacy
bases detach in 3.0 — catch :class:`InfomapError` (or a subclass) instead. See
:doc:`errors`.
