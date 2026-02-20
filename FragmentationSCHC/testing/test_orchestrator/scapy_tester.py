from scapy.all import *

# Capa de enlace (Ethernet)
eth = Ether(src="00:11:22:33:44:55", dst="00:15:5d:02:64:2d")

# Capa de red (IPv6)
ipv6 = IPv6(src="2001:db8::1", dst="fc00:1::1")

# Capa de transporte (ICMPv6 Echo Request)
icmp = ICMPv6EchoRequest(data='A'*900)

# Combinar todas las capas
pkt = eth / ipv6 / icmp

pkt.show()

sendp(pkt, iface="lo")  # Cambia "eth0" por tu interfaz
