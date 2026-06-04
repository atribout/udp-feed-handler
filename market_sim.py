import socket
import struct
import time
import random

MCAST_GRP = '127.0.0.1'
MCAST_PORT = 1234

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

# Keep track of live orders to enable cancellations
live_orders = {} 
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
            if len(live_orders) > 0:
                rand_val = random.random()
                if rand_val < 0.15:
                    msg_type = b'C' # 15% Cancel
                elif rand_val < 0.35:
                    msg_type = b'E' # 20% Execute
            
            packet = None
            
            if msg_type == b'A':
                qty = random.randint(1, 100)
                side = b'B' if random.random() > 0.5 else b'S'
                
                # Big-Endian : >
                packet = struct.pack('>QcQiIc', seq_num, b'A', order_id_counter, current_price, qty, side)
                live_orders[order_id_counter] = qty
                order_id_counter += 1
                
            elif msg_type == b'C':
                target_id = random.choice(list(live_orders.keys()))
                del live_orders[target_id]
                
                packet = struct.pack('>QcQ', seq_num, b'C', target_id)

            elif msg_type == b'E':
                target_id = random.choice(list(live_orders.keys()))
                remaining_qty = live_orders[target_id]

                exec_qty = random.randint(1, remaining_qty)
                live_orders[target_id] -= exec_qty

                if live_orders[target_id] <= 0:
                    del live_orders[target_id]

                packet = struct.pack('>QcQI', seq_num, b'E', target_id, exec_qty)

            sock.sendto(packet, (MCAST_GRP, MCAST_PORT))
            seq_num += 1

        # --- PAUSE BETWEEN BURSTS ---
        # Sleep briefly (1ms to 10ms) to let the C++ consumer catch up
        # This creates the "sawtooth" pattern
        time.sleep(random.uniform(0.001, 0.010)) 

        if seq_num % 1000 == 0:
            print(f"Stats: {seq_num} msgs sent. Current Price: {current_price}")

except KeyboardInterrupt:
    print("\n Simulation stopped.")
    sock.close()