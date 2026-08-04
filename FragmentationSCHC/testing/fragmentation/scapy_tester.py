from scapy.all import *

eth = Ether(src="00:15:5d:02:6e:40", dst="00:00:00:00:00:00")
ipv6 = IPv6(src="2001:470:1f2b:12e::6", dst="2001:4860:4860::8888")
icmp = ICMPv6EchoRequest(data='A'*903)
pkt = eth / ipv6 / icmp

pkt.show()
sendp(pkt, iface="lo")

# waitTime = 150  # Tiempo en segundos entre cada paquete
# N = 5
# ahora = datetime.now()
# formateado = ahora.strftime("Hora de Inicio: %d/%m/%Y %H:%M:%S")
# print(formateado)
# print(f"Enviando {N} paquetes...")
# for i in range(N):
#     print(f"Enviando paquete {i+1} mas una pausa de {waitTime}s...")
#     sendp(pkt, iface="lo")  # Cambia "eth0" por tu interfaz
#     print(f"Esperando {waitTime}s...")
#     time.sleep(waitTime)
# print("¡Envíos completados!")
# ahora = datetime.now()
# formateado = ahora.strftime("Hora de Termino: %d/%m/%Y %H:%M:%S")
# print(formateado)