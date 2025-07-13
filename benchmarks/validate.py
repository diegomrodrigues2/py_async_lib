from benchmarks.throughput import bench


def main():
    dur = bench(1000)
    print(f"runtime: {dur:.6f}s")


if __name__ == "__main__":
    main()
