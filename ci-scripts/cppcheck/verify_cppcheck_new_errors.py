#!/usr/bin/env python3

# pylint: disable=invalid-name
# pylint: disable=consider-using-with

"""Import needed modules"""
import sys
import xml.etree.ElementTree as ET

# retrieve input variables
input_file = sys.argv[1]

# extract xml
input_dict = ET.parse(input_file).getroot()

if int(input_dict.attrib["errors"]) == 0 and int(input_dict.attrib["failures"]) == 0:
    print("No Errors or Failures dectected!")
    sys.exit(0)
else:
    print("Errors found!")
    print(f"Found {input_dict.attrib['errors']} new errors!")
    sys.exit(1)
