import socket
import struct
import time
import random

MCAST_GRP = '127.0.0.1'
MCAST_PORT = 1234

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

order_id = 1

try:
    while True:
        price = random.randint(95, 105)
        qty = random.randint(1,100)
        side = b'B' if random.random() > 0.5 else b'S'
        msg_type = b'A'

        packet = struct.pack('=cQiIc', msg_type, order_id, price, qty, side)

        sock.sendto(packet, (MCAST_GRP, MCAST_PORT))

        if order_id % 10 == 0:
             print(f"Sent Order #{order_id}: {qty} @ {price} ({side.decode()})")

        order_id +=1

except KeyboardInterrupt:
        print("Halted.")