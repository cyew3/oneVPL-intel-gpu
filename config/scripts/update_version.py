import pathlib
import argparse
import datetime
import re


DELIMITER = '.'
FILE_VERSION = "{date}{DELIMITER}{revision}"
MSDK_PRODUCT_VERSION = "{major_version}{DELIMITER}{minor_version}{DELIMITER}{msdk_major_api}{DELIMITER}{msdk_minor_api}"
VPL_PRODUCT_VERSION = "{major_version}{DELIMITER}{minor_version}{DELIMITER}{vpl_major_api}{DELIMITER}{vpl_minor_api}"
HIDDEN_FILE_VERSION = "{date}{DELIMITER}{build_number}"
HIDDEN_MSDK_PRODUCT_VERSION = "{major_version}{DELIMITER}{minor_version}{DELIMITER}{msdk_major_api}{DELIMITER}{msdk_minor_api}"
HIDDEN_VPL_PRODUCT_VERSION = "{major_version}{DELIMITER}{minor_version}{DELIMITER}{vpl_major_api}{DELIMITER}{vpl_minor_api}"

MSDK_PATTERN = {r'"FileVersion", "(.*)"': HIDDEN_FILE_VERSION,
                r'"ProductVersion", "(.*)"': HIDDEN_MSDK_PRODUCT_VERSION,
                r'FILEVERSION (.*)': FILE_VERSION,
                r'PRODUCTVERSION (.*)': MSDK_PRODUCT_VERSION}
VPL_PATTERN = {r'"FileVersion", "(.*)"': HIDDEN_FILE_VERSION,
               r'"ProductVersion", "(.*)"': HIDDEN_VPL_PRODUCT_VERSION,
               r'FILEVERSION (.*)': FILE_VERSION,
               r'PRODUCTVERSION (.*)': VPL_PRODUCT_VERSION}
MFT_PATTERN = {r'MFX_MF_FILE_VERSION +(.*)': FILE_VERSION,
               r'MFX_MF_PRODUCT_VERSION +(.*)': MSDK_PRODUCT_VERSION,
               r'MFX_MF_FILE_VERSION_STR +"(.*)"': HIDDEN_FILE_VERSION,
               r'MFX_MF_PRODUCT_VERSION_STR +(.*)': HIDDEN_MSDK_PRODUCT_VERSION}


FILES_TO_UPDATE = {
    r'mdp_msdk-lib\_studio\mfx_lib\libmfx-gen.rc': VPL_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\libmfxsw.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\libmfxhw.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxsw_plugin_hevce.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxsw_plugin_hevcd.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\plugin\mfxplugin_hw.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\camera_pipe\camera_pipe.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxhw_plugin_hevce.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\mfx_lib\plugin\libmfxhw_plugin_h264la.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\h263d\mfx_h263d_plugin.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\_studio\h263e\mfx_h263e_plugin.rc': MSDK_PATTERN,
    r'mdp_msdk-lib\api\mfx_loader_dll\mfx_loader_dll_hw.rc': MSDK_PATTERN,
    r'mdp_msdk-mfts\samples\sample_plugins\mfoundation\mf_utils\include\mf_version.h': MFT_PATTERN,
    r'mdp_msdk-lib\api\intel_api_uwp\src\intel_gfx_api.rc': MSDK_PATTERN
}


def prepare_data_for_patterns(args):
    date = datetime.date.today().strftime(f"%y{DELIMITER}%m{DELIMITER}%d")
    major_version = "21"
    minor_version = "0"

    msdk_api_source_file_path = r"mdp_msdk-lib\api\include\mfxdefs.h"
    msdk_major_api = get_version_from_file(args.path / msdk_api_source_file_path, r"MFX_VERSION_MAJOR")
    msdk_minor_api = get_version_from_file(args.path / msdk_api_source_file_path, r"MFX_VERSION_MINOR")

    vpl_api_source_file_path = r"mdp_msdk-lib\api\vpl\mfxdefs.h"
    vpl_major_api = get_version_from_file(args.path / vpl_api_source_file_path, r"MFX_VERSION_MAJOR")
    vpl_minor_api = get_version_from_file(args.path / vpl_api_source_file_path, r"MFX_VERSION_MINOR")
    # Get last part of QB build number; Example: 1.0.1234 -> 1234
    build_number = args.build_number.split('.')[-1]
    # Get first 8 symbols from revision (SHA-1)
    revision = args.revision[:8]
    return locals()


def update_file(path, patterns_to_update, data_for_patterns):
    print(f"Updating {str(path)}")
    with open(path, 'rt') as file:
        content = file.readlines()
    is_lines_changed = False
    new_content = []
    for line in content:
        for pattern, new_value_pattern in patterns_to_update.items():
            match = re.search(pattern, line)
            if match:
                delimiter_in_file = re.search("[.,]", match.group(1)).group(0)
                new_value = new_value_pattern.format(DELIMITER=DELIMITER, **data_for_patterns).replace(DELIMITER, delimiter_in_file)
                new_line = line.replace(match.group(1), new_value)
                if new_line != line:
                    print(f"\tReplaced value for {pattern} field from {match.group(1)} to {new_value}")
                    is_lines_changed = True
                    line = new_line
                    break
                else:
                    print(f"\tValue for {pattern} did not changed to {new_value} in line:\n{new_line}")
                    return 1
        new_content.append(line)
    if is_lines_changed:
        print(f"Write changes to file")
        with open(path, 'w+') as file:
            file.writelines(new_content)
        return 0
    else:
        print(f'File was not changed, please check patterns in this script with target file format')
        return 1


def get_version_from_file(path, pattern, suffix=r"[ =](.+?)\n"):
    with open(path, 'r') as file:
        content = file.read()
        result = re.search(pattern + suffix, content)
    if result is None:
        print(f'Pattern {pattern + suffix} not found in {path}')
    else:
        result = result.group(1)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--path", type=lambda p: pathlib.Path(p),
                        help="Path to folder with product repositories")
    parser.add_argument("-r", "--revision", default='00000000',
                        help="Revision (SHA-1) of the commit")
    parser.add_argument("-b", "--build-number", default='0.0.0',
                        help="Quick Build build number; Expected format: 0.0.0")
    args = parser.parse_args()
    data_for_patterns = prepare_data_for_patterns(args)
    exit_code = 0
    for file, patterns_to_update in FILES_TO_UPDATE.items():
        exit_code += update_file(args.path / file, patterns_to_update, data_for_patterns)
    exit(exit_code)


if __name__ == '__main__':
    main()
