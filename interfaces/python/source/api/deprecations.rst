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
  3.0. Pass them through :class:`Options` instead::

      import infomap

      options = infomap.Options(regularized=True, flow_tolerance=1e-12)
      result = infomap.run("network.net", options=options)

  The same :class:`Options` object works with :meth:`Network.run`. Every option
  remains available — only its *entry point* moves from a per-call keyword to
  :class:`Options`.

The advanced-tier deprecations are documentation-only: no runtime warning fires,
so existing code keeps running unchanged until 3.0.

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
