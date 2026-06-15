#!/usr/bin/env python3
# 故意在 Content-Type 后面混入 \r\n 并注入伪造的 Header
print("Content-Type: text/html\r\nContent-Length: 0\r\n\r\nHTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>You got hacked!</h1>")
print()