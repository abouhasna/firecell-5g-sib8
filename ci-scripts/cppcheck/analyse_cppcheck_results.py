#!/usr/bin/env python3

# pylint: disable=invalid-name
# pylint: disable=consider-using-with

"""Import needed modules"""
import sys
import xml.etree.ElementTree as ET
import copy

# retrieve input variables
input_file = sys.argv[1]
template = sys.argv[2]
output_file = sys.argv[3]

# extract both xml
input_dict = ET.parse(input_file).getroot()
template_dict = ET.parse(template).getroot()

output_dict = copy.deepcopy(input_dict)

# we go the loop in reverse order because the output_dic gets updated "in real time"
# otherwise the ids would not point to the same element anymore between output_dict and input_dict
for i in range((len(input_dict[1]) - 1), -1, -1):
    for j in range((len(template_dict[1]) - 1), -1, -1):
        if ET.tostring(input_dict[1][i]) == ET.tostring(template_dict[1][j]):
            # print(i, j)
            # print(input_dict[1][i].attrib)
            # print(template_dict[1][j].attrib)
            output_dict[1].remove(output_dict[1][i])
            break

### write xml to file
tree = ET.ElementTree(output_dict)
tree.write(output_file, encoding="utf-8", xml_declaration=True)
