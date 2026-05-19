import numbers


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
        for column, value in zip(columns, values):
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


def scalar_numeric(value, *, scalar_message, numeric_message):
    if isinstance(value, bool):
        raise ValueError(numeric_message)
    if isinstance(value, (str, bytes)):
        raise ValueError(numeric_message)
    try:
        len(value)
    except TypeError:
        pass
    else:
        raise ValueError(scalar_message)
    if not isinstance(value, numbers.Real):
        raise ValueError(numeric_message)
    return value


def first_order_unpacker():
    def unpack(row, row_length):
        if row_length == 2:
            source_id, target_id = row
            weight = 1.0
        else:
            source_id, target_id, weight = row
        return (
            scalar_numeric(
                source_id,
                scalar_message="Each link source value must be scalar.",
                numeric_message="Link source and target values must be numeric.",
            ),
            scalar_numeric(
                target_id,
                scalar_message="Each link target value must be scalar.",
                numeric_message="Link source and target values must be numeric.",
            ),
        ), scalar_numeric(
            weight,
            scalar_message="Each link weight value must be scalar.",
            numeric_message="Link weight values must be numeric.",
        )

    return RowUnpacker(2, "(source_id, target_id, [weight])", unpack)


def flat_multilayer_unpacker(value_names):
    def unpack(row, row_length):
        if row_length == len(value_names):
            values = tuple(row)
            weight = 1.0
        else:
            *values, weight = row
        return tuple(
            scalar_numeric(
                value,
                scalar_message=f"Each multilayer {name} value must be scalar.",
                numeric_message="Multilayer id values must be numeric.",
            )
            for name, value in zip(value_names, values)
        ), scalar_numeric(
            weight,
            scalar_message="Each multilayer weight value must be scalar.",
            numeric_message="Multilayer weight values must be numeric.",
        )

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
            scalar_numeric(
                source_layer_id,
                scalar_message="Each multilayer source layer value must be scalar.",
                numeric_message="Multilayer layer and node values must be numeric.",
            ),
            scalar_numeric(
                source_node_id,
                scalar_message="Each multilayer source node value must be scalar.",
                numeric_message="Multilayer layer and node values must be numeric.",
            ),
            scalar_numeric(
                target_layer_id,
                scalar_message="Each multilayer target layer value must be scalar.",
                numeric_message="Multilayer layer and node values must be numeric.",
            ),
            scalar_numeric(
                target_node_id,
                scalar_message="Each multilayer target node value must be scalar.",
                numeric_message="Multilayer layer and node values must be numeric.",
            ),
        ), scalar_numeric(
            weight,
            scalar_message="Each multilayer link weight value must be scalar.",
            numeric_message="Multilayer link weight values must be numeric.",
        )

    return RowUnpacker(4, "(source_node, target_node, [weight])", unpack)
