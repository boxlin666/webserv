#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Declaring a redirect status code.
print("Status: 302 Found")
# Intentionally missing the "Location:" header (or leaving it empty).
print("Content-Type: text/html")
print() # Blank line indicating the end of HTTP headers

print("<html><body><h1>You should not see this page. The server must intercept this and return a 502 Bad Gateway error!</h1></body></html>")