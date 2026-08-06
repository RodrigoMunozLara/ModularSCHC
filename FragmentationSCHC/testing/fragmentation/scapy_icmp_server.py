#!/usr/bin/env python3
import time
import threading
from scapy.all import *
from scapy.layers.inet6 import IPv6, ICMPv6EchoReply
from scapy.layers.l2 import Ether
from scapy.arch.linux import L2Socket 

# === CONFIGURACIÓN ===
INTERFACE = "lo"
conf.use_pcap = False
conf.L2socket = L2Socket 

# Configura aquí los segundos de retraso antes de responder
TIEMPO_ESPERA_SEGUNDOS = 20.0  
# =====================

# Memoria para el filtro anti-duplicados por número de secuencia
PAQUETES_PROCESADOS = set()

def transmitir_respuesta_tardia(reply_packet, delay, seq, ip_origen):
    """Espera el tiempo configurado en segundo plano y transmite."""
    time.sleep(delay)
    sendp(reply_packet, iface=INTERFACE, verbose=False)
    print(f"[-] Respuesta ICMPv6 (Tipo 129) enviada con éxito a {ip_origen} (SEQ: {seq}) tras {delay}s.\n")

def responder_icmpv6(packet):
    global PAQUETES_PROCESADOS
    
    # Convertimos el paquete detectado a sus bytes puros inmediatamente
    packet_bytes = bytes(packet)
    
    # 1. Validar tamaño mínimo estructural de Capa 2 + IPv6 + ICMPv6
    if len(packet_bytes) < 62:
        return
        
    # 2. FILTRO ANTI-DUPLICADOS (Índice [54] Corregido): El byte 54 indica el tipo ICMPv6.
    # Solo procesamos si es un Echo Request (0x80 = Tipo 128). 
    # Cuando el hilo envíe la respuesta (0x81 = Tipo 129), esta línea la ignorará al instante.
    if packet_bytes[54] != 0x80:
        return

    # Extraer ID y Sequence usando sus desplazamientos de bytes fijos
    echo_id = int.from_bytes(packet_bytes[58:60], byteorder='big')
    echo_seq = int.from_bytes(packet_bytes[60:62], byteorder='big')
    
    # 3. Filtro por secuencia para evitar dobles ejecuciones por ráfaga
    id_unico_paquete = (echo_id, echo_seq)
    if id_unico_paquete in PAQUETES_PROCESADOS:
        return
    PAQUETES_PROCESADOS.add(id_unico_paquete)
    
    if len(PAQUETES_PROCESADOS) > 100:
        PAQUETES_PROCESADOS.clear()
        PAQUETES_PROCESADOS.add(id_unico_paquete)

    # Extraer direcciones IP del mapeo de Scapy
    ip_origen = packet[IPv6].src
    ip_destino = packet[IPv6].dst
    
    # Extraer el payload completo exacto desde el byte 62 hasta el final
    raw_data = packet_bytes[62:]

    print(f"[+] Petición legítima detectada desde {ip_origen} | ID: {echo_id}, SEQ: {echo_seq}")
    print(f"    [i] Programando envío en segundo plano para dentro de {TIEMPO_ESPERA_SEGUNDOS} segundos...")

    # 4. Construir capas básicas de transporte y red
    reply_payload = ICMPv6EchoReply(id=echo_id, seq=echo_seq)
    reply_ip = IPv6(src=ip_destino, dst=ip_origen, hlim=64)
    
    if hasattr(reply_payload, 'cksum'):
        del reply_payload.cksum
    
    # 5. Ensamblar en Capa 2 concatenando los datos puros al final (raw_data)
    # Esto garantiza que el payload vuelva a viajar idéntico sin alteración
    reply_packet = (
        Ether(src="00:00:00:00:00:00", dst="00:00:00:00:00:00", type=0x86dd) / 
        reply_ip / 
        reply_payload / 
        raw_data
    )
    
    # 6. Lanzar en hilo paralelo asíncrono para no bloquear la captura
    hilo_envio = threading.Thread(
        target=transmitir_respuesta_tardia, 
        args=(reply_packet, TIEMPO_ESPERA_SEGUNDOS, echo_seq, ip_origen)
    )
    hilo_envio.start()

print(f"[*] Servidor iniciado en '{INTERFACE}' con extracción por bytes puros y retraso de {TIEMPO_ESPERA_SEGUNDOS}s...")

# Sniffer pasivo en localhost
sniff(iface=INTERFACE, prn=responder_icmpv6, store=False)
