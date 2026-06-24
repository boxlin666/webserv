#!/usr/bin/env python3
import sys
import time

def main():
    print("Content-Type: text/plain\r\n\r\n", end="")
    sys.stdout.flush()

    try:
        while True:
            print("Hello world!")
            #time.sleep(0.0001)

    except BrokenPipeError:
        print("[CGI Debug] Client disconnected.", file=sys.stderr)
        sys.exit(0)

    except KeyboardInterrupt:
        sys.exit(0)

if __name__ == "__main__":
    main()
