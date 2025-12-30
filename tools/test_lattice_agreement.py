#!/usr/bin/env python3

import sys
import argparse
from pathlib import Path
from collections import defaultdict

class LatticeAgreementValidator:
    def __init__(self, log_dir, num_processes, failed_processes):
        self.log_dir = Path(log_dir)
        self.num_processes = num_processes
        self.failed_processes = set(failed_processes)
        self.num_shots = 0
        
        # Structure: proposals[shot_index][process_id] = set(elements)
        self.proposals = defaultdict(dict)
        # Structure: decisions[shot_index][process_id] = set(elements)
        self.decisions = defaultdict(dict)

    def parse_files(self):
        """Parses config files for proposals and output files for decisions."""
        print(f"Parsing files in {self.log_dir}...")

        for i in range(1, self.num_processes + 1):
            config_file = self.log_dir / f"proc{i:02d}.config"
            output_file = self.log_dir / f"proc{i:02d}.output"

            # Parse Proposals from Config
            if config_file.exists():
                with open(config_file, 'r') as f:
                    lines = [line.strip() for line in f if line.strip()]
                    if lines:
                        # First line: "shots processes ..."
                        self.num_shots = int(lines[0].split()[0])
                        # Subsequent lines are proposals for each shot
                        for shot_idx, line in enumerate(lines[1:]):
                            if shot_idx < self.num_shots:
                                val_set = set(map(int, line.split()))
                                self.proposals[shot_idx][i] = val_set

            # Parse Decisions from Output
            if output_file.exists():
                with open(output_file, 'r') as f:
                    lines = [line.strip() for line in f if line.strip()]
                    for shot_idx, line in enumerate(lines):
                        if shot_idx < self.num_shots:
                            val_set = set(map(int, line.split()))
                            self.decisions[shot_idx][i] = val_set

        print(f"Parsed data for {self.num_shots} shots across {self.num_processes} processes.\n")

    def verify_la1_validity(self, shot):
        """LA1: I_i subset O_i AND O_i subset Union of all I_j"""
        violations = []
        all_proposals_union = set().union(*self.proposals[shot].values())

        for p_id, O_i in self.decisions[shot].items():
            I_i = self.proposals[shot].get(p_id, set())
            
            # Part 1: I_i ⊆ O_i
            if not I_i.issubset(O_i):
                violations.append(f"Proc {p_id} decision {O_i} does not contain its proposal {I_i}")
            
            # Part 2: O_i ⊆ ∪ I_j
            if not O_i.issubset(all_proposals_union):
                extra = O_i - all_proposals_union
                violations.append(f"Proc {p_id} decided elements {extra} not proposed by anyone")

        return violations

    def verify_la2_consistency(self, shot):
        """LA2: O_i ⊆ O_j or O_j ⊆ O_i"""
        violations = []
        p_ids = list(self.decisions[shot].keys())
        
        for i in range(len(p_ids)):
            for j in range(i + 1, len(p_ids)):
                O_i = self.decisions[shot][p_ids[i]]
                O_j = self.decisions[shot][p_ids[j]]
                
                if not (O_i.issubset(O_j) or O_j.issubset(O_i)):
                    violations.append(f"Consistency failed between Proc {p_ids[i]} and {p_ids[j]} (Incomparable sets)")
        
        return violations

    def verify_la3_termination(self, shot):
        """LA3: Every correct process eventually decides"""
        violations = []
        correct_processes = set(range(1, self.num_processes + 1)) - self.failed_processes
        
        for p_id in correct_processes:
            if p_id not in self.decisions[shot]:
                violations.append(f"Correct process {p_id} did not decide for shot {shot}")
        
        return violations

    def run_validation(self):
        results = {"Validity": True, "Consistency": True, "Termination": True}
        
        for shot in range(self.num_shots):
            # print(f"--- Validating Shot {shot + 1}/{self.num_shots} ---")
            
            v_errors = self.verify_la1_validity(shot)
            c_errors = self.verify_la2_consistency(shot)
            t_errors = self.verify_la3_termination(shot)

            if v_errors or c_errors or t_errors:
                print(f"    Shot {shot+1}/{self.num_shots} failed validation:")

            if v_errors:
                results["Validity"] = False
                for e in v_errors[:3]: print(f"  [LA1 FAIL] {e}")
            
            if c_errors:
                results["Consistency"] = False
                for e in c_errors[:3]: print(f"  [LA2 FAIL] {e}")
                
            if t_errors:
                results["Termination"] = False
                for e in t_errors[:3]: print(f"  [LA3 FAIL] {e}")

        print("\nFINAL SUMMARY")
        print("=" * 30)
        for prop, passed in results.items():
            print(f"{prop:15}: {'PASS' if passed else 'FAIL'}")
        return all(results.values())

def main():
    parser = argparse.ArgumentParser(description='Validate Lattice Agreement properties')
    parser.add_argument('-l', '--log-dir', required=True, help='Directory with config and output files')
    parser.add_argument('-p', '--processes', type=int, required=True, help='Total number of processes')
    parser.add_argument('-f', '--failed', nargs='*', type=int, default=[], help='List of failed process IDs')

    args = parser.parse_args()
    validator = LatticeAgreementValidator(args.log_dir, args.processes, args.failed)
    validator.parse_files()
    success = validator.run_validation()
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()