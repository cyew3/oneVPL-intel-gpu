"""Promotions from VPL repo to driver repo"""
import datetime
import logging
import os
import re
import subprocess
import sys
import time
from typing import Optional

from github import Github
from github.Commit import Commit
from github.CommitStatus import CommitStatus
from github.Issue import Issue

from config import SOURCE, TARGET, WORK_BRANCH, TEMPLATE, DATA_SOURCE, DATA_TARGETS
from utils.helper import NotFound, get_proxy

proxy = get_proxy()
os.environ['http_proxy'] = os.environ['https_proxy'] = proxy

g = Github(os.environ.get('STATUS_SECRET'))

logging.getLogger(__name__)
logging.basicConfig(
    stream=sys.stdout,
    format='%(asctime)s %(levelname)-8s %(message)s',
    level=logging.INFO,
    datefmt='%Y-%m-%d %H:%M:%S')


def execute(command, cwd):
    """Execute command"""
    return subprocess.run(command, cwd=cwd, shell=False,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          encoding='utf-8', errors='backslashreplace')


class ComponentUpdater:
    """Main promotion class"""

    def _is_open_pr_exists(self):
        """Check PR existence"""
        opened_pulls = g.search_issues(f'is:pr is:open head:{WORK_BRANCH}')

        if opened_pulls.totalCount == 0:
            logging.info('Open PRs not found')

        else:
            logging.info('These PRs should be merged before new promotion starts:')

            for pull in opened_pulls:
                logging.info('\t%s', pull.url)

            if self._is_restart_ci_build_needed(opened_pulls[0]):
                if self._send_build_comment(opened_pulls[0]):
                    logging.info('Sent /build comment')

        return opened_pulls.totalCount

    @staticmethod
    def _get_verify_status(commit: Commit) -> Optional[CommitStatus]:
        """Get pull request commit status"""
        statuses = commit.get_statuses()
        logging.debug('Loaded %s statuses', statuses.totalCount)
        for status in statuses:
            if status.description == 'Quickbuild verify build' and status.context == 'verify':
                logging.info('Status for %s check is %s', status.context, status.state)
                return status

            elif status.description == 'Quickbuild ci build' and status.context == 'ci':
                logging.info('Status for %s check is %s', status.context, status.state)
                return status

            else:
                logging.debug('Status for %s check is %s', status.context, status.state)
        return None

    def _is_restart_ci_build_needed(self, issue: Issue) -> bool:
        build_number = re.search(rf'{re.escape(TEMPLATE)}(\d+)', issue.title).group(1)
        logging.info('Check build status for %s promotion', build_number)

        # Get the last build comment time
        latest_build_comment_time = None
        comments = sorted(issue.get_comments(), key=lambda c: c.updated_at, reverse=True)
        for comment in comments:
            if comment.body == '/build':
                latest_build_comment_time = comment.updated_at
                break

        verify_check_status = None
        for commit in issue.as_pull_request().get_commits():
            verify_check_status = self._get_verify_status(commit)
            if verify_check_status is not None:
                break

        if verify_check_status is None and \
                (latest_build_comment_time is None or (latest_build_comment_time + datetime.timedelta(minutes=20)) < datetime.datetime.utcnow()):
            return True

        if verify_check_status.state == 'failure':
            check_status_time = verify_check_status.updated_at
            if latest_build_comment_time is None or \
                    (latest_build_comment_time + datetime.timedelta(minutes=20)) < check_status_time:
                return True
        else:
            logging.info('Waiting for rebuild')

        return False

    @staticmethod
    def _send_build_comment(issue: Issue) -> bool:
        """Add /build comment to the PR to rebuild"""
        return issue.create_comment('/build') is not None

    def _get_version_for_update(self) -> bool:
        try:
            current_promoted_version = DATA_TARGETS[0].get()
        except KeyError:
            logging.error('%s key not found', DATA_TARGETS[0].xpath)
            return False

        # Extract build number (\d+) and increase it by 1
        try:
            self._next_build_number = int(re.search(DATA_SOURCE.pattern, current_promoted_version).group(2)) + 2
        except AttributeError:
            logging.error('Can\'t find build number by pattern %s in %s', DATA_SOURCE.pattern, current_promoted_version)
            return False

        while 1:
            self._next_version = re.sub(DATA_SOURCE.pattern, lambda c: f"{c.group(1)}{self._next_build_number}",
                                        current_promoted_version)
            try:
                next_rev = SOURCE.repo.rev_parse(self._next_version)
            except Exception:
                logging.error('Tag %s doesn\'t exist', self._next_version)
                return False

            logging.info('Checkouting %s', self._next_version)
            SOURCE.repo.git.checkout(self._next_version)

            commit_message = SOURCE.repo.head.commit.message.splitlines()
            auto_generated_commit_templates = [r'^Updating component (\S+) to revision (\S+)',
                                               r'^Updating components based on subscription rules \(#\d+\)']

            for commit_template in auto_generated_commit_templates:
                if re.match(commit_template, commit_message[0]):
                    logging.info('%s is auto-generated commit. Try to get next commit', self._next_version)
                    self._next_build_number += 1
                    break
            else:
                github_source = g.get_repo(f'{SOURCE.organization}/{SOURCE.name}')
                commit = github_source.get_commit(sha=str(next_rev))
                source_ci_status = self._get_verify_status(commit)

                if source_ci_status.state != 'success':
                    logging.info('CI for %s not passed. Try to get next commit', self._next_version)
                    self._next_build_number += 1
                else:
                    self._author = f'{SOURCE.repo.head.commit.author.name} <{SOURCE.repo.head.commit.author.email}>'
                    match = re.match(r'(.*)\s\(#(\d+)\)$', commit_message[0])
                    if match:
                        commit_message[0] = f'{TEMPLATE}{self._next_build_number} {match.group(1)}'
                        commit_message.insert(1, '')
                        commit_message.insert(2, f'From https://github.com/{SOURCE.organization}/{SOURCE.name}/pull/{match.group(2)}')

                    else:
                        logging.error('Cannot parse PR number, link to PR was not updated')
                    self._commit_message = '\n'.join(commit_message)
                    return True

    @staticmethod
    def _update_files():
        logging.info('Updating files')

        try:
            value = DATA_SOURCE.get()
        except NotFound:
            return False

        for target in DATA_TARGETS:
            if not target.set(value):
                return False

        return True

    def _git_commit(self):
        logging.info('Pushing changes to remote')
        try:
            target_branch = f'{WORK_BRANCH}/{self._next_build_number}'
            push_change_commands = [
                ['git', 'checkout', '-b', target_branch],
                ['git', 'add', '-A'],
                ['git', 'commit', f'--author="{self._author}"', '-m', self._commit_message, '--no-verify'],
                ['git', 'push', '-u', 'origin', target_branch, '-f']
            ]

            logging.info('Create commit in repo %s', TARGET.repo)
            for command in push_change_commands:
                logging.info('Command: %s', command)
                commit_process = execute(command, str(TARGET.path))
                if commit_process.returncode:
                    match = re.match('fatal: A branch named \'(.*)\' already exists.', commit_process.stdout)
                    if match:
                        logging.warning('Branch %s already exists, trying to recover', match.group(1))
                        logging.info('Deleting branch %s', match.group(1))
                        process = execute(['git', 'branch', '-D', match.group(1)], str(TARGET.path))
                        if process.returncode:
                            logging.error(process.stdout)
                            return False
                        else:
                            logging.info('Success!')
                            commit_process = execute(command, str(TARGET.path))
                            if commit_process.returncode:
                                logging.error(commit_process.stdout)
                                return False
                    else:
                        logging.error(commit_process.stdout)
                        return False

                logging.info('Output: %s', commit_process.stdout)

            # Creating pull request
            title = execute(['git', 'show', '--pretty=format:%s', '-s', target_branch, '--'], str(TARGET.path)).stdout
            body = execute(['git', 'show', '--pretty=format:%b', '-s', target_branch, '--'], str(TARGET.path)).stdout
            github_target = g.get_repo(f'{TARGET.organization}/{TARGET.name}')
            pull_request = github_target.create_pull(title=title,
                                                     body=body,
                                                     head=target_branch,
                                                     base=TARGET.branch)
            pull_request.add_to_labels('automerge')
            logging.info('Pull request created: %s', pull_request.url)

        except Exception as exc:
            logging.exception('Pushing was failed: %s', exc)
            return False

        return True

    def update(self):
        """Make promotions"""
        exit_code = self._is_open_pr_exists()
        if exit_code is False or exit_code > 0:
            return True

        actions = [
            SOURCE.prepare,
            TARGET.prepare,
            self._get_version_for_update,
            self._update_files,
            self._git_commit
        ]

        for action in actions:
            if not action():
                return False

        return True


def main():
    """Main function"""
    updater = ComponentUpdater()

    while True:
        try:
            updater.update()
            time.sleep(300)
            print(f"\n{'*' * 40}\n")
        except Exception as exc:
            logging.error('Update failed with error %s', exc)
            sys.exit(1)


if __name__ == '__main__':
    main()
