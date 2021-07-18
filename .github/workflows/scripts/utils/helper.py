"""
Additional functions to work with repos and automation
"""
import csv
import logging
import os
import re
import shutil
from pathlib import Path
from typing import Optional

import dpath.util
import git
from pypac import get_pac, PACSession
from ruamel.yaml import YAML

TEMP_DIR = Path(__file__).resolve().parent.parent / 'tmp'
TEMP_DIR.mkdir(parents=True, exist_ok=True)


def get_proxy():
    pac = get_pac(url='http://wpad.intel.com/wpad.dat')
    session = PACSession(pac)
    return next(iter(session.get('https://api.github.com').connection.proxy_manager))


class Repo:
    """Class to work with local repos"""

    def __init__(self, repo: str, branch: str = 'master', organization: str = 'intel-innersource'):
        self.organization = organization
        self.name = repo
        self.path = TEMP_DIR / self.name
        self.repo: Optional[git.Repo] = None
        self.branch = branch
        self.url = f'https://{os.environ.get("STATUS_SECRET")}@github.com/{self.organization}/{self.name}'

    def prepare(self) -> bool:
        """Clone and cleanup repo"""
        if self.path.exists():
            logging.info('Repo %s found at %s', self.name, self.path)
            self.repo = git.Repo(str(self.path))
            if self.cleanup():
                pass
                logging.info('Checkouting %s branch', self.branch)
                try:
                    self.repo.git.checkout(self.branch)
                except git.exc.GitCommandError:
                    shutil.rmtree(self.path)
                    return self.prepare()

                logging.info('Pulling %s branch', self.branch)
                self.repo.git.pull('origin', f'{self.branch}:{self.branch}')
        else:
            logging.info('Cloning %s repo', self.name)
            self.repo = git.Repo.clone_from(self.url, self.path, branch=self.branch)

        return True

    def cleanup(self) -> bool:
        """Clean repository"""
        try:
            logging.info('Resetting %s repo', self.name)
            self.repo.git.reset('--hard')
            logging.info('Cleaning %s repo', self.name)
            self.repo.git.clean('-xdf')
        except Exception as exc:
            logging.exception('Failed to clean %s repo: %s', self.repo, exc)
            return False

        return True


class PromotionInterface:
    """Generic interfaces class"""

    def __init__(self, repo, path, xpath):
        self.repo = repo
        self.path = path
        self.xpath = xpath

    def get(self):
        """Get value"""
        return None

    def set(self, value: str):
        """Set value"""
        return False


class RepositoryInterface(PromotionInterface):
    """Interface to work with Git repository"""

    def __init__(self, repo: Repo, path, xpath):
        super().__init__(repo, path, xpath)
        self.repo = repo
        self.type = path
        self.pattern = xpath

    def get(self):
        """Get tag or last revision"""
        if self.type not in ('tag', 'revision'):
            logging.error('Unknown repository type %s', self.type)
            raise UnknownType

        if self.type == 'tag':
            logging.info('Looking for tags...')
            tags = self.repo.repo.git.tag('--points-at', self.repo.repo.rev_parse('HEAD')).splitlines()
            for tag in tags:
                if re.match(self.pattern, tag):
                    return tag

            logging.error('Tag pattern %s not found in:', self.pattern)
            for tag in tags:
                logging.error('\t%s', tag)

            raise NotFound

        if self.type == 'revision':
            return self.repo.repo.rev_parse('HEAD')


class ManifestInterface(PromotionInterface):
    """Interface to work with manifests files"""

    def get(self):
        """Get value from manifest file by xpath"""
        yaml = YAML()
        with (Path(self.repo.repo.working_tree_dir) / self.path).open() as manifest:
            parts = yaml.load(manifest)
        return dpath.util.get(parts, self.xpath)

    def set(self, value: str):
        """Set value in the manifest file by xpath"""
        yaml = YAML()
        manifest_file = Path(self.repo.repo.working_tree_dir) / self.path
        with manifest_file.open() as manifest:
            content = yaml.load(manifest)
        old_value = dpath.util.get(content, self.xpath)

        dpath.util.set(content, self.xpath, value)

        with manifest_file.open('w') as manifest:
            yaml.dump(content, manifest)

        logging.info('Updated value %s with %s at %s in %s', old_value, value, self.xpath, self.path)

        return True


class ManifestTailedInterface(PromotionInterface):
    """Interface to work with manifests files"""

    def get(self):
        """Get value from manifest file by xpath"""
        yaml = YAML()
        with (Path(self.repo.repo.working_tree_dir) / self.path).open() as manifest:
            parts = yaml.load(manifest)
        return dpath.util.get(parts, self.xpath)

    def set(self, value: str):
        """Set value in the manifest file by xpath"""
        yaml = YAML()
        manifest_file = Path(self.repo.repo.working_tree_dir) / self.path
        with manifest_file.open() as manifest:
            content = yaml.load(manifest)
        old_value = dpath.util.get(content, self.xpath)
        new_value = value.split('/')[-1]

        dpath.util.set(content, self.xpath, new_value)
        with manifest_file.open('w') as manifest:
            yaml.dump(content, manifest)

        logging.info('Updated %s with %s at %s in %s', old_value, new_value, self.xpath, self.path)

        return True


class MediaVersionsCSVInterface(PromotionInterface):
    """Interface to work with MediaVersion.csv file"""

    def set(self, value: str):
        """Set value in the mediaVersions.csv file by xpath (plugin_name/asset_key)"""
        logging.info('Changing mediaVersions.csv file')

        plugin_name, asset_key = self.xpath.split('/')

        mediaversions_path = Path(self.repo.repo.working_tree_dir) / self.path

        try:
            new_value = value.split('/')[-1]
            with mediaversions_path.open() as csvfile:
                reader = csv.DictReader(csvfile, delimiter=',')
                fieldnames = reader.fieldnames
                data = list(row for row in reader)
                for row in data:
                    if row['plugin_name'] == plugin_name and row['asset_key'] == asset_key:
                        old_value = row['asset_version']

                        if old_value == new_value:
                            logging.info('Value %s for plugin %s with %s asset key not changed', new_value, plugin_name, asset_key)
                            return True

                        row['asset_version'] = new_value
                        break
                else:
                    logging.warning('Plugin %s with %s asset key not found', plugin_name, asset_key)
                    return False

            with mediaversions_path.open('w', newline='') as csvfile:
                writer = csv.DictWriter(csvfile, fieldnames)
                try:
                    writer.writeheader()
                except TypeError:
                    logging.error('File %s is empty', mediaversions_path)
                    return False

                writer.writerows(data)

            logging.info('Updated %s with %s in plugin %s with %s asset key in %s', old_value, new_value, plugin_name, asset_key, mediaversions_path)

        except FileNotFoundError:
            logging.error('File %s not found', mediaversions_path)
            return False

        except Exception as exc:
            logging.exception('Changing mediaVersions.csv file failed: %s', exc)
            return False

        return True


class PromotionException(Exception):
    """Generic promotion class exception"""


class UnknownType(PromotionException):
    """Unknown type exception"""


class NotFound(PromotionException):
    """Somethin not found exception"""
