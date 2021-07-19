import os
import regex as re
import portion as P
import time
import logging
import shutil


def get_logger(logger_name, log_file, level=logging.INFO):
    l = logging.getLogger(logger_name)
    formatter = logging.Formatter('%(asctime)s : %(message)s', "%Y-%m-%d %H:%M:%S")
    fileHandler = logging.FileHandler(log_file, mode='w')
    fileHandler.setFormatter(formatter)

    l.setLevel(level)
    l.addHandler(fileHandler)

    return logging.getLogger(logger_name)


def get_del_ind_if(macros, code_content, f_path, define_list_flag):
    global del_index, overwrite_flag
    if define_list_flag:
        match_macro = r"\n[ \f\r\t\v]*((#\s*if\s*\!defined[\s\(]*)|(#\s*ifndef\s*))((" + r"[\s\)])|(".join(macros) + r"[\s\)]))"
    else:
        match_macro = r"\n[ \f\r\t\v]*((#\s*if\s*defined[\s\(]*)|(#\s*ifdef\s*))((" + r"[\s\)])|(".join(macros) + r"[\s\)]))"
    # \n\s* avoids #if in the comment
    # \s*\(* means there might be a ( or might not be
    # [\s\)] means the end of a macro
    s = re.compile(match_macro)
    marco_search_result = re.finditer(s, code_content)
    for search_result in marco_search_result:
        # Search given macros
        macro_start = search_result.regs[0][0]
        if macro_start in exclude_index:
            continue
        pattern_if_endif = re.compile(r'(\n[ \f\r\t\v]*#\s*if)|(\n[ \f\r\t\v]*#\s*endif.*)|(#\s*else.*)|(#\s*elif)')
        finditer_if_endif = re.finditer(pattern_if_endif, code_content, pos=macro_start)
        stack = 0
        else_detected = False
        for find_result in finditer_if_endif:
            # Search # and determine correspondence
            if find_result.group(1):
                stack += 1
            elif find_result.group(2):
                stack -= 1
                if stack == 0:
                    if else_detected:
                        del_index.append([find_result.span(0)[0]+1, find_result.span(0)[1]+1])  # delete endif
                    else:
                        del_index.append([macro_start+1, find_result.span(0)[1]+1])  # delete all
                    break
            elif find_result.group(3):
                if stack == 1:
                    del_index.append([macro_start+1, find_result.span(0)[1]+1])  # delete from if to else
                    else_detected = True
            else:
                if stack == 1:
                    overwrite_flag = False
                    line_num = code_content[0:find_result.span(0)[0]].count("\n") + 1
                    logger1.info("Line " + str(line_num) + " #elif detected in " + f_path)
                    break


def get_del_ind_ifn(macros, code_content, f_path, define_list_flag):
    global del_index, overwrite_flag
    if define_list_flag:
        match_macro = r"\n[ \f\r\t\v]*((#\s*if\s*defined[\s\(]*)|(#\s*ifdef\s*))((" + r"\b.*)|(".join(macros) + r"\b.*))"
    else:
        match_macro = r"\n[ \f\r\t\v]*((#\s*if\s*\!defined[\s\(]*)|(#\s*ifndef\s*))((" + r"\b.*)|(".join(macros) + r"\b.*))"
    s = re.compile(match_macro)
    marco_search_result = re.finditer(s, code_content)
    for search_result in marco_search_result:
        # Search given macros
        macro_start = search_result.regs[0][0]
        if macro_start in exclude_index:
            continue
        #if code_content[search_result.regs[0][1]-1] == "\n":
        #    del_index.append([search_result.regs[0][0], search_result.regs[0][1]-1])  # Delete if !defined
        #else:
        del_index.append([search_result.regs[0][0]+1, search_result.regs[0][1]+1])  # Delete if !defined

        pattern_if_endif = re.compile(r'(\n[ \f\r\t\v]*#\s*if)|(\n[ \f\r\t\v]*#\s*endif.*)|(\n[ \f\r\t\v]*#\s*else.*)|(#\s*elif)')
        finditer_if_endif = re.finditer(pattern_if_endif, code_content, pos=macro_start)
        # There might be overlap like #endif\n#if if match #endif.*?\n
        # However overlapped can not be set to true, otherwise \n\n#if will be matched more than once.

        stack = 0
        else_detected = False
        for find_result in finditer_if_endif:
            # Search # and determine correspondence
            if find_result.group(1):  # if
                stack += 1
            elif find_result.group(2):  # endif
                stack -= 1
                if stack == 0:
                    if else_detected:
                        del_index.append([else_start+1, find_result.span(0)[1]+1])  # Delete from else to endif
                    else:
                        del_index.append([find_result.span(0)[0]+1, find_result.span(0)[1]+1])  # Delete endif
                    break
            elif find_result.group(3):  # else
                if stack == 1:
                    else_start = find_result.span(0)[0]
                    else_detected = True
            else:
                if stack == 1:
                    overwrite_flag = False
                    line_num = code_content[0:find_result.span(0)[0]].count("\n") + 1
                    logger1.info("Line " + str(line_num) + " #elif detected in " + f_path)
                    del del_index[-1]
                    break


def detect_comb_logic(code_content, f_path):
    global overwrite_flag, exclude_index
    match_comb = re.finditer(
        r"\n\s*((#\s*if\s*defined)|(#\s*ifdef)|(#\s*if\s*\!defined)|(#\s*ifdef))(((.)|(\\\s*?\n))*?)((\|\|)|(\&\&))", code_content)
    
    for match_c in match_comb:
        overwrite_flag = False
        skip_flag=False
        for exception in exceptionlist_macros:
            match_f = re.findall(r''+exception+'',code_content, pos=match_c.span(0)[0], endpos=match_c.span(0)[1])
            if len(match_f):
                skip_flag=True 
        if skip_flag:
            continue
        else:        
            exclude_index.append(match_c.regs[0][0])
            line_num = code_content[0:match_c.span(1)[0]].count("\n") + 1
            logger2.info("Line " + str(line_num) + " in file " + f_path + " detect " + match_c.group(1) + match_c.group(
                6) + match_c.group(10))


def detect_define(code_content, f_path):
    global overwrite_flag
    match_comb = re.finditer(
        r"\n[ \t]*((#\s*define(\s+)(\w+)\s*\n)|(#\s*if\s*1)|(#\s*if\s*0)|(#\s*define((\s)|(\\\n))+?((\s)|(\\\s*?\n)|(//[^\n]*))*?\n))", code_content, overlapped=True)

    for match_c in match_comb:
        match_h = re.findall(r"#\s*define\s*_\S*_H\S*\s*\n",code_content, pos=match_c.span(0)[0], endpos=match_c.span(0)[1])
        if len(match_h):
            continue
        overwrite_flag = False
        skip_flag=False
        for exception in exceptionlist_macros:
            match_f = re.findall(r''+exception+'',code_content, pos=match_c.span(0)[0], endpos=match_c.span(0)[1])
            if len(match_f):
                skip_flag=True 
        if skip_flag:
            continue
        else:               
            line_num = code_content[0:match_c.span(1)[0]].count("\n") + 1
            logger3.info("Line " + str(line_num) + " in file " + f_path + " detect " + match_c.group(1))

def recursive_copy(src, dest, ignore = None):
    if not os.path.isdir(dest):
        os.makedirs(dest)
    files = os.listdir(src)
    if ignore is not None:
        ignored = ignore(src, files)
    else:
        ignored = []
    for file in files:
        if file not in ignored:
            if os.path.isdir(os.path.join(src, file)):
                recursive_copy(os.path.join(src, file), os.path.join(dest, file), ignore)
            else:
                shutil.copyfile(os.path.join(src, file), os.path.join(dest, file))

def ignore_copy_files(src, files):
    exclude_files = []
    for file in files:
        if os.path.join(src,file) in exceptionfile_list_full:
            exclude_files.append(file)
    return exclude_files


start_time = time.time()
config_file = open("config.txt", "r").read()
cur_path=os.getcwd()
work_dir = os.path.abspath(os.path.join(os.getcwd(), "."))

os.chdir(work_dir)
print('The work directory: '+work_dir)

definelist_macros = re.findall(re.compile("DefineList.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]
undefinelist_macros = re.findall(re.compile("UndefineList.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]

exceptionlist_macros = re.findall(re.compile("ExceptionList.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]
exceptionfile_list = re.findall(re.compile("ExceptionFiles.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]
print(exceptionfile_list)
folder_list = re.findall(re.compile("FolderList.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]
scanfolder_list = re.findall(re.compile("ScanFolder.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]
file_list = re.findall(re.compile("FileList.*?EndOfConfig", re.DOTALL), config_file)[0].split("\n")[1:-1]

folder_macros=re.findall(re.compile("DeleteFolderName.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]
file_macros=re.findall(re.compile("DeleteFileName.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2]

opensource_root_exists=len(re.findall(re.compile("OpenSourceRoot.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2])
print (opensource_root_exists)
opensource_root = ""
if opensource_root_exists > 0:
    opensource_root=(re.findall(re.compile("OpenSourceRoot.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2])[0]
closesource_root=(re.findall(re.compile("CloseSourceRoot.*?\n\n", re.DOTALL), config_file)[0].split("\n")[1:-2])[0]

remove_dir=os.path.join(work_dir, closesource_root)
remove_dir_list=os.walk(remove_dir)

#delete folders with names
print('---Delete folders with name starts---')
for root, dirs, files in remove_dir_list:
    for dirs_name in dirs:
        if dirs_name in folder_macros:
            shutil.rmtree(os.path.join(root, dirs_name))
print('---Delete folders with name ends---')  

remove_dir_list=os.walk(remove_dir)

#delete files with names
print('---Delete files with name starts---')
for root, dirs, files in remove_dir_list:
    for files_name in files:
        if files_name in file_macros:
            os.remove(os.path.join(root, files_name))
print('---Delete files with name ends---')

##delete files with path
print('---Delete files with path starts---')
for delete_file in file_list:
    if len(delete_file)>0:
        delete_file_fullpath = os.path.join(closesource_root, delete_file)
        if os.path.exists(delete_file_fullpath):
            os.remove(delete_file_fullpath)
        #else:
            #print(delete_file_fullpath, "does not exist.")
print('---Delete files with path ends---')

##delete folders with path
print('---Delete folders with path starts---')
for delete_folder in folder_list:
    if len(delete_folder)>0:
        delete_folder_fullpath = os.path.join(closesource_root, delete_folder)
    if os.path.exists(delete_folder_fullpath):
        print('delete folder')
        print(delete_folder_fullpath)
        shutil.rmtree(delete_folder_fullpath)
    else:
        print(delete_folder_fullpath, "does not exist.")
print('---Delete folders with path ends---')

overwrite_flag = True
keep_ind_dict = {}
isexist=os.path.exists('./violation')
if isexist:
    print('violation folder exists.')
else:
    os.mkdir('./violation')
    print('make violation folder successfully.')
logger1 = get_logger('else', "./violation/else.log")
logger2 = get_logger('combinationLogic', "./violation/comb.log")
logger3 = get_logger('defineMacro', "./violation/def.log")

scanfolder_list_full = []
exceptionfile_list_full = []

print('---Trim starts---')
for i in range(0,len(exceptionfile_list)):
    exceptionfile_list_full.append(os.path.join(work_dir,closesource_root,exceptionfile_list[i]))
print(exceptionfile_list_full)
for j in range(0,len(scanfolder_list)):
    scanfolder_list_full.append(os.path.join(work_dir,closesource_root,scanfolder_list[j]))

for folder in scanfolder_list_full:
    g = os.walk(folder)
    for path, dir_list, file_list in g:
        for file_name in file_list:
            # if file_name not in file_name_list:
            file_path = os.path.join(path, file_name)
            if file_path in exceptionfile_list_full:
                continue
            # Open and read code files
            code_file = open(file_path, 'r', encoding='utf-8')
            file_content = code_file.read()
            code_file.close()
            exclude_index = []
            detect_comb_logic(file_content, file_path)
            detect_define(file_content,file_path)
            del_index = []
            get_del_ind_if(undefinelist_macros, file_content, file_path, False)  # False means undefinelist
            get_del_ind_ifn(undefinelist_macros, file_content, file_path, False)
            get_del_ind_if(definelist_macros, file_content, file_path, True)  # True means definelist
            get_del_ind_ifn(definelist_macros, file_content, file_path, True)
            keep_code = P.closed(0, len(file_content))
            for [min_ind, max_ind] in del_index:
                keep_code = keep_code - P.closed(min_ind, max_ind)
            keep_ind_dict[file_path] = keep_code
print('---Trim ends---')

#if overwrite_flag
if 1:
    for file_path, keep_code in keep_ind_dict.items():
        # Open and read code files
        code_file = open(file_path, 'r', encoding='utf-8')
        file_content = code_file.read()
        code_file.close()

        file_content_new = ""
        for keep_code_index in keep_code:
            file_content_new += file_content[keep_code_index.lower:keep_code_index.upper]
        # Open and write code files
        code_file = open(file_path, 'w', encoding='utf-8')
        code_file.write(file_content_new)
        code_file.close()

if opensource_root_exists == 0:
    print('The opensource directory does not exist, skip replace.')
if opensource_root_exists > 0:
    close_dir=os.path.join(work_dir, closesource_root)
    print('The closesource directory: '+close_dir)
    open_dir=os.path.join(work_dir, opensource_root)
    print('The opensource directory: '+open_dir)
    if(os.path.exists(open_dir)):
        print('The opensource directory exists.')
        for x in range(0,len(scanfolder_list)):
            open_remove_folder_path = os.path.join(open_dir, scanfolder_list[x])
            close_copy_folder_path = os.path.join(close_dir, scanfolder_list[x])
            print('The copy folder path: '+close_copy_folder_path)
            print('The replaced folder path: '+open_remove_folder_path)
            #shutil.rmtree(open_remove_folder_path)
            #shutil.copytree(close_copy_folder_path,open_remove_folder_path)
            recursive_copy(close_copy_folder_path,open_remove_folder_path,ignore_copy_files)
end_time = time.time()
print("Execution Time: ", end_time - start_time)
print('Successfully done！')