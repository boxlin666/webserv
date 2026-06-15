#!/usr/bin/env python3
# -*- coding: utf-8 -*-

print("Status: 200 OK")
print("Content-Type: text/html")

# Conflict: You just declared it's HTML, why push an image type again?
print("Content-Type: image/jpeg") 

print("X-Debug-Server: CatServer/1.0.0")
print() # Blank line indicating the end of HTTP headers

print("<html><body><h1>Test: Duplicate Content-Type!</h1><p>Although the script specifies a secondary image/jpeg header, you see this page because your WebServer successfully resolved the conflict or threw a 502 Bad Gateway!</p></body></html>")