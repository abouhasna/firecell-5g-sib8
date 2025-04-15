#!/usr/bin/env python3

# pylint: disable=invalid-name
# pylint: disable=consider-using-with
# pylint: disable=line-too-long


"""Import needed modules"""
import sys
import re
import os
import junitparser

input_file = sys.argv[1]
output_file = sys.argv[2]

TTCN_TEST = os.getenv("TTCN_TEST")
allowed_time_process_all_with_statistics = 900  # 15 minutes
allowed_time_process_all_without_statistics = 660  # 11 minutes

# file looks like this:
# Time processed Pyshark:  0.0006051063537597656
# Time output:  1.1456067562103271
#  TOTAL: 192644 MAC: 2205 PDCP: 32 RLC: 63
# Infos statistic for packets count:
#  Numbers of packet NO_EVENT_NAME: 144085
#  Numbers of packet BCCH_BCH_Message: 53
#  Numbers of packet BCCH_DL_SCH_Message: 159
#  Numbers of packet P7_CELL_SRCH_IND: 7620
#  Numbers of packet NFAPI_TX_REQUEST: 13501
#  Numbers of packet NFAPI_DL_CONFIG_REQUEST: 13540
#  Numbers of packet NFAPI_RACH_INDICATION: 7
#  Numbers of packet NFAPI_UL_CONFIG_REQUEST: 2497
#  Numbers of packet NFAPI_CRC_INDICATION: 2160
#  Numbers of packet NFAPI_RX_ULSCH_INDICATION: 2160
#  Numbers of packet LTE_MAC_UL_PDU: 2160
#  Numbers of packet UL_CCCH_Message: 7
#  Numbers of packet DL_CCCH_Message: 7
#  Numbers of packet LTE_MAC_DL_PDU: 41
#  Numbers of packet NFAPI_HARQ_INDICATION: 35
#  Numbers of packet NFAPI_HI_DCI0_REQUEST: 4471
#  Numbers of packet UL_RLC_AM_PDU: 28
#  Numbers of packet UL_LTE_PDCP_PDU: 18
#  Numbers of packet DL_RLC_AM_PDU: 35
#  Numbers of packet UL_DCCH_Message: 18
#  Numbers of packet DL_DCCH_Message: 17
#  Numbers of packet DL_LTE_PDCP_PDU: 14
#  Numbers of packet NFAPI_RX_SR_INDICATION: 3
#  Numbers of packet PCCH_Message: 4
#  Numbers of packet LTE_MAC_PCCH_PDU: 4
# Time process all:  263.000125169754

# open file and parse its content
with open(input_file, "r+", encoding="utf-8") as file:
    for line in file:
        regexp_1 = re.search(r"Time process all:  (\d.*)", line)
        if regexp_1 is not None:
            break
with open(input_file, "r+", encoding="utf-8") as file:
    for line in file:
        regexp_2 = re.search(r" TOTAL: (\d*) MAC: (\d*) PDCP: (\d*) RLC: (\d*)", line)
        if regexp_2 is not None:
            break
# retrieve packet counts
packet_counts_dict = {}
with open(input_file, "r+", encoding="utf-8") as file:
    for line in file:
        regexp_3 = re.search(r" Numbers of packet (\w*): (\d*)", line)
        if regexp_3 is not None:
            packet_counts_dict[regexp_3[1]] = int(regexp_3[2])

time_process_all = float(regexp_1[1])
total = int(regexp_2[1])
mac = int(regexp_2[2])
pdcp = int(regexp_2[3])
rlc = int(regexp_2[4])

# analyse results
# Our conditions:
# total, mac, pdcp, rlc are all strictly positive
# time_process_all is under allowed_time_process_all seconds
# all values for the number of packets are strictly positive


# check status here (sucess boolean)
sucess = True

if not packet_counts_dict:
    allowed_time_process = allowed_time_process_all_without_statistics
else:
    allowed_time_process = allowed_time_process_all_with_statistics


if (
    time_process_all >= allowed_time_process
    or total <= 0
    or mac <= 0
    or pdcp <= 0
    or rlc <= 0
):
    sucess = False
for key, value in packet_counts_dict.items():
    if value <= 0:
        sucess = False


# display result / generate report
suite = junitparser.TestSuite(f"ttcn_lttng_{TTCN_TEST}")
case = junitparser.TestCase(f"{TTCN_TEST}_babeltrace_log_analysis")

if not sucess:
    print(
        f"Time process all is over {allowed_time_process} seconds or one of total, mac, pdcp, rlc or any packet count is not striclty positive! \n\
Time process all is {time_process_all} \n\
TOTAL is {total} \n\
MAC is {mac} \n\
PDCP is {pdcp} \n\
RLC is {rlc} \n\
Please also check each packet counts!"
    )
    case.result = [junitparser.Error()]
    case.time = round(time_process_all, 0)
    suite.add_testcase(case)
    # Add suite to JunitXml
    xml = junitparser.JUnitXml()
    xml.add_testsuite(suite)
    xml.write(output_file)
    sys.exit(1)

else:
    print("All check passed")
    case.result = [junitparser]
    case.time = round(time_process_all, 0)
    suite.add_testcase(case)
    # Add suite to JunitXml
    xml = junitparser.JUnitXml()
    xml.add_testsuite(suite)
    xml.write(output_file)
    sys.exit(0)
