from scapy.all import *

# Capa de enlace (Ethernet)
eth = Ether(src="00:15:5d:02:6e:40", dst="00:00:00:00:00:00")

# Capa de red (IPv6)
#ipv6 = IPv6(src="2001:db8::1", dst="fc00:1::1")
ipv6 = IPv6(src="2001:470:1f2b:12e::6", dst="2001:470:1f2a:12e::1")
# Capa de transporte (ICMPv6 Echo Request)
icmp = ICMPv6EchoRequest(data='A'*903)

# Combinar todas las capas
pkt = eth / ipv6 / icmp

pkt.show()

sendp(pkt, iface="lo")  # Cambia "eth0" por tu interfaz
