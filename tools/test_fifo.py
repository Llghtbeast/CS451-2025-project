#!/usr/bin/env python3

import subprocess
import sys
import os
import re
from collections import defaultdict
from pathlib import Path

class FIFOURBVerifier:
    def __init__(self, output_dir, num_processes):
        self.output_dir = Path(output_dir)
        self.num_processes = num_processes
        self.failed_processes = set()
        self.broadcasts = defaultdict(list)  # process_id -> [message_ids]
        self.deliveries = defaultdict(lambda: defaultdict(list))  # process_id -> sender_id -> [message_ids]
        
    def run_stress_test(self, stress_cmd):
        """Run the stress test and capture output"""
        print(f"Running stress test: {' '.join(stress_cmd)}")
        print("=" * 80)
        
        try:
            result = subprocess.run(
                stress_cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )
            
            print("STDOUT:")
            print(result.stdout)
            print("\nSTDERR:")
            print(result.stderr)
            print("=" * 80)
            
            # Extract failed processes from output
            self._extract_failed_processes(result.stdout)
            self._extract_failed_processes(result.stderr)
            
            return result.returncode == 0
            
        except subprocess.TimeoutExpired:
            print("ERROR: Stress test timed out!")
            return False
        except Exception as e:
            print(f"ERROR: Failed to run stress test: {e}")
            return False
    
    def _extract_failed_processes(self, output):
        """Extract process IDs that received SIGTERM"""
        pattern = r"Sending SIGTERM to process (\d+)"
        matches = re.findall(pattern, output)
        for match in matches:
            process_id = int(match)
            self.failed_processes.add(process_id)
            print(f"Detected failed process: {process_id}")
    
    def parse_output_files(self):
        """Parse all output files and extract broadcasts and deliveries"""
        print("\nParsing output files...")
        
        for i in range(1, self.num_processes + 1):
            output_file = self.output_dir / f"{i}.output"
            
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
                                print(f"WARNING: Invalid broadcast in {i}.output line {line_num}: {line}")
                    
                    # Parse delivery: "d p i"
                    elif line.startswith('d '):
                        parts = line.split()
                        if len(parts) == 3:
                            try:
                                sender = int(parts[1])
                                msg_id = int(parts[2])
                                self.deliveries[i][sender].append(msg_id)
                            except ValueError:
                                print(f"WARNING: Invalid delivery in {i}.output line {line_num}: {line}")
        
        print(f"Parsed {len(self.broadcasts)} process output files")
        print(f"Failed processes: {sorted(self.failed_processes) if self.failed_processes else 'None'}")
    
    def verify_validity(self):
        """URB Validity: If a correct process broadcasts m, then it eventually delivers m"""
        print("\n" + "=" * 80)
        print("Checking VALIDITY property...")
        violations = []
        
        correct_processes = set(range(1, self.num_processes + 1)) - self.failed_processes
        
        for process_id in correct_processes:
            broadcasted = set(self.broadcasts[process_id])
            delivered = set(self.deliveries[process_id][process_id])
            
            not_delivered = broadcasted - delivered
            if not_delivered:
                violations.append(f"Process {process_id} broadcast but didn't deliver: {sorted(not_delivered)[:10]}")
        
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
        violations = []
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, messages in self.deliveries[process_id].items():
                seen = set()
                for msg_id in messages:
                    if msg_id in seen:
                        violations.append(f"Process {process_id} delivered ({sender_id}, {msg_id}) multiple times")
                    seen.add(msg_id)
        
        if violations:
            print("NO DUPLICATION VIOLATED:")
            for v in violations[:10]:  # Show first 10
                print(f"  - {v}")
            return False
        else:
            print("NO DUPLICATION satisfied")
            return True
    
    def verify_no_creation(self):
        """URB No Creation: If a process delivers m with sender s, then s previously broadcast m"""
        print("\n" + "=" * 80)
        print("Checking NO CREATION property...")
        violations = []
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, delivered_msgs in self.deliveries[process_id].items():
                broadcasted = set(self.broadcasts[sender_id])
                
                for msg_id in delivered_msgs:
                    if msg_id not in broadcasted:
                        violations.append(f"Process {process_id} delivered ({sender_id}, {msg_id}) but {sender_id} never broadcast it")
        
        if violations:
            print("NO CREATION VIOLATED:")
            for v in violations[:10]:
                print(f"  - {v}")
            return False
        else:
            print("NO CREATION satisfied")
            return True
    
    def verify_uniform_agreement(self):
        """URB Uniform Agreement: If a process delivers m, then all correct processes eventually deliver m"""
        print("\n" + "=" * 80)
        print("Checking UNIFORM AGREEMENT property...")
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
                    violations.append(f"Message ({sender_id}, {msg_id}) delivered by some but not all correct processes. Missing: {sorted(missing)[:5]}")
        
        if violations:
            print("UNIFORM AGREEMENT VIOLATED:")
            for v in violations[:10]:
                print(f"  - {v}")
            return False
        else:
            print("UNIFORM AGREEMENT satisfied")
            return True
    
    def verify_fifo_order(self):
        """FIFO Order: Messages from the same sender are delivered in the order they were broadcast"""
        print("\n" + "=" * 80)
        print("Checking FIFO ORDER property...")
        violations = []
        
        for process_id in range(1, self.num_processes + 1):
            for sender_id, delivered_msgs in self.deliveries[process_id].items():
                # Check if delivered messages are in ascending order
                for i in range(len(delivered_msgs) - 1):
                    if delivered_msgs[i] > delivered_msgs[i + 1]:
                        violations.append(
                            f"Process {process_id} delivered from {sender_id} out of order: "
                            f"{delivered_msgs[i]} before {delivered_msgs[i + 1]}"
                        )
                        break  # One violation per (process, sender) pair is enough
        
        if violations:
            print("FIFO ORDER VIOLATED:")
            for v in violations[:10]:
                print(f"  - {v}")
            return False
        else:
            print("FIFO ORDER satisfied")
            return True
    
    def run_verification(self):
        """Run all verification checks"""
        print("\n" + "=" * 80)
        print("STARTING VERIFICATION")
        print("=" * 80)
        
        results = {
            "Validity": self.verify_validity(),
            "No Duplication": self.verify_no_duplication(),
            "No Creation": self.verify_no_creation(),
            "Uniform Agreement": self.verify_uniform_agreement(),
            "FIFO Order": self.verify_fifo_order()
        }
        
        print("\n" + "=" * 80)
        print("VERIFICATION SUMMARY")
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
    # Configuration
    stress_cmd = [
        "python3", "../tools/stress.py", "fifo",
        "-r", "../template_cpp/run.sh",
        "-l", "../test_output/",
        "-p", "10",
        "-m", "1000"
    ]
    
    output_dir = "../test_output/"
    num_processes = 10
    
    # Create verifier
    verifier = FIFOURBVerifier(output_dir, num_processes)
    
    # Run stress test
    success = verifier.run_stress_test(stress_cmd)
    if not success:
        print("\n⚠️  Stress test reported errors, but continuing verification...")
    
    # Parse output files
    verifier.parse_output_files()
    
    # Run verification
    all_passed = verifier.run_verification()
    
    # Exit with appropriate code
    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()