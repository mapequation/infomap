def is_numpy_input(value):
    return value.__class__.__module__.startswith("numpy")


def as_contiguous_numeric_array(np, array, name):
    if array.dtype.kind not in "uif":
        raise ValueError(f"Numpy {name} arrays must have a numeric dtype.")

    if array.dtype.kind == "f" and array.dtype.itemsize not in (4, 8):
        array = array.astype(np.float64, copy=False)
    elif array.dtype.kind in "ui" and array.dtype.itemsize not in (1, 2, 4, 8):
        array = array.astype(np.float64, copy=False)

    return np.ascontiguousarray(array)


def normalize_numpy_links(
    links,
    *,
    name,
    valid_columns,
    column_description,
    require_32_or_64_bit=False,
):
    import numpy as np

    links_array = np.asarray(links)
    columns = " or ".join(str(column) for column in valid_columns)
    if links_array.ndim != 2:
        raise ValueError(f"Numpy {name} arrays must be 2D with {columns} columns.")
    if links_array.shape[1] not in valid_columns:
        raise ValueError(
            f"Numpy {name} arrays must have {columns} columns: {column_description}."
        )

    if require_32_or_64_bit:
        if links_array.dtype.kind not in "uif":
            raise ValueError(f"Numpy {name} arrays must have a numeric dtype.")
        if links_array.dtype.itemsize not in (4, 8):
            raise ValueError(
                f"Numpy {name} arrays must use 32-bit or 64-bit numeric values."
            )
        return np.ascontiguousarray(links_array)

    return as_contiguous_numeric_array(np, links_array, name)


def split_optional_weight_rows(
    rows,
    *,
    row_name,
    valid_lengths,
    unpack,
    length_description,
):
    columns = [[] for _ in range(unpack.column_count)]
    weights = []

    for row in rows:
        try:
            row_length = len(row)
        except TypeError as err:
            raise ValueError(
                f"Each {row_name} must be a sequence of {length_description}."
            ) from err

        if row_length not in valid_lengths:
            raise ValueError(
                f"Each {row_name} must contain {length_description}: {unpack.description}."
            )

        values, weight = unpack(row, row_length)
        for column, value in zip(columns, values, strict=True):
            column.append(value)
        weights.append(weight)

    return (*columns, weights)


class RowUnpacker:
    def __init__(self, column_count, description, callback):
        self.column_count = column_count
        self.description = description
        self._callback = callback

    def __call__(self, row, row_length):
        return self._callback(row, row_length)


def first_order_unpacker():
    def unpack(row, row_length):
        if row_length == 2:
            source_id, target_id = row
            return (source_id, target_id), 1.0
        source_id, target_id, weight = row
        return (source_id, target_id), weight

    return RowUnpacker(2, "(source_id, target_id, [weight])", unpack)


def flat_multilayer_unpacker(value_names):
    def unpack(row, row_length):
        if row_length == len(value_names):
            return tuple(row), 1.0
        *values, weight = row
        return tuple(values), weight

    return RowUnpacker(
        len(value_names),
        f"({', '.join([*value_names, '[weight]'])})",
        unpack,
    )


def paired_multilayer_unpacker():
    def unpack(row, row_length):
        if row_length == 2:
            source_node, target_node = row
            weight = 1.0
        else:
            source_node, target_node, weight = row

        try:
            source_layer_id, source_node_id = source_node
            target_layer_id, target_node_id = target_node
        except (TypeError, ValueError) as err:
            raise ValueError(
                "Each multilayer node must contain 2 values: (layer_id, node_id)."
            ) from err

        return (
            source_layer_id,
            source_node_id,
            target_layer_id,
            target_node_id,
        ), weight

    return RowUnpacker(4, "(source_node, target_node, [weight])", unpack)


def add_bulk_links(
    links,
    *,
    numpy_method,
    list_method,
    name,
    valid_columns,
    column_description,
    valid_lengths,
    unpack,
    length_description,
    require_32_or_64_bit=False,
):
    """Route a links input to the right C++ bulk constructor.

    NumPy input goes to ``numpy_method`` (an ``addXFromNumpy2D`` SWIG bound
    method); any other iterable is normalized once and handed as parallel
    sequences to ``list_method`` (an ``addX`` SWIG bound method). The hot path
    stays in C++; no per-element Python loop is introduced. Shared by the
    single-layer and multilayer bulk builders.
    """
    if is_numpy_input(links):
        links_array = normalize_numpy_links(
            links,
            name=name,
            valid_columns=valid_columns,
            column_description=column_description,
            require_32_or_64_bit=require_32_or_64_bit,
        )
        numpy_method(
            links_array,
            links_array.shape[0],
            links_array.shape[1],
            links_array.dtype.kind,
            links_array.dtype.itemsize,
        )
        return

    sequences = split_optional_weight_rows(
        links,
        row_name=name,
        valid_lengths=valid_lengths,
        unpack=unpack,
        length_description=length_description,
    )
    list_method(*sequences)
