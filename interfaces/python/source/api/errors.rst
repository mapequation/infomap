Errors
======

.. currentmodule:: infomap

Everything the engine reports as a failure — an unreadable input file, an
invalid ``args`` string, an option conflict, an unwritable output path — is
raised as :class:`InfomapError` or one of its subclasses, with the engine's
message preserved. Catch the base class to handle any Infomap failure, or a
subclass for targeted handling::

    import infomap

    try:
        result = infomap.run("network.net")
    except infomap.NetworkParseError as error:
        print(f"Bad input file: {error}")
    except infomap.InfomapError as error:
        print(f"Infomap failed: {error}")

Compatibility
-------------

Through 2.x, :class:`InfomapError` inherits :class:`RuntimeError` — the type
engine errors were raised as before the taxonomy existed — so existing
``except RuntimeError`` code keeps working. :class:`NotRunError` additionally
keeps :class:`ValueError` in its MRO, the type the results-before-run guard
raised previously. Plan for these legacy bases to detach in 3.0: catch
:class:`InfomapError` (or a subclass), not the legacy bases.

Errors that are not Infomap failures keep their standard Python types:
invalid argument *values* passed to the Python API raise :class:`ValueError`
or :class:`TypeError` as usual.

The taxonomy
------------

.. autoexception:: InfomapError

.. autoexception:: NetworkParseError

.. autoexception:: NotRunError

.. autoexception:: StaleResultError
