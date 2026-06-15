#!/usr/bin/env python3
# -*- coding: utf-8 -*-

print("Status: 200 OK")
# Conflict: Since it's a successful response (200), why redirect?
print("Location: /index.html") 
print("Content-Type: text/html")
print() # Blank line indicating the end of HTTP headers

print("<html><body><h1>Although the script specifies a Location header, you see this page instead of being redirected because the status code is 200 OK!</h1></body></html>")