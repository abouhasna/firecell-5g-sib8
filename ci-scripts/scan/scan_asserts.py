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

def remove_filename_extension(filename: str):
    return filename.replace('.xml', '')

def get_repos() -> str:
    return os.path.basename(get_repos_path())

def get_path_from_repos(file:str) -> str :
    splits = file.split('/')
    # if debug:
    #     print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}repos: {Fore.YELLOW}{get_repos()}{Style.RESET_ALL}')
    #     print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}file: {Fore.YELLOW}{file}{Style.RESET_ALL}')
    #     print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}splits: {Fore.YELLOW}{splits}{Style.RESET_ALL}')
    
    index = splits.index(get_repos())
    splits = splits[index+1:]
    path =  '/'.join(splits)
    
    # if debug:
    #     print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}path from repos: {Fore.YELLOW}{path}{Style.RESET_ALL}')
        
    return path
    
def get_repos_path() -> str:
    path_elements = os.path.dirname(os.path.realpath(__file__)).split('/')
    repos_path = ''
    for el in path_elements[1:-2]:
        repos_path += f'/{el}'
    return repos_path

def build_data(conf:dict) -> dict:
    
    testsuite = {}
    
    today = datetime.datetime.now()
    testsuite.setdefault('name', "assert dectection")
    testsuite.setdefault('timestamp', today.strftime("%Y-%m-%dT%H:%M:%S.%f"))
    testsuite.setdefault('hostname', os.uname()[1])
    testsuite.setdefault('tests', 0)
    testsuite.setdefault('failures', 0)
    testsuite.setdefault('errors', 0)
    testsuite.setdefault('time', "1")
    
    testsuite.setdefault('testcases', {})

    
    for batch in conf['batches']:
        tag = batch['tag']
        command = batch['command']
        
        if debug:
            print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}tag: {tag}')
            print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}command: {command}')
        
        command = batch['command']
        command = command.replace('<root>',get_repos_path())
        if debug:
            print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}command: {command}')
        outputs = subprocess.getoutput(command).splitlines()
        
        # pprint.pprint(outputs)
        
        for output in outputs:
            # if debug:
            #     print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}output: {output}')
            #     print(f'{Fore.BLUE}-- [debug]:{Style.RESET_ALL}output[1]: {output[0]}')
                
            #remove unexpected output
            if output[0] != '/':
                continue
            
            fields = output.split(':')
            
            cleaned_file_path = get_path_from_repos(fields[0])
            
            extension = '.' + cleaned_file_path.split('.')[-1]
            if extension in conf['extensions_accepted'] and os.path.basename(cleaned_file_path) not in conf['files_to_exclude']:
                
                testsuite['testcases'].setdefault(cleaned_file_path, {})
                testsuite['testcases'][cleaned_file_path].setdefault('name', cleaned_file_path)
                testsuite['testcases'][cleaned_file_path].setdefault('classname', "Assert detection")
                testsuite['testcases'][cleaned_file_path].setdefault('time', "1")
                testsuite['testcases'][cleaned_file_path].setdefault('errors', [])
                
                record = {}
                testsuite['tests'] += 1
                testsuite['errors'] += 1
                record.setdefault('file', cleaned_file_path)
                record.setdefault('message', html.escape(fields[2]))
                record.setdefault('cwe', '617')
                record.setdefault('type', f'{tag}')
                record.setdefault('line', fields[1])
                
                testsuite['testcases'][cleaned_file_path]['errors'].append(record)
    return testsuite

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

#-----------------------------------------------------------------
def main():
    global debug
    global conf
    parser = argparse.ArgumentParser(description='search assert')
    parser.add_argument('-d','--debug', action='store_true', help='Run in debug mode')
    parser.add_argument('-ci','--ci', action='store_true', help='ci mode to change output path')
    parser.add_argument("-c", "--config", help="yaml config file", type=str, nargs='?', default='./config_scan_asserts.yml')
    parser.add_argument("-o", "--output", type=str, help="xml output file path", required=True)

    args = parser.parse_args()

    debug = getattr(args,'debug')
    ci_mode = getattr(args,'ci')
    config_file = getattr(args,'config')
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

                    
    data = build_data(conf)
    output_xml_string = to_xml_string(data, 3)
        
    tree = ET.ElementTree(ET.fromstring(output_xml_string))
    with open(f"{final_output_xml_file}", 'wb') as f:
        tree.write(f, encoding='utf-8')

    if debug:
        print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.YELLOW}{final_output_xml_file}{Style.RESET_ALL} created !!')
#------------------------------------------
if __name__ == "__main__":
    main()
