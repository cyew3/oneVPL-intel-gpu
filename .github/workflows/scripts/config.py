"""Promotion config"""
from utils.helper import Repo, RepositoryInterface, ManifestInterface, MediaVersionsCSVInterface, ManifestTailedInterface

FACELESS_ACCOUNT = 'Intel-MSDK'
SOURCE = Repo(repo='drivers.gpu.media.sdk.lib')
TARGET = Repo(repo='drivers.gpu.unified',
              branch='comp/media')
WORK_BRANCH = 'vpl/auto'
TEMPLATE = f'[RT][Embargo][VPL] {SOURCE.branch}-1.0.'

DATA_SOURCE = RepositoryInterface(SOURCE, 'tag', rf'(build/ci/mediasdk-ci-{SOURCE.branch}-1.0.)(\d+)')

DATA_TARGETS = (
    ManifestInterface(TARGET, 'config/manifests/mediasdk/linux.yml', 'components/vpl/revision'),
    ManifestTailedInterface(TARGET, 'config/manifests/installonly/windows.yml', 'components/VPL/asset_version'),
    # MediaVersionsCSVInterface(TARGET, 'config/testversions/mediaVersions.csv', 'test_media_lucas/vpl_tools_win'),
    # MediaVersionsCSVInterface(TARGET, 'config/testversions/mediaVersions.csv', 'test_media_lucas/vpl_tools_lin'),
    # MediaVersionsCSVInterface(TARGET, 'config/testversions/mediaVersions.csv', 'test_msdk/vpl_tools-windows'),
    # MediaVersionsCSVInterface(TARGET, 'config/testversions/mediaVersions.csv', 'test_msdk/vpl_tools-linux')
)
