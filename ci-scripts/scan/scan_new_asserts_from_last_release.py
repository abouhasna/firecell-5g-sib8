#!/usr/bin/env python3

# pylint: disable=invalid-name
# pylint: disable=consider-using-with

"""Import needed modules"""
import os
import sys
import argparse
import pprint
import re
import subprocess
import yaml
import html
from colorama import Fore, Back, Style
import xml.etree.ElementTree as ET
import datetime

#--------------------------------------------------------------------------------
def get_tags(conf: dict) -> list:
    """get all git tags from the current branch

    Args:
        conf (dict): configuration

    Returns:
        list: tags list
    """
    command = conf['get_tags_command']
    outputs = subprocess.getoutput(command).splitlines()
    return outputs

#--------------------------------------------------------------------------------
def get_last_tag(tags:list) -> str:
    return tags[-1]

#--------------------------------------------------------------------------------
def get_last_previous_release_tag(conf:dict, tags:list) -> str:
    """calculate le last previous release tag with major release level (from configuration)

    Args:
        conf (dict): configuration
        tags (list): list of tags

    Returns:
        str: la previous release tag
    """
    last_tag = tags[-1]
    reverse_tags = list(tags)
    reverse_tags.reverse()
    major_release_indice = conf['major_release_indice'] -1

    last_tag_levels = last_tag[1:].split('.')
    for tag in reverse_tags:
        tag_levels = tag[1:].split('.')
        if int(tag_levels[major_release_indice]) < int(last_tag_levels[major_release_indice]):
            return tag
            
#--------------------------------------------------------------------------------
def get_repos() -> str:
    return os.path.basename(get_repos_path())

#--------------------------------------------------------------------------------
def get_repos_path() -> str:
    path_elements = os.path.dirname(os.path.realpath(__file__)).split('/')
    repos_path = ''
    for el in path_elements[1:-2]:
        repos_path += f'/{el}'
    return repos_path

#--------------------------------------------------------------------------------
def build_assert_lines(conf:dict, file: str, lines: list, start:int) -> list:
    """ analyse a bunch of line. calculate line number and output csv lines

    Args:
        conf (dict): configuration
        file (str): file path
        lines (list): git diff lines
        start (int): start reference

    Returns:
        list: assert lines in csv format file:line:assert_message
    """
    if debug:
        print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN} build_assert_lines{Style.RESET_ALL}')
        print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN}  file: {file}{Style.RESET_ALL}')
        print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN}  start: {start}{Style.RESET_ALL}')
        print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}lines:')
        print(f'{Fore.CYAN}')
        pprint.pprint(lines)
        print(f'{Style.RESET_ALL}')
    assert_lines = []
    i = -1
    for line in lines:
        i += 1
        for p in conf['assert_patterns']:
            if debug:
                # print(f'p: \'{Fore.RED}{p}{Style.RESET_ALL}\' - line: \'{Fore.GREEN}{line}{Style.RESET_ALL}\'')
                pass
            m = re.match(p, line)
            if m:
                string = fr"{file}:{start+i}:{line[2:]}"
                string = string.replace('\n','\\n')
                assert_lines.append(string)
                if debug:
                    print(fr'{Fore.BLUE}-- [debug]{Style.RESET_ALL} accounting  \'{Fore.GREEN}{string}{Style.RESET_ALL}\':{Fore.MAGENTA}{start+i}{Style.RESET_ALL}')
                    
    return assert_lines

#--------------------------------------------------------------------------------
def build_data(conf:dict, last_previous_release_tag:str , csv_lines:list) -> dict:
    """This collect all asserts since le last release
    it builds data needed to generate xml files with values in output csv_lines
    Args:
        conf (dict): configuration
        last_previous_release_tag (str): last previous release tag
        csv_lines (list): lines in csv format like <files>,<line number><assert_line>

    Returns:
        dict: data needed to build xml file
    """
    print(fr'{Fore.BLUE}-- [debug]{Style.RESET_ALL} {Fore.YELLOW}In build_data(): {Style.RESET_ALL}')
    testsuite = {}
    today = datetime.datetime.now()
    testsuite.setdefault('name', f"assert dectection since {last_previous_release_tag}")
    testsuite.setdefault('timestamp', today.strftime("%Y-%m-%dT%H:%M:%S.%f"))
    testsuite.setdefault('hostname', os.uname()[1])
    testsuite.setdefault('tests', 0)
    testsuite.setdefault('failures', 0)
    testsuite.setdefault('errors', 0)
    testsuite.setdefault('time', "1")
    testsuite.setdefault('testcases', {})
    
    for output in csv_lines:
        fields = output.split(':')
        if debug:
            print(fr'{Fore.BLUE}-- [debug]{Style.RESET_ALL} fields: {Fore.YELLOW}{fields}{Style.RESET_ALL}')
        
        extension = '.' + fields[0].split('.')[-1]
        if extension in conf['extensions_accepted'] and fields[0] not in conf['files_to_exclude']:
            
            testsuite['testcases'].setdefault(fields[0], {})
            
            testsuite['testcases'][fields[0]].setdefault('name', fields[0])
            testsuite['testcases'][fields[0]].setdefault('classname', f"{last_previous_release_tag} Assert detection")
            testsuite['testcases'][fields[0]].setdefault('time', "1")
            testsuite['testcases'][fields[0]].setdefault('errors', [])
            
            record = {}
            testsuite['tests'] += 1
            testsuite['errors'] += 1
            record.setdefault('file', fields[0])
            record.setdefault('message', html.escape(fields[2]))
            record.setdefault('cwe', '617')
            record.setdefault('type', f'{last_previous_release_tag}')
            record.setdefault('line', fields[1])
            
            testsuite['testcases'][fields[0]]['errors'].append(record)
    
    return testsuite

#--------------------------------------------------------------------------------
def build_data_split_by_folders(conf:dict, last_previous_release_tag:str , csv_lines:list) -> dict:
    """This split data into list of folders given by configuration yaml file
    build data needed to generate xml files with values in output csv_lines

    Args:
        conf (dict): configuration
        last_previous_release_tag (str): last previous release tag
        csv_lines (list): lines in csv format like <files>,<line number><assert_line>

    Returns:
        dict: data needed to build xml file
    """
    
    print(fr'{Fore.BLUE}-- [debug]{Style.RESET_ALL} {Fore.YELLOW}In build_data_split_by_folders(): {Style.RESET_ALL}')
    testsuites = {}

    classify_folders = conf['classify_folders'] 
    extended_classify_folders = classify_folders
    
    today = datetime.datetime.now()
    for folder in extended_classify_folders:
        testsuites.setdefault(folder, {})     
        testsuites[folder].setdefault('name', folder)
        testsuites[folder].setdefault('timestamp', today.strftime("%Y-%m-%dT%H:%M:%S.%f"))
        testsuites[folder].setdefault('hostname', os.uname()[1])
        testsuites[folder].setdefault('tests', 0)
        testsuites[folder].setdefault('failures', 0)
        testsuites[folder].setdefault('errors', 0)
        testsuites[folder].setdefault('time', "1")
        testsuites[folder].setdefault('testcases', {})

    for output in csv_lines:
        fields = output.split(':')
        if debug:
            print(fr'{Fore.BLUE}-- [debug]{Style.RESET_ALL} fields: {Fore.YELLOW}{fields}{Style.RESET_ALL}')
            
        for folder in classify_folders:
            if is_subpath_in_path(folder, fields[0]):
                extension = '.' + fields[0].split('.')[-1]
                if extension in conf['extensions_accepted'] and fields[0] not in conf['files_to_exclude']:
                    
                    testsuites[folder]['testcases'].setdefault(fields[0], {})
                    
                    testsuites[folder]['testcases'][fields[0]].setdefault('name', fields[0])
                    testsuites[folder]['testcases'][fields[0]].setdefault('classname', f"{last_previous_release_tag} {folder} Assert detection")
                    testsuites[folder]['testcases'][fields[0]].setdefault('time', "1")
                    testsuites[folder]['testcases'][fields[0]].setdefault('errors', [])
                    
                    record = {}
                    testsuites[folder]['tests'] += 1
                    testsuites[folder]['errors'] += 1
                    record.setdefault('file', fields[0])
                    record.setdefault('message', html.escape(fields[2]))
                    record.setdefault('cwe', '617')
                    record.setdefault('type', f'{last_previous_release_tag}')
                    record.setdefault('line', fields[1])
                    
                    testsuites[folder]['testcases'][fields[0]]['errors'].append(record)
                
    return testsuites

#--------------------------------------------------------------------------------
def get_extension(file:str) -> str:
    splits = file.split('.')
    return f".{splits[-1]}"

#--------------------------------------------------------------------------------
def extract_line_number_starting_reference(line: str) -> str:
    # @@ -25,8 +25,8 @@
    # return 25 taken from +25,8
    match = re.search(r'\+[0-9]+[ ,]', line)
    string = match.group()
    return string[1:-1]
    
    
#--------------------------------------------------------------------------------
def get_git_diff(conf:dict, tag: str) -> list:
    command = conf['command_git_diff']
    command = command.replace('<repos_path>', get_repos_path())
    command = command.replace('<tag>', tag)
    outputs = subprocess.getoutput(command).splitlines()
    return outputs

#--------------------------------------------------------------------------------
def git_diff_parse(conf:dict, tag:str) -> list:
    """ Parse git diff

    Args:
        conf (dict): configuration dict
        tag (str): tag version

    Returns:
        list: lines in csv format file:line:assert line
    """
    data = get_git_diff(conf, tag)
    
    is_candidate = False
    results = []
    
    start = 0
    current_file = ''
    current_lines = []
    debug_index = 0
    for line in data:
        if debug:
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} line: {line}')
        # new candidate
        if re.match('^-[+]{3}', line) or re.match('^[+]{3}', line) or re.match('^---', line):

            if debug:
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA} git_diff_parse{Style.RESET_ALL}')
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA}  file: {current_file}{Style.RESET_ALL}')
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA}  start: {start}{Style.RESET_ALL}')
            
            if current_file != '' and is_candidate:
                asserts = build_assert_lines(conf, current_file, current_lines, start)
                for l in asserts:
                    results.append(l)
                    
                if debug:
                        print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.GREEN} release record{Style.RESET_ALL}')
            
            current_file = line
            current_file = line[6:]
            if debug:
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA} *current_line: {line}{Style.RESET_ALL}')
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA} *current_file: {current_file}{Style.RESET_ALL}')
            if debug:
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} current file: {Fore.GREEN}{current_file}{Style.RESET_ALL}')
                
            current_lines = []
            start = 0
            if debug:
                    print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.GREEN} Reset of record{Style.RESET_ALL}')
                    
            extension = get_extension(current_file)
            if not re.match('^---', line) and extension in conf['extensions_accepted']:
                is_candidate = True
                if debug:
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} is_candidate: {Fore.GREEN}{is_candidate}{Style.RESET_ALL}')
            else:
                is_candidate = False
                if debug:
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} is_candidate: {Fore.LIGHTRED_EX}{is_candidate}{Style.RESET_ALL}')
                        
        elif re.match('^[-+]?@@', line):
            if is_candidate:

                # add new file
                asserts = build_assert_lines(conf, current_file, current_lines, start)
                for l in asserts:
                    results.append(l)
            
                start = int(extract_line_number_starting_reference(line))
                debug_index = start-1
                if debug:
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} start: {Fore.GREEN}{str(start)}{Style.RESET_ALL}')

                current_lines = []
                if debug:
                    print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.GREEN} Reset of record{Style.RESET_ALL}')

                    
            else:
                start = 0
                current_lines = []
                if debug:
                    print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.GREEN} Reset of record{Style.RESET_ALL}')
            
        elif is_line_to_exclude(conf, line):
            if debug:
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} line to exclude={Fore.LIGHTRED_EX}{line}{Style.RESET_ALL}')
        else:
            if is_candidate:
                current_lines.append(line)
                
                if debug:
                    debug_index +=1
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} record line  {debug_index}: {Fore.CYAN}{line}{Style.RESET_ALL}')
            
    return results

#--------------------------------------------------------------------------------
def is_line_to_exclude(conf:dict, line: str) ->bool :
    for p in conf['diff_patterns_to_exclude']:
        if re.match(p, line):
            return True
    return False

#--------------------------------------------------------------------------------
def is_subpath_in_path(subpath:str, path:str) -> bool:
    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA} In is_subpath_in_path(){Style.RESET_ALL}')
    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} subpath {Fore.CYAN}{subpath}{Style.RESET_ALL}')
    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} path {Fore.CYAN}{path}{Style.RESET_ALL}')
    
    if re.search(f"^{subpath}", path):
        print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} {Fore.GREEN}{subpath} is in {path}{Style.RESET_ALL}')
        return True
    else:
        print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} {Fore.RED}{subpath} is not  in {path}{Style.RESET_ALL}')
    return False

#--------------------------------------------------------------------------------
def remove_filename_extension(filename: str) -> str:
    return filename.replace('.xml', '')


#--------------------------------------------------------------------------------
def to_xml_string(data:dict, indent:int) -> str:
    """_summary_

    Args:
        data (list): list of dictionnary
            keys: 
                - name str
                - classname str
                - time
                - errors []
                      - file
                      - message
                      - cwe : code vulnerability error code
                      - type
                      - line
        indent (int): nb space indentation
        
    Returns:
        str: xml string
    """

    output = ""
    ind =''
    for i in range(indent):
        ind += " "
        
    output += '<?xml version="1.0" encoding="UTF-8"?>\n'
    
    output += f'<testsuite name=\"{data['name']}\"'
    output += f' timestamp=\"{data['timestamp']}\"'
    output += f' hostname=\"{data['hostname']}\"'
    output += f' tests=\"{data['tests']}\"'
    output += f' failures=\"{data['failures']}\"'
    output += f' errors=\"{data['errors']}\"'
    output += f' time=\"{data['time']}\"'
    output += '>\n'
    
    for testcase in list(data['testcases'].keys()):
        for error in data['testcases'][testcase]['errors']:
            output += f'{ind}<testcase name=\"{testcase}:{error['line']}\"'
            output += f' classname=\"{data['testcases'][testcase]['classname']}\"'
            output += f' time=\"{data['testcases'][testcase]['time']}\"'
            output += '>\n'
            
            output += f'{ind}{ind}<error type=\"{error['type']}\"'
            output += f' file=\"{error['file']}\"'
            output += f' line=\"{error['line']}\"'
            output += f' message=\"{error['line']}:{error['message']}\"'
            output += '/>\n'
            output += f'{ind}</testcase>\n'
            
    output += '</testsuite>\n'
    
    return output


#--------------------------------------------------------------------------------
def to_xml_string_split_by_folder(data:dict, indent:int) -> str:
    """_summary_

    Args:
        data (list): list of dictionnary
            keys: 
                - name str
                - classname str
                - time
                - errors []
                      - file
                      - message
                      - cwe : code vulnerability error code
                      - type
                      - line
        indent (int): nb space indentation
        
    Returns:
        str: xml string
    """

    output = ""
    ind =''
    for i in range(indent):
        ind += " "
        
    output += '<?xml version="1.0" encoding="UTF-8"?>\n'
    output += '<testsuites>\n'
    
    for k in list(data.keys()):
        output += f'{ind}<testsuite name=\"{data[k]['name']}\"'
        output += f' timestamp=\"{data[k]['timestamp']}\"'
        output += f' hostname=\"{data[k]['hostname']}\"'
        output += f' tests=\"{data[k]['tests']}\"'
        output += f' failures=\"{data[k]['failures']}\"'
        output += f' errors=\"{data[k]['errors']}\"'
        output += f' time=\"{data[k]['time']}\"'
        output += '>\n'
        
        for testcase in list(data[k]['testcases'].keys()):
            for error in data[k]['testcases'][testcase]['errors']:
                output += f'{ind}{ind}<testcase name=\"{testcase}:{error['line']}\"'
                output += f' classname=\"{data[k]['testcases'][testcase]['classname']}\"'
                output += f' time=\"{data[k]['testcases'][testcase]['time']}\"'
                output += '>\n'
                
                output += f'{ind}{ind}{ind}<error type=\"{error['type']}\"'
                output += f' file=\"{error['file']}\"'
                output += f' line=\"{error['line']}\"'
                output += f' message=\"{error['line']}:{error['message']}\"'
                output += '/>\n'
                output += f'{ind}{ind}</testcase>\n'
                
        output += f'{ind}</testsuite>\n'
    output += '</testsuites>\n'
    
    return output


#--------------------------------------------------------------------------------
def main():
    global debug
    parser = argparse.ArgumentParser(description='search assert')
    parser.add_argument('-d','--debug', action='store_true', help='Run in debug mode')
    parser.add_argument('-ci','--ci', action='store_true', help='ci mode to change output path')
    parser.add_argument("-c", "--config", help="yaml config file", type=str, nargs='?', default='./config_scan_new_asserts_from_last_release.yml')
    parser.add_argument("-o", "--output", type=str, help="xml output file path", required=True)
    parser.add_argument('-s','--split', action='store_true', help='split results into a list of folders')

    args = parser.parse_args()

    debug = getattr(args,'debug')
    ci_mode = getattr(args,'ci')
    config_file = getattr(args,'config')
    split_by_folders = getattr(args,'split')
    output_xml_file = getattr(args,'output')

    output_xml_filename = os.path.basename(output_xml_file)

    final_output_xml_file = output_xml_file
    output_dir = os.path.dirname(output_xml_file)

    # if we are in ci context:
    if ci_mode:
        output_dir = f'{os.getcwd()}'
        final_output_xml_file = f"{output_dir}/{os.path.basename(output_xml_file)}"

    if debug:
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} current path: {os.getcwd()}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} debug: {debug}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} ci_mode: {ci_mode}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} config_file: {config_file}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} output_xml_file: {output_xml_file}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} output_dir: {output_dir}")

    if not os.path.exists(config_file):
        print(f'{Fore.LIGHTRED_EX}ERROR: config_file {config_file} does not exist !!{Style.RESET_ALL}')
        sys.exit(1)

    if not re.match('.*[.]xml$',output_xml_filename):
        print(f'{Fore.LIGHTRED_EX}ERROR: wrong output filename with {output_xml_filename} must end by .xml{Style.RESET_ALL}')
        sys.exit(1)

    # load config
    conf = {}
    with open(config_file, encoding="utf-8") as f:
        conf = yaml.safe_load(f)
    f.close()

    if debug:
        print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL} config:')
        print(Fore.GREEN)
        pprint.pprint(conf)
        print(Style.RESET_ALL)
    output_html_filename = f"{remove_filename_extension(output_xml_filename)}.html"
    if debug:
        print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL} html filename: {output_html_filename}' )

    # parse
    root_dir = get_repos_path()
    if debug:
        print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}root_dir: {root_dir}')

    tags = get_tags(conf)
    if debug:
        print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}tags:')
        print(f'{Fore.CYAN}')
        pprint.pprint(tags)
        print(f'{Style.RESET_ALL}')

    last_previous_release_tag = get_last_previous_release_tag(conf, tags)
    if debug:
        print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}last previous release tag: {last_previous_release_tag}')

    csv_lines = git_diff_parse(conf, last_previous_release_tag)
    
    data = build_data(conf, last_previous_release_tag, csv_lines)
    data_split_by_folders = build_data_split_by_folders(conf, last_previous_release_tag, csv_lines)
    
    if split_by_folders:
        final_output_split_by_folders_xml_file = final_output_xml_file.replace('.xml', '_split.xml')
        output_xml_string_split_by_folders = to_xml_string_split_by_folder(data_split_by_folders, 3)
        if debug:
            print(f"{Fore.BLUE}-- [debug] final_output_split_by_folders_xml_file: {Style.RESET_ALL} {Fore.CYAN}{final_output_split_by_folders_xml_file}{Style.RESET_ALL}")
        
        tree = ET.ElementTree(ET.fromstring(output_xml_string_split_by_folders))
        with open(f"{final_output_split_by_folders_xml_file}", 'wb') as f:
            tree.write(f, encoding='utf-8')
    else:
        output_xml_string = to_xml_string(data, 3)
        tree = ET.ElementTree(ET.fromstring(output_xml_string))
        with open(f"{final_output_xml_file}", 'wb') as f:
            tree.write(f, encoding='utf-8')
        
        
#------------------------------------------
if __name__ == "__main__":
    main()
