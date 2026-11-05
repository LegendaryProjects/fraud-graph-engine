# power law distribution means most of the accounts have low transactions while very few 
# have thousands of transaction matching real world scenario

import struct
import random
import time
import numpy as np

NUM_NODES = 500_000
TOTAL_EDGES = 5_000_000
POWER_LAW_ALPHA = 2.1

FRAUD_CYCLE_RATIO = 0.015
FRAUD_CLUSTER_RATIO = 0.010

def generate_dataset(output_filename):
    print(f"Generating {TOTAL_EDGES} transactions for {NUM_NODES} accounts.")

    num_cycle_edges = int(TOTAL_EDGES*FRAUD_CYCLE_RATIO)
    num_cluster_edges = int(TOTAL_EDGES*FRAUD_CLUSTER_RATIO)
    num_normal_edges = TOTAL_EDGES - num_cycle_edges - num_cluster_edges

    transactions = []
    base_timestamp = int(time.time()*1000)

    print(f"Injecting {num_normal_edges} normal edges (Power-law distribution)")

    sources = np.random.zipf(POWER_LAW_ALPHA, num_normal_edges) % NUM_NODES
    destinations = np.random.zipf(POWER_LAW_ALPHA, num_normal_edges) % NUM_NODES

    for i in range(num_normal_edges):
        u = int(sources[i])
        v = int(destinations[i])
        if u == v:
            v = (v+1)%NUM_NODES
        amount = round(random.uniform(10.0, 5000.0), 2)
        transactions.append((u, v, amount))

    print(f"Injecting {num_cycle_edges} money laundernf edges")
    edges_injected = 0
    while edges_injected < num_cycle_edges:
        cycle_length = random.randint(3, 6)
        if edges_injected + cycle_length > num_cycle_edges:
            break

        cycle_nodes = random.sample(range(NUM_NODES), cycle_length)
        amount = round(random.uniform(10000.0, 50000.0), 2)

        for i in range(cycle_length):
            u = cycle_nodes[i]
            v = cycle_nodes[(i+1)%cycle_length]
            transactions.append((u, v, amount))
            edges_injected+=1

    print(f"Injecting {num_cluster_edges} mule cluster edges")
    edges_injected = 0
    while edges_injected < num_cluster_edges:
        cluster_size = random.randint(10, 50)
        if edges_injected + cluster_size > num_cluster_edges:
            break

        hub_node = random.randint(0, NUM_NODES-1)
        mule_nodes = random.sample(range(NUM_NODES), cluster_size)
        amount = round(random.uniform(100.0, 500.0), 2)

        for mule in mule_nodes:
            transactions.append((mule, hub_node, amount))
            edges_injected += 1

    print("Shuffling transaction stream")
    random.shuffle(transactions)

    print(f"Packing and writing to {output_filename}")

    with open(output_filename, 'wb') as f:
        for idx, (u, v, amount) in enumerate(transactions):
            ts = base_timestamp + idx*2
            packed_bytes = struct.pack('<IIdQ', u, v, amount, ts)
            f.write(packed_bytes)

    print(f"Dataset successfully written to {output_filename} ({TOTAL_EDGES*24/(1024*1024):.2f} MB)")

if __name__ == "__main__":
    generate_dataset("transactions.bin")