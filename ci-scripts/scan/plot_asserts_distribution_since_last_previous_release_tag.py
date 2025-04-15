#!/usr/bin/python3
import argparse
import pandas as pd
from datetime import datetime
import os
import sys
import inspect
import subprocess
import pprint
import re
import yaml
import matplotlib.pyplot as plt
# import matplotlib
import numpy as np
from colorama import Fore, Back, Style

class DatasetBuilder:
    """This class collect data compute them and make a csv string.
    csv string can be saved into a file by calling save_into_csv_file
    """
    def __init__(self, conf:dict, csv_file:str) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            
        self.conf = conf
        self.csv_file = csv_file
        self.tags = DatasetBuilder.collect_tags(self.conf['get_tags_command'])
        self.major_release_indice = self.conf['major_release_indice'] - 1
        
        self.last_previous_release_tag = self.calculate_last_previous_release_tag(self.major_release_indice, self.tags)
        print(f'-- last previous release tag: {Fore.GREEN}{self.last_previous_release_tag}{Style.RESET_ALL}')
        
        self.data = self.build_data()

        self.csv_string = self.data_to_csv_string(self.data, conf['csv_headers'])
        
        # save csv_file
        self.save_into_csv_file(self.csv_string, self.csv_file)

        self.df = pd.read_csv(csv_file)
        
    #------------------------------------------
    def build_assert_lines(self, assert_patterns:list, file: str, lines: list, start:int) -> list:
        """ analyse a bunch of line. calculate line number and output csv lines

        Args:
            file (str): file path
            lines (list): git diff lines
            start (int): start reference

        Returns:
            list: assert lines in csv format file:line:assert_message
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN}  file: {file}{Style.RESET_ALL}')
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN}  start: {start}{Style.RESET_ALL}')
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}lines:')
            print(f'{Fore.CYAN}')
            pprint.pprint(lines)
            print(f'{Style.RESET_ALL}')
            
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}assert_patterns:')
            print(f'{Fore.CYAN}')
            pprint.pprint(assert_patterns)
            print(f'{Style.RESET_ALL}')
            
        assert_lines = []
        i = -1
        for line in lines:
            i += 1
            for p in assert_patterns:
                if debug:
                    # print(f'p: \'{Fore.RED}{p}{Style.RESET_ALL}\' - line: \'{Fore.GREEN}{line}{Style.RESET_ALL}\'')
                    pass
                m = re.match(p, line)
                if m:
                    # string = fr"{file}:{start+i}:{line[2:]}"
                    string = fr"{file}:{start+i}:{line}"
                    string = string.replace('\n','\\n')
                    assert_lines.append(string)
                    if debug:
                        print(fr'{Fore.BLUE}-- [debug]{Style.RESET_ALL} accounting  \'{Fore.GREEN}{string}{Style.RESET_ALL}\':{Fore.MAGENTA}{start+i}{Style.RESET_ALL}')
                        
        return assert_lines
        
    #------------------------------------------
    def build_data(self) -> list:
        # output data exepected format:
        # [{'name': '/', 'new': 333, 'removed': 110, 'total': 4806},
        # {'name': 'openair1', 'new': 20, 'removed': 14, 'total': 772},
        # {'name': 'openair2', 'new': 189, 'removed': 65, 'total': 2896},
        # {'name': 'openair3', 'new': 24, 'removed': 23, 'total': 451},
        # {'name': 'others', 'new': 100, 'removed': 8, 'total': 687}]
        
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        # calcul for all
        all_asserts = DatasetBuilder.count_all_asserts(self.conf['command_find_all_asserts'], 
                                                            self.conf['extensions_accepted'], 
                                                            self.conf['grep_assert_patterns'])
        
        git_diff_csv_lines = self.git_diff_parse(self.conf['command_git_diff'], self.last_previous_release_tag)
        
        all_new_asserts = self.count_new_asserts(git_diff_csv_lines)
        all_removed_asserts = self.count_removed_asserts(git_diff_csv_lines)
        
        # print(f'-- all asserts number: {Fore.GREEN}{self.all_asserts}{Style.RESET_ALL}')
        
        data = []
        
        all_dataset = {}
        all_dataset.setdefault('name', 'All')
        all_dataset.setdefault('total', all_asserts)
        all_dataset.setdefault('new', all_new_asserts)
        all_dataset.setdefault('removed', all_removed_asserts)
        data.append(all_dataset)
        
        sum_total = 0
        sum_new = 0
        sum_removed = 0
        
        for folder in self.conf['classify_folders']:
            dataset = {}
            dataset.setdefault('name', folder)        
            
            total = self.count_all_asserts_in_folder_and_subfolders(self.conf['command_find_all_asserts'],
                                                                     self.conf['extensions_accepted'],
                                                                     self.conf['grep_assert_patterns'],
                                                                     folder_to_apply=folder)
            dataset.setdefault('total', total)
            
            new_asserts = self.count_new_asserts_in_folder_and_subfolders(folder, git_diff_csv_lines)
            dataset.setdefault('new', new_asserts)
            
            removed_asserts = self.count_removed_asserts_in_folder_and_subfolders(folder, git_diff_csv_lines)
            dataset.setdefault('removed', removed_asserts)
            
            data.append(dataset)
            
            sum_total += total
            sum_new += new_asserts
            sum_removed += removed_asserts
        
        # others
        if debug:
            print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL}others:")
        others_total = DatasetBuilder.count_all_asserts_except_paths(self.conf['command_find_all_asserts'], 
                                                                        self.conf['extensions_accepted'], 
                                                                        self.conf['grep_assert_patterns'], 
                                                                        self.conf['classify_folders'])
        others_new_asserts = self.count_new_asserts_except_paths(self.conf['classify_folders'], git_diff_csv_lines)
        others_removed_asserts = self.count_removed_asserts_except_paths(self.conf['classify_folders'], git_diff_csv_lines)
    
        others = {}
        others.setdefault('name', 'others')
        others.setdefault('total', others_total)
        others.setdefault('new', others_new_asserts)
        others.setdefault('removed', others_removed_asserts)
        data.append(others)

        sum_total += others_total
        sum_new += others_new_asserts
        sum_removed += others_removed_asserts
        
        if debug:
            print(f"{Fore.CYAN}")
            pprint.pprint(data)
            print(f"{Style.RESET_ALL}")
            print(f"{Fore.BLUE}-- [debug] sum:{Style.RESET_ALL} 'new': {Fore.GREEN}{sum_new}{Style.RESET_ALL} 'removed': {Fore.GREEN}{sum_removed}{Style.RESET_ALL} 'total': {Fore.GREEN}{sum_total}{Style.RESET_ALL}")
                    
        return data

    #--------------------------------------------------------------------------------
    def calculate_last_previous_release_tag(self, major_release_indice, tags:list) -> str:
        """calculate le last previous release tag with major release level (from configuration)

        Args:
            major_release_indice (str): major release indice
            tags (list): list of tags

        Returns:
            str: la previous release tag
        """
        last_tag = tags[-1]
        reverse_tags = list(tags)
        reverse_tags.reverse()

        last_tag_levels = last_tag[1:].split('.')
        for tag in reverse_tags:
            tag_levels = tag[1:].split('.')
            if int(tag_levels[major_release_indice]) < int(last_tag_levels[major_release_indice]):
                return tag

    #------------------------------------------
    @staticmethod
    def collect_tags(command:str) -> list:
        outputs = subprocess.getoutput(command).splitlines()
        return outputs
    
    #------------------------------------------
    @staticmethod
    def count_all_asserts(command:str,
                          extensions_accepted:list,
                          assert_patterns:list,
                          paths_to_exclude:list=None,
                          folder_to_apply=None) -> int:
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} command: {Fore.GREEN}{command}{Style.RESET_ALL}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} extensions_accepted: {Fore.GREEN}{extensions_accepted}{Style.RESET_ALL}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} assert_patterns: {Fore.GREEN}{assert_patterns}{Style.RESET_ALL}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} paths_to_exclude: {Fore.GREEN}{paths_to_exclude}{Style.RESET_ALL}")
            
        files_accepted = []
        for ext in extensions_accepted:
            files_accepted.append(f"^[^:,;]*{ext}")
            
        #
        file_selector = ''
        if paths_to_exclude is None or not paths_to_exclude:
            file_selector = Tools.shape_find_file_selector(extensions_accepted)
        else:
            file_selector = Tools.shape_find_file_selector(extensions_accepted, o=True)
                
        # build files selection:
        if folder_to_apply is None:
            command = command.replace('<repos_path>', Tools.get_repos_path())
        else:
            command = command.replace('<repos_path>', f"{Tools.get_repos_path()}/{folder_to_apply}")
            
        command = command.replace('<paths_to_exclude>', Tools.shape_find_paths_to_exclude(paths_to_exclude))
        command = command.replace('<assert_selector>', Tools.shape_assert_selector(assert_patterns))
        command = command.replace('<file_selector>', file_selector)
                        
        if debug:
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} command: {command}")
            
        
        outputs = subprocess.getoutput(command).splitlines()
                        
        return len(outputs)

    #------------------------------------------
    @staticmethod
    def count_all_asserts_except_paths(command:str, 
                                       extensions_accepted:list,
                                       assert_patterns:list, 
                                       paths_to_exclude:list) -> int:
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            
        return DatasetBuilder.count_all_asserts(command,
                                                extensions_accepted,
                                                assert_patterns,
                                                paths_to_exclude=paths_to_exclude,)

    #------------------------------------------
    @staticmethod
    def count_all_asserts_in_folder_and_subfolders(command:str,
                                       extensions_accepted:list,
                                       assert_patterns:list,
                                       folder_to_apply:str,
                                       paths_to_exclude:list=None) -> int:
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            
        return DatasetBuilder.count_all_asserts(command,
                                                extensions_accepted,
                                                assert_patterns,
                                                paths_to_exclude=paths_to_exclude,
                                                folder_to_apply=folder_to_apply)



# #------------------------------------------
#     @staticmethod
#     def count_all_assert_in_folder_and_subfolders(command:str, 
#                                                   relative_folder_path:str, 
#                                                   extensions_accepted:list, 
#                                                   assert_patterns:list) -> int:
        
#         if debug:
#             print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            
#         total = 0
        
#         # build files selection:
#         command = command.replace('<repos_path>', f'{Tools.get_repos_path()}/{relative_folder_path}')
#         command = command.replace('<assert_selector>', Tools.shape_assert_selector(assert_patterns))
#         command = command.replace('<file_selector>', Tools.shape_grep_file_selector(extensions_accepted))
#         if debug:
#             print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <repos_path>: {Tools.get_repos_path()}")
#             print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <assert_selector>: {Tools.shape_assert_selector(assert_patterns)}")
#             print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <file_selector>: {Tools.shape_grep_file_selector(extensions_accepted)}")
#             print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} command: {command}")
            
        
#         outputs = subprocess.getoutput(command).splitlines()
#         total = int(outputs[-1])
        
#         return total

    #------------------------------------------
    def count_new_asserts(self, csv_lines:list) -> int:
        """count lines with [0-9]+:[+]
        line ex: openair3/SECU/nas_stream_eia2.c:49:+  DevAssert((stream_cipher->blength & 0x7) == 0);
        it parses a git diff output
        
        Args:
            csv_lines (list): list come from grep field_separator = ':'

        Returns:
            int: number
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        n = 0
        for l in csv_lines:
            splits = l.split(':')
            if re.match('^[+]',splits[2]):
                n += 1
        return n

    #------------------------------------------
    def count_new_asserts_in_folder_and_subfolders(self, folder:str, csv_lines:list) -> int:
        """count lines with [0-9]+:[+]
        line ex: openair3/SECU/nas_stream_eia2.c:49:+  DevAssert((stream_cipher->blength & 0x7) == 0);
        it parses a git diff output

        Args:
            folder (str): folder
            csv_lines (list): list come from grep field_separator = ':'

        Returns:
            int: number
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        n = 0
        for l in csv_lines:
            splits = l.split(':')
            if folder in splits[0] and re.match('^[+]',splits[2]):
                n += 1
        return n

    #------------------------------------------
    def count_new_asserts_except_paths(self, paths_to_exclude:list, csv_lines:list) -> int:
        """count lines with [0-9]+:[+]
        line ex: openair3/SECU/nas_stream_eia2.c:49:+  DevAssert((stream_cipher->blength & 0x7) == 0);
        it parses a git diff output

        Args:
            folder (str): folder
            csv_lines (list): list come from grep field_separator = ':'

        Returns:
            int: number
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        
        sorted_lines = []
        for l in csv_lines:
            splits = l.split(':')
            to_exclude = False
            for p in paths_to_exclude:
                
                if f"{p}" in splits[0]:
                    to_exclude = True
                    break
                else:
                    pass
                
            if not to_exclude and re.match('^[+]',splits[2]):
                sorted_lines.append(l)
                            
        return len(sorted_lines)


    #------------------------------------------
    def count_removed_asserts(self, csv_lines:list) -> int:
        """count lines with [0-9]+:[+]
        line ex: openair3/SECU/nas_stream_eia2.c:49:-  DevAssert((stream_cipher->blength & 0x7) == 0);
        it parses a git diff output

        Args:
            csv_lines (list): list come from grep field_separator = ':'

        Returns:
            int: number
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        n = 0
        for l in csv_lines:
            splits = l.split(':')
            if re.match('^[-]',splits[2]):
                n += 1
        return n

    #------------------------------------------
    def count_removed_asserts_in_folder_and_subfolders(self, folder:str, csv_lines:list) -> int:
        """count lines with [0-9]+:[+]
        line ex: openair3/SECU/nas_stream_eia2.c:49:-  DevAssert((stream_cipher->blength & 0x7) == 0);
        it parses a git diff output

        Args:
            folder (str): folder
            csv_lines (list): list come from grep field_separator = ':'

        Returns:
            int: number
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        n = 0
        for l in csv_lines:
            splits = l.split(':')
            if folder in splits[0] and re.match('^[-]',splits[2]):
                n += 1
        return n

    #------------------------------------------
    def count_removed_asserts_except_paths(self, paths_to_exclude:list, csv_lines:list) -> int:
        """count lines with [0-9]+:[+]
        line ex: openair3/SECU/nas_stream_eia2.c:49:-  DevAssert((stream_cipher->blength & 0x7) == 0);
        it parses a git diff output

        Args:
            folder (str): folder
            csv_lines (list): list come from grep field_separator = ':'

        Returns:
            int: number
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        
        sorted_lines = []
        for l in csv_lines:
            splits = l.split(':')
            to_exclude = False
            for p in paths_to_exclude:
                
                if f"{p}" in splits[0]:
                    to_exclude = True
                    break
                else:
                    pass
                
            if not to_exclude and re.match('^[-]',splits[2]):
                sorted_lines.append(l)
                            
        return len(sorted_lines)


    #------------------------------------------
    def data_to_csv_string(self, data:list, headers:list) -> str:
        """transform data in csv string

        Args:
            data (list of dict): data 
            headers (list) : first line of csv file
        Returns:
            str: csv string
        """
        # data exepected format:
        # [{'name': '/', 'new': 333, 'removed': 110, 'total': 4806},
        # {'name': 'openair1', 'new': 20, 'removed': 14, 'total': 772},
        # {'name': 'openair2', 'new': 189, 'removed': 65, 'total': 2896},
        # {'name': 'openair3', 'new': 24, 'removed': 23, 'total': 451},
        # {'name': 'others', 'new': 100, 'removed': 8, 'total': 687}]

        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            
        string = f"{','.join(headers)}\n"
        for line in data:
            string += f"{line['name']},{line['new']},{line['removed']},{line['total']}\n"
    
        if debug:
            print(f"{Fore.BLUE}-- [debug] csv_string: {Style.RESET_ALL}")
            print(f"{Fore.CYAN}{string}{Style.RESET_ALL}")
            
        return string

    #------------------------------------------
    def extract_line_number_starting_reference(self, line: str) -> str:
        # @@ -25,8 +25,8 @@
        # return 25 taken from +25,8
        match = re.search(r'\+[0-9]+[ ,]', line)
        string = match.group()
        return string[1:-1]

    #------------------------------------------
    def get_csv_string(self) -> str:
        return self.csv_string
    
    #------------------------------------------
    def get_data(self) -> list:
        return self.data

    #------------------------------------------
    def get_df(self) -> list:
        return self.df

    #------------------------------------------
    def get_last_previous_release_tag(self) -> str:
        return self.last_previous_release_tag
    
    #------------------------------------------
    def get_tags(self) -> list:
        return self.tags

    #------------------------------------------
    def save_into_csv_file(self, csv_string:str, csv_file:str) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        fo = open(os.path.expanduser(csv_file),'w+', encoding="utf-8")
        fo.write(csv_string)
        fo.close()

        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}csv file {Fore.GREEN}{csv_file}{Style.RESET_ALL} is created")

    #--------------------------------------------------------------------------------
    def get_git_diff(self, command:str, tag: str) -> list:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        command = command.replace('<repos_path>', Tools.get_repos_path())
        command = command.replace('<tag>', tag)
        outputs = subprocess.getoutput(command).splitlines()
        return outputs

    #--------------------------------------------------------------------------------
    def git_diff_parse(self, git_diff_command:str, tag:str) -> list:
        """ Parse git diff

        Args:
            conf (dict): configuration dict
            tag (str): tag version

        Returns:
            list: lines in csv format file:line:assert line
        """
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
            
        data = self.get_git_diff(git_diff_command, tag)
        
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
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN} git_diff_parse{Style.RESET_ALL}')
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN}  file: {current_file}{Style.RESET_ALL}')
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN}  start: {start}{Style.RESET_ALL}')
                
                if current_file != '' and is_candidate:
                    asserts = self.build_assert_lines(self.conf['git_grep_assert_patterns'], current_file, current_lines, start)
                    for l in asserts:
                        results.append(l)
                        
                    if debug:
                            print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.GREEN} release record{Style.RESET_ALL}')
                
                current_file = line
                current_file = line[6:]
                if debug:
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN} *current_line: {line}{Style.RESET_ALL}')
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.CYAN} *current_file: {current_file}{Style.RESET_ALL}')
                if debug:
                    print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} current file: {Fore.GREEN}{current_file}{Style.RESET_ALL}')
                    
                current_lines = []
                start = 0
                if debug:
                        print(f'{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.GREEN} Reset of record{Style.RESET_ALL}')
                        
                extension = Tools.get_extension(current_file)
                if not re.match('^---', line) and extension in self.conf['extensions_accepted']:
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
                    asserts = self.build_assert_lines(self.conf['git_grep_assert_patterns'], current_file, current_lines, start)
                    for l in asserts:
                        results.append(l)
                
                    start = int(self.extract_line_number_starting_reference(line))
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
                
            elif self.is_line_to_exclude(self.conf['diff_patterns_to_exclude'], line):
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
    def is_line_to_exclude(self, diff_patterns_to_exclude:list, line: str) ->bool :
        for p in diff_patterns_to_exclude:
            if re.match(p, line):
                return True
        return False



#----------------END of class DatasetBuilder--------------------------
class PlotBar:
    #------------------------------------------
    def __init__(self, conf:dict, df:object , tag: str, percentage_mode=False) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        self.conf = conf
        self.df = df
        self.df.columns = self.conf['df_columns']
        for col in self.conf['df_columns'][1:]:
            self.df = self.df[df[col] != '-']
            self.df[col] = pd.to_numeric(df[col])
        # print
        print("-- df:")
        print(f"{Fore.YELLOW}{self.df}{Style.RESET_ALL}")
       
        self.df = self.df.set_index(self.conf['df_columns'][0])
        
        i_folder = conf['indices']['folder']
        i_new = conf['indices']['new']
        i_removed = conf['indices']['removed']
        i_total = conf['indices']['total']
        
        
        # Adjusting the figure size
        w = conf['w_bar_chart_fig_size']
        h = conf['h_bar_chart_fig_size']
        fig, ax = plt.subplots(figsize=(w, h))

        X_axis = np.arange(len(self.df.index))
        
        bar1 = plt.bar(X_axis - 0.2, df[conf['df_columns'][i_total]], color=conf['serie_colors'][i_total], width = 0.2)
        bar2 = plt.bar(X_axis + 0, df[conf['df_columns'][i_new]], color=conf['serie_colors'][i_new], width = 0.2)
        bar3 = plt.bar(X_axis + 0.2, df[conf['df_columns'][i_removed]], color=conf['serie_colors'][i_removed], width = 0.2)
        
        # Adding a plot title and customizing its font size
        title = self.conf['title']
        title = title.replace('<repos>', Tools.get_repos())
        title = title.replace('<tag>', tag)
        plt.title(title, fontsize=20)

        plt.xticks(X_axis, self.df.index)
        # Adding axis labels and customizing their font size
        plt.ylabel(conf['y_label'], fontsize=15)

        plt.legend( (bar1, bar2, bar3), (conf['csv_headers'][i_total],
                                         conf['csv_headers'][i_new],
                                         conf['csv_headers'][i_removed]) )
        # Display grid
        plt.grid(True)
        
        y_inc = conf['bar_labels_y_increment']
        yp_inc = conf['bar_percentage_labels_y_increments']
        # Rotaing axis ticks and customizing their font size
        langle = 0
        tangle = 0
        if len(df[self.conf['df_columns'][0]]) >= conf['threshold_labels_angle']:
            tangle = self.conf['tick_label_angle']
            langle = self.conf['labels_angle']
            
        fontsize = conf['hbar_label_fontsize']
        plt.xticks(rotation=tangle, fontsize=10)
        

        for index,data in enumerate(df[self.conf['df_columns'][i_total]]):
            if data > 0 :
                plt.text(x=index+conf['bar_labels_x_increments'][i_total] , y=data+y_inc , s=f"{data}" , fontdict=dict(fontsize=fontsize,fontweight='bold'), rotation=langle)

        for index,data in enumerate(df[self.conf['df_columns'][i_new]]):
            if data > 0 :
                plt.text(x=index+conf['bar_labels_x_increments'][i_new] , y=data+y_inc , s=f"{data}" , fontdict=dict(fontsize=fontsize,fontweight='bold'), rotation=langle)
            
        for index,data in enumerate(df[self.conf['df_columns'][i_removed]]):
            if data > 0 :
                plt.text(x=index+conf['bar_labels_x_increments'][i_removed] , y=data+y_inc , s=f"{data}" , fontdict=dict(fontsize=fontsize,fontweight='bold'), rotation=langle)


        whole = {}
        whole[i_total] = df[self.conf['df_columns'][i_total]].iloc[0]
        whole[i_new] = df[self.conf['df_columns'][i_new]].iloc[0]
        whole[i_removed] = df[self.conf['df_columns'][i_removed]].iloc[0]

        color = self.conf['percentage_labels_color']
        
        if percentage_mode:
            for index,data in enumerate(df[self.conf['df_columns'][i_total]]):
                if data > 0 :
                    plt.text(x=index+conf['bar_labels_x_increments'][i_total] , y=data+yp_inc , s=f"{self.percentage(data,whole[i_total])}" , fontdict=dict(fontsize=fontsize), color=color, rotation=langle)

            for index,data in enumerate(df[self.conf['df_columns'][i_new]]):
                if data > 0 :
                    plt.text(x=index+conf['bar_labels_x_increments'][i_new] , y=data+yp_inc , s=f"{self.percentage(data,whole[i_new])}" , fontdict=dict(fontsize=fontsize), color=color, rotation=langle)
                
            for index,data in enumerate(df[self.conf['df_columns'][i_removed]]):
                if data > 0 :
                    plt.text(x=index+conf['bar_labels_x_increments'][i_removed] , y=data+yp_inc , s=f"{self.percentage(data,whole[i_removed])}" , fontdict=dict(fontsize=fontsize), color=color, rotation=langle)

        # text current date
        today = datetime.now()
        creation_date = f"{today.strftime("%Y-%m-%d %H:%M:%S")}"

        plt.text(x=len(conf['classify_folders'])+conf['bar_xdate_inc'], y=max(df[conf['df_columns'][i_total]]+conf['bar_ydate_inc']), s=f"{creation_date}", fontdict=dict(fontsize=10), color=conf['date_color'])
        # plt.text(x=len(conf['classify_folders'])/2, y=-300, s=f"{creation_date}", fontdict=dict(fontsize=10), color=conf['date_color'])
       
    def percentage(self, part:int, whole:int) -> str:
        return f"{Tools.calculate_percentage(part, whole, decimals=self.conf['n_decimals'])} %"
       
    #------------------------------------------
    def savefig(self, png_file:str) -> None:
        plt.savefig(png_file)
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}png file {Fore.GREEN}{png_file}{Style.RESET_ALL} is created")

    #------------------------------------------
    def show(self) ->None:
        plt.show()

#----------------END of class PlotBar--------------------------
class PlotBarHorizontal:
    #------------------------------------------
    def __init__(self, conf:dict, df:object , tag: str, percentage_mode=False) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}{Fore.MAGENTA}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}(){Style.RESET_ALL}")
        self.conf = conf
        self.df = df
        self.df.columns = self.conf['df_columns']
        for col in self.conf['df_columns'][1:]:
            self.df = self.df[df[col] != '-']
            self.df[col] = pd.to_numeric(df[col])
        # print
        print("-- df:")
        print(f"{Fore.YELLOW}{self.df}{Style.RESET_ALL}")
       
        self.df = self.df.set_index(self.conf['df_columns'][0])
        
        i_folder = conf['indices']['folder']
        i_new = conf['indices']['new']
        i_removed = conf['indices']['removed']
        i_total = conf['indices']['total']
        
        # Adjusting the figure size
        w = conf['w_horizontal_bar_chart_fig_size']
        h = conf['h_horizontal_bar_chart_fig_size']
        fig, ax = plt.subplots(figsize=(w, h))

        Y_axis = np.arange(len(self.df.index))
        
        bar1 = plt.barh(Y_axis - 0.2, df[conf['df_columns'][i_total]], color=conf['serie_colors'][i_total], height=0.2)
        bar2 = plt.barh(Y_axis + 0, df[conf['df_columns'][i_new]], color=conf['serie_colors'][i_new], height=0.2)
        bar3 = plt.barh(Y_axis + 0.2, df[conf['df_columns'][i_removed]], color=conf['serie_colors'][i_removed], height=0.2)
        
        # Adding a plot title and customizing its font size
        title = self.conf['title']
        title = title.replace('<repos>', Tools.get_repos())
        title = title.replace('<tag>', tag)
        plt.title(title, fontsize=20)

        plt.yticks(Y_axis, self.df.index)
        # Adding axis labels and customizing their font size
        plt.xlabel(conf['y_label'], fontsize=15)

        plt.legend( (bar1, bar2, bar3), (conf['csv_headers'][i_total],
                                         conf['csv_headers'][i_new],
                                         conf['csv_headers'][i_removed]) )
        # Display grid
        plt.grid(True)
        
        x_inc = conf['hbar_labels_x_increment']
        xp_inc = conf['hbar_percentage_labels_x_increments']
        # Rotaing axis ticks and customizing their font size
        langle = 0
        tangle = 0
        fontsize = conf['hbar_label_fontsize']
        # if len(df[self.conf['df_columns'][0]]) >= conf['threshold_labels_angle']:
        #     tangle = self.conf['tick_label_angle']
        #     langle = self.conf['labels_angle']
            
        plt.xticks(rotation=tangle, fontsize=10)
        

        for index,data in enumerate(df[self.conf['df_columns'][i_total]]):
            if data > 0 :
                # plt.text(x=data+conf['hbar_labels_x_increments'][i_total] , y=index+y_inc , s=f"{data}" , fontdict=dict(fontsize=10,fontweight='bold'), rotation=langle)
                plt.text(x=data+x_inc, y=index+conf['hbar_labels_y_increments'][i_total] , s=f"{data}" , fontdict=dict(fontsize=fontsize,fontweight='bold'), rotation=langle)

        for index,data in enumerate(df[self.conf['df_columns'][i_new]]):
            if data > 0 :
                # plt.text(x=data+conf['labels_x_increments'][i_new] , y=index+y_inc , s=f"{data}" , fontdict=dict(fontsize=10,fontweight='bold'), rotation=langle)
                plt.text(x=data+x_inc , y=index+conf['hbar_labels_y_increments'][i_new] , s=f"{data}" , fontdict=dict(fontsize=fontsize,fontweight='bold'), rotation=langle)
            
        for index,data in enumerate(df[self.conf['df_columns'][i_removed]]):
            if data > 0 :
                # plt.text(x=data+conf['labels_x_increments'][i_removed] , y=index+y_inc , s=f"{data}" , fontdict=dict(fontsize=10,fontweight='bold'), rotation=langle)
                plt.text(x=data+x_inc , y=index+conf['hbar_labels_y_increments'][i_removed] , s=f"{data}" , fontdict=dict(fontsize=fontsize,fontweight='bold'), rotation=langle)


        whole = {}
        whole[i_total] = df[self.conf['df_columns'][i_total]].iloc[-1]
        whole[i_new] = df[self.conf['df_columns'][i_new]].iloc[-1]
        whole[i_removed] = df[self.conf['df_columns'][i_removed]].iloc[-1]

        color = self.conf['percentage_labels_color']
        
        if percentage_mode:
            for index,data in enumerate(df[self.conf['df_columns'][i_total]]):
                if data > 0 :
                    # plt.text(x=index+conf['labels_x_increments'][i_total] , y=data+yp_inc , s=f"{self.percentage(data,whole[i_total])}" , fontdict=dict(fontsize=10), color=color, rotation=langle)
                    plt.text( x=data+xp_inc ,y=index+conf['hbar_labels_y_increments'][i_total] , s=f"{self.percentage(data,whole[i_total])}" , fontdict=dict(fontsize=fontsize), color=color, rotation=langle)

            for index,data in enumerate(df[self.conf['df_columns'][i_new]]):
                if data > 0 :
                    # plt.text(x=index+conf['labels_x_increments'][i_new] , y=data+yp_inc , s=f"{self.percentage(data,whole[i_new])}" , fontdict=dict(fontsize=10), color=color, rotation=langle)
                    plt.text( x=data+xp_inc ,y=index+conf['hbar_labels_y_increments'][i_new], s=f"{self.percentage(data,whole[i_new])}" , fontdict=dict(fontsize=fontsize), color=color, rotation=langle)
                
            for index,data in enumerate(df[self.conf['df_columns'][i_removed]]):
                if data > 0 :
                    # plt.text(x=index+conf['labels_x_increments'][i_removed] , y=data+yp_inc , s=f"{self.percentage(data,whole[i_removed])}" , fontdict=dict(fontsize=10), color=color, rotation=langle)
                    plt.text(x=data+xp_inc ,y=index+conf['hbar_labels_y_increments'][i_removed], s=f"{self.percentage(data,whole[i_removed])}" , fontdict=dict(fontsize=fontsize), color=color, rotation=langle)

        # text current date
        today = datetime.now()
        creation_date = f"{today.strftime("%Y-%m-%d %H:%M:%S")}"

        # plt.text(x=len(conf['classify_folders'])+conf['xdate_inc'], y=max(df[conf['df_columns'][i_total]]+300), s=f"{creation_date}", fontdict=dict(fontsize=10), color=conf['date_color'])
        plt.text(x=max(df[conf['df_columns'][i_total]])+conf['hbar_xdate_inc'], y=len(conf['classify_folders'])+conf['hbar_ydate_inc'], s=f"{creation_date}", fontdict=dict(fontsize=10), color=conf['date_color'])
       
    def percentage(self, part:int, whole:int) -> str:
        return f"{Tools.calculate_percentage(part, whole, decimals=self.conf['n_decimals'])} %"
       
    #------------------------------------------
    def savefig(self, png_file:str) -> None:
        plt.savefig(png_file)
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}png file {Fore.GREEN}{png_file}{Style.RESET_ALL} is created")

    #------------------------------------------
    def show(self) ->None:
        plt.show()

#----------------END of class PlotBarHorizontal--------------------------
class Tools:
    @staticmethod
    def calculate_percentage(part, whole, decimals=1):
        if whole == 0:
            return "Cannot divide by zero!"
        percentage = (part / whole) * 100
        return round(percentage,decimals)
        
    #------------------------------------------
    @staticmethod
    def get_extension(file:str) -> str:
        splits = file.split('.')
        return f".{splits[-1]}"
    
    
    #------------------------------------------
    @staticmethod
    def get_repos() -> str:
        return os.path.basename(Tools.get_repos_path())

    #------------------------------------------
    @staticmethod
    def get_repos_path() -> str:
        path_elements = os.path.dirname(os.path.realpath(__file__)).split('/')
        repos_path = ''
        for el in path_elements[1:-2]:
            repos_path += f'/{el}'
        return repos_path

    #------------------------------------------
    @staticmethod
    def get_root_path() -> str:
        return f"{os.path.dirname(__file__)}/../.."
    
    #------------------------------------------
    @staticmethod
    def is_subpath_in_path(subpath:str, path:str) -> bool:
        if debug:
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL}{Fore.MAGENTA} In is_subpath_in_path(){Style.RESET_ALL}')
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} subpath {Fore.CYAN}{subpath}{Style.RESET_ALL}')
            print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} path {Fore.CYAN}{path}{Style.RESET_ALL}')
        
        if re.search(f"^{subpath}", path):
            if debug:
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} {Fore.GREEN}{subpath} is in {path}{Style.RESET_ALL}')
            return True
        else:
            if debug:
                print(f'{Fore.BLUE}-- [debug]{Style.RESET_ALL} {Fore.RED}{subpath} is not  in {path}{Style.RESET_ALL}')
        return False
    
    #------------------------------------------
    @staticmethod
    def print_list_in_file(alist:list, file:str, debug_mode=False) -> None:
        suffix = ''
        end = ''
        if debug_mode:
            suffix = f"{Fore.BLUE}-- [debug] {Style.RESET_ALL}{Fore.YELLOW}"
            end = f"{Style.RESET_ALL}"
            
        fo = open(os.path.expanduser(file),'w+', encoding="utf-8")
        for l in alist:
            fo.write(f"{suffix}{l}{end}\n")
        fo.close()
    #------------------------------------------
    @staticmethod
    def shape_find_file_selector(extensions_accepted:list, o:bool=False) -> str :
        
        if extensions_accepted is None or not extensions_accepted:
            return ''
        
        string =''
        if o:
            string ="-o "
            
        string += f"\\( -name '*{extensions_accepted[0]}'"
        
        for ext in extensions_accepted[1:-1]:
            string += f" -o -name '*{ext}'"
            
        string += f" -o -name '*{extensions_accepted[-1]}' \\)"
        return string

    #------------------------------------------
    @staticmethod
    def shape_find_paths_to_exclude(paths_to_exclude:list) -> str :
        
        if paths_to_exclude is None or not paths_to_exclude:
            return ''
        
        string =f"\\( -path '{Tools.get_repos_path()}/{paths_to_exclude[0]}'"
        
        for ext in paths_to_exclude[1:-1]:
            string += f" -o -path '{Tools.get_repos_path()}/{ext}'"
            
        string += f" -o -path '{Tools.get_repos_path()}/{paths_to_exclude[-1]}' \\) -prune"
        return string
    #------------------------------------------

    
    #------------------------------------------
    @staticmethod
    def shape_grep_file_selector(extensions_accepted:list) -> str :
        
        if extensions_accepted is None or not extensions_accepted:
            return ''

        string =''
        for ext in extensions_accepted:
            string += f"\\{ext}:\\|"
        return f"{string[:-2]}"
    
    #------------------------------------------
    @staticmethod
    def shape_assert_selector(assert_patterns:list) -> str :
        
        if assert_patterns is None or not assert_patterns:
            return ''
        
        string =''
        for p in assert_patterns:
            string += f"{p}\\|"
        return f"{string[:-2]}"
    
        
#----------------END of class Tools--------------------------


def main():
    global debug
    parser = argparse.ArgumentParser(description='search assert')
    parser.add_argument('-d','--debug', action='store_true', help='Run in debug mode')
    parser.add_argument('-ci','--ci', action='store_true', help='ci mode to change output path')
    parser.add_argument("-c", "--config", help="yaml config file", type=str, nargs='?', default='./plot_asserts_distribution_since_last_previous_release_tag.yml')
    parser.add_argument("-o", "--output_csv", help="csv output file", type=str, nargs='?', default='./asserts_in_out.csv')
    parser.add_argument('-p','--percentage', action='store_true', help='plot in percentage mode')
    parser.add_argument('-pl','--horizontal', action='store_true', help='plot horizontal bar chart')

    args = parser.parse_args()
    debug = getattr(args,'debug')
    ci_mode = getattr(args,'ci')
    config_file = getattr(args,'config')
    csv_file = getattr(args,'output_csv')
    percentage_mode = getattr(args,'percentage')
    horizontal = getattr(args,'horizontal')
    
    if csv_file[-4:] != '.csv':
        print(f"{Fore.RED}ERROR: output_csv {csv_file} must end by \'.csv\' !!{Style.RESET_ALL}")
    
    png_file = f"{csv_file[0:-4]}.png"

    output_dir = os.path.dirname(csv_file)
    
    if debug:
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} current path: {os.getcwd()}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} repos_path: {Tools.get_repos_path()}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} debug: {debug}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} percentage_mode: {percentage_mode}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} ci_mode: {ci_mode}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} config_file: {config_file}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} csv_file: {csv_file}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} png_file: {png_file}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} output_dir: {output_dir}")

    
    # if we are in ci context:
    if ci_mode:
        output_dir = f'{os.getcwd()}'
        csv_file = f"{output_dir}/{os.path.basename(csv_file)}"
        png_file = f"{output_dir}/{os.path.basename(png_file)}"
        
        if debug:
            print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} we are in CI mode")
            print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} output_dir: {output_dir}")
            print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} csv_file: {csv_file}")
            print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} png_file: {png_file}")
            

    # load config
    conf = {}
    with open(config_file, encoding="utf-8") as f:
        conf = yaml.safe_load(f)
    f.close()


    builder = DatasetBuilder(conf, csv_file)
    
    if horizontal:
        #reverse df rows
        df = builder.get_df()
        df = df.iloc[::-1].reset_index(drop=True)
        plotbar = PlotBarHorizontal(conf, df, builder.get_last_previous_release_tag(), percentage_mode=percentage_mode)
    else:
        plotbar = PlotBar(conf, builder.get_df(), builder.get_last_previous_release_tag(), percentage_mode=percentage_mode)
        
    plotbar.savefig(png_file)

    if not ci_mode:
        plotbar.show()

#------------------------------------------
if __name__ == "__main__":
    main()
