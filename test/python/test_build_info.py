import infomap


def test_build_info_reports_enabled_features_tuple():
    info = infomap.build_info()

    assert set(info) == {"enabled_features"}
    assert isinstance(info["enabled_features"], tuple)
