import socket
import struct
import time
import random

MCAST_GRP = '127.0.0.1'
MCAST_PORT = 1234

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

# Keep track of live orders to enable cancellations
live_orders = [] 
current_price = 10000 # Price in cents (100.00)
order_id_counter = 1
seq_num = 1

print(f"Market simulator started on {MCAST_PORT}...")

try:
    while True:
        burst_size = random.randint(10, 100)
        
        for _ in range(burst_size):
            move = random.choices([-5, 0, 5], weights=[0.3, 0.4, 0.3])[0]
            current_price += move
            if current_price < 100: current_price = 100 # Price floor

            msg_type = b'A'
            if len(live_orders) > 0 and random.random() < 0.2:
                msg_type = b'C'
            
            packet = None
            
            if msg_type == b'A':
                qty = random.randint(1, 100)
                side = b'B' if random.random() > 0.5 else b'S'
                
                packet = struct.pack('=QcQiIc', seq_num, b'A', order_id_counter, current_price, qty, side)
                
                live_orders.append(order_id_counter)
                order_id_counter += 1
                
            else:
                target_id = random.choice(live_orders)
                live_orders.remove(target_id)
                
                packet = struct.pack('=QcQ', seq_num, b'C', target_id)

            sock.sendto(packet, (MCAST_GRP, MCAST_PORT))
            seq_num += 1

        # --- 4. PAUSE BETWEEN BURSTS ---
        # Sleep briefly (1ms to 10ms) to let the C++ consumer catch up
        # This creates the "sawtooth" pattern
        time.sleep(random.uniform(0.001, 0.010)) 

        if seq_num % 1000 == 0:
            print(f"Stats: {seq_num} msgs sent. Current Price: {current_price}")

except KeyboardInterrupt:
    print("\n Simulation stopped.")
    sock.close()