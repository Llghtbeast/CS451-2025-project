#!/usr/bin/env python3

import sys
import argparse
from collections import defaultdict
from pathlib import Path


class FIFOURBValidator:
    def __init__(self, output_dir, num_processes, failed_processes):
        self.output_dir = Path(output_dir)
        self.num_processes = num_processes
        self.failed_processes = set(failed_processes)
        self.broadcasts = defaultdict(list)  # process_id -> [message_ids]
        self.deliveries = defaultdict(lambda: defaultdict(list))  # process_id -> sender_id -> [message_ids]
        
    def parse_output_files(self):
        """Parse all output files and extract broadcasts and deliveries"""
        print("\nParsing output files...")
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
        
        # Print statistics
        total_broadcasts = sum(len(msgs) for msgs in self.broadcasts.values())
        total_deliveries = sum(
            sum(len(msgs) for msgs in sender_msgs.values())
            for sender_msgs in self.deliveries.values()
        )
        print(f"Total broadcasts: {total_broadcasts}")
        print(f"Total deliveries: {total_deliveries}")
    
    def verify_validity(self):
        """URB Validity: If a correct process broadcasts m, then it eventually delivers m"""
        print("\n" + "=" * 80)
        print("Checking VALIDITY property...")
        print("Property: If a correct process broadcasts m, then it eventually delivers m")
        violations = []
        
        correct_processes = set(range(1, self.num_processes + 1)) - self.failed_processes
        
        for process_id in correct_processes:
            broadcasted = set(self.broadcasts[process_id])
            delivered = set(self.deliveries[process_id][process_id])
            
            not_delivered = broadcasted - delivered
            if not_delivered:
                violations.append(
                    f"Process {process_id} broadcast {len(not_delivered)} messages but didn't deliver them. "
                    f"Examples: {sorted(not_delivered)[:10]}"
                )
        
        if violations:
            print("VALIDITY VIOLATED:")
            for v in violations:
                print(f"  - {v}")
            return False
        else:
            print("VALIDITY satisfied")
            return True
    
    def verify_no_duplication(self):
        """URB No Duplication: No message is delivered more than once"""
        print("\n" + "=" * 80)
        print("Checking NO DUPLICATION property...")
        print("Property: No message is delivered more than once")
        violations = []
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, messages in self.deliveries[process_id].items():
                seen = {}
                for idx, msg_id in enumerate(messages):
                    if msg_id in seen:
                        violations.append(
                            f"Process {process_id} delivered ({sender_id}, {msg_id}) multiple times "
                            f"(at positions {seen[msg_id]} and {idx})"
                        )
                    else:
                        seen[msg_id] = idx
        
        if violations:
            print("NO DUPLICATION VIOLATED:")
            for v in violations[:10]:  # Show first 10
                print(f"  - {v}")
            if len(violations) > 10:
                print(f"  ... and {len(violations) - 10} more violations")
            return False
        else:
            print("NO DUPLICATION satisfied")
            return True
    
    def verify_no_creation(self):
        """URB No Creation: If a process delivers m with sender s, then s previously broadcast m"""
        print("\n" + "=" * 80)
        print("Checking NO CREATION property...")
        print("Property: If a process delivers m with sender s, then s previously broadcast m")
        violations = []
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, delivered_msgs in self.deliveries[process_id].items():
                broadcasted = set(self.broadcasts[sender_id])
                
                for msg_id in delivered_msgs:
                    if msg_id not in broadcasted:
                        violations.append(
                            f"Process {process_id} delivered ({sender_id}, {msg_id}) "
                            f"but process {sender_id} never broadcast message {msg_id}"
                        )
        
        if violations:
            print("NO CREATION VIOLATED:")
            for v in violations[:10]:
                print(f"  - {v}")
            if len(violations) > 10:
                print(f"  ... and {len(violations) - 10} more violations")
            return False
        else:
            print("NO CREATION satisfied")
            return True
    
    def verify_uniform_agreement(self):
        """URB Uniform Agreement: If a process delivers m, then all correct processes eventually deliver m"""
        print("\n" + "=" * 80)
        print("Checking UNIFORM AGREEMENT property...")
        print("Property: If a process delivers m, then all correct processes eventually deliver m")
        violations = []
        
        correct_processes = set(range(1, self.num_processes + 1)) - self.failed_processes
        
        # Collect all messages delivered by any process
        all_delivered = defaultdict(set)  # sender_id -> set of msg_ids
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, messages in self.deliveries[process_id].items():
                all_delivered[sender_id].update(messages)
        
        # Check that all correct processes delivered these messages
        for sender_id, msg_ids in all_delivered.items():
            for msg_id in msg_ids:
                delivering_correct = [p for p in correct_processes 
                                     if msg_id in self.deliveries[p][sender_id]]
                
                if len(delivering_correct) < len(correct_processes):
                    missing = correct_processes - set(delivering_correct)
                    violations.append(
                        f"Message ({sender_id}, {msg_id}) delivered by {len(delivering_correct)}/{len(correct_processes)} "
                        f"correct processes. Missing: {sorted(missing)[:5]}"
                    )
        
        if violations:
            print("UNIFORM AGREEMENT VIOLATED:")
            for v in violations[:10]:
                print(f"  - {v}")
            if len(violations) > 10:
                print(f"  ... and {len(violations) - 10} more violations")
            return False
        else:
            print("UNIFORM AGREEMENT satisfied")
            return True
    
    def verify_fifo_order(self):
        """FIFO Order: Messages from the same sender are delivered in the order they were broadcast"""
        print("\n" + "=" * 80)
        print("Checking FIFO ORDER property...")
        print("Property: Messages from the same sender are delivered in the order they were broadcast")
        violations = []
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, delivered_msgs in self.deliveries[process_id].items():
                # Check if delivered messages are in ascending order
                for i in range(len(delivered_msgs) - 1):
                    if delivered_msgs[i] > delivered_msgs[i + 1]:
                        violations.append(
                            f"Process {process_id} delivered messages from process {sender_id} out of order: "
                            f"message {delivered_msgs[i]} delivered before message {delivered_msgs[i + 1]}"
                        )
                        break  # One violation per (process, sender) pair is enough
        
        if violations:
            print("FIFO ORDER VIOLATED:")
            for v in violations[:10]:
                print(f"  - {v}")
            if len(violations) > 10:
                print(f"  ... and {len(violations) - 10} more violations")
            return False
        else:
            print("FIFO ORDER satisfied")
            return True
    
    def run_validation(self):
        """Run all validation checks"""
        print("\n" + "=" * 80)
        print("STARTING VALIDATION")
        print("=" * 80)
        
        results = {
            "Validity": self.verify_validity(),
            "No Duplication": self.verify_no_duplication(),
            "No Creation": self.verify_no_creation(),
            "Uniform Agreement": self.verify_uniform_agreement(),
            "FIFO Order": self.verify_fifo_order()
        }
        
        print("\n" + "=" * 80)
        print("VALIDATION SUMMARY")
        print("=" * 80)
        for prop, passed in results.items():
            status = "PASS" if passed else "FAIL"
            print(f"{prop:20s}: {status}")
        
        all_passed = all(results.values())
        print("=" * 80)
        if all_passed:
            print("ALL CHECKS PASSED!")
        else:
            print("SOME CHECKS FAILED")
        print("=" * 80)
        
        return all_passed


def main():
    parser = argparse.ArgumentParser(
        description='Validate FIFO Uniform Reliable Broadcast output files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # No failed processes
  python3 validate_fifo.py -l ../test_output/ -p 10
  
  # With failed processes 3 and 7
  python3 validate_fifo.py -l ../test_output/ -p 10 -f 3 7
  
  # With single failed process
  python3 validate_fifo.py -l ../test_output/ -p 10 -f 5
        '''
    )
    
    parser.add_argument('-l', '--log-dir', required=True, help='Output directory containing proc*.output files')
    parser.add_argument('-p', '--processes', type=int, required=True, help='Total number of processes')
    parser.add_argument('-f', '--failed', nargs='*', type=int, default=[], help='List of failed process IDs (space-separated)')
    
    args = parser.parse_args()
    
    # Validate failed process IDs
    for proc_id in args.failed:
        if proc_id < 1 or proc_id > args.processes:
            print(f"ERROR: Failed process ID {proc_id} is out of range [1, {args.processes}]")
            sys.exit(1)
    
    # Create validator
    validator = FIFOURBValidator(args.log_dir, args.processes, args.failed)
    
    # Parse output files
    validator.parse_output_files()
    
    # Run validation
    all_passed = validator.run_validation()
    
    # Exit with appropriate code
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()