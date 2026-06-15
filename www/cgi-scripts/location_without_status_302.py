#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Intentionally omitting the "Status:" header.
print("Location: https://www.google.com")
print("Content-Type: text/html")
print() # Blank line indicating the end of HTTP headers

# Body content (browsers usually ignore this during a successful redirect)
print("<html><body><h1>Redirecting you to Google... (The server should automatically supply a 302 status code)</h1></body></html>")