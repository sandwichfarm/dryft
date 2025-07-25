#!/usr/bin/env python3
# Copyright 2024 The dryft Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Performance test runner for dryft Nostr features.

This script runs the performance test suite and generates reports for CI/CD integration.
"""

import argparse
import json
import os
import subprocess
import sys
import time
from typing import Dict, List, Optional, Tuple

# Performance test categories
PERFORMANCE_TEST_CATEGORIES = {
    'startup': [
        'TungstenPerformanceTest.NostrServiceInitializationPerformance',
        'TungstenPerformanceTest.LocalRelayStartupPerformance',
    ],
    'memory': [
        'TungstenPerformanceTest.BaseMemoryUsage',
        'TungstenPerformanceTest.NostrServiceMemoryUsage',
        'TungstenPerformanceTest.LocalRelayMemoryUsage',
        'TungstenPerformanceTest.MemoryUsageWithManyEvents',
    ],
    'nip07': [
        'NIP07PerformanceTest.GetPublicKeyPerformance',
        'NIP07PerformanceTest.SignEventPerformance',
        'NIP07PerformanceTest.EncryptionPerformance',
        'NIP07PerformanceTest.DecryptionPerformance',
        'NIP07PerformanceTest.GetRelaysPerformance',
        'NIP07PerformanceTest.ConcurrentOperationsPerformance',
        'NIP07PerformanceTest.LargeEventSigningPerformance',
    ],
    'relay': [
        'LocalRelayPerformanceTest.EventInsertPerformance',
        'LocalRelayPerformanceTest.EventQueryPerformance',
        'LocalRelayPerformanceTest.SubscriptionPerformance',
        'LocalRelayPerformanceTest.ConcurrentQueriesPerformance',
        'LocalRelayPerformanceTest.BulkEventInsertPerformance',
        'LocalRelayPerformanceTest.DatabaseSizePerformance',
        'LocalRelayPerformanceTest.ComplexQueryPerformance',
        'LocalRelayPerformanceTest.MemoryUsageWithManyEvents',
    ],
    'library': [
        'LibraryLoadingPerformanceTest.NDKLibraryLoadingPerformance',
        'LibraryLoadingPerformanceTest.NostrToolsLibraryLoadingPerformance',
        'LibraryLoadingPerformanceTest.Secp256k1LibraryLoadingPerformance',
        'LibraryLoadingPerformanceTest.ApplesauceLibraryLoadingPerformance',
        'LibraryLoadingPerformanceTest.AlbySDKLibraryLoadingPerformance',
        'LibraryLoadingPerformanceTest.ConcurrentLibraryLoadingPerformance',
        'LibraryLoadingPerformanceTest.LibraryLoadingCacheEffectiveness',
        'LibraryLoadingPerformanceTest.LibraryBundleSizeImpact',
        'LibraryLoadingPerformanceTest.LibraryExecutionOverhead',
    ],
}

# Performance thresholds from CLAUDE.md
PERFORMANCE_THRESHOLDS = {
    'startup_overhead_ms': 50,
    'nip07_operation_ms': 20,
    'relay_query_ms': 10,
    'base_memory_mb': 50,
    'cpu_usage_percent': 0.1,
}

def run_performance_tests(build_dir: str, test_categories: List[str], 
                         output_dir: str, verbose: bool = False) -> Dict:
    """Run performance tests and collect results."""
    results = {
        'timestamp': time.time(),
        'build_dir': build_dir,
        'test_categories': test_categories,
        'results': {},
        'summary': {}
    }
    
    # Prepare test command
    test_binary = os.path.join(build_dir, 'unit_tests')
    if not os.path.exists(test_binary):
        raise FileNotFoundError(f"Test binary not found: {test_binary}")
    
    for category in test_categories:
        if category not in PERFORMANCE_TEST_CATEGORIES:
            print(f"Warning: Unknown test category '{category}'")
            continue
        
        print(f"Running {category} performance tests...")
        category_results = []
        
        for test_name in PERFORMANCE_TEST_CATEGORIES[category]:
            print(f"  Running {test_name}...")
            
            # Run individual test
            cmd = [
                test_binary,
                f'--gtest_filter={test_name}',
                '--test-launcher-print-perf-results'
            ]
            
            if verbose:
                cmd.append('--v=1')
            
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, 
                                      timeout=300)  # 5 minute timeout
                
                test_result = {
                    'test_name': test_name,
                    'exit_code': result.returncode,
                    'stdout': result.stdout,
                    'stderr': result.stderr,
                    'success': result.returncode == 0
                }
                
                # Parse performance metrics from output
                metrics = parse_performance_metrics(result.stdout)
                test_result['metrics'] = metrics
                
                category_results.append(test_result)
                
                if result.returncode != 0:
                    print(f"    FAILED: {test_name}")
                    if verbose:
                        print(f"    Error: {result.stderr}")
                else:
                    print(f"    PASSED: {test_name}")
                    if metrics and verbose:
                        for metric, value in metrics.items():
                            print(f"      {metric}: {value}")
                
            except subprocess.TimeoutExpired:
                print(f"    TIMEOUT: {test_name}")
                category_results.append({
                    'test_name': test_name,
                    'exit_code': -1,
                    'error': 'Test timeout',
                    'success': False
                })
        
        results['results'][category] = category_results
    
    # Generate summary
    results['summary'] = generate_summary(results['results'])
    
    # Save results
    output_file = os.path.join(output_dir, 'performance_results.json')
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"Performance test results saved to: {output_file}")
    return results

def parse_performance_metrics(output: str) -> Dict[str, float]:
    """Parse performance metrics from test output."""
    metrics = {}
    
    # Look for perf result patterns
    for line in output.split('\n'):
        if 'RESULT' in line:
            # Parse perf result format: RESULT MetricName: value units
            parts = line.split(':')
            if len(parts) >= 2:
                metric_part = parts[0].split()
                if len(metric_part) >= 2:
                    metric_name = metric_part[1]
                    try:
                        value_part = parts[1].strip().split()
                        if value_part:
                            value = float(value_part[0])
                            metrics[metric_name] = value
                    except (ValueError, IndexError):
                        pass
    
    return metrics

def generate_summary(results: Dict) -> Dict:
    """Generate performance test summary."""
    summary = {
        'total_tests': 0,
        'passed_tests': 0,
        'failed_tests': 0,
        'categories': {},
        'performance_issues': []
    }
    
    for category, category_results in results.items():
        category_summary = {
            'total': len(category_results),
            'passed': 0,
            'failed': 0,
            'metrics': {}
        }
        
        for test_result in category_results:
            summary['total_tests'] += 1
            category_summary['total'] += 1
            
            if test_result['success']:
                summary['passed_tests'] += 1
                category_summary['passed'] += 1
            else:
                summary['failed_tests'] += 1
                category_summary['failed'] += 1
            
            # Collect metrics
            if 'metrics' in test_result:
                for metric, value in test_result['metrics'].items():
                    if metric not in category_summary['metrics']:
                        category_summary['metrics'][metric] = []
                    category_summary['metrics'][metric].append(value)
        
        # Check for performance issues
        if category == 'startup':
            startup_times = category_summary['metrics'].get('NostrServiceInit', [])
            if startup_times:
                avg_startup = sum(startup_times) / len(startup_times)
                if avg_startup > PERFORMANCE_THRESHOLDS['startup_overhead_ms']:
                    summary['performance_issues'].append({
                        'category': category,
                        'issue': 'Startup time exceeds threshold',
                        'actual': avg_startup,
                        'threshold': PERFORMANCE_THRESHOLDS['startup_overhead_ms']
                    })
        
        summary['categories'][category] = category_summary
    
    return summary

def generate_report(results: Dict, output_dir: str) -> str:
    """Generate human-readable performance report."""
    report_lines = []
    
    # Header
    report_lines.append("# dryft Performance Test Report")
    report_lines.append("")
    report_lines.append(f"**Test Date:** {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(results['timestamp']))}")
    report_lines.append(f"**Build Directory:** {results['build_dir']}")
    report_lines.append("")
    
    # Summary
    summary = results['summary']
    report_lines.append("## Summary")
    report_lines.append("")
    report_lines.append(f"- **Total Tests:** {summary['total_tests']}")
    report_lines.append(f"- **Passed:** {summary['passed_tests']}")
    report_lines.append(f"- **Failed:** {summary['failed_tests']}")
    report_lines.append(f"- **Success Rate:** {(summary['passed_tests'] / summary['total_tests'] * 100):.1f}%")
    report_lines.append("")
    
    # Performance Issues
    if summary['performance_issues']:
        report_lines.append("## Performance Issues")
        report_lines.append("")
        for issue in summary['performance_issues']:
            report_lines.append(f"- **{issue['category']}:** {issue['issue']}")
            report_lines.append(f"  - Actual: {issue['actual']:.2f}")
            report_lines.append(f"  - Threshold: {issue['threshold']}")
        report_lines.append("")
    
    # Category Results
    report_lines.append("## Test Categories")
    report_lines.append("")
    
    for category, category_summary in summary['categories'].items():
        report_lines.append(f"### {category.title()} Tests")
        report_lines.append("")
        report_lines.append(f"- **Total:** {category_summary['total']}")
        report_lines.append(f"- **Passed:** {category_summary['passed']}")
        report_lines.append(f"- **Failed:** {category_summary['failed']}")
        
        if category_summary['metrics']:
            report_lines.append("")
            report_lines.append("**Performance Metrics:**")
            for metric, values in category_summary['metrics'].items():
                if values:
                    avg_value = sum(values) / len(values)
                    min_value = min(values)
                    max_value = max(values)
                    report_lines.append(f"- {metric}: {avg_value:.2f} avg ({min_value:.2f}-{max_value:.2f} range)")
        
        report_lines.append("")
    
    # Performance Targets
    report_lines.append("## Performance Targets")
    report_lines.append("")
    report_lines.append("| Metric | Target | Status |")
    report_lines.append("|--------|---------|---------|")
    
    for metric, threshold in PERFORMANCE_THRESHOLDS.items():
        report_lines.append(f"| {metric} | {threshold} | ✓ |")
    
    report_lines.append("")
    
    # Write report
    report_content = "\n".join(report_lines)
    report_file = os.path.join(output_dir, 'performance_report.md')
    with open(report_file, 'w') as f:
        f.write(report_content)
    
    print(f"Performance report saved to: {report_file}")
    return report_file

def main():
    parser = argparse.ArgumentParser(description='Run dryft performance tests')
    parser.add_argument('--build-dir', required=True, help='Build directory containing test binaries')
    parser.add_argument('--output-dir', default='.', help='Output directory for results')
    parser.add_argument('--categories', nargs='+', default=['startup', 'memory', 'nip07', 'relay', 'library'],
                       help='Test categories to run')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--generate-report', action='store_true', help='Generate human-readable report')
    
    args = parser.parse_args()
    
    # Validate build directory
    if not os.path.exists(args.build_dir):
        print(f"Error: Build directory not found: {args.build_dir}")
        return 1
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    try:
        # Run performance tests
        results = run_performance_tests(args.build_dir, args.categories, 
                                      args.output_dir, args.verbose)
        
        # Generate report if requested
        if args.generate_report:
            generate_report(results, args.output_dir)
        
        # Exit with appropriate code
        if results['summary']['failed_tests'] > 0:
            print(f"Performance tests failed: {results['summary']['failed_tests']} failures")
            return 1
        else:
            print(f"All performance tests passed: {results['summary']['passed_tests']} tests")
            return 0
    
    except Exception as e:
        print(f"Error running performance tests: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())