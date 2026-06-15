#!/usr/bin/env python3
import sys
import time

def main():
    print("Content-Type: text/plain\r\n\r\n", end="")
    sys.stdout.flush()

    loop_times = 10000000
    try:
        while loop_times:
            print("Hello world!")
            loop_times -= 1
            time.sleep(0.0001)

    except BrokenPipeError:
        print("[CGI Debug] Client disconnected.", file=sys.stderr)
        sys.exit(0)

    except KeyboardInterrupt:
        sys.exit(0)

if __name__ == "__main__":
    main()