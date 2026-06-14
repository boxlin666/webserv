#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import urllib.parse

print("Content-Type: text/html; charset=utf-8\r")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
database_dir = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "database"))

def get_user_id_from_cookie():
    """Fetch the HTTP_COOKIE envp prepared by the parent process"""
    cookies = os.environ.get('HTTP_COOKIE', '')
    if cookies:
        for cookie in cookies.split(';'):
            parts = cookie.strip().split('=', 1)
            if len(parts) == 2 and parts[0] == "id":
                return parts[1]
    return None

user_id = get_user_id_from_cookie()

# remove the session id named file in database 
"""curl -v -X POST -b ./webserv_cookie.txt -c ./webserv_cookie.txt -L http://localhost:8080/logout/logout.py (POST + POST)"""
"""curl -v -L -b ./webserv_cookie.txt -c ./webserv_cookie.txt -d "" http://localhost:8080/logout/logout.py (POST + GET)"""
if user_id:
    session_file = os.path.join(database_dir, f"{user_id}.txt")
    if os.path.exists(session_file):
        try:
            os.remove(session_file) # delete session_id.txt file in database folder 
        except Exception:
            pass

# return to the login_index.html webpage and clean up the data inside cookie.txt file
print("Status: 302 Found\r")
print("Location: /login/login_index.html\r")
print("Set-Cookie: id=deleted; Expires=Thu, 01-Jan-1970 00:00:00 GMT; Path=/; Max-Age=0\r")
print("Set-Cookie: lab_mutation=deleted; Expires=Thu, 01-Jan-1970 00:00:00 GMT; Path=/; Max-Age=0\r")

print("\r")