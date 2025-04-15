#!/usr/bin/env python3

# pylint: disable=invalid-name
# pylint: disable=consider-using-with

"""Import needed modules"""
import os
import tarfile
import re
from dateutil import parser
import junitparser

ttcn_tests = []

# unarchive the artifacts in each TTCN job directory
for file in os.listdir():
    if file.endswith(".tar.gz") and file.startswith("logs_"):
        ttcn_testcase = re.match(r"logs\_(TC[0-9_a-z]*)\.tar\.gz", file)[1]
        ttcn_tests.append(ttcn_testcase)
        tar = tarfile.open(file)
        os.makedirs(ttcn_testcase)
        tar.extractall(path=ttcn_testcase)

# print(ttcn_tests)
success_string = "1 pass (100.00 %)"

# Create suite and add cases
suite = junitparser.TestSuite("ttcn")

# Loop over testcases and add them to the suite
for ttcn_testcase in ttcn_tests:
    case = junitparser.TestCase(ttcn_testcase)
    for fname in os.listdir(ttcn_testcase):
        if fname.endswith("MERGED.log"):
            # Full path
            with open(
                ttcn_testcase + "/" + fname,
                "r",
                encoding="utf-8",
            ) as f:
                # Retrieve duration
                # retrieve first and last line of the log
                date_time_start_string = " ".join(f.readline().strip("\n").split()[:2])
                for line in f:
                    pass
                date_time_end_string = " ".join(line.split()[:2])
                date_time_start = parser.parse(date_time_start_string)
                date_time_end = parser.parse(date_time_end_string)
                duration = (date_time_end - date_time_start).total_seconds()
                # check result and add to testsuite
                with open(
                    ttcn_testcase + "/" + fname,
                    "r",
                    encoding="utf-8",
                ) as f:
                    if success_string in f.read():
                        # TTCN test was sucessfull, add it to the test suite
                        print(f"Test {ttcn_testcase} passed and took {duration}")
                        case.result = [junitparser]
                        case.time = duration
                    else:
                        # Add TTCN test as errored to the test suite
                        print(
                            f"Test {ttcn_testcase} did not passed and took {duration}"
                        )
                        case.result = [junitparser.Error()]
                        case.time = duration
                    f.close()
    suite.add_testcase(case)

# Add suite to JunitXml
xml = junitparser.JUnitXml()
xml.add_testsuite(suite)
xml.write("ttcn-report.xml")
