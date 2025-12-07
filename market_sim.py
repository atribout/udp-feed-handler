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

print(f"Market simulator started on {MCAST_PORT}...")
print("Generating volatility and micro-bursts...")

try:
    while True:
        # --- 1. MICRO-BURST SIMULATION ---
        # Send between 10 and 100 orders in a rapid burst
        burst_size = random.randint(10, 100)
        
        for _ in range(burst_size):
            
            # --- 2. PRICE RANDOM WALK ---
            # Price moves by -5, 0, or +5 cents
            move = random.choices([-5, 0, 5], weights=[0.3, 0.4, 0.3])[0]
            current_price += move
            if current_price < 100: current_price = 100 # Price floor

            # --- 3. DECISION: ADD OR CANCEL? ---
            # 80% chance to Add, 20% to Cancel (if we have live orders)
            msg_type = b'A'
            if len(live_orders) > 0 and random.random() < 0.2:
                msg_type = b'C' # Cancel
            
            packet = None
            
            if msg_type == b'A':
                # --- BUILD ADD ORDER ---
                qty = random.randint(1, 100)
                side = b'B' if random.random() > 0.5 else b'S'
                
                packet = struct.pack('=cQiIc', b'A', order_id_counter, current_price, qty, side)
                
                live_orders.append(order_id_counter)
                order_id_counter += 1
                
            else:
                # --- BUILD CANCEL ORDER ---
                target_id = random.choice(live_orders)
                live_orders.remove(target_id)
                
                packet = struct.pack('=cQ', b'C', target_id)

            sock.sendto(packet, (MCAST_GRP, MCAST_PORT))

        # --- 4. PAUSE BETWEEN BURSTS ---
        # Sleep briefly (1ms to 10ms) to let the C++ consumer catch up
        # This creates the "sawtooth" pattern
        time.sleep(random.uniform(0.001, 0.010)) 

        if order_id_counter % 1000 == 0:
            print(f"Stats: {order_id_counter} msgs sent. Current Price: {current_price}")

except KeyboardInterrupt:
    print("\n Simulation stopped.")
    sock.close()