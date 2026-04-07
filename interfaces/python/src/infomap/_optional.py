try:
    import pandas as _pandas
except ImportError:
    _pandas = None


def get_pandas():
    return _pandas
