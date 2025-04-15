#!/usr/bin/python3
import argparse
import pandas as pd
from datetime import datetime
import os
import sys
import inspect
import subprocess
import pprint
import yaml
import matplotlib.pyplot as plt
# import matplotlib
import numpy as np
from colorama import Fore, Back, Style

class DatasetBuilder:
    """This class collect data compute them and make a csv string.
    csv string can be saved into a file by calling save_into_csv_file
    """
    def __init__(self, conf:dict) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}()")
        self.conf = conf
        self.tags = DatasetBuilder.collect_tags(self.conf['get_tags_command'])
        self.data = self.build_data()
        self.csv_string = self.data_to_csv_string()
        
    #------------------------------------------
    def build_data(self) -> list:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}()")
        data = []
        for i in range(0,len(self.tags)-1):
            if i == 0:
                d = {}
                d.setdefault('tag', self.tags[i])
                d.setdefault('in', 0)
                d.setdefault('out', 0)
                d.setdefault('total', DatasetBuilder.count_all_asserts(self.conf, self.tags[i]))
                data.append(d)
                
            ni = 0
            no = 0
            for pattern in self.conf['in_grep_patterns']:
                command = f"git diff {self.tags[i]} {self.tags[i+1]} | {pattern}"
                ins = subprocess.getoutput(command).splitlines()
                ni += len(ins)
            
            for pattern in self.conf['out_grep_patterns']:
                command = f"git diff {self.tags[i]} {self.tags[i+1]} | {pattern}"
                outs = subprocess.getoutput(command).splitlines()
                no += len(outs)
                
            d = {}
            d.setdefault('tag', self.tags[i+1])
            d.setdefault('in', ni)
            d.setdefault('out', no)
            d.setdefault('total', DatasetBuilder.count_all_asserts(self.conf, self.tags[i+1]))
            data.append(d)
                    
        return data

    #------------------------------------------
    @staticmethod
    def collect_tags(command:str) -> list:
        outputs = subprocess.getoutput(command).splitlines()
        return outputs
    
    @staticmethod
    def count_all_asserts(conf, tag) -> int:
        
        if debug:
            print(f"{Fore.BLUE}-- [debub] {Style.RESET_ALL}{Fore.YELLOW}in count_all_asserts(){Style.RESET_ALL}")
            
        total = 0
        
        # build files selection:
        command = conf['command_total_asserts']
        command = command.replace('<repos_path>', Tools.get_repos_path())
        command = command.replace('<tag>', tag)
        command = command.replace('<assert_selector>', Tools.shape_assert_selector(conf))
        command = command.replace('<file_selector>', Tools.shape_file_selector(conf))
        if debug:
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <repos_path>: {Tools.get_repos_path()}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <assert_selector>: {Tools.shape_assert_selector(conf)}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <tag>: {tag}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} <file_selector>: {Tools.shape_file_selector(conf)}")
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} command: {command}")
            
        
        outputs = subprocess.getoutput(command).splitlines()
        if debug:
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} outputs: {outputs}")
            
        total = int(outputs[-1])
        
        if debug:
            print(f"{Fore.BLUE}-- [debug] {Style.RESET_ALL} total: {total}")
        
        return total


    #------------------------------------------
    def get_csv_string(self) -> str:
        return self.csv_string
    
    #------------------------------------------
    def get_data(self) -> list:
        return self.data

    #------------------------------------------
    def data_to_csv_string(self) -> str:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}()")
        string = f"{self.conf['csv_headers']}\n"
        for line in self.data:
            string += f"{line['tag']},{line['in']},{line['out']},{line['total']}\n"
        return string

    #------------------------------------------
    def get_tags(self) -> list:
        return self.tags

    #------------------------------------------
    def save_into_csv_file(self, csv_file:str) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}()")
        fo = open(os.path.expanduser(csv_file),'w+', encoding="utf-8")
        fo.write(self.get_csv_string())
        fo.close()

        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}csv file {Fore.GREEN}{csv_file}{Style.RESET_ALL} is created")

#----------------END of class DatasetBuilder--------------------------
class Plotbar:
    #------------------------------------------
    def __init__(self, conf:dict, df:object) -> None:
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}in {self.__class__.__name__}.{inspect.currentframe().f_code.co_name}()")
        self.df = df
        self.df.columns = conf['df_columns']
        for col in conf['df_columns'][1:]:
            self.df = self.df[df[col] != '-']
            self.df[col] = pd.to_numeric(df[col])

        if debug:
            print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} df:")
            print(f"{Fore.CYAN}{self.df}{Style.RESET_ALL}")
       
        self.df = self.df.set_index('Version')

        i_version = conf['indices']['version']
        i_new = conf['indices']['new']
        i_removed = conf['indices']['removed']
        i_total = conf['indices']['total']
        
        
        # Adjusting the figure size
        fig, ax = plt.subplots(figsize=(20, 10))

        X_axis = np.arange(len(self.df.index))
        
        bar1 = plt.bar(X_axis - 0.2, df[conf['df_columns'][i_total]], color=conf['serie_colors'][i_total], width = 0.2)
        bar2 = plt.bar(X_axis + 0, df[conf['df_columns'][i_new]], color=conf['serie_colors'][i_new], width = 0.2)
        bar3 = plt.bar(X_axis + 0.2, df[conf['df_columns'][i_removed]], color=conf['serie_colors'][i_removed], width = 0.2)
        
        # Adding a plot title and customizing its font size
        title = conf['title']
        title = title.replace('<repos>', Tools.get_repos())
        plt.title(title, fontsize=20)

        plt.xticks(X_axis, self.df.index)
        # Adding axis labels and customizing their font size
        plt.ylabel(conf['y_label'], fontsize=15)

        plt.legend( (bar1, bar2, bar3), ('All asserts', 'New Asserts', 'Removed Asserts') )
        
        # Display grid
        plt.grid(True)
        
        # Rotaing axis ticks and customizing their font size
        plt.xticks(rotation=30, fontsize=10)

        for index,data in enumerate(df['All']):
            if data > 0 :
                plt.text(x=index-0.2 , y =data+1 , s=f"{data}" , fontdict=dict(fontsize=10), rotation=30)


        for index,data in enumerate(df['In']):
            if data > 0 :
                plt.text(x=index-0.2 , y =data+1 , s=f"{data}" , fontdict=dict(fontsize=10), rotation=30)
            
        for index,data in enumerate(df['Out']):
            if data > 0 :
                plt.text(x=index-0 , y =data+1 , s=f"{data}" , fontdict=dict(fontsize=10), rotation=30)

        # text current date
        today = datetime.now()
        creation_date = f"{today.strftime("%Y-%m-%d %H:%M:%S")}"

        plt.text(x=len(df['All'])+conf['xdate_inc'], y=max(df[conf['df_columns'][i_total]]+300), s=f"{creation_date}", fontdict=dict(fontsize=10), color=conf['date_color'])
       
    #------------------------------------------
    def savefig(self, png_file:str) -> None:
        plt.savefig(png_file)
        if debug:
            print(f"{Fore.BLUE}-- [debug]: {Style.RESET_ALL}png file {Fore.GREEN}{png_file}{Style.RESET_ALL} is created")

    #------------------------------------------
    def show(self) ->None:
        plt.show()

#----------------END of class Plotbar--------------------------
class Tools:
    
    @staticmethod
    def get_repos() -> str:
        return os.path.basename(Tools.get_repos_path())

    @staticmethod
    def get_repos_path() -> str:
        path_elements = os.path.dirname(os.path.realpath(__file__)).split('/')
        repos_path = ''
        for el in path_elements[1:-2]:
            repos_path += f'/{el}'
        return repos_path

    @staticmethod
    def get_root_path() -> str:
        return f"{os.path.dirname(__file__)}/../.."
    
    @staticmethod
    def shape_file_selector(conf: dict) -> str :
        string =''
        for ext in conf['extensions_accepted']:
            string += f"\\{ext}:\\|"
        return f"{string[:-2]}"
    
    @staticmethod
    def shape_assert_selector(conf:dict) -> str :
        string =''
        for p in conf['assert_patterns']:
            string += f"{p}\\|"
        return f"'{string[:-2]}'"
    
        
#----------------END of class Tools--------------------------


def main():
    global debug
    parser = argparse.ArgumentParser(description='search assert')
    parser.add_argument('-d','--debug', action='store_true', help='Run in debug mode')
    parser.add_argument('-ci','--ci', action='store_true', help='ci mode to change output path')
    parser.add_argument("-c", "--config", help="yaml config file", type=str, nargs='?', default='./config_plot_asserts_in_out.yml')
    parser.add_argument("-o", "--output_csv", help="csv output file", type=str, nargs='?', default='./asserts_in_out.csv')

    args = parser.parse_args()
    debug = getattr(args,'debug')
    ci_mode = getattr(args,'ci')
    config_file = getattr(args,'config')
    csv_file = getattr(args,'output_csv')
    
    if csv_file[-4:] != '.csv':
        print(f"{Fore.RED}ERROR: output_csv {csv_file} must end by \'.csv\' !!{Style.RESET_ALL}")
    
    png_file = f"{csv_file[0:-4]}.png"

    output_dir = os.path.dirname(csv_file)
    
    if debug:
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} current path: {os.getcwd()}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} repos_path: {Tools.get_repos_path()}")
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} debug: {debug}")
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

    builder = DatasetBuilder(conf)
        
    tags = builder.get_tags()
    if debug:
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} tags list:")
        print(Fore.CYAN)
        pprint.pprint(tags)
        print(Style.RESET_ALL)
        
    csv_string = builder.get_csv_string()
    if debug:
        print(f"{Fore.BLUE}-- [debug]:{Style.RESET_ALL} csv_string:")
        print(Fore.CYAN)
        print(f"{Fore.CYAN}{csv_string}{Style.RESET_ALL}")
        print(Style.RESET_ALL)
    
    # save csv_file
    builder.save_into_csv_file(csv_file)

    # make panda dataframe
    df = pd.read_csv(csv_file)
    plotbar = Plotbar(conf, df)
    plotbar.savefig(png_file)

    if not ci_mode:
        plotbar.show()

#------------------------------------------
if __name__ == "__main__":
    main()
