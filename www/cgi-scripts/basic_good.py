#!/usr/bin/python3
# -*- coding: utf-8 -*-

print("Status: 200 OK")
print("Content-Type: text/plain")
print("X-Debug-Test: Large-Data-Stream")
print()

for i in range(800):
    print(f"[{i:03d}] This is a lot of data to fill the pipe buffer and test WebServer non-blocking read...")