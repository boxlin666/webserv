#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import uuid
import time
import urllib.parse

print("Content-Type: text/html; charset=utf-8\r")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

database_dir = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "database"))
success_page_path = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "login", "success.html"))

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

#------------------main logic here------------------
"""Authentification with cookie.txt file case"""
"""curl -v -b ./webserv_cookie.txt http://localhost:8080/auth/auth_check.py"""
if user_id and os.path.exists(os.path.join(database_dir, f"{user_id}.txt")):
    with open(os.path.join(database_dir, f"{user_id}.txt"), "r") as f:
        lines = f.readlines()
    
    # fetch the login information from the database folder 
    username = lines[0].strip()
    password = lines[1].strip()
    chosen_color = lines[2].strip()

    # finish up the cgi header print 
    print("\r")

    #print the cgi response body part, we fetch the sucecess.html file and replace the keyword below with database info
    with open(success_page_path, "r", encoding="utf-8") as page:
        print(page.read()
              .replace("{{USERNAME}}", username)
              .replace("{{PASSWORD}}", password)
              .replace("{{COLOR}}", chosen_color.upper()))

#if the cookie.txt does not exist!
else:
    """curl -v -b ./no_exist_cookie.txt http://localhost:8080/auth/auth_check.py""" 
    print("Status: 403 Forbidden\r")
    print("\r")
    print("the Cookie file is missing or incorrect!")
