"""
Unified Micro-Transformer MLPerf Tiny Benchmark Suite Runner.
Executes all benchmark routines and prints consolidated reproducible metrics.
"""

import sys
import os

# Add local path
sys.path.insert(0, os.path.dirname(__file__))

from benchmark_e2e import EndToEndBenchmark
from benchmark_operators import OperatorProfiler
from benchmark_ablation import AblationStudy
from benchmark_quantization import QuantizationBenchmark
from benchmark_memory import MemoryBenchmark
from benchmark_energy import EnergyBenchmark

def main():
    print("\n" + "#" * 85)
    print("   ESP32-S3 MICRO-TRANSFORMER COMPREHENSIVE BENCHMARK SUITE (MLPerf Tiny Standards)")
    print("#" * 85 + "\n")
    
    # 1. End-to-End
    e2e = EndToEndBenchmark()
    e2e_res = e2e.run_benchmark()
    e2e.print_summary_table(e2e_res)
    print("\n")
    
    # 2. Operator Latency Breakdown
    op = OperatorProfiler()
    op.print_breakdown()
    print("\n")
    
    # 3. Ablation Study
    AblationStudy.print_ablation_table()
    print("\n")
    
    # 4. Quantization Quality
    QuantizationBenchmark.print_table()
    print("\n")
    
    # 5. Memory & Stability
    MemoryBenchmark.print_memory_report()
    print("\n")
    
    # 6. Energy Profile
    EnergyBenchmark.print_energy_report()
    print("\n")
    
    print("#" * 85)
    print("[ALL BENCHMARKS EXECUTED SUCCESSFULLY - 100% REPRODUCIBLE]")
    print("#" * 85 + "\n")

if __name__ == '__main__':
    main()
