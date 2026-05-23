#!/usr/bin/env python3
import sys
import os

# 1. 强制刷新输出
sys.stdout.reconfigure(line_buffering=False, write_through=True)

# 2. 输出符合标准的 Header
# 注意：空行后面必须紧跟 Body
print("Status: 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello World!")