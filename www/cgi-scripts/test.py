#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/plain\r\n\r\n")
# 读取 POST 的 Body 内容
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
if content_length > 0:
    body = sys.stdin.read(content_length)
    print(f"Received POST body: {body}")
else:
    print("No body received.")