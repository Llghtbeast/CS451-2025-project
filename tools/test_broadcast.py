#!/usr/bin/env python3

import sys
import argparse
from collections import defaultdict
from pathlib import Path
from tqdm import tqdm


class BroadcastValidator:
    def __init__(self, output_dir, num_processes, failed_processes, mode):
        self.output_dir = Path(output_dir)
        self.num_processes = num_processes
        self.failed_processes = set(failed_processes)
        self.mode = mode  # 'beb' or 'fifo'
        self.broadcasts = defaultdict(list)  # process_id -> [message_ids]
        self.deliveries = defaultdict(lambda: defaultdict(list))  # process_id -> sender_id -> [message_ids]
        
    def parse_output_files(self):
        """Parse all output files and extract broadcasts and deliveries"""
        print(f"\nParsing output files for {self.mode.upper()}...")
        print(f"Output directory: {self.output_dir}")
        print(f"Number of processes: {self.num_processes}")
        print(f"Failed processes: {sorted(self.failed_processes) if self.failed_processes else 'None'}")
        print()
        
        for i in range(1, self.num_processes + 1):
            output_file = self.output_dir / f"proc{i:02d}.output"
            
            if not output_file.exists():
                print(f"WARNING: Output file {output_file} not found")
                continue
            
            with open(output_file, 'r') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if not line:
                        continue
                    
                    # Parse broadcast: "b i"
                    if line.startswith('b '):
                        parts = line.split()
                        if len(parts) == 2:
                            try:
                                msg_id = int(parts[1])
                                self.broadcasts[i].append(msg_id)
                            except ValueError:
                                print(f"WARNING: Invalid broadcast in proc{i:02d}.output line {line_num}: {line}")
                    
                    # Parse delivery: "d p i"
                    elif line.startswith('d '):
                        parts = line.split()
                        if len(parts) == 3:
                            try:
                                sender = int(parts[1])
                                msg_id = int(parts[2])
                                self.deliveries[i][sender].append(msg_id)
                            except ValueError:
                                print(f"WARNING: Invalid delivery in proc{i:02d}.output line {line_num}: {line}")
        
        print(f"Successfully parsed {len(self.broadcasts)} process output files")
        
        total_broadcasts = sum(len(msgs) for msgs in self.broadcasts.values())
        total_deliveries = sum(
            sum(len(msgs) for msgs in sender_msgs.values())
            for sender_msgs in self.deliveries.values()
        )
        print(f"Total broadcasts: {total_broadcasts}")
        print(f"Total deliveries: {total_deliveries}")
    
    def verify_validity(self):
        """Validity: If a correct process broadcasts m, then every correct process delivers m"""
        print("\nChecking VALIDITY property...")
        violations = []
        correct_processes = set(range(1, self.num_processes + 1)) - self.failed_processes
        
        for sender_id in correct_processes:
            broadcasted = set(self.broadcasts[sender_id])
            
            for receiver_id in correct_processes:
                delivered = set(self.deliveries[sender_id][receiver_id])
                not_delivered = broadcasted - delivered
            
                if not_delivered:
                  for i in range(min(len(not_delivered), 5)):
                    violations.append(
                        f"Correct process {receiver_id} failed to deliver msg {not_delivered.pop()} "
                        f"broadcast by correct process {sender_id}"
                    )
        
        if violations:
            print("VALIDITY VIOLATED:")
            for v in violations[:10]: print(f"  - {v}")
            return False
        print("VALIDITY satisfied")
        return True

    def verify_no_duplication(self):
        """No Duplication: No message is delivered more than once"""
        print("\nChecking NO DUPLICATION property...")
        violations = []
        for process_id in range(1, self.num_processes + 1):
            for sender_id, messages in self.deliveries[process_id].items():
                seen = set()
                for msg_id in messages:
                    if msg_id in seen:
                        violations.append(f"Process {process_id} delivered ({sender_id}, {msg_id}) multiple times")
                        break
                    seen.add(msg_id)
        
        if violations:
            print("NO DUPLICATION VIOLATED:")
            for v in violations[:10]: print(f"  - {v}")
            return False
        print("NO DUPLICATION satisfied")
        return True

    def verify_no_creation(self):
        """No Creation: If a process delivers m with sender s, then s previously broadcast m"""
        print("\nChecking NO CREATION property...")
        violations = []
        for process_id in range(1, self.num_processes + 1):
            for sender_id, delivered_msgs in self.deliveries[process_id].items():
                broadcasted = set(self.broadcasts[sender_id])
                for msg_id in delivered_msgs:
                    if msg_id not in broadcasted:
                        violations.append(f"Process {process_id} delivered phantom msg ({sender_id}, {msg_id})")
        
        if violations:
            print("NO CREATION VIOLATED:")
            for v in violations[:10]: print(f"  - {v}")
            return False
        print("NO CREATION satisfied")
        return True

    def verify_uniform_agreement(self):
        """Uniform Agreement: If a process delivers m, then all correct processes deliver m"""
        print("\nChecking UNIFORM AGREEMENT property...")
        violations = []
        correct_processes = set(range(1, self.num_processes + 1)) - self.failed_processes
        
        all_delivered = defaultdict(set)
        for process_id in range(1, self.num_processes + 1):
            for sender_id, messages in self.deliveries[process_id].items():
                all_delivered[sender_id].update(messages)
        
        for sender_id, msg_ids in all_delivered.items():
            for msg_id in tqdm(msg_ids, leave=False):
                for cp in correct_processes:
                    if msg_id not in self.deliveries[cp][sender_id]:
                        violations.append(f"Msg ({sender_id}, {msg_id}) not delivered by correct process {cp}")
                        if len(violations) >= 5: break
        
        if violations:
            print("UNIFORM AGREEMENT VIOLATED:")
            for v in violations[:10]: print(f"  - {v}")
            return False
        print("UNIFORM AGREEMENT satisfied")
        return True

    def verify_fifo_order(self):
        """FIFO Order: Messages from the same sender are delivered in broadcast order"""
        print("\nChecking FIFO ORDER property...")
        violations = []
        for process_id in range(1, self.num_processes + 1):
            for sender_id, delivered_msgs in self.deliveries[process_id].items():
                for i in range(len(delivered_msgs) - 1):
                    if delivered_msgs[i] > delivered_msgs[i + 1]:
                        violations.append(f"Process {process_id} delivered {sender_id}'s msgs out of order: {delivered_msgs[i]} before {delivered_msgs[i+1]}")
                        break
        
        if violations:
            print("FIFO ORDER VIOLATED:")
            for v in violations[:10]: print(f"  - {v}")
            return False
        print("FIFO ORDER satisfied")
        return True

    def run_validation(self):
        """Run validation checks based on mode"""
        results = {
            "Validity": self.verify_validity(),
            "No Duplication": self.verify_no_duplication(),
            "No Creation": self.verify_no_creation(),
        }
        
        if self.mode == 'fifo':
            results["Uniform Agreement"] = self.verify_uniform_agreement()
            results["FIFO Order"] = self.verify_fifo_order()
        
        print("\nSUMMARY")
        print("=" * 80)
        for prop, passed in results.items():
            status = "PASS" if passed else "FAIL"
            print(f"{prop:20s}: {status}")
        
        all_passed = all(results.values())
        print("=" * 80)
        return all_passed


def main():
    parser = argparse.ArgumentParser(description='Validate Broadcast output files')
    
    # Add positional argument for mode
    parser.add_argument('mode', choices=['beb', 'fifo'], help='Broadcast primitive to test')
    parser.add_argument('-l', '--log-dir', required=True, help='Output directory')
    parser.add_argument('-p', '--processes', type=int, required=True, help='Total processes')
    parser.add_argument('-f', '--failed', nargs='*', type=int, default=[], help='Failed process IDs')
    
    args = parser.parse_args()
    
    for proc_id in args.failed:
        if proc_id < 1 or proc_id > args.processes:
            print(f"ERROR: Failed process ID {proc_id} is out of range")
            sys.exit(1)
    
    validator = BroadcastValidator(args.log_dir, args.processes, args.failed, args.mode)
    validator.parse_output_files()
    all_passed = validator.run_validation()
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()