from benchmarks.throughput import bench


def main():
    cas, std = bench(1000)
    print(f"casyncio: {cas:.6f}s")
    print(f"asyncio: {std:.6f}s")


if __name__ == "__main__":
    main()
